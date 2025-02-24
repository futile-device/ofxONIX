//
//  BaseDevice.h
//
//  Created by Matt Gingold on 13.09.2024.
//
#include <Eigen/Dense>
#include "oni.h"
#include "onix.h"

#include <cassert>
#include <string>
#include <vector>
#include <map>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <syncstream>

#include "../Type/Log.h"
#include "../Type/RegisterTypes.h"
#include "../Type/SettingTypes.h"
#include "../Type/FrameTypes.h"
#include "../Type/GlobalTypes.h"

#include "../Processor/BaseProcessor.h"
#include "../Processor/Rhs2116MultiProcessor.h"

#pragma once

namespace ONI{

struct Spike{

    size_t probe = 0;
    std::vector<float> rawWaveform;
    size_t acquisitionTime = 0;
    size_t maxSampleIndex = 0;
    size_t minSampleIndex = 0;
    float minVoltage = INFINITY;
    float maxVoltage = -INFINITY;

        // copy assignment (copy-and-swap idiom)
    Spike& Spike::operator=(Spike other) noexcept{
        std::swap(probe, other.probe);
        std::swap(rawWaveform, other.rawWaveform);
        std::swap(acquisitionTime, other.acquisitionTime);
        std::swap(maxSampleIndex, other.maxSampleIndex);
        std::swap(minSampleIndex, other.minSampleIndex);
        std::swap(minVoltage, other.minVoltage);
        std::swap(maxVoltage, other.maxVoltage);
        return *this;
    }

};

inline bool operator==(const Spike& lhs, const Spike& rhs){
    return (lhs.probe == rhs.probe &&
            lhs.rawWaveform == rhs.rawWaveform &&
            lhs.acquisitionTime == rhs.acquisitionTime &&
            lhs.maxSampleIndex == rhs.maxSampleIndex &&
            lhs.minSampleIndex == rhs.minSampleIndex &&
            lhs.minVoltage == rhs.minVoltage &&
            lhs.maxVoltage == rhs.maxVoltage);
}
inline bool operator!=(const Spike& lhs, const Spike& rhs) { return !(lhs == rhs); }

namespace Interface{
class SpikeInterface;
};

namespace Processor{

class SpikeProcessor : public BaseProcessor{

public:

    friend class ONI::Interface::SpikeInterface;

    SpikeProcessor(){
        BaseProcessor::processorTypeID = ONI::Processor::TypeID::SPIKE_PROCESSOR;
        BaseProcessor::processorName = toString(processorTypeID);
    }

	~SpikeProcessor(){
        LOGDEBUG("SpikeProcessor DTOR");
        bThread = false;
        if(thread.joinable()) thread.join();
    };

    void setup(ONI::Processor::BufferProcessor* source){

        LOGDEBUG("Setting up SpikeProcessor Processor");

        assert(source != nullptr);

        bufferProcessor = source;
        //bufferProcessor->subscribeProcessor("SpikeProcessor", ONI::Processor::SubscriptionType::POST_PROCESSOR, this);

        BaseProcessor::numProbes = bufferProcessor->getNumProbes();

        settings.spikeEdgeDetectionType = ONI::Settings::SpikeEdgeDetectionType::EITHER;

        settings.positiveDeviationMultiplier = 4.5f;
        settings.negativeDeviationMultiplier = 4.5f;

        settings.spikeWaveformLengthMs = 3;
        settings.spikeWaveformLengthSamples = settings.spikeWaveformLengthMs * RHS2116_SAMPLE_FREQUENCY_MS;
        settings.spikeWaveformBufferSize = 10;

        reset();

    }

