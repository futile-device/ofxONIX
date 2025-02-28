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

#include "ofMain.h"

#pragma once

#define FRONT_BUFFER 0
#define BACK_BUFFER 1

namespace ONI{

namespace Interface{
class AudioInterface;
}

namespace Processor{

//class SpikeProcessor;

class AudioProcessor : public BaseProcessor{

public:

    //friend class SpikeProcessor;
    //friend class BufferProcessor;
    //friend class RecordProcessor;
    friend class ONI::Interface::AudioInterface;

    AudioProcessor(){
        BaseProcessor::processorTypeID = ONI::Processor::TypeID::AUDIO_PROCESSOR;
        BaseProcessor::processorName = toString(processorTypeID);
    }

	~AudioProcessor(){
        LOGDEBUG("AudioProcessor DTOR");
        close();
    };


    void setup(ONI::Processor::BaseProcessor* source){

        LOGDEBUG("Setting up Audio Processor");
        //processorTypeID = ONI::Processor::TypeID::BUFFER_PROCESSOR;
        //processorName = toString(processorTypeID);

        this->source = source;
        this->source->subscribeProcessor("AudioProcessor", ONI::Processor::SubscriptionType::POST_PROCESSOR, this);

        BaseProcessor::numProbes = source->getNumProbes();

        reset();



    }
    

    void reset(){

        LOGINFO("Reset Audio Processor");

        mutex.lock();
        resetBuffers();
        resetDevices();
        mutex.unlock();



        //bThread = false;
        //if(thread.joinable()) thread.join();
        //bThread = true;
        //thread = std::thread(&ONI::Processor::AudioProcessor::processBuffers, this);
    }

	inline void process(oni_frame_t* frame){}; // nothing
    float phase = 0;
	inline void process(ONI::Frame::BaseFrame& frame){

        ONI::Frame::Rhs2116MultiFrame* f = reinterpret_cast<ONI::Frame::Rhs2116MultiFrame*>(&frame);
        
        mutex.lock();
        phase += 0.4;
        for(size_t probe = 0; probe < numProbes; ++probe){
            
            audioBuffer[FRONT_BUFFER][probe].push_back(sin(phase));// ofMap(f->ac_uV[probe], ONI::Global::model.getSpikeProcessor()->getNegStDevMultiplied(probe), ONI::Global::model.getSpikeProcessor()->getPosStDevMultiplied(probe), -1.0, 1.0, true));
        }
        mutex.unlock();
       // dataMutex[DENSE_MUTEX].lock();
        //audioBuffer.push(*reinterpret_cast<ONI::Frame::Rhs2116MultiFrame*>(&frame)); // TODO:: right now I am assuming a Rhs2116MultiProcessor/multiframe but I shouldn't be!!!


	}

    void audioOut(ofSoundBuffer& outBuffer){

        if(!(ONI::Global::model.isAquiring() || ONI::Global::model.getRecordProcessor()->isPlaying())) return; // we need to be getting frames

        mutex.lock();
        if(audioBuffer[FRONT_BUFFER].size() == 0 || audioBuffer[BACK_BUFFER].size() == 0){ // we need to be playing
            LOGDEBUG("Buffers not setup");
            mutex.unlock();
            return;
        }
        
        //for(int k = 0; k < 256; k++){
        //    phase += 0.4;
        //    for(size_t probe = 0; probe < numProbes; ++probe){

        //        audioBuffer[FRONT_BUFFER][probe].push_back(sin(phase));// ofMap(f->ac_uV[probe], ONI::Global::model.getSpikeProcessor()->getNegStDevMultiplied(probe), ONI::Global::model.getSpikeProcessor()->getPosStDevMultiplied(probe), -1.0, 1.0, true));
        //    }
        //}


        if(audioBuffer[FRONT_BUFFER][0].size() < 10){ // we need enough frames to be sensible
            LOGDEBUG("Buffer not full enough");
            mutex.unlock();
            return;
        }

        //if(audioBuffer[FRONT_BUFFER][0].size() > 512) clear(BACK_BUFFER);

        swapBuffers();
        mutex.unlock();
        size_t probe[] = {50, 50};
        size_t j = 0;
        //for(size_t probe = 0; probe < numProbes; ++probe){
            //LOGDEBUG("%d || %d", audioBuffer[FRONT_BUFFER][probe].size(), audioBuffer[BACK_BUFFER][probe].size());
        //for(size_t j = 0; j < 1; ++j){
            ofSoundBuffer b; b.setNumChannels(1); b.setSampleRate(30000);
            b.copyFrom(audioBuffer[BACK_BUFFER][probe[j]], 1, audioBuffer[BACK_BUFFER][probe[j]].size());
            ofSoundBuffer h; h.setNumChannels(1); h.setSampleRate(outBuffer.getSampleRate()); h.resize(outBuffer.getNumFrames());
            b.resampleTo(h, 0, h.getNumFrames(), 1.0, false, ofSoundBuffer::InterpolationAlgorithm::Hermite);

            for(size_t i = 0; i < outBuffer.getNumFrames(); ++i){
                outBuffer[i * outBuffer.getNumChannels() + 0] = b.getBuffer()[i];// *sin(i);
                //outBuffer[i * outBuffer.getNumChannels() + 1] = h.getBuffer()[i];
            }
        //}

            //b.resampleTo(outBuffer, 0, b.getNumFrames(), 1.0, false, ofSoundBuffer::InterpolationAlgorithm::Linear);
            clear(BACK_BUFFER);
        
        
    }

