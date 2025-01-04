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
#include "../Type/DeviceTypes.h"
#include "../Processor/BaseProcessor.h"

#pragma once

namespace ONI{

namespace Interface{
class FrameProcessorInterface;
}

namespace Processor{

class FrameProcessor : public BaseProcessor{

public:

    friend class ONI::Interface::FrameProcessorInterface;

	~FrameProcessor(){
        close();
    };

    void setup(ONI::Device::Rhs2116MultiDevice* multi){

        LOGDEBUG("Setting up FrameProcessor");

        BaseProcessor::processorName = "FrameProcessor";

        if(multi->devices.size() == 0){
            LOGERROR("Add devices to Rhs2116MultiDevice before setting up frame processing");
            assert(false);
            return;
        }

        this->multi = multi;
        this->multi->subscribeProcessor("FrameProcessor", ONI::Processor::ProcessorType::POST_PROCESSOR, this);

        settings.setBufferSizeMillis(5000);
        settings.setSparseStepSizeMillis(10);

        resetBuffers();
        resetProbeData();

        bThread = true;

        thread = std::thread(&ONI::Processor::FrameProcessor::processProbeData, this);

    }


	inline void process(oni_frame_t* frame){}; // nothing

	inline void process(ONI::Frame::BaseFrame& frame){
		
        //const std::lock_guard<std::mutex> lock(mutex);

        fineBuffer.push(*reinterpret_cast<ONI::Frame::Rhs2116MultiFrame*>(&frame));
        sparseBuffer.push(*reinterpret_cast<ONI::Frame::Rhs2116MultiFrame*>(&frame));

	}

    void processProbeData(){

        while(bThread){

            if(sparseBuffer.isFrameNew()){ // is this helping at all? Or just locking up threads?!

                std::vector<ONI::Frame::Rhs2116MultiFrame> frameBuffer = sparseBuffer.getBuffer();
                std::sort(frameBuffer.begin(), frameBuffer.end(), ONI::Frame::acquisition_clock_compare());

                mutex.lock();

                size_t numProbes = probeData.acProbeStats.size();
                size_t frameCount = frameBuffer.size();

                for(size_t probe = 0; probe < numProbes; ++probe){

                    probeData.acProbeStats[probe].sum = 0;
                    probeData.dcProbeStats[probe].sum = 0;

                    for(size_t frame = 0; frame < frameCount; ++frame){

                        probeData.probeTimeStamps[probe][frame] =  uint64_t((frameBuffer[frame].getDeltaTime() - frameBuffer[0].getDeltaTime()) / 1000);
                        probeData.acProbeVoltages[probe][frame] = frameBuffer[frame].ac_uV[probe]; //0.195f * (frames1[frame].ac[probe     ] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
                        probeData.dcProbeVoltages[probe][frame] = frameBuffer[frame].dc_mV[probe]; //-19.23 * (frames1[frame].dc[probe     ] - 512) / 1000.0f;   // -19.23 mV × (ADC result – 512) divide by 1000 for V?

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

                mutex.unlock();
            }else{
                std::this_thread::yield(); // if we don't yield then the thread spins too fast while waiting for a buffer.isFrameNew()
            }



        }

    }
    void resetBuffers(){ // const bool& bUseSamplesForSize = true // Should we give user the choice?

        //const std::lock_guard<std::mutex> lock(mutex);

        fineBuffer.resize(settings.getBufferSizeSamples(), 1);
        sparseBuffer.resize(settings.getBufferSizeMillis(), settings.getSparseStepMillis(), settings.getSampleRateHz());

    }

    void resetProbeData(){

        const std::lock_guard<std::mutex> lock(mutex);

        size_t numProbes = multi->numProbes;
        size_t sparseBufferSizeSamples = sparseBuffer.size();

        if(probeData.acProbeVoltages.size() == numProbes){
            if(probeData.acProbeVoltages[0].size() == sparseBufferSizeSamples){
                return;
            }
        }

        probeData.acProbeVoltages.resize(numProbes);
        probeData.dcProbeVoltages.resize(numProbes);
        probeData.acProbeStats.resize(numProbes);
        probeData.dcProbeStats.resize(numProbes);
        probeData.probeTimeStamps.resize(numProbes);

        for(size_t probe = 0; probe < numProbes; ++probe){
            probeData.acProbeVoltages[probe].resize(sparseBufferSizeSamples);
            probeData.dcProbeVoltages[probe].resize(sparseBufferSizeSamples);
            probeData.probeTimeStamps[probe].resize(sparseBufferSizeSamples);
        }

    }


    void close(){

        bThread = false;
        if(thread.joinable()) thread.join();

        fineBuffer.clear();
        sparseBuffer.clear();

        probeData.acProbeStats.clear();
        probeData.dcProbeStats.clear();
        probeData.acProbeVoltages.clear();
        probeData.dcProbeVoltages.clear();
        probeData.probeTimeStamps.clear();

    }

protected:

    ONI::Frame::Rhs2116ProbeData probeData; // TODO: Generalise this for other frame types
    
    ONI::DataBuffer<ONI::Frame::Rhs2116MultiFrame> fineBuffer;   // contains all frames at full sample rate
    ONI::DataBuffer<ONI::Frame::Rhs2116MultiFrame> sparseBuffer; // contains a sparse buffer sampled every N samples
    

    ONI::Device::Rhs2116MultiDevice* multi = nullptr;

    ONI::Settings::FrameProcessorSettings settings;

    std::atomic_bool bThread = false;

    std::thread thread;
    std::mutex mutex;

};


} // namespace Processor
} // namespace ONI