    void reset(){

        LOGINFO("SpikeProcessor RESET");

        bThread = false;
        if (thread.joinable()) thread.join();

        //probeStats.clear();
        nextPeekDetectBufferCount.clear();
        spikeIndexes.clear();
        spikeCounts.clear();
        probeSpikes.clear();

        //probeStats.resize(numProbes);
        nextPeekDetectBufferCount.resize(numProbes);
        spikeIndexes.resize(numProbes);
        spikeCounts.resize(numProbes);
        probeSpikes.resize(numProbes);

        for(size_t probe = 0; probe < numProbes; ++probe){
            nextPeekDetectBufferCount[probe] = 0;
            spikeIndexes[probe] = 0;
            spikeCounts[probe] = 0;
            probeSpikes[probe].resize(settings.spikeWaveformBufferSize);
            for(size_t i = 0; i < settings.spikeWaveformBufferSize; ++i){
                probeSpikes[probe][i].rawWaveform.resize(settings.spikeWaveformLengthSamples);
            }
        }

        bThread = true;
        thread = std::thread(&ONI::Processor::SpikeProcessor::processSpikes, this);

    };

	inline void process(oni_frame_t* frame){};
	inline void process(ONI::Frame::BaseFrame& frame){}

    void processSpikes() {

        while(bThread) {

            spikeMutex.lock();

            // get a reference to the dense buffer (ie., all samples)
            bufferProcessor->dataMutex[DENSE_MUTEX].lock();

            ONI::FrameBuffer& buffer = bufferProcessor->denseBuffer;

            if(buffer.isFrameNew()){ // remember that atm only one consumer will get the correct new frame as it's auto reset to false

                // get the buffer count which is the global counter for frames going into the buffer
                uint64_t bufferCount = buffer.getBufferCount();

                if(bufferCount > buffer.size()){ // wait until the buffer is full

                    // we are going to search for spikes from the 'central' time point in the frame buffer
                    size_t centralSampleIDX = size_t(std::floor(buffer.getCurrentIndex() + buffer.size() / 2.0)) % buffer.size();

                    // get a reference to the central "current" frame
                    ONI::Frame::Rhs2116MultiFrame& frame = buffer.getFrameAt(centralSampleIDX);
                    
                    // check each probe to see if there's a spike...
                    for(size_t probe = 0; probe < numProbes; ++probe) {
                        
                        // after we discover a spike on a probe channel we suppress detection till after the waveform capture TODO: what about overlapping spikes?
                        if(bufferCount < nextPeekDetectBufferCount[probe]) continue;

                        using ONI::Settings::SpikeEdgeDetectionType;


                        if(frame.ac_uV[probe] < -bufferProcessor->getProbeStats()[probe].deviation * settings.negativeDeviationMultiplier &&
                           (settings.spikeEdgeDetectionType == SpikeEdgeDetectionType::FALLING || 
                            settings.spikeEdgeDetectionType == SpikeEdgeDetectionType::EITHER ||
                            settings.spikeEdgeDetectionType == SpikeEdgeDetectionType::BOTH)){


                            // search forward for first peak index minSampleOffset forces the search to start a 
                            // //little bit after the initial detection to avoid false positive min/max post detection
                            size_t peakOffsetIndex = 0; float peakVoltage = 0;
                            for(size_t offset = settings.minSampleOffset + 1; offset < settings.spikeWaveformLengthSamples; ++offset){
                                float previous = buffer.getAcuVFloatRaw(probe, centralSampleIDX + offset - 1)[0];
                                float current = buffer.getAcuVFloatRaw(probe, centralSampleIDX + offset)[0];
                                if(previous > current){ // the next value is less than the last we are starting to fall
                                    peakOffsetIndex = offset - 1;
                                    peakVoltage = previous;
                                    break; // stop searching!
                                }
                            }
                            
                            if((settings.spikeEdgeDetectionType == SpikeEdgeDetectionType::FALLING || settings.spikeEdgeDetectionType == SpikeEdgeDetectionType::EITHER) ||
                               (settings.spikeEdgeDetectionType == SpikeEdgeDetectionType::BOTH && peakVoltage > bufferProcessor->getProbeStats()[probe].deviation * settings.positiveDeviationMultiplier)){ // ...reject if max voltage is not over the threshold
                                
                                size_t halfLength = std::floor(settings.spikeWaveformLengthSamples / 2);
 
                                // store the spike data and waveform
                                ONI::Spike& spike = probeSpikes[probe][spikeIndexes[probe]];
                                //spike.rawWaveform.resize(settings.spikeWaveformLengthSamples);
                                if(settings.bFallingAlignMax){ // by default align to the peaks
                                    spike.minVoltage = frame.ac_uV[probe];
                                    spike.minSampleIndex = halfLength - peakOffsetIndex;
                                    spike.maxVoltage = peakVoltage;
                                    spike.maxSampleIndex = halfLength;
                                    spike.acquisitionTime = buffer.getFrameAt(centralSampleIDX + peakOffsetIndex).getAcquisitionTime();
                                    float* rawAcUv = buffer.getAcuVFloatRaw(probe, centralSampleIDX - halfLength + peakOffsetIndex);
                                    std::memcpy(&spike.rawWaveform[0], &rawAcUv[0], sizeof(float) * settings.spikeWaveformLengthSamples);
                                    //addSpikeForAnalysis(spike);
                                }else{
                                    spike.minVoltage = frame.ac_uV[probe];
                                    spike.minSampleIndex = halfLength;
                                    spike.maxVoltage = peakVoltage;
                                    spike.maxSampleIndex = halfLength + peakOffsetIndex;
                                    spike.acquisitionTime = buffer.getFrameAt(centralSampleIDX).getAcquisitionTime();
                                    float* rawAcUv = buffer.getAcuVFloatRaw(probe, centralSampleIDX - halfLength);
                                    std::memcpy(&spike.rawWaveform[0], &rawAcUv[0], sizeof(float) * settings.spikeWaveformLengthSamples);
                                    //addSpikeForAnalysis(spike);
                                }

                                // cache this spike detection buffer count (so we can suppress re-detecting the same spike)
                                nextPeekDetectBufferCount[probe] = bufferCount + settings.spikeWaveformLengthSamples;// halfLength + peakOffsetIndex; // is this reasonable?
                                spikeIndexes[probe] = (spikeIndexes[probe] + 1) % settings.spikeWaveformBufferSize;
                                spikeCounts[probe]++;
                                //if(spikeCounts[probe] < settings.spikeWaveformBufferSize) spikeCounts[probe]++;

                            }


                        }
                        

                        if(frame.ac_uV[probe] > bufferProcessor->getProbeStats()[probe].deviation * settings.positiveDeviationMultiplier &&
                           (settings.spikeEdgeDetectionType == SpikeEdgeDetectionType::RISING || 
                            settings.spikeEdgeDetectionType == SpikeEdgeDetectionType::EITHER ||
                            settings.spikeEdgeDetectionType == SpikeEdgeDetectionType::BOTH)){


                            // search backward for first trough index
                            size_t troughOffsetIndex = 0; float troughVoltage = 0;
                            for(size_t offset = settings.minSampleOffset + 1; offset < settings.spikeWaveformLengthSamples; ++offset){
                                float previous = buffer.getAcuVFloatRaw(probe, centralSampleIDX - offset - 1)[0];
                                float current = buffer.getAcuVFloatRaw(probe, centralSampleIDX - offset)[0];
                                if(previous > current){ // the next value is less than the last we are starting to rise
                                    troughOffsetIndex = offset - 1;
                                    troughVoltage = current;
                                    break; // stop searching!
                                }
                            }


                            if((settings.spikeEdgeDetectionType == SpikeEdgeDetectionType::RISING || settings.spikeEdgeDetectionType == SpikeEdgeDetectionType::EITHER) ||
                               (settings.spikeEdgeDetectionType == SpikeEdgeDetectionType::BOTH && troughVoltage < -bufferProcessor->getProbeStats()[probe].deviation * settings.negativeDeviationMultiplier)){ // ...reject if min voltage is not under the threshold

                                size_t halfLength = std::floor(settings.spikeWaveformLengthSamples / 2);

                                // store the spike data and waveform
                                if(!settings.bRisingAlignMin){ // by default align to the peaks
                                    ONI::Spike& spike = probeSpikes[probe][spikeIndexes[probe]];
                                    //spike.rawWaveform.resize(settings.spikeWaveformLengthSamples);
                                    spike.minVoltage = troughVoltage;
                                    spike.minSampleIndex = halfLength - troughOffsetIndex;
                                    spike.maxVoltage = frame.ac_uV[probe];
                                    spike.maxSampleIndex = halfLength;
                                    spike.acquisitionTime = frame.getAcquisitionTime();
                                    float* rawAcUv = buffer.getAcuVFloatRaw(probe, centralSampleIDX - halfLength);
                                    std::memcpy(&spike.rawWaveform[0], &rawAcUv[0], sizeof(float) * settings.spikeWaveformLengthSamples);
                                    //addSpikeForAnalysis(spike);
                                }else{
                                    ONI::Spike& spike = probeSpikes[probe][spikeIndexes[probe]];
                                    spike.minVoltage = troughVoltage;
                                    spike.minSampleIndex = halfLength;
                                    spike.maxVoltage = frame.ac_uV[probe];
                                    spike.maxSampleIndex = halfLength + troughOffsetIndex;
                                    spike.acquisitionTime = frame.getAcquisitionTime();
                                    float* rawAcUv = buffer.getAcuVFloatRaw(probe, centralSampleIDX - troughOffsetIndex - halfLength);
                                    std::memcpy(&spike.rawWaveform[0], &rawAcUv[0], sizeof(float) * settings.spikeWaveformLengthSamples);
                                    //addSpikeForAnalysis(spike);
                                }
                                
                                

                                // cache this spike detection buffer count (so we can suppress re-detecting the same spike)
                                nextPeekDetectBufferCount[probe] = bufferCount + settings.spikeWaveformLengthSamples;//+ halfLength; // is this reasonable?
                                spikeIndexes[probe] = (spikeIndexes[probe] + 1) % settings.spikeWaveformBufferSize;
                                spikeCounts[probe]++;
                                
                                //if(spikeCounts[probe] < settings.spikeWaveformBufferSize) spikeCounts[probe]++;

                            }

                        }

                    }
                }

            }

            bufferProcessor->dataMutex[DENSE_MUTEX].unlock();

            

            spikeMutex.unlock();

        }

    }