    inline void clear(const int& bufferIDX){
        for(size_t probe = 0; probe < numProbes; ++probe){
            audioBuffer[bufferIDX][probe].clear();
        }
    }

    void close(){
        bThread = false;
        if(thread.joinable()) thread.join();
        mutex.lock();
        audioBuffer[FRONT_BUFFER].clear();
        audioBuffer[BACK_BUFFER].clear();
        devices.clear();
        mutex.unlock();
        //audioBuffer.clear();
    }
    
private:

    void resetDevices(){
        stream.close();
        devices.clear();
        //stream.printDeviceList();
        devices = stream.getDeviceList(ofSoundDevice::Api::MS_WASAPI);
        for(ofSoundDevice& device : devices){
            char buf[256];
            std::sprintf(buf, "[%02d||%02d] %s", device.inputChannels, device.outputChannels, device.name.c_str());
            deviceDescriptions.push_back(std::string(buf));
        }

        for(size_t idx = 0; idx < deviceDescriptions.size(); ++idx){
            LOGINFO("Audio Device %02d [i||o]: %s", idx, deviceDescriptions[idx].c_str());
        }

        ofSoundStreamSettings settings; // TODO: make this resetable? callable? closeable?
        settings.bufferSize = 256;
        settings.numBuffers = 4;
        settings.sampleRate = 24000;
        settings.numInputChannels = 0;
        settings.numOutputChannels = 2;
        settings.setOutDevice(devices[0]); // TODO: make selectable etc, for now default device
        settings.setOutListener(this);
        stream.setup(settings);

    }

    void resetBuffers(){
        audioBuffer[FRONT_BUFFER].clear();
        audioBuffer[FRONT_BUFFER].resize(BaseProcessor::numProbes);
        audioBuffer[BACK_BUFFER].clear();
        audioBuffer[BACK_BUFFER].resize(BaseProcessor::numProbes);
    }

    inline void swapBuffers(){
        std::swap(audioBuffer[FRONT_BUFFER], audioBuffer[BACK_BUFFER]);
    }

    inline void processBuffers(){

        using namespace std::chrono;

        while(bThread){
            // (unsigned int)std::floor(RHS2116_SAMPLE_FREQUENCY_HZ)
            //ofSoundBuffer b; b.setNumChannels(1); b.setSampleRate((unsigned int)std::floor(RHS2116_SAMPLE_FREQUENCY_HZ));
            //b.copyFrom(audioBuffer[0], 1, audioBuffer[0].size());
            //ofSoundBuffer h; h.setNumChannels(1); h.setSampleRate(48000);
            //b.resampleTo(h, 0, b.getNumFrames(), 1.0, false, ofSoundBuffer::InterpolationAlgorithm::Hermite);
            
        }

    }



protected:

    ofSoundStream stream;
    ofSoundStreamSettings settings;

    std::vector<ofSoundDevice> devices;
    std::vector<std::string> deviceDescriptions;

    std::vector< std::vector<float> > audioBuffer[2];

    ONI::Processor::BaseProcessor* source = nullptr;

    //ONI::Settings::BufferProcessorSettings settings;

    std::atomic_bool bThread = false;
    std::thread thread;
    std::mutex mutex;



};


} // namespace Processor
} // namespace ONI










