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

class Context;

namespace Processor{

class RecordProcessor : public BaseProcessor{

public:

	friend class ONI::Context;

	enum State{
		PLAYING = 0,
		RECORDING,
		STOPPED,
		PAUSED
	};

	RecordProcessor(){
		BaseProcessor::processorTypeID = ONI::Processor::TypeID::RECORD_PROCESSOR;
		BaseProcessor::processorName = toString(processorTypeID);
	}

	~RecordProcessor(){};

    void setup(){

        LOGDEBUG("Setting up RecordProcessor");

    }

	void reset(){
	
		//if(state == RECORDING){ // reset and rewrite the system acquisition clock
		//	LOGINFO("Actually start recording");
		//	contextDataStream = std::fstream(ofToDataPath("data_stream.dat"), std::ios::binary | std::ios::in | std::ios::out | std::ios::app);
		//	contextTimeStream = std::fstream(ofToDataPath("time_stream.dat"), std::ios::binary | std::ios::in | std::ios::out | std::ios::app);
		//	contextDataStream.seekg(0, ::std::ios::beg);
		//	contextTimeStream.seekg(0, ::std::ios::beg);
		//	using namespace std::chrono;
		//	uint64_t systemAcquisitionTimeStamp = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
		//	contextTimeStream.write(reinterpret_cast<char*>(&systemAcquisitionTimeStamp), sizeof(uint64_t));
		//	if(contextTimeStream.bad()) LOGERROR("Bad init time frame write");
		//}

	};

	inline void process(oni_frame_t* frame){};
	inline void process(ONI::Frame::BaseFrame& frame){
		

	}

	inline const std::atomic_uint& getState(){
		return state;
	}

	inline bool isPlaying(){
		return (state == PLAYING);
	}

	inline bool isRecording(){
		return (state == RECORDING);
	}

	inline bool isStopped(){
		return (state == STOPPED);
	}

	inline bool isPaused(){
		return (state == PAUSED);
	}

	void setLoopPlayback(const bool& loop){
		bLoopPlayback = loop;
	}

	void play(){
		if(state == PLAYING || state == RECORDING) stopStreams();
		//if(bIsAcquiring) stopAcquisition();
		//const std::lock_guard<std::mutex> lock(mutex);
		LOGINFO("Start Playing");
		state = PLAYING;
		streamMutex.lock();
		contextDataStream = std::fstream(ofToDataPath("data_stream.dat"), std::ios::binary | std::ios::in);// | std::ios::out);// | std::ios::app);
		contextTimeStream = std::fstream(ofToDataPath("time_stream.dat"), std::ios::binary | std::ios::in);// | std::ios::out);// | std::ios::app);
		contextDataStream.seekg(0, ::std::ios::beg);
		contextTimeStream.seekg(0, ::std::ios::beg);
		contextTimeStream.read(reinterpret_cast<char*>(&lastAcquireTimeStamp),  sizeof(uint64_t));
		streamMutex.unlock();
		systemAcquisitionTimeStamp = lastAcquireTimeStamp;
		if(contextTimeStream.bad()) LOGERROR("Bad init time frame read");
		for(auto& device : ONI::Global::model.getDevices()) device.second->reset();
		ONI::Global::model.getBufferProcessor()->reset();
		bPlaybackNeedsRestart = false;
		bThread = true;
		
		thread = std::thread(&RecordProcessor::playFrames, this);

		//startAcquisition();
	}
	std::string autoNameData = "";
	std::string autoNameTime = "";
	void record(){
		if(state == PLAYING || state == RECORDING) stopStreams();
		//if(bIsAcquiring) stopAcquisition();
		LOGINFO("Start Recording");
		//const std::lock_guard<std::mutex> lock(mutex);
		std::ostringstream osD;
		std::ostringstream osT;
		osD << "data_stream" << ofGetTimestampString() << ".dat";
		osT << "time_stream" << ofGetTimestampString() << ".dat";
		streamMutex.lock();
		contextDataStream = std::fstream(ofToDataPath("data_stream.dat"), std::ios::binary | std::ios::out);// | std::ios::out);// | std::ios::app);
		contextTimeStream = std::fstream(ofToDataPath("time_stream.dat"), std::ios::binary | std::ios::out);// | std::ios::out);// | std::ios::app);
		contextDataStream.seekg(0, ::std::ios::beg);
		contextTimeStream.seekg(0, ::std::ios::beg);
		streamMutex.unlock();
		using namespace std::chrono;
		uint64_t systemAcquisitionTimeStamp = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
		contextTimeStream.write(reinterpret_cast<char*>(&systemAcquisitionTimeStamp), sizeof(uint64_t));
		if(contextTimeStream.bad()) LOGERROR("Bad init time frame write");
		for(auto& device : ONI::Global::model.getDevices()) device.second->reset();
		//ONI::Global::model.getBufferProcessor()->reset();

		state = RECORDING;
		//startAcquisition();
	}