    inline void addSpikeForAnalysis(const Spike& spike){
        if(allSpikes.size() < maxSpikeSampleSize){
            allSpikes.push_back(spike);
            allWaveforms.push_back(spike.rawWaveform);
        }
    }

    void processSVD(){

        if(allSpikes.size() >= maxSpikeSampleSize){

            LOGDEBUG("Let's analyse the spikes %i", allSpikes.size());

            //using Eigen::MatrixXf;
            fu::Timer svdTimer;
            svdTimer.start();
            Eigen::MatrixXf mat(allWaveforms.size(), allWaveforms[0].size());

            for(size_t i = 0; i < allWaveforms.size(); ++i){
                mat.row(i) = Eigen::Map<Eigen::VectorXf>(allWaveforms[i].data(), allWaveforms[i].size());
            }

            //mat.conservativeResize(maxSpikeSampleSize, maxSpikeSampleSize);
            //mat.normalize();

            Eigen::JacobiSVD<Eigen::MatrixXf> svd(mat, Eigen::ComputeFullU | Eigen::ComputeFullV);

            auto PCA = mat * svd.matrixV();

            Eigen::MatrixXf U =svd.matrixU();
            Eigen::MatrixXf V =svd.matrixV();
            Eigen::MatrixXf sigma = Eigen::MatrixXf::Zero(U.cols(), svd.singularValues().size());// = svd.singularValues().asDiagonal().toDenseMatrix();
            for(int i = 0; i < svd.singularValues().size(); ++i){
                sigma(i, i) = svd.singularValues()(i);
            }



            int N = 15;
            Eigen::MatrixXf sigma_NN = sigma;
            sigma_NN.conservativeResize(N, N);
            Eigen::MatrixXf U_NN =svd.matrixU();
            U_NN.conservativeResize(U_NN.rows(), N);
            Eigen::MatrixXf V_NN =svd.matrixV();
            V_NN.conservativeResize(V_NN.rows(), N);

            Eigen::MatrixXf recovered = U * sigma * V.transpose();
            Eigen::MatrixXf reduced = U_NN * sigma_NN * V_NN.transpose();



            LOGDEBUG("S: %i %i || U: %i %i || V: %i %i", sigma_NN.rows(), sigma_NN.cols(), U_NN.rows(), U_NN.cols(), V_NN.rows(), V_NN.cols());
            LOGDEBUG("SVD Time: %0.3f [%i, %i]", svdTimer.stop<fu::millis>(), mat.rows(), mat.cols());

            int idx = 2;
            for(int i = 0; i < recovered.cols(); ++i){
                LOGDEBUG("%0.6f => %0.6f == %0.6f =~ %0.6f", allWaveforms[idx][i], mat(idx, i), recovered(idx, i), reduced(idx, i));
            }
            idx = 42;
            for(int i = 0; i < recovered.cols(); ++i){
                LOGDEBUG("%0.6f => %0.6f == %0.6f =~ %0.6f", allWaveforms[idx][i], mat(idx, i), recovered(idx, i), reduced(idx, i));
            }

            std::cout << "sigma\n" << svd.singularValues() << std::endl;
            Eigen::VectorXf  wtf = svd.singularValues();
            wtf.normalize();
            std::cout << "sigma norm\n" << wtf << std::endl;

            Eigen::VectorXf cum = wtf;

            for(int i = 0; i < wtf.size(); ++i){
                if(i > 0) cum[i] = cum[i - 1] + wtf[i];
            }

            std::cout << "cum \n" << cum << std::endl;

            Eigen::VectorXf sigsq = wtf;
            Eigen::VectorXf cumsq = wtf;
            cumsq[0] = wtf[0] * wtf[0];

            for(int i = 0; i < wtf.size(); ++i){
                sigsq[i] = wtf[i] * wtf[i];
                if(i > 0) cumsq[i] = cumsq[i - 1] + sigsq[i];
            }

            std::cout << "nsig2\n" << sigsq << std::endl;
            //std::cout << "NSIG\n" << wtf * wtf << std::endl;
            std::cout << "cims2\n" << cumsq << std::endl;

            Eigen::VectorXf nexp = sigsq;
            Eigen::VectorXf cexp = sigsq;
            cexp[0] = sigsq[0] / cumsq[cumsq.size() - 1];
            int idx95 = -1;
            for(int i = 0; i < wtf.size(); ++i){
                nexp[i] = sigsq[i] / cumsq[cumsq.size() - 1];
                if(i > 0) cexp[i] = cexp[i - 1] + nexp[i];
                if(cexp[i] > 0.95 && idx95 == -1) idx95 = i;
            }

            std::cout << "idx95: " << idx95 << std::endl;
            std::cout << "nexp\n" << nexp << std::endl;
            std::cout << "cexp\n" << cexp << std::endl;



            allSpikes.clear();
            allWaveforms.clear();

        }

    }

