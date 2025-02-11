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
#include "../Type/FrameBuffer.h"
#include "../Type/RegisterTypes.h"
#include "../Type/SettingTypes.h"
#include "../Type/FrameTypes.h"
#include "../Type/GlobalTypes.h"
#include "../Processor/BaseProcessor.h"
//#include "../Processor/Rhs2116MultiProcessor.h"

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

        reset();

        //bThread = true;
        //thread = std::thread(&ONI::Processor::BufferProcessor::processProbeDataThread, this);

    }

    void reset(){
        bThread = false;
        if(thread.joinable()) thread.join();
        resetBuffers();
        resetProbeData();
        bThread = true;
        thread = std::thread(&ONI::Processor::BufferProcessor::processBuffers, this);
    }

	inline void process(oni_frame_t* frame){}; // nothing

	inline void process(ONI::Frame::BaseFrame& frame){
#ifdef USE_FRAMEBUFFER
        dataMutex[DENSE_MUTEX].lock();
        denseBuffer.push(*reinterpret_cast<ONI::Frame::Rhs2116MultiFrame*>(&frame)); // TODO:: right now I am assuming a Rhs2116MultiProcessor/multiframe but I shouldn't be!!!
        dataMutex[DENSE_MUTEX].unlock();

        dataMutex[SPARSE_MUTEX].lock();
        sparseBuffer.push(*reinterpret_cast<ONI::Frame::Rhs2116MultiFrame*>(&frame));
        dataMutex[SPARSE_MUTEX].unlock();

        //for(auto& it : postProcessors){
        //    it.second->process(frame);
        //}

#else
        dataMutex.lock();
        denseBuffer.push(*reinterpret_cast<ONI::Frame::Rhs2116MultiFrame*>(&frame)); // TODO:: right now I am assuming a Rhs2116MultiProcessor/multiframe but I shouldn't be!!!
        sparseBuffer.push(*reinterpret_cast<ONI::Frame::Rhs2116MultiFrame*>(&frame));
        dataMutex.unlock();
#endif

        

	}

    void resetBuffers(){ // const bool& bUseSamplesForSize = true // Should we give user the choice?

        lockAll();

        denseBuffer.clear();
        sparseBuffer.clear();
#ifdef USE_FRAMEBUFFER
        denseBuffer.resizeBySamples(settings.getBufferSizeSamples(), 1, numProbes);
        sparseBuffer.resizeByMillis(settings.getBufferSizeMillis(), settings.getSparseStepMillis(), numProbes);
#else
        denseBuffer.resize(settings.getBufferSizeSamples(), 1);
        sparseBuffer.resize(settings.getBufferSizeMillis(), settings.getSparseStepMillis(), RHS2116_SAMPLE_FREQUENCY_HZ);
#endif

        unlockAll();

        resetProbeData();
    }

    void resetProbeData(){
        lockAll();
        denseProbeData[FRONT_BUFFER].clear();
        denseProbeData[BACK_BUFFER].clear();
        sparseProbeData[FRONT_BUFFER].clear();
        sparseProbeData[BACK_BUFFER].clear();
        frameBuffer.clear();
        denseProbeData[FRONT_BUFFER].resize(BaseProcessor::numProbes, denseBuffer.size());
        denseProbeData[BACK_BUFFER].resize(BaseProcessor::numProbes, denseBuffer.size());
        sparseProbeData[FRONT_BUFFER].resize(BaseProcessor::numProbes, sparseBuffer.size());
        sparseProbeData[BACK_BUFFER].resize(BaseProcessor::numProbes, sparseBuffer.size());
        frameBuffer.resize(sparseBuffer.size());
        unlockAll();
        //denseProbeData.resize(source->getNumProbes(), denseBuffer.size());

    }

    inline void lockAll() {
#ifdef USE_FRAMEBUFFER
        dataMutex[DENSE_MUTEX].lock();
        processMutex[DENSE_MUTEX].lock();
        dataMutex[SPARSE_MUTEX].lock();
        processMutex[SPARSE_MUTEX].lock();
#else
        dataMutex.lock();
        processMutex.lock();
#endif
    }

    inline void unlockAll() {
#ifdef USE_FRAMEBUFFER
        processMutex[DENSE_MUTEX].unlock();
        dataMutex[DENSE_MUTEX].unlock();
        processMutex[SPARSE_MUTEX].unlock();
        dataMutex[SPARSE_MUTEX].unlock();
#else
        processMutex.unlock();
        dataMutex.unlock();
#endif
    }

    void close(){

        bThread = false;
        if(thread.joinable()) thread.join();

        denseBuffer.clear();
        sparseBuffer.clear();

        //.clear();
        denseProbeData[FRONT_BUFFER].clear();
        denseProbeData[BACK_BUFFER].clear();
        sparseProbeData[FRONT_BUFFER].clear();
        sparseProbeData[BACK_BUFFER].clear();
        frameBuffer.clear();

    }


    
