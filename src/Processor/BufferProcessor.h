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
//#include "../Processor/Rhs2116MultiProcessor.h"

#pragma once

namespace ONI{

namespace Interface{
class BufferInterface;
}

namespace Processor{

class BufferProcessor : public BaseProcessor{

public:

    friend class ONI::Interface::BufferInterface;

    BufferProcessor(){
        BaseProcessor::processorTypeID = ONI::Processor::TypeID::BUFFER_PROCESSOR;
        BaseProcessor::processorName = toString(processorTypeID);
    }

	~BufferProcessor(){
        close();
    };


    void setup(ONI::Processor::BaseProcessor* source){

        LOGDEBUG("Setting up Buffer Processor");
        processorTypeID = ONI::Processor::TypeID::BUFFER_PROCESSOR;
        processorName = toString(processorTypeID);

        //if(multi->devices.size() == 0){
        //    LOGERROR("Add devices to Rhs2116MultiProcessor before setting up frame processing");
        //    assert(false);
        //    return;
        //}

        this->source = source;
        this->source->subscribeProcessor("BufferProcessor", ONI::Processor::SubscriptionType::POST_PROCESSOR, this);

        BaseProcessor::numProbes = source->getNumProbes();

        settings.setBufferSizeMillis(5000);
        settings.setSparseStepSizeMillis(10);

        reset();

        bThread = true;

        thread = std::thread(&ONI::Processor::BufferProcessor::processProbeDataThread, this);

    }

    void reset(){
        resetBuffers();
        resetProbeData();
    }

	inline void process(oni_frame_t* frame){}; // nothing

	inline void process(ONI::Frame::BaseFrame& frame){
        //const std::lock_guard<std::mutex> lock(mutex);
        dataMutex.lock();
        denseBuffer.push(*reinterpret_cast<ONI::Frame::Rhs2116MultiFrame*>(&frame)); // TODO:: right now I am assuming a Rhs2116MultiProcessor/multiframe but I shouldn't be!!!
        sparseBuffer.push(*reinterpret_cast<ONI::Frame::Rhs2116MultiFrame*>(&frame));
        dataMutex.unlock();

	}

    void resetBuffers(){ // const bool& bUseSamplesForSize = true // Should we give user the choice?

        dataMutex.lock();
        denseBuffer.resize(settings.getBufferSizeSamples(), 1);
        sparseBuffer.resize(settings.getBufferSizeMillis(), settings.getSparseStepMillis(), settings.getSampleRateHz());
        dataMutex.unlock();

        resetProbeData();

    }

    void resetProbeData(){

        processMutex.lock();
        sparseProbeData[0].resize(BaseProcessor::numProbes, sparseBuffer.size());
        sparseProbeData[1].resize(BaseProcessor::numProbes, sparseBuffer.size());
        frameBuffer.resize(sparseBuffer.size());
        processMutex.unlock();
        //denseProbeData.resize(source->getNumProbes(), denseBuffer.size());

    }


    void close(){

        bThread = false;
        if(thread.joinable()) thread.join();

        denseBuffer.clear();
        sparseBuffer.clear();

        //denseProbeData.clear();
        sparseProbeData[0].clear();
        sparseProbeData[1].clear();
        frameBuffer.clear();

    }

    const volatile uint64_t getProcessorTimeNs(){
        return processorTimeNs;
    }

private:

    volatile uint64_t processorTimeNs = 0;
    std::vector<ONI::Frame::Rhs2116MultiFrame> frameBuffer;