	inline bool isPlaybackLoopRequired(){
		return bLoopPlayback && bPlaybackNeedsRestart;
	}

	void stopStreams(){
		LOGINFO("Stop Play/Record Streams");

		if(bThread){
			bThread = false;
			if(thread.joinable()) thread.join();
		}

		//const std::lock_guard<std::mutex> lock(mutex); // ??
		streamMutex.lock();
		contextDataStream.close();
		contextTimeStream.close();
		streamMutex.unlock();
	}

	void stop(){
		stopStreams();
		LOGINFO("Stop Playing/Recording");

		state = STOPPED;
		//bStopPlaybackThread = false;
	}

	void pause(){
		LOGINFO("Pause Playing/Recording");
		state = PAUSED;
		// what else?
	}

private:

	void recordFrame(oni_frame_t* frame){

		if(state == RECORDING){

			//if(frame->dev_idx == 256 || frame->dev_idx == 257){

				using namespace std::chrono;
				uint64_t systemAcquisitionTimeStamp = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();

				ONI::Frame::Rhs2116DataRaw * frame_out = new ONI::Frame::Rhs2116DataRaw;

				frame_out->time = frame->time;
				frame_out->dev_idx = frame->dev_idx;
				frame_out->data_sz = frame->data_sz;

				std::memcpy(frame_out->data, frame->data, frame->data_sz);
				streamMutex.lock();
				contextDataStream.write(reinterpret_cast<char*>(frame_out), sizeof(ONI::Frame::Rhs2116DataRaw));
				if(contextDataStream.bad()){
					LOGERROR("Bad data frame write");
				}

				contextTimeStream.write(reinterpret_cast<char*>(&systemAcquisitionTimeStamp), sizeof(uint64_t));
				if(contextTimeStream.bad()){
					LOGERROR("Bad time frame write");
				}
				streamMutex.unlock();
				delete frame_out;

			//}

		}else{
			std::this_thread::yield();
		}

	}

	void playFrames(){

		while(bThread){

			if(state == PLAYING){

				
				streamMutex.lock();
				contextTimeStream.read(reinterpret_cast<char*>(&systemAcquisitionTimeStamp),  sizeof(uint64_t));
				if(contextTimeStream.bad()) LOGERROR("Bad time frame read");

				ONI::Frame::Rhs2116DataRaw * frame_in = new ONI::Frame::Rhs2116DataRaw;

				contextDataStream.read(reinterpret_cast<char*>(frame_in),  sizeof(ONI::Frame::Rhs2116DataRaw));
				if(contextDataStream.bad()) LOGERROR("Bad data frame read");

				if(contextDataStream.eof()){
					if(bLoopPlayback){
						LOGINFO("Playback reached LOOP");
						bPlaybackNeedsRestart = true;
						streamMutex.unlock();
						break;
					}else{
						LOGINFO("Playback reached EOF");
						stop(); // ? or do we just pause?
					}
				}
				streamMutex.unlock();

				oni_frame_t* frame = reinterpret_cast<oni_frame_t*>(frame_in); // this is kinda nasty ==> will it always work

				frame->data = new char[74];
				memcpy(frame->data, frame_in->data, 74);

				std::map<uint32_t, ONI::Device::BaseDevice*>& devices = ONI::Global::model.getDevices();

				auto it = devices.find((uint32_t)frame->dev_idx);

				if(it == devices.end()){
					LOGERROR("ONI Device doesn't exist with idx: %i", frame->dev_idx);
				}else{
					//LOGDEBUG("Play device frame: %i", frame->dev_idx);
					auto device = it->second;
					device->process(frame);	

				}

				delete [] frame->data;
				delete frame_in;

				int64_t deltaTimeStamp = systemAcquisitionTimeStamp - lastAcquireTimeStamp;

				if(deltaTimeStamp < 0){
					LOGALERT("Playback delta time negative: %i", deltaTimeStamp);
					deltaTimeStamp = 0;
				}

				using namespace std::chrono;
				uint64_t elapsed = 0;
				uint64_t start = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();

				while(elapsed <= deltaTimeStamp){
					elapsed = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count() - start;
					std::this_thread::yield();
				}

				//LOGDEBUG("delta: %i || actual: %i", deltaTimeStamp, elapsed);

				lastAcquireTimeStamp = systemAcquisitionTimeStamp;


			}else{
				LOGDEBUG("Should never have to break playback here??!");
				break;
			}

			



		}

	}

protected:


	std::fstream contextDataStream;
	std::fstream contextTimeStream;

	uint64_t systemAcquisitionTimeStamp = 0;
	uint64_t lastAcquireTimeStamp = 0;

	std::atomic_uint state = STOPPED;
	std::atomic_bool bLoopPlayback = true;
	std::atomic_bool bPlaybackNeedsRestart = false;

	//std::atomic_bool bStopPlaybackThread = false;
	std::atomic_bool bThread = false;

	std::thread thread;
	std::mutex streamMutex;

};


} // namespace Processor
} // namespace ONI










