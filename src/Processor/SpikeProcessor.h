//
//  BaseDevice.h
//
//  Created by Matt Gingold on 13.09.2024.
//

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
#include "../Type/DataBuffer.h"
#include "../Type/RegisterTypes.h"
#include "../Type/SettingTypes.h"
#include "../Type/FrameTypes.h"
#include "../Type/GlobalTypes.h"

#include "../Processor/BaseProcessor.h"
#include "../Processor/Rhs2116MultiProcessor.h"

#pragma once

namespace ONI{

struct Spike{
    std::vector<float> rawWaveform;
    size_t bufferCount = 0;
    size_t maxIDX = 0;
    size_t minIDX = 0;
    float minVoltage = INFINITY;
    float maxVoltage = -INFINITY;
};
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

	~SpikeProcessor(){};

    void setup(ONI::Processor::BufferProcessor* source){

        LOGDEBUG("Setting up SpikeProcessor Processor");

        assert(source != nullptr);

        bufferProcessor = source;
        //bufferProcessor->subscribeProcessor("SpikeProcessor", ONI::Processor::SubscriptionType::POST_PROCESSOR, this);

        BaseProcessor::numProbes = bufferProcessor->getNumProbes();

        standardDevWindowSzMs = 500;
        standardDevWindowSzSamples = standardDevWindowSzMs * RHS2116_SAMPLE_FREQUENCY_MS;

        spikeMsLength = 0.6;
        spikeSampleLength = spikeMsLength * RHS2116_SAMPLE_FREQUENCY_MS;
        spikeBufferSize = 10;

        

        reset();

    }

    void reset(){


        bThread = false;
        if (thread.joinable()) thread.join();

        probeStats.resize(numProbes);

        spikeDetectedLastBufferCount.resize(numProbes);
        spikeIndexes.resize(numProbes);
        spikeCounts.resize(numProbes);
        spikes.resize(numProbes);

        for(size_t probe = 0; probe < numProbes; ++probe){
            spikeDetectedLastBufferCount[probe] = 0;
            spikeIndexes[probe] = 0;
            spikeCounts[probe] = 0;
            spikes[probe].resize(spikeBufferSize);
            for(size_t i = 0; i < spikeBufferSize; ++i){
                spikes[probe][i].rawWaveform.resize(spikeSampleLength);
            }
        }

        bThread = true;
        thread = std::thread(&ONI::Processor::SpikeProcessor::processSpikes, this);
    };

	inline void process(oni_frame_t* frame){};
	inline void process(ONI::Frame::BaseFrame& frame){}

    void processSpikes() {

        while(bThread) {

            // get the AC probe statistics including mean/std dev etc ]
            // which are calculate on the sparse buffer
            bufferProcessor->processMutex[SPARSE_MUTEX].lock();
            probeStats = bufferProcessor->sparseProbeData[FRONT_BUFFER].acProbeStats;
            bufferProcessor->processMutex[SPARSE_MUTEX].unlock();

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
                    
                    // check each probe to see if it's
                    for(size_t probe = 0; probe < numProbes; ++probe) {
                        
                        // after we discover a spike on a probe channel we suppress detection till after the waveform capture TODO: what about overlapping spikes?
                        if(bufferCount < spikeDetectedLastBufferCount[probe] + spikeSampleLength) continue;

                        // for now let's just use std deviation thresholds but TODO: add other thresholding methods
                        bool bSpiked =  frame.ac_uV[probe] < -probeStats[probe].deviation * 3;

                        if(bSpiked){

                            // setup the spike data
                            ONI::Spike& spike = spikes[probe][spikeIndexes[probe]];

                            spike.minVoltage = frame.ac_uV[probe];

                            // TODO: add timestamping
                            spike.bufferCount = bufferCount;
                            spike.rawWaveform.resize(spikeSampleLength);

                            // get a raw pointer to the float values (this is the fastest way to access underlying sample data from the buffer vectors)
                            float* rawAcUv = buffer.getAcuVFloatRaw(probe, centralSampleIDX); // NB: we are making all waveforms same length, not how lugin-GUI does it

                            // store spike waveform and find max/min points
                            // in theory min should always be the central point
                            // and the max immediately afterwards, but let's see...
                            for(size_t s = 1; s < spikeSampleLength; ++s){

                                //spike.rawWaveform[s] = rawAcUv[s]; // is stored

                                if(rawAcUv[s - 1] > rawAcUv[s]){ // is max
                                    spike.maxVoltage = rawAcUv[s];
                                    spike.maxIDX = s;
                                    break;
                                }

                                //if(rawAcUv[s] < spike.minVoltage){ // is min
                                //    spike.minVoltage = rawAcUv[s];
                                //    spike.minIDX = s;
                                //}

                            }
                            
                            if(spike.maxVoltage > spike.minVoltage + probeStats[probe].deviation * 3){ //abs(spike.maxVoltage - spike.minVoltage) > probeStats[probe].deviation * 10

                                rawAcUv = buffer.getAcuVFloatRaw(probe, centralSampleIDX + spike.maxIDX - std::floor(spikeSampleLength / 2));

                                for(size_t s = 0; s < spikeSampleLength; ++s){

                                    spike.rawWaveform[s] = rawAcUv[s]; // is stored

                                }
                                // cache this spike detection buffer count (so we can suppress re-detecting the same spike)
                                spikeDetectedLastBufferCount[probe] = bufferCount;
                                spikeIndexes[probe] = (spikeIndexes[probe] + 1) % spikeBufferSize;
                                spikeCounts[probe]++;
                            }


                        }
                    }
                }

            }