    inline void processProbeData(ONI::DataBuffer<ONI::Frame::Rhs2116MultiFrame>& buffer, ONI::Frame::Rhs2116ProbeData& probeData){

        if(buffer.isFrameNew()){ // is this helping at all? Or just locking up threads?!

            using namespace std::chrono;
            

            dataMutex.lock();
            //std::vector<ONI::Frame::Rhs2116MultiFrame> frameBuffer = buffer.getBuffer();
            
            buffer.copySortedBuffer(frameBuffer);
            dataMutex.unlock();

            //std::this_thread::yield(); //??
            //std::sort(frameBuffer.begin(), frameBuffer.end(), ONI::Frame::acquisition_clock_compare());
            volatile uint64_t startTime = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
            //processMutex.lock();
            
            size_t numProbes = probeData.acProbeVoltages.size();
            size_t frameCount = std::min(probeData.acProbeVoltages[0].size(), frameBuffer.size()); // make sure we don't overflow during a buffer resize

            for(size_t probe = 0; probe < numProbes; ++probe){

                probeData.acProbeStats[probe].sum = 0;
                probeData.dcProbeStats[probe].sum = 0;

                for(size_t frame = 0; frame < frameCount; ++frame){

                    probeData.probeTimeStamps[probe][frame] =  uint64_t((frameBuffer[frame].getDeltaTime() - frameBuffer[0].getDeltaTime()) / 1000);
                    probeData.acProbeVoltages[probe][frame] = frameBuffer[frame].ac_uV[probe]; //0.195f * (frames1[frame].ac[probe     ] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
                    probeData.dcProbeVoltages[probe][frame] = frameBuffer[frame].dc_mV[probe]; //-19.23 * (frames1[frame].dc[probe     ] - 512) / 1000.0f;   // -19.23 mV × (ADC result – 512) divide by 1000 for V?
                    probeData.stimProbeData[probe][frame] = (float)frameBuffer[frame].stim; //frameBuffer[frame].stim ? 8.0f : 0.0f;

                    probeData.acProbeStats[probe].sum += probeData.acProbeVoltages[probe][frame];
                    probeData.dcProbeStats[probe].sum += probeData.dcProbeVoltages[probe][frame];

                }

                probeData.acProbeStats[probe].mean = probeData.acProbeStats[probe].sum / frameCount;
                probeData.dcProbeStats[probe].mean = probeData.dcProbeStats[probe].sum / frameCount;

                probeData.acProbeStats[probe].ss = 0;
                probeData.dcProbeStats[probe].ss = 0;

                for(size_t frame = 0; frame < frameCount; ++frame){
                    float acdiff = probeData.acProbeVoltages[probe][frame] - probeData.acProbeStats[probe].mean;
                    float dcdiff = probeData.dcProbeVoltages[probe][frame] - probeData.dcProbeStats[probe].mean;
                    probeData.acProbeStats[probe].ss += acdiff * acdiff; 
                    probeData.dcProbeStats[probe].ss += dcdiff * dcdiff;
                }

                probeData.acProbeStats[probe].variance = probeData.acProbeStats[probe].ss / (frameCount - 1);  // use population (N) or sample (n-1) deviation?
                probeData.acProbeStats[probe].deviation = sqrt(probeData.acProbeStats[probe].variance);

                probeData.dcProbeStats[probe].variance = probeData.dcProbeStats[probe].ss / (frameCount - 1);  // use population (N) or sample (n-1) deviation?
                probeData.dcProbeStats[probe].deviation = sqrt(probeData.dcProbeStats[probe].variance);

            }

            //processMutex.unlock();
            processorTimeNs = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count() - startTime;
            

        }else{
            std::this_thread::yield(); // if we don't yield then the thread spins too fast while waiting for a buffer.isFrameNew()
        }

    }

    void processProbeDataThread(){

        while(bThread){

            //processProbeData(denseBuffer, denseProbeData);
            processProbeData(sparseBuffer, sparseProbeData[1]);
            //std::this_thread::yield(); // if we don't yield then the thread spins too fast while waiting for a buffer.isFrameNew()
            processMutex.lock();
            std::swap(sparseProbeData[0], sparseProbeData[1]);
            processMutex.unlock();

        }

    }

protected:

    //ONI::Frame::Rhs2116ProbeData denseProbeData; // TODO: Generalise this for other frame types
    ONI::Frame::Rhs2116ProbeData sparseProbeData[2];

    ONI::DataBuffer<ONI::Frame::Rhs2116MultiFrame> denseBuffer;   // contains all frames at full sample rate
    ONI::DataBuffer<ONI::Frame::Rhs2116MultiFrame> sparseBuffer; // contains a sparse buffer sampled every N samples
    

    ONI::Processor::BaseProcessor* source = nullptr;

    ONI::Settings::BufferProcessorSettings settings;

    std::atomic_bool bThread = false;

    std::thread thread;

    std::mutex dataMutex;
    std::mutex processMutex;

};


} // namespace Processor
} // namespace ONI