    inline float getMillisPerStep(){
        return bufferProcessor->sparseBuffer.getMillisPerStep();
    }

    inline float getPosStDevMultiplied(const size_t& probe){
        return bufferProcessor->getProbeStats()[probe].deviation * settings.positiveDeviationMultiplier;
    }


    inline float getNegStDevMultiplied(const size_t& probe){
        return -bufferProcessor->getProbeStats()[probe].deviation * settings.negativeDeviationMultiplier;
    }

    inline float getStDev(const size_t& probe){
        return bufferProcessor->getProbeStats()[probe].deviation;
    }

    fu::Event<ONI::Spike> spikeEvent;
     

protected:

    ONI::Settings::SpikeSettings settings;

    std::vector<size_t> nextPeekDetectBufferCount;
    std::vector<size_t> spikeIndexes;
    std::vector<size_t> spikeCounts;

    size_t maxSpikeSampleSize = 200;
    std::vector< ONI::Spike > allSpikes;
    std::vector< std::vector<float> > allWaveforms;

    std::vector< std::vector< ONI::Spike > > probeSpikes;

    ONI::Processor::BufferProcessor* bufferProcessor;

    std::atomic_bool bThread = false;
    std::thread thread;
    std::mutex spikeMutex;

};


} // namespace Processor
} // namespace ONI










