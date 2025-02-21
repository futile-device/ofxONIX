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
#include "../Type/FrameBuffer.h"
#include "../Type/RegisterTypes.h"
#include "../Type/SettingTypes.h"
#include "../Type/FrameTypes.h"
#include "../Type/GlobalTypes.h"
#include "../Processor/BaseProcessor.h"
#include "../Processor/RecordProcessor.h"



#pragma once

#define USE_FRAMEBUFFER 1

#define FRONT_BUFFER 0
#define BACK_BUFFER 1

#define DENSE_MUTEX 0
#define SPARSE_MUTEX 1

namespace ONI{

namespace Interface{
class BufferInterface;
}

namespace Processor{

class SpikeProcessor;

class BufferProcessor : public BaseProcessor{

public:

    friend class SpikeProcessor;
    friend class ONI::Interface::BufferInterface;

    BufferProcessor(){
        BaseProcessor::processorTypeID = ONI::Processor::TypeID::BUFFER_PROCESSOR;
        BaseProcessor::processorName = toString(processorTypeID);
    }

	~BufferProcessor(){
        LOGDEBUG("BufferProcessor DTOR");
        close();
    };


    void setup(ONI::Processor::BaseProcessor* source){

        LOGDEBUG("Setting up Buffer Processor");
        //processorTypeID = ONI::Processor::TypeID::BUFFER_PROCESSOR;
        //processorName = toString(processorTypeID);

        this->source = source;
        this->source->subscribeProcessor("BufferProcessor", ONI::Processor::SubscriptionType::POST_PROCESSOR, this);

        BaseProcessor::numProbes = source->getNumProbes();

        settings.setBufferSizeMillis(5000);
        settings.setSparseStepSizeMillis(1);
        settings.autoThresholdMs = 1000;
        settings.bUseAutoThreshold = true;

        reset();

    }
    

    void reset(){
        LOGINFO("Reset Buffer Processor");
        bThread = false;
        if(thread.joinable()) thread.join();
        resetBuffers();
        resetProbeData();
        bThread = true;
        thread = std::thread(&ONI::Processor::BufferProcessor::processBuffers, this);
    }

	inline void process(oni_frame_t* frame){}; // nothing

	inline void process(ONI::Frame::BaseFrame& frame){

        dataMutex[DENSE_MUTEX].lock();
        denseBuffer.push(*reinterpret_cast<ONI::Frame::Rhs2116MultiFrame*>(&frame)); // TODO:: right now I am assuming a Rhs2116MultiProcessor/multiframe but I shouldn't be!!!
        dataMutex[DENSE_MUTEX].unlock();
        std::this_thread::yield(); // ????
        dataMutex[SPARSE_MUTEX].lock();
        sparseBuffer.push(*reinterpret_cast<ONI::Frame::Rhs2116MultiFrame*>(&frame));
        dataMutex[SPARSE_MUTEX].unlock();

	}

    void resetBuffers(){ // const bool& bUseSamplesForSize = true // Should we give user the choice?
        lockAll();
        using namespace std::chrono;
        lastThresholdTime = duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count();
        denseBuffer.clear();
        sparseBuffer.clear();
        denseBuffer.resizeBySamples(settings.getBufferSizeSamples(), 1, numProbes);
        sparseBuffer.resizeByMillis(settings.getBufferSizeMillis(), settings.getSparseStepMillis(), numProbes);
        unlockAll();
        resetProbeData();
    }

    void resetProbeData(){
        lockAll();
        probeStats.clear();
        probeStats.resize(BaseProcessor::numProbes);
        sparseTimeStamps.resize(sparseBuffer.size());
        for(size_t frame = 0; frame < sparseTimeStamps.size(); ++frame) sparseTimeStamps[frame] = frame * sparseBuffer.getMillisPerStep();
        unlockAll();
    }

    inline void lockAll() {
        dataMutex[DENSE_MUTEX].lock();
        dataMutex[SPARSE_MUTEX].lock();
    }

    inline void unlockAll() {
        dataMutex[DENSE_MUTEX].unlock();
        dataMutex[SPARSE_MUTEX].unlock();
    }

    void close(){
        bThread = false;
        if(thread.joinable()) thread.join();
        denseBuffer.clear();
        sparseBuffer.clear();
        probeStats.clear();
    }
    
private:


    inline void processBuffers(){

        using namespace std::chrono;

        while(bThread){

            if(settings.bUseAutoThreshold && (ONI::Global::model.isAquiring() || ONI::Global::model.getRecordProcessor()->isPlaying())){
                uint64_t tnow = duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count();
                if(tnow - lastThresholdTime > settings.autoThresholdMs){
                    calculateThresholds();
                    lastThresholdTime = duration_cast<milliseconds>(high_resolution_clock::now().time_since_epoch()).count();
                }
            }

        }

    }

    inline void calculateThresholds(){

        //LOGDEBUG("Check thresholds");

        dataMutex[SPARSE_MUTEX].lock();

        for(size_t probe = 0; probe < numProbes; ++probe){

            probeStats[probe].sum = 0;

            float* acProbeVoltages = sparseBuffer.getAcuVFloatRaw(probe, 0);

            for(size_t frame = 0; frame < sparseBuffer.size(); ++frame){
                probeStats[probe].sum += acProbeVoltages[frame];
            }

            probeStats[probe].mean = probeStats[probe].sum / sparseBuffer.size();
            probeStats[probe].std = 0;

            for(size_t frame = 0; frame < sparseBuffer.size(); ++frame){
                float acdiff = acProbeVoltages[frame] - probeStats[probe].mean;
                probeStats[probe].std += acdiff * acdiff;
            }

            probeStats[probe].variance = probeStats[probe].std / (sparseBuffer.size() - 1);  // use population (N) or sample (n-1) deviation?
            probeStats[probe].deviation = sqrt(probeStats[probe].variance);

        }

        dataMutex[SPARSE_MUTEX].unlock();

    }

    inline const std::vector<ONI::Frame::ProbeStatistics>& getProbeStats(){
        const std::lock_guard<std::mutex> lock(dataMutex[SPARSE_MUTEX]);
        return probeStats;
    }


protected:

    uint64_t lastThresholdTime = 0;

    std::vector<float> sparseTimeStamps;
    std::vector<ONI::Frame::ProbeStatistics> probeStats;

    ONI::FrameBuffer denseBuffer;   // contains all frames at full sample rate
    ONI::FrameBuffer sparseBuffer; // contains a sparse buffer sampled every N samples
    std::mutex dataMutex[2];

    ONI::Processor::BaseProcessor* source = nullptr;

    ONI::Settings::BufferProcessorSettings settings;

    std::atomic_bool bThread = false;

    std::thread thread;



};


} // namespace Processor
} // namespace ONI










