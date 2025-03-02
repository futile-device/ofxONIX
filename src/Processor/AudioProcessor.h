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


constexpr int minResampledBuffers = 0;
constexpr int targetBufferSize = 1024;
constexpr int targetSampleRate = 24000;
constexpr float targetBufferTimeMs = 1000.0 / targetSampleRate * targetBufferSize; // 10.6666666666 ms
constexpr int sourceSampleRate = RHS2116_SAMPLE_FREQUENCY_HZ;
constexpr int sourceBufferSize =  std::ceil(sourceSampleRate / 1000.0 * targetBufferTimeMs); // ~322;
constexpr float conversionRatio = (float)sourceSampleRate / (float)targetSampleRate;

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
        lowpassFilters.resize(numProbes);
        bandpassFilters.resize(numProbes);
        for(size_t probe = 0; probe < numProbes; ++probe){
            bandpassFilters[probe] = new Dsp::SmoothedFilterDesign<Dsp::Butterworth::Design::BandPass    // design type
                <4>,                                   // order
                1,                                     // number of channels (must be const)
                Dsp::DirectFormII>(1);                 // realization
        }

        for(size_t probe = 0; probe < numProbes; ++probe){
            lowpassFilters[probe] = new Dsp::SmoothedFilterDesign<Dsp::Butterworth::Design::LowPass     // design type
                <4>,                                   // order
                1,                                     // number of channels (must be const)
                Dsp::DirectFormII>(1);
        }
        int frequency = 1000;
        int Q = 1.25;

        Dsp::Params params;
        params[0] = targetSampleRate;	// sample rate
        params[1] = 4;
        params[2] = targetSampleRate / 2;						// center frequency
        params[3] = Q;							// band width

        Dsp::Params params1;
        params1[0] = targetSampleRate; // sample rate
        params1[1] = 4;                          // order
        params1[2] = 800;     // center frequency
        params1[3] = 5;           // Q

        

        for(size_t probe = 0; probe < numProbes; ++probe){
            bandpassFilters[probe]->setParams(params1);
            lowpassFilters[probe]->setParams(params);
        }
    
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

    
	inline void process(ONI::Frame::BaseFrame& frame){

        ONI::Frame::Rhs2116MultiFrame* f = reinterpret_cast<ONI::Frame::Rhs2116MultiFrame*>(&frame);

        //size_t probe = 50;
        float sample = 0;// ofMap(f->ac_uV[probe], ONI::Global::model.getSpikeProcessor()->getNegStDevMultiplied(probe), ONI::Global::model.getSpikeProcessor()->getPosStDevMultiplied(probe), -1.0, 1.0, true);
        size_t nSelected = 0;
        for(size_t probe = 0; probe < numProbes; ++probe){
            if(ONI::Global::model.getChannelSelect()[probe]){
                ++nSelected;
                float s = ofMap(f->ac_uV[probe],
                                ONI::Global::model.getSpikeProcessor()->getNegStDevMultiplied(probe),
                                ONI::Global::model.getSpikeProcessor()->getPosStDevMultiplied(probe),
                                -1.0, 1.0, true);
                sample += s;
            }

        }
        
        sample /= nSelected;

        size_t bufferIndex = bufferCount % sourceBufferSize * 1; // change to 2 if doing 2 x oversampling 

        //rhsFrameInBuffer.getBuffer().push_back(ofLerp(lastSample, sample, 0.5)); // 2 x oversampling??!
        rhsFrameInBuffer.getBuffer()[bufferIndex] = sample;
        //lastSample = sample;

        ++bufferCount;

        if(bufferIndex == sourceBufferSize * 1 - 1){ // change to 2 if doing 2 x oversampling 
            ofSoundBuffer t; t.resize(targetBufferSize);
            rhsFrameInBuffer.resampleTo(t, 0, targetBufferSize, (float)(sourceSampleRate * 1) / (float)targetSampleRate, ofSoundBuffer::InterpolationAlgorithm::Hermite);
            t.setSampleRate(targetSampleRate);
            //float* f = &t.getBuffer()[0];
            //bandpassFilters[0]->process(t.getNumFrames(), &f);
            //lowpassFilters[0]->process(t.getNumFrames(), &f);
            mutex.lock();
            rhsResampledBuffer.push(t);
            mutex.unlock();
            //rhsFrameInBuffer.clear();
        }

	}

    void audioOut(ofSoundBuffer& outBuffer){

        if(!(ONI::Global::model.isAquiring() || ONI::Global::model.getRecordProcessor()->isPlaying())) return; // we need to be getting frames

        mutex.lock();

        if(rhsResampledBuffer.size() > minResampledBuffers){
            for(size_t i = 0; i < outBuffer.getNumFrames(); ++i){
                outBuffer[i * outBuffer.getNumChannels() + 0] = rhsResampledBuffer.front().getBuffer()[i];
                outBuffer[i * outBuffer.getNumChannels() + 1] = rhsResampledBuffer.front().getBuffer()[i];
            }
            rhsResampledBuffer.pop();

        }else{
            for(size_t i = 0; i < outBuffer.getNumFrames(); ++i){
                outBuffer[i * outBuffer.getNumChannels() + 0] = 0;
                outBuffer[i * outBuffer.getNumChannels() + 1] = 0;
            }
            LOGDEBUG("Buffer not full enough");
        }

        mutex.unlock();
        
    }

    void close(){
        //bThread = false;
        //if(thread.joinable()) thread.join();
        mutex.lock();
        rhsResampledBuffer.empty();
        rhsFrameInBuffer.clear();
        devices.clear();
        mutex.unlock();

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
        settings.bufferSize = targetBufferSize;
        settings.sampleRate = targetSampleRate;
        settings.numBuffers = 4;
        settings.numInputChannels = 0;
        settings.numOutputChannels = 2;
        settings.setOutDevice(devices[0]); // TODO: make selectable etc, for now default device
        settings.setOutListener(this);
        stream.setup(settings);
    }

    void resetBuffers(){
        rhsResampledBuffer.empty();
        rhsFrameInBuffer.clear();
        rhsFrameInBuffer.resize(sourceBufferSize);
        bufferCount = 0;
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

    uint64_t bufferCount = 0;

    ofSoundBuffer rhsFrameInBuffer;
    std::queue<ofSoundBuffer> rhsResampledBuffer;

    float lastSample = 0; // used for oversampling 

    ofSoundStream stream;
    ofSoundStreamSettings settings;

    std::vector<ofSoundDevice> devices;
    std::vector<std::string> deviceDescriptions;

    ONI::Processor::BaseProcessor* source = nullptr;

    //ONI::Settings::BufferProcessorSettings settings;

    std::atomic_bool bThread = false;
    std::thread thread;
    std::mutex mutex;

    std::vector<Dsp::Filter*> lowpassFilters;
    std::vector<Dsp::Filter*> bandpassFilters;

};


} // namespace Processor
} // namespace ONI



    //size_t framesSinceLastTime = 0;
    //size_t lastBufferCount = 0;
    //size_t startIndex = 0;
            //ONI::Processor::BufferProcessor* bufferProcessor = ONI::Global::model.getBufferProcessor();
        //bufferProcessor->dataMutex[DENSE_MUTEX].lock();
        //ONI::FrameBuffer& buffer = bufferProcessor->denseBuffer;
        //size_t thisBufferCount = buffer.getBufferCount();
        //rhsFrameInBuffer.resize(sourceBufferSize);
        //framesSinceLastTime = thisBufferCount - lastBufferCount;
        //lastBufferCount = thisBufferCount;
        //startIndex = startIndex - framesSinceLastTime + sourceBufferSize;
        //std::cout << startIndex << std::endl;
        //float* rawFrames = buffer.getAcuVFloatRaw(50, startIndex);
        //rhsFrameInBuffer.copyFrom(rawFrames, sourceBufferSize, 1, sourceSampleRate);
        //bufferProcessor->dataMutex[DENSE_MUTEX].unlock();

        //ofSoundBuffer t; t.resize(targetBufferSize);
        //rhsFrameInBuffer.resampleTo(t, 0, targetBufferSize, sourceBufferSize/targetBufferSize, ofSoundBuffer::InterpolationAlgorithm::Hermite);
        //t.setSampleRate(targetSampleRate);
        //rhsResampledBuffer.push(t);