private:

    std::vector<ONI::Frame::Rhs2116MultiFrame> frameBuffer;

#ifndef USE_FRAMEBUFFER
    

    inline void processProbeData(ONI::DataBuffer<ONI::Frame::Rhs2116MultiFrame>& buffer, ONI::Frame::Rhs2116ProbeData& probeData){

        dataMutex.lock();
        bool bIsFrameNew = buffer.isFrameNew();
        dataMutex.unlock();

        if(bIsFrameNew){ // is this helping at all? Or just locking up threads?!

            dataMutex.lock();
            buffer.copySortedBuffer(frameBuffer);
            dataMutex.unlock();

            //using namespace std::chrono;
            //volatile uint64_t startTime = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();

            processMutex.lock();

            size_t frameCount = std::min(probeData.acProbeVoltages[0].size(), frameBuffer.size()); // make sure we don't overflow during a buffer resize
            

            for(size_t probe = 0; probe < numProbes; ++probe){

                probeData.acProbeStats[probe].sum = 0;
                probeData.dcProbeStats[probe].sum = 0;

                for(size_t frame = 0; frame < frameCount; ++frame){
                    float t = ((frameBuffer[frame].getDeltaTime() - frameBuffer[0].getDeltaTime()) / 1000.0f);
                    probeData.probeTimeStamps[probe][frame] = t;
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
            processMutex.unlock();

            //processorTimeNs = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count() - startTime;
            

        }else{
            std::this_thread::yield(); // if we don't yield then the thread spins too fast while waiting for a buffer.isFrameNew()
        }

    }
#endif

#ifdef USE_FRAMEBUFFER
    inline void bufferToProbeData(ONI::FrameBuffer& buffer, ONI::Frame::Rhs2116ProbeData* probeData, const size_t& mutexID) {

        dataMutex[mutexID].lock();
        bool bIsFrameNew = buffer.isFrameNew();
        dataMutex[mutexID].unlock();

        if (bIsFrameNew) { // is this helping at all? Or just locking up threads?!

            dataMutex[mutexID].lock();
            buffer.copySortedProbeData(probeData[BACK_BUFFER]);
            dataMutex[mutexID].unlock();

            processMutex[mutexID].lock();
            std::swap(probeData[FRONT_BUFFER], probeData[BACK_BUFFER]);
            processMutex[mutexID].unlock();
             

        }else{
            std::this_thread::yield();
        }

    }
#endif

    void processBuffers(){

        while(bThread){

            using namespace std::chrono;
            volatile uint64_t startTime = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();

#ifndef USE_FRAMEBUFFER
            processProbeData(sparseBuffer, sparseProbeData[BACK_BUFFER]); // let's use 1 as the back buffer

#else
            
           // bufferToProbeData(denseBuffer, denseProbeData, 0);
            bufferToProbeData(sparseBuffer, sparseProbeData, 1);



            //std::this_thread::yield();

#endif // !USE_FRAMEBUFFER


            processorTimeNs = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count() - startTime;

        }

    }

protected:

    ONI::Frame::Rhs2116ProbeData denseProbeData[2]; 
    ONI::Frame::Rhs2116ProbeData sparseProbeData[2];

#ifdef USE_FRAMEBUFFER
    ONI::FrameBuffer denseBuffer;   // contains all frames at full sample rate
    ONI::FrameBuffer sparseBuffer; // contains a sparse buffer sampled every N samples
    std::mutex dataMutex[2];
    std::mutex processMutex[2];
#else
    ONI::DataBuffer<ONI::Frame::Rhs2116MultiFrame> denseBuffer;   // contains all frames at full sample rate
    ONI::DataBuffer<ONI::Frame::Rhs2116MultiFrame> sparseBuffer; // contains a sparse buffer sampled every N samples
    std::mutex dataMutex;
    std::mutex processMutex;
#endif

    ONI::Processor::BaseProcessor* source = nullptr;

    ONI::Settings::BufferProcessorSettings settings;

    std::atomic_uint64_t processorTimeNs = 0;
    std::atomic_bool bThread = false;

    std::thread thread;



};


} // namespace Processor
} // namespace ONI










