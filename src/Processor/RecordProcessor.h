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
#include <filesystem>

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

namespace Interface{
class RecordInterface;
};

namespace Processor{

class RecordProcessor : public BaseProcessor{

public:

	friend class ONI::Interface::RecordInterface;
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

	~RecordProcessor(){
		LOGINFO("RecordProcessor DTOR");
		stopStreams();
	};

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

	// this is annoying, but I need access to the 
	// Conetxt when starting play and pause, so
	// I'm doing this in a nasty update hoo way
	// probably an event or signal would be better

	void play(){
		bPlaybackNeedsStart = true;
	}

	void record(){
		bRecordNeedsStart = true;
	}

	inline bool isPlaybackDependencyResetRequired(const bool& reset = true){
		if(bPlaybackNeedsDependencyReset){
			if(reset) bPlaybackNeedsDependencyReset = false;
			return true;
		}
		return false;
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

	std::string getTime(){
		return ONI::GetAcquisitionTimeStamp(settings.acquisitionStartTime, settings.acquisitionCurrentTime);
	}

	std::vector<std::string> getAllRecordingFolders(const std::string& path = ""){
		std::vector<std::string> eFolders;
		if(path == "") settings.executableDataPath = ONI::GetExecutableDataPath();
		std::string recordingsFolder = settings.executableDataPath + "\\data\\recordings\\";
		for(const auto& entry : std::filesystem::directory_iterator(recordingsFolder)){
			if(entry.is_directory() && entry.path().string().find("experiment_") != std::string::npos){
				//LOGDEBUG("Experiment Folder I: %s", entry.path().string().c_str());
				eFolders.push_back(entry.path().string());
			} else{
				//LOGDEBUG("Some other file or folder: %s", entry.path().string().c_str());
			}
		}
		std::sort(eFolders.begin(), eFolders.end());
		std::reverse(eFolders.begin(), eFolders.end());
		//for(const std::string& f : eFolders){
		//	LOGDEBUG("Experiment Folder S: %s", f.c_str());
		//}
		return eFolders;
	}

	std::string getInfoFromFolder(const std::string& path){

		std::string info = "";

		if(std::filesystem::is_directory(path) && path.find("experiment_") != std::string::npos){

			LOGINFO("Loading info for folder: %s", path.c_str());

			if(settings.executableDataPath == "") settings.executableDataPath = ONI::GetExecutableDataPath();
			std::string recordingsFolder = path;

			std::string exp = "experiment_";
			std::string fileTimeStamp = path.substr(path.find(exp) + exp.size());
			std::ostringstream osI; osI << path << "\\info_" << fileTimeStamp << ".txt";


			std::ifstream infostream;
			infostream.open(osI.str().c_str());
			std::string line; std::ostringstream osInf;
			while(std::getline(infostream, line)){
				osInf << line << "\n";
			}
			info = osInf.str();

		}else{
			LOGERROR("Path for info not valid: %s", path.c_str());
			//info = "";
		}

		return info;

	}

	inline bool getStreamNamesFromFolder(const std::string& path){

		// TODO: sanity check this thing
		if(std::filesystem::is_directory(path) && path.find("experiment_") != std::string::npos){

			LOGINFO("Loading experiment folder: %s", path.c_str());

			if(settings.executableDataPath == "") settings.executableDataPath = ONI::GetExecutableDataPath();
			//std::string recordingsFolder = settings.executableDataPath + "\\data\\recordings\\";

			settings.recordFolder = path;
			std::string exp = "experiment_";
			settings.fileTimeStamp = path.substr(path.find(exp) + exp.size());
			settings.timeStamp = ONI::ReverseTimeStamp(settings.fileTimeStamp);

			std::ostringstream osD; osD << path << "\\data_stream_" << settings.fileTimeStamp << ".dat";
			std::ostringstream osT; osT << path << "\\time_stream_" << settings.fileTimeStamp << ".dat";
			std::ostringstream osI; osI << path << "\\info_" << settings.fileTimeStamp << ".txt";

			settings.dataFileName = osD.str();
			settings.timeFileName = osT.str();
			settings.infoFileName = osI.str();

			std::ifstream infostream;
			infostream.open(settings.infoFileName.c_str());
			std::string line; std::ostringstream osInf;
			while(std::getline(infostream, line)){
				osInf << line << "\n";
			}
			settings.info = osInf.str();

			return true;
		} else{
			LOGERROR("Not a valid experiment folder: %s", path);
			return false;
		}

	}

	std::string getInfoForFolder(const std::string& path){
		
	}

private:

	void _play(){

		bPlaybackNeedsStart = bRecordNeedsStart = false;

		if(state == PLAYING || state == RECORDING) stopStreams();
		//if(bIsAcquiring) stopAcquisition();
		//const std::lock_guard<std::mutex> lock(mutex);
		LOGINFO("Start Playing");

		if(settings.dataFileName == "" || settings.timeFileName == ""){
			LOGINFO("No file set, attempt to play last recorded...");
			std::vector<std::string> folders = getAllRecordingFolders();
			if(folders.size() == 0) return;
			if(!getStreamNamesFromFolder(folders[0])) return;
		}

		
		streamMutex.lock();
		contextDataStream = std::fstream(settings.dataFileName, std::ios::binary | std::ios::in);// | std::ios::out);// | std::ios::app);
		contextTimeStream = std::fstream(settings.timeFileName, std::ios::binary | std::ios::in);// | std::ios::out);// | std::ios::app);
		contextDataStream.seekg(0, ::std::ios::beg);
		contextTimeStream.seekg(0, ::std::ios::beg);
		contextTimeStream.read(reinterpret_cast<char*>(&lastAcquireTimeStamp), sizeof(uint64_t));
		settings.acquisitionStartTime = settings.acquisitionCurrentTime = systemAcquisitionTimeStamp = lastAcquireTimeStamp;
		streamMutex.unlock();

		if(contextTimeStream.bad()) LOGERROR("Bad init time frame read");
		for(auto& device : ONI::Global::model.getDevices()) device.second->reset();
		//ONI::Global::model.getBufferProcessor()->reset();
		bPlaybackNeedsRestart = false;
		bPlaybackNeedsDependencyReset = true;
		state = PLAYING;

		bThread = true;

		thread = std::thread(&RecordProcessor::playFrames, this);

		//startAcquisition();
	}


	void _record(){

		bPlaybackNeedsStart = bRecordNeedsStart = false;

		if(state == PLAYING || state == RECORDING) stopStreams();
		//if(bIsAcquiring) stopAcquisition();
		LOGINFO("Start Recording");
		//const std::lock_guard<std::mutex> lock(mutex);

		settings.timeStamp = ONI::GetTimeStamp();
		settings.fileTimeStamp = ONI::ReverseTimeStamp(settings.timeStamp);

		if(settings.executableDataPath == "") settings.executableDataPath = ONI::GetExecutableDataPath();

		std::ostringstream osP; osP << settings.executableDataPath << "\\data\\recordings\\experiment_" <<  settings.fileTimeStamp;

		settings.recordFolder = osP.str();

		std::ostringstream osD; osD << osP.str() << "\\data_stream_" << settings.fileTimeStamp << ".dat";
		std::ostringstream osT; osT << osP.str() << "\\time_stream_" << settings.fileTimeStamp << ".dat";
		std::ostringstream osI; osI << osP.str() << "\\info_" << settings.fileTimeStamp << ".txt";

		settings.dataFileName = osD.str();
		settings.timeFileName = osT.str();
		settings.infoFileName = osI.str();

		bool bFolder = std::filesystem::create_directories(settings.recordFolder.c_str());

		if(!bFolder){
			LOGERROR("Could not create folder: %s", settings.recordFolder.c_str());
			return;
		} else{
			LOGINFO("Created recording folder: %s", settings.recordFolder.c_str())
		}

		std::ofstream infostream;
		infostream.open(settings.infoFileName.c_str());

		infostream << "Time: " << settings.timeStamp << "\n\n";
		infostream << "Description: " << settings.description << "\n\n";

		for(auto& device : ONI::Global::model.getDevices()) {
			device.second->reset();
			infostream << device.second->info() << "\n\n";
		}

		//infostream << ONI::Global::model.getRhs2116MultiProcessor()->info() << "\n\n";

		infostream.close();

		streamMutex.lock();

		contextDataStream = std::fstream(settings.dataFileName.c_str(), std::ios::binary | std::ios::out);// | std::ios::out);// | std::ios::app);
		contextTimeStream = std::fstream(settings.timeFileName.c_str(), std::ios::binary | std::ios::out);// | std::ios::out);// | std::ios::app);
		contextDataStream.seekg(0, ::std::ios::beg);
		contextTimeStream.seekg(0, ::std::ios::beg);

		using namespace std::chrono;
		uint64_t systemAcquisitionTimeStamp = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();

		contextTimeStream.write(reinterpret_cast<char*>(&systemAcquisitionTimeStamp), sizeof(uint64_t));

		if(contextTimeStream.bad()) LOGERROR("Bad init time frame write");
		settings.acquisitionStartTime = systemAcquisitionTimeStamp;

		streamMutex.unlock();
		//ONI::Global::model.getBufferProcessor()->reset();

		state = RECORDING;
		//startAcquisition();
	}

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

				settings.acquisitionCurrentTime = systemAcquisitionTimeStamp;

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
				ONI::GetAcquisitionTimeStamp(settings.acquisitionStartTime, settings.acquisitionCurrentTime);
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

				settings.acquisitionCurrentTime = systemAcquisitionTimeStamp;

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

	ONI::Settings::RecordSettings settings;

	std::fstream contextDataStream;
	std::fstream contextTimeStream;

	uint64_t systemAcquisitionTimeStamp = 0;
	uint64_t lastAcquireTimeStamp = 0;

	std::atomic_uint state = STOPPED;
	std::atomic_bool bLoopPlayback = true;
	std::atomic_bool bPlaybackNeedsDependencyReset = false;
	std::atomic_bool bPlaybackNeedsRestart = false;

	std::atomic_bool bPlaybackNeedsStart = false; 
	std::atomic_bool bRecordNeedsStart = false;

	//std::atomic_bool bStopPlaybackThread = false;
	std::atomic_bool bThread = false;

	std::thread thread;
	std::mutex streamMutex;

};


} // namespace Processor
} // namespace ONI