            bufferProcessor->dataMutex[DENSE_MUTEX].unlock();

            //ONI::FrameBuffer& buffer = bufferProcessor->sparseBuffer;

            //uint64_t bufferCount = buffer.getBufferCount();
            //if(bufferCount > buffer.size()){ // ok the buffer is full

            //    size_t centralIDX = std::floor(buffer.size() / 2.0);
            //    size_t statFrameCount = 2 * standardDevWindowSzSamples / buffer.getStep();;

            //    for(size_t probe = 0; probe < numProbes; ++probe){
            //        for(size_t frame = centralIDX - standardDevWindowSzSamples; frame < centralIDX + standardDevWindowSzSamples; ++frame){
            //            probeStats[probe].sum += buffer.getFrameAt(frame).ac_uV[probe];
            //        }
            //        probeStats[probe].mean = probeStats[probe].sum / statFrameCount;

            //        probeStats[probe].ss = 0;

            //        for(size_t frame = centralIDX - standardDevWindowSzSamples; frame < centralIDX + standardDevWindowSzSamples; ++frame){
            //            float acdiff = buffer.getFrameAt(frame).ac_uV[probe] - probeStats[probe].mean;
            //            probeStats[probe].ss += acdiff * acdiff;
            //        }

            //        probeStats[probe].variance = probeStats[probe].ss / (statFrameCount - 1);  // use population (N) or sample (n-1) deviation?
            //        probeStats[probe].deviation = std::sqrt(probeStats[probe].variance);

            //    }

            //}

            //bufferProcessor->dataMutex[SPARSE_MUTEX].unlock();

            //std::this_thread::sleep_for(std::chrono::milliseconds(1));


            //    
            //    ONI::Frame::Rhs2116MultiFrame& frame = buffer.getFrameAt(centralIDX);
            //    for(size_t probe = 0; probe < numProbes; ++probe) {
            //        bufferProcessor->dataMutex[SPARSE_MUTEX].lock();
            //        bool bSpiked = frame.ac_uV[probe] < bufferProcessor->sparseProbeData[FRONT_BUFFER].acProbeStats[probe].deviation * 3;
            //        bufferProcessor->dataMutex[SPARSE_MUTEX].unlock();
            //        if(bSpiked){
            //            ONI::Spike& spike = spikes[probe][spikeIndexes[probe]];
            //            spike.bufferCount = bufferCount;
            //            spike.rawWaveform.resize(spikeSampleLength);
            //            float* rawAcUv = buffer.getAcuVFloatRaw(probe, centralIDX - std::floor(spikeSampleLength / 2.0));
            //            for(size_t s = 0; s < spikeSampleLength; ++s){
            //                spike.rawWaveform[s] = rawAcUv[s];
            //            }
            //            spikeIndexes[probe] =  (spikeIndexes[probe] + 1) % spikeBufferSize;
            //            //LOGDEBUG("Spike? %0.3f > %0.3f", lastFrame.ac_uV[probe], sparseProbeData->acProbeStats[probe].deviation * 4);
            //        }
            //    }
            //}

            //ONI::Frame::Rhs2116MultiFrame lastFrame = bufferProcessor->denseBuffer.getLastMultiFrame();




        }

    }

    float getStDev(const size_t& probe){
        return probeStats[probe].deviation;
    }

protected:

    int standardDevWindowSzMs = 0;
    size_t standardDevWindowSzSamples = 0;

    float spikeMsLength = 0;
    size_t spikeSampleLength = 0;
    size_t spikeBufferSize = 10;

    std::vector<size_t> spikeDetectedLastBufferCount;
    std::vector<size_t> spikeIndexes;
    std::vector<size_t> spikeCounts;

    std::vector< std::vector< ONI::Spike > > spikes;

    std::vector<ONI::Frame::ProbeStatistics> probeStats;

    ONI::Processor::BufferProcessor* bufferProcessor;

    std::atomic_bool bThread = false;
    std::thread thread;

};


} // namespace Processor
} // namespace ONI










