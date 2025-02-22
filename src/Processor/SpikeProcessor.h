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
                                    addSpikeForAnalysis(spike);
                                }else{
                                    spike.minVoltage = frame.ac_uV[probe];
                                    spike.minSampleIndex = halfLength;
                                    spike.maxVoltage = peakVoltage;
                                    spike.maxSampleIndex = halfLength + peakOffsetIndex;
                                    spike.acquisitionTime = buffer.getFrameAt(centralSampleIDX).getAcquisitionTime();
                                    float* rawAcUv = buffer.getAcuVFloatRaw(probe, centralSampleIDX - halfLength);
                                    std::memcpy(&spike.rawWaveform[0], &rawAcUv[0], sizeof(float) * settings.spikeWaveformLengthSamples);
                                    addSpikeForAnalysis(spike);
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
                                    addSpikeForAnalysis(spike);
                                }else{
                                    ONI::Spike& spike = probeSpikes[probe][spikeIndexes[probe]];
                                    spike.minVoltage = troughVoltage;
                                    spike.minSampleIndex = halfLength;
                                    spike.maxVoltage = frame.ac_uV[probe];
                                    spike.maxSampleIndex = halfLength + troughOffsetIndex;
                                    spike.acquisitionTime = frame.getAcquisitionTime();
                                    float* rawAcUv = buffer.getAcuVFloatRaw(probe, centralSampleIDX - troughOffsetIndex - halfLength);
                                    std::memcpy(&spike.rawWaveform[0], &rawAcUv[0], sizeof(float) * settings.spikeWaveformLengthSamples);
                                    addSpikeForAnalysis(spike);
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

            if(allSpikes.size() >= spikeSampleSize){
                LOGDEBUG("Let's analyse the spikes %i", allSpikes.size());
                allSpikes.clear();
                allWaveforms.clear();
                using Eigen::MatrixXd;
                MatrixXd m(2, 2);
                m(0, 0) = 3;
                m(1, 0) = 2.5;
                m(0, 1) = -1;
                m(1, 1) = m(1, 0) + m(0, 1);
                std::cout << m << std::endl;
            }

            spikeMutex.unlock();

        }

    }

    inline void addSpikeForAnalysis(const Spike& spike){
        if(allSpikes.size() < spikeSampleSize){
            allSpikes.push_back(spike);
            allWaveforms.push_back(spike.rawWaveform);
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

    size_t spikeSampleSize = 200;
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










