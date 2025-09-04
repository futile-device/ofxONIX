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
#include <windows.h>
#include <timeapi.h>

#include "../Type/Log.h"
#include "../Type/RegisterTypes.h"
#include "../Type/SettingTypes.h"
#include "../Type/FrameTypes.h"
#include "../Type/GlobalTypes.h"

#include "../Processor/BaseProcessor.h"
#include "../Processor/Rhs2116MultiProcessor.h"
//#include "../Processor/Rhs2116StimProcessor.h"

#pragma once

#define FILE_VERSION 2

namespace ONI{

// from: https://stackoverflow.com/questions/22226872/two-problems-when-writing-to-wav-c/22227440#22227440
template<typename T>
void write(std::ofstream& stream, const T& t) {
	stream.write((const char*)&t, sizeof(T));
}

template<typename SampleType>
void writeWAVData(const char* outFile, SampleType* buf, size_t bufSize, int sampleRate, short channels)
{
	std::ofstream stream(outFile, std::ios::binary);                // Open file stream at "outFile" location

	/* Header */
	stream.write("RIFF", 4);                                        // sGroupID (RIFF = Resource Interchange File Format)
	write<int>(stream, 36 + bufSize);                               // dwFileLength
	stream.write("WAVE", 4);                                        // sRiffType

	/* Format Chunk */
	stream.write("fmt ", 4);                                        // sGroupID (fmt = format)
	write<int>(stream, 16);                                         // Chunk size (of Format Chunk)
	write<short>(stream, 3);                                        // Format (1 = PCM) (3 == FLOAT)
	write<short>(stream, channels);                                 // Channels
	write<int>(stream, sampleRate);                                 // Sample Rate
	write<int>(stream, sampleRate * channels * sizeof(SampleType)); // Byterate
	write<short>(stream, channels * sizeof(SampleType));            // Frame size aka Block align
	write<short>(stream, 8 * sizeof(SampleType));                   // Bits per sample 

	/* Data Chunk */
	stream.write("data", 4);                                        // sGroupID (data)
	stream.write((const char*)&bufSize, 4);                         // Chunk size (of Data, and thus of bufferSize)
	stream.write((const char*)buf, bufSize);                        // The samples DATA!!!
}

class Context;

namespace Interface{
class RecordInterface;
};

namespace Processor{

class Rhs2116StimProcessor;

class RecordProcessor : public BaseProcessor{

public:

	friend class ONI::Context;
	friend class ONI::Processor::Rhs2116StimProcessor;
	friend class ONI::Interface::RecordInterface;
	

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
		oscHeartBeat.setup("127.0.0.1", 4000);

    }

	void reset(){
	

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
	// I'm doing this in a nasty update hohum anyway
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
		contextStimStream.close();
		contextStimTypes.close();
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

	std::string getCurrentTimeStamp(){
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
			settings.infoFileName = osI.str();

			std::ifstream infostream;
			infostream.open(settings.infoFileName.c_str());
			std::string line; std::ostringstream osInf;
			while(std::getline(infostream, line)) osInf << line << "\n";
			infostream.close();
			info = osInf.str();

			//std::string version = getStringSetting("Version: ", info); // do the update on load DONT DO THIS HERE
			//if(version == "") upgradeToVersion(1);
			//if(version == "1") upgradeToVersion(2);

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

			// load the info metadata first
			std::ostringstream osI; osI << settings.recordFolder << "\\info_" << settings.fileTimeStamp << ".txt";
			settings.infoFileName = osI.str();

			loadInfoSettings();

			// set heartbeat setting
			std::string hz = getStringSetting("HeartBeat Hz: ", settings.info);
			settings.heartBeatRateHz = std::atoi(hz.c_str());
			ONI::Device::HeartBeatDevice* heartBeatDevice = (ONI::Device::HeartBeatDevice*)ONI::Global::model.getDevice(0); 
			heartBeatDevice->setFrequencyHz(settings.heartBeatRateHz);

			// set channel map
			std::string strChannelMap = getStringSetting("Channel Map: ", settings.info);
			std::vector<std::string> strChannelVector = ofSplitString(strChannelMap, ",", true, true);
			std::vector<size_t> channelMap; channelMap.resize(strChannelVector.size());
			for(size_t probe = 0; probe < strChannelVector.size(); ++probe){
				channelMap[probe] = std::atoi(strChannelVector[probe].c_str());
			}
			ONI::Global::model.getChannelMapProcessor()->setChannelMap(channelMap);
			ONI::Global::model.getChannelMapProcessor()->updateChannelMaps();

			std::ostringstream osD; osD << settings.recordFolder << "\\data_stream_" << settings.fileTimeStamp << ".dat";
			std::ostringstream osT; osT << settings.recordFolder << "\\time_stream_" << settings.fileTimeStamp << ".dat";
			std::ostringstream osS; osS << settings.recordFolder << "\\stim_stream_" << settings.fileTimeStamp << ".dat";
			std::ostringstream osP; osP << settings.recordFolder << "\\stim_types_" << settings.fileTimeStamp << ".dat";

			settings.dataFileName = osD.str();
			settings.timeFileName = osT.str();
			settings.stimFileName = osS.str();
			settings.stimTypesFileName = osP.str();

			// always check version at the end so we can just redo saving 
			// a new file metadata header with already loaded values
			// [if you want to force some of this we can do it in the getFileInfo function above]
			std::string version = getStringSetting("Version: ", settings.info);
			if(version == "") upgradeToVersion(1);
			if(version == "1") upgradeToVersion(2);

			

			return true;
		} else{
			LOGERROR("Not a valid experiment folder: %s", path);
			return false;
		}

	}

	void loadInfoSettings(){
		assert(settings.infoFileName != "");
		std::ifstream infostream;
		infostream.open(settings.infoFileName.c_str());
		std::string line; std::ostringstream osInf;
		while(std::getline(infostream, line))osInf << line << "\n";
		infostream.close();
		settings.info = osInf.str();
	}

	void saveInfoSettings(){
		
		assert(settings.infoFileName != "");

		std::ofstream infostream;
		infostream.open(settings.infoFileName.c_str());

		infostream << "Time: " << settings.timeStamp << "\n\n";
		infostream << "Version: " << settings.version << "\n\n";
		infostream << "Description: " << settings.description << "\n\n";

		for(auto& device : ONI::Global::model.getDevices()) {
			device.second->reset();
			infostream << device.second->info() << "\n\n";
		}

		infostream << "Channel Map: ";
		const std::vector<size_t>& channelMap = ONI::Global::model.getChannelMapProcessor()->getChannelMap();
		for(size_t probe = 0; probe < channelMap.size(); ++probe){
			infostream << std::to_string(channelMap[probe]) << (probe == channelMap.size() - 1 ? "" : ",");
		}
		//size_t t = infostream.tellp();
		infostream.close();

	}

	void upgradeToVersion(const int& toVersionNumber){

		LOGALERT("Updating settings for %s to Version %d", settings.dataFileName.c_str(), toVersionNumber);

		if(toVersionNumber == 1){
			
			settings.version = std::to_string(toVersionNumber);

			// add version number
			// add channel map

			saveInfoSettings();
			loadInfoSettings();
		}

		if(toVersionNumber == 2){

			// add spike and stim streams

			streamMutex.lock();

			contextTimeStream = std::fstream(settings.timeFileName, std::ios::binary | std::ios::in);// | std::ios::out);// | std::ios::app);
			contextTimeStream.seekg(0, ::std::ios::end);
			int length = contextTimeStream.tellg();
			int nFrames = length / sizeof(uint64_t);
			contextTimeStream.close();

			contextStimTypes = std::fstream(settings.stimTypesFileName, std::ios::binary | std::ios::out);
			contextStimStream = std::fstream(settings.stimFileName, std::ios::binary | std::ios::out);
			contextStimTypes.seekg(0, ::std::ios::beg);
			contextStimStream.seekg(0, ::std::ios::beg);

			int stimID = -1; // every frame of the spike stream contains a number refering to the stim settings applied during that frame -> -1 means no stim
			for(size_t frame = 0; frame < nFrames; ++frame) contextStimStream.write(reinterpret_cast<char*>(&stimID), sizeof(int));
			contextStimTypes.close();
			contextStimStream.close();

			streamMutex.unlock();

			settings.version = std::to_string(toVersionNumber);
			saveInfoSettings();
			loadInfoSettings();

			/*ONI::Settings::Rhs2116StimulusSettings& stagedSettings = ONI::Global::model.getRhs2116StimProcessor()->stagedSettings; // remember to do it with actual device settngs
			ONI::Settings::Rhs2116StimulusSettingsRaw64 saveSettings; 
			saveSettings.stepSize = stagedSettings.stepSize;
			std::memcpy(saveSettings.stimuli, stagedSettings.stimuli.data(), 64 * sizeof(ONI::Rhs2116StimulusData));
			contextStimStream.write(reinterpret_cast<char*>(&saveSettings), sizeof(ONI::Settings::Rhs2116StimulusSettingsRaw64));
			contextStimStream.close();



			contextStimStream = std::fstream(settings.stimFileName.c_str(), std::ios::binary | std::ios::in);
			contextStimTypes.seekg(0, ::std::ios::beg);

			ONI::Settings::Rhs2116StimulusSettingsRaw64 loadSettings;
			contextStimStream.read(reinterpret_cast<char*>(&loadSettings), sizeof(ONI::Settings::Rhs2116StimulusSettingsRaw64));

			stagedSettings.stimuli.clear();
			stagedSettings.stepSize = loadSettings.stepSize;
			std::memcpy(stagedSettings.stimuli.data(), loadSettings.stimuli, 64 * sizeof(ONI::Rhs2116StimulusData));
			contextStimStream.close();
			//ONI::Global::model.getRhs2116StimProcessor()->applyStagedStimuliToDevice();
			*/
			



		}
	}

	inline std::string getStringSetting(const std::string& settingName, std::string& settingsInfo){
		size_t start = settingsInfo.find(settingName.c_str());
		if(start == std::string::npos){ // there isn't that setting
			LOGALERT("Setting %s does not exist! Is this an old file version?", settingName.c_str());
			return "";
		}
		start += settingName.size();
		size_t end = settingsInfo.find("\n", start);
		std::string s = settingsInfo.substr(start, end - start);
		return s;
	}

private:



	void _play(){

		//timeBeginPeriod(1);

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

		// open all the file streams for reading
		contextStimStream = std::fstream(settings.stimFileName, std::ios::binary | std::ios::in);
		contextStimTypes = std::fstream(settings.stimTypesFileName, std::ios::binary | std::ios::in);
		contextDataStream = std::fstream(settings.dataFileName, std::ios::binary | std::ios::in);
		contextTimeStream = std::fstream(settings.timeFileName, std::ios::binary | std::ios::in);

		// seek time and data streams to the beginning
		contextStimStream.seekg(0, ::std::ios::beg);
		contextDataStream.seekg(0, ::std::ios::beg);

		// get the first acquire time stamp and set up the clock
		contextTimeStream.seekg(0, ::std::ios::end);
		int tlength = contextTimeStream.tellg();
		contextTimeStream.seekg(tlength - sizeof(uint64_t), ::std::ios::beg); 
		contextTimeStream.read(reinterpret_cast<char*>(&settings.acquisitionEndTime), sizeof(uint64_t));
		if(contextTimeStream.bad()) LOGERROR("Bad init time frame read");
		contextTimeStream.seekg(0, ::std::ios::beg);
		contextTimeStream.read(reinterpret_cast<char*>(&lastAcquireTimeStamp), sizeof(uint64_t));
		if(contextTimeStream.bad()) LOGERROR("Bad init time frame read");
		settings.acquisitionStartTime = settings.acquisitionCurrentTime = systemAcquisitionTimeStamp = lastAcquireTimeStamp;
		settings.fileLengthTimeStamp = ONI::GetAcquisitionTimeStamp(settings.acquisitionStartTime, settings.acquisitionEndTime);

		// load all the device stimili types
		contextStimTypes.seekg(0, ::std::ios::end);
		int slength = contextStimTypes.tellg();
		int numStimTypes = slength / sizeof(ONI::Settings::Rhs2116StimulusSettingsRaw64);
		contextStimTypes.seekg(0, ::std::ios::beg);
		
		allStimSettings.clear();
		allStimSettings.resize(numStimTypes);
		for(size_t t = 0; t < numStimTypes; ++t){
			ONI::Settings::Rhs2116StimulusSettingsRaw64 settingType;
			contextStimTypes.read(reinterpret_cast<char*>(&settingType), sizeof(ONI::Settings::Rhs2116StimulusSettingsRaw64));
			allStimSettings[t].stepSize = settingType.stepSize;
			allStimSettings[t].stimuli.resize(64);
			std::memcpy(allStimSettings[t].stimuli.data(), settingType.stimuli, 64 * sizeof(ONI::Rhs2116StimulusData));
		}

		stimulusID = -1;

		streamMutex.unlock();

		for(auto& device : ONI::Global::model.getDevices()) device.second->reset();

		bPlaybackNeedsRestart = false;
		bPlaybackNeedsDependencyReset = true;
		state = PLAYING;

		realHeartBeatTimer.start();
		playBackSpeedFactor = 1.4; // hacky but helps

		bThread = true;
		thread = std::thread(&RecordProcessor::playFrames, this);
	}
	

	void _record(){

		bPlaybackNeedsStart = bRecordNeedsStart = false;

		if(state == PLAYING || state == RECORDING) stopStreams();

		LOGINFO("Start Recording");

		settings.version = std::to_string(FILE_VERSION);

		settings.timeStamp = ONI::GetTimeStamp();
		settings.fileTimeStamp = ONI::ReverseTimeStamp(settings.timeStamp);

		if(settings.executableDataPath == "") settings.executableDataPath = ONI::GetExecutableDataPath();

		std::ostringstream osPR; osPR << settings.executableDataPath << "\\data\\recordings\\experiment_" <<  settings.fileTimeStamp;

		settings.recordFolder = osPR.str();

		std::ostringstream osD; osD << settings.recordFolder << "\\data_stream_" << settings.fileTimeStamp << ".dat";
		std::ostringstream osT; osT << settings.recordFolder << "\\time_stream_" << settings.fileTimeStamp << ".dat";
		std::ostringstream osS; osS << settings.recordFolder << "\\stim_stream_" << settings.fileTimeStamp << ".dat";
		std::ostringstream osP; osP << settings.recordFolder << "\\stim_types_" << settings.fileTimeStamp << ".dat";
		std::ostringstream osI; osI << settings.recordFolder << "\\info_" << settings.fileTimeStamp << ".txt";

		settings.dataFileName = osD.str();
		settings.timeFileName = osT.str();
		settings.stimFileName = osS.str();
		settings.stimTypesFileName = osP.str();
		settings.infoFileName = osI.str();

		bool bFolder = std::filesystem::create_directories(settings.recordFolder.c_str());

		if(!bFolder){
			LOGERROR("Could not create folder: %s", settings.recordFolder.c_str());
			return;
		} else{
			LOGINFO("Created recording folder: %s", settings.recordFolder.c_str())
		}

		saveInfoSettings();

		streamMutex.lock();

		contextStimStream = std::fstream(settings.stimFileName, std::ios::binary | std::ios::out);
		contextStimTypes = std::fstream(settings.stimTypesFileName, std::ios::binary | std::ios::out);
		contextDataStream = std::fstream(settings.dataFileName, std::ios::binary | std::ios::out);
		contextTimeStream = std::fstream(settings.timeFileName, std::ios::binary | std::ios::out);

		
		contextStimStream.seekg(0, ::std::ios::beg);
		contextStimTypes.seekg(0, ::std::ios::beg);
		contextDataStream.seekg(0, ::std::ios::beg);
		contextTimeStream.seekg(0, ::std::ios::beg);

		

		using namespace std::chrono;
		uint64_t systemAcquisitionTimeStamp = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();

		contextTimeStream.write(reinterpret_cast<char*>(&systemAcquisitionTimeStamp), sizeof(uint64_t));

		if(contextTimeStream.bad()) LOGERROR("Bad init time frame write");
		settings.acquisitionStartTime = systemAcquisitionTimeStamp;

		bIsStimulating = false;
		stimulusID = -1;
		allStimSettings.clear();

		streamMutex.unlock();

		state = RECORDING;

	}

	
	bool bIsStimulating = false;
	int stimulusID = -1;
	std::vector<ONI::Settings::Rhs2116StimulusSettings> allStimSettings;
	ONI::Settings::Rhs2116StimulusSettings defaultStimDeviceSettings;

	inline void setStimTriggerDevices(const ONI::Settings::Rhs2116StimulusSettings& deviceSettings){
		const std::lock_guard<std::mutex> lock(streamMutex);

		stimulusID = -1;
		for(size_t s = 0; s < allStimSettings.size(); ++s){
			if(allStimSettings[s] == deviceSettings){
				stimulusID = s;
				break;
			}
		}

		if(stimulusID == -1){
			stimulusID = allStimSettings.size();
			allStimSettings.push_back(deviceSettings);
			ONI::Settings::Rhs2116StimulusSettingsRaw64 saveSettings;
			saveSettings.stepSize = deviceSettings.stepSize;
			std::memcpy(saveSettings.stimuli, deviceSettings.stimuli.data(), 64 * sizeof(ONI::Rhs2116StimulusData));
			contextStimTypes.write(reinterpret_cast<char*>(&saveSettings), sizeof(ONI::Settings::Rhs2116StimulusSettingsRaw64));
		}

	}

	inline const std::vector<ONI::Settings::Rhs2116StimulusSettings>& getAllStimulusSettings(){
		return allStimSettings;
	}

	inline const int& getStimID(){
		return stimulusID;
	}

	inline void setStimRecording(const bool& b){
		const std::lock_guard<std::mutex> lock(streamMutex);
		bIsStimulating = b;
	}

	void recordFrame(oni_frame_t* frame){

		if(state == RECORDING){

			using namespace std::chrono;
			uint64_t systemAcquisitionTimeStamp = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();

			ONI::Frame::Rhs2116DataRaw* frame_out = new ONI::Frame::Rhs2116DataRaw;

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

			int stimID = bIsStimulating ? stimulusID : -1;
			contextStimStream.write(reinterpret_cast<char*>(&stimID), sizeof(int));
			if(contextStimStream.bad()){
				LOGERROR("Bad stim frame write");
			}

			streamMutex.unlock();
			delete frame_out;

			settings.fileLengthTimeStamp = ONI::GetAcquisitionTimeStamp(settings.acquisitionStartTime, settings.acquisitionCurrentTime);

		}else{
			std::this_thread::yield();
		}

	}

	
	void playFrames(){

		while(bThread){

			if(state == PLAYING){

				using namespace std::chrono;
				int elapsed = 0;
				int start = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();

				streamMutex.lock();
				contextTimeStream.read(reinterpret_cast<char*>(&systemAcquisitionTimeStamp),  sizeof(uint64_t));
				if(contextTimeStream.bad()) LOGERROR("Bad time frame read");

				settings.acquisitionCurrentTime = systemAcquisitionTimeStamp;

				contextStimStream.read(reinterpret_cast<char*>(&stimulusID), sizeof(int));
				if(contextStimStream.bad()){
					LOGERROR("Bad stim frame write");
				}

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

					// This is HORRIBLE but it works - for various reasons
					// (clock drift, load variation, thread locks), the system
					// timestamps are drifting badly out of time, so this hack
					// compares the heartbeat Hz to the real time between decoding
					// the heartbeat frames and adjusts a multiplier for the 
					// elapsed time between frames - like I said it's HORRIBLE
					// 
					// TODO: use a lower resolution clock to buffer frames???

					if(frame->dev_idx == 0){ 
						realHeartBeatAvg = (double)(realHeartBeatTimer.stop() / nanos_to_seconds * settings.heartBeatRateHz);
						realHeartBeatTimer.start();
						if(realHeartBeatAvg > 1) playBackSpeedFactor += (realHeartBeatAvg - 1) / 2.0;
						if(realHeartBeatAvg < 1) playBackSpeedFactor -= (1 - realHeartBeatAvg) / 2.0;
						//LOGDEBUG("Heartbeat Acq: %I64u %0.3f %0.3f", frame->time, realHeartBeatAvg, mFactor);
					}
					
					auto device = it->second;
					device->process(frame);	

					sendHeartBeat();


				}

				delete [] frame->data;
				delete frame_in;

				int deltaTimeStamp = systemAcquisitionTimeStamp - lastAcquireTimeStamp;

				if(deltaTimeStamp < 0){
					LOGALERT("Playback delta time negative: %i", deltaTimeStamp);
					deltaTimeStamp = 0;
				}

				

				while(deltaTimeStamp - elapsed * playBackSpeedFactor  > 0){ // hmmm this is a total HORRIBLE hack for now
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

	void exportToAudio(){

		stop();

		bBadFrame = false;
		nextDeviceCounter = 0;
		multiFrameBufferRaw.resize(4);

		contextDataStream = std::fstream(settings.dataFileName, std::ios::binary | std::ios::in);// | std::ios::out);// | std::ios::app);
		contextDataStream.seekg(0, ::std::ios::beg);


		std::vector<float> samples;
		size_t probe = 50;




		while(!contextDataStream.eof()){
			ONI::Frame::Rhs2116DataRaw* frame_in = new ONI::Frame::Rhs2116DataRaw;
			contextDataStream.read(reinterpret_cast<char*>(frame_in), sizeof(ONI::Frame::Rhs2116DataRaw));
			if(contextDataStream.bad()) LOGERROR("Bad data frame read");
			oni_frame_t* frame = reinterpret_cast<oni_frame_t*>(frame_in); // this is kinda nasty ==> will it always work

			frame->data = new char[74];
			memcpy(frame->data, frame_in->data, 74);

			size_t nextDeviceIndex = nextDeviceCounter % 4;
			//LOGDEBUG("Dev-IDX: %i", frame->dev_idx);
			ONI::Frame::Rhs2116DataExtended& frameRaw = multiFrameRawMap[frame->dev_idx];
			std::memcpy(&frameRaw, frame->data, frame->data_sz); // copy the data payload including hub clock
			frameRaw.acqTime = frame->time;						 // copy the acquisition clock
			frameRaw.deltaTime = ONI::Global::model.getAcquireDeltaTimeMicros(frame->time);
			frameRaw.devIdx = frame->dev_idx;

			if(nextDeviceCounter > 0 && nextDeviceCounter % 4 == 0){

				if(multiFrameRawMap.size() == 4){

					if(bBadFrame){
						LOGDEBUG("Pickup  multiframe %i (%llu) ==> %i", frame->dev_idx, frameRaw.hubTime, nextDeviceCounter);
						bBadFrame = false;
					}

					//lastMultiFrameRawMap = multiFrameRawMap;

					// order the frames by ascending device idx
					for(size_t i = 0; i < 4; ++i){
						multiFrameBufferRaw[i] = std::move(multiFrameRawMap[ONI::Global::model.getRhs2116DeviceOrderIDX()[i]]);
					}

					// process the multi frame
					ONI::Frame::Rhs2116MultiFrame processedFrame(multiFrameBufferRaw, ONI::Global::model.getChannelMapProcessor()->getChannelMap());

					/////////////////////////////////////////
					/////// PROCESS THE MULTIFRAME
					////////////////////////////////////////
					static float phase = 0;
					phase += 0.4;
					float sample = sin(phase);// ofMap(processedFrame.ac_uV[probe], -0.06, 0.06, -1.0, 1.0, true);
					samples.push_back(sample);

					if(samples.size() > RHS2116_SAMPLE_FREQUENCY_HZ * 60){ // 30 seconds
						LOGDEBUG("Try saving a chunk of samples");

						ofSoundBuffer b; b.setNumChannels(1);
						b.copyFrom(samples, 1, samples.size());
						b.setSampleRate(RHS2116_SAMPLE_FREQUENCY_HZ);
						//ofSoundBuffer h; h.setNumChannels(1);  h.resize(48000 * 30);
						//b.resampleTo(h, 0, h.getNumFrames(), 1.0, false, ofSoundBuffer::InterpolationAlgorithm::Hermite);
						//h.setSampleRate(48000);

						std::string path = ofToDataPath("sin.wav");

						writeWAVData(path.c_str(), b.getBuffer().data(), b.getBuffer().size(), RHS2116_SAMPLE_FREQUENCY_HZ, 1);


						samples.clear();
					}

					// clear the map to keep tracking device idx
					multiFrameRawMap.clear();

				} else{

					//LOGERROR("Out of order for multiframe %i ==> %i", frame->dev_idx, nextDeviceCounter);
					//size_t t = multiFrameRawMap.size();
					//std::memcpy(&errorFrameRaw, frame->data, frame->data_sz); // copy the data payload including hub clock
					//errorFrameRaw.acqTime = frame->time;						 // copy the acquisition clock
					//errorFrameRaw.deltaTime = ONI::Device::BaseDevice::getAcqDeltaTimeMicros(frame->time);
					//errorFrameRaw.devIdx = frame->dev_idx;

					LOGDEBUG("Dropped multiframe %i (%llu) ==> %i", frame->dev_idx, reinterpret_cast<ONI::Frame::Rhs2116DataExtended*>(frame->data)->hubTime, nextDeviceCounter);

					bBadFrame = true;
					--nextDeviceCounter; // decrease the device id counter so we can check if this gets corrected on next frame
				}
			}

			++nextDeviceCounter;

			delete[] frame->data;
			delete frame_in;

		}

		LOGDEBUG("Samples: %d", samples.size());

	}

	inline void sendHeartBeat(){
		frameCounter++;
		if(frameCounter > 30000){
			//LOGDEBUG("BEAT");
			frameCounter = 0;
			ofxOscMessage m;
			m.setAddress("/alive");
			oscHeartBeat.sendMessage(m);
		}
	}

protected:

	ofxOscSender oscHeartBeat;
	uint64_t frameCounter = 0;

	fu::Timer realHeartBeatTimer;
	double realHeartBeatAvg = 1;
	float playBackSpeedFactor = 1.4;
	bool bBadFrame = false;
	int nextDeviceCounter = 0;

	ONI::Frame::Rhs2116DataExtended errorFrameRaw;
	std::map<uint32_t, ONI::Frame::Rhs2116DataExtended> lastMultiFrameRawMap;
	std::map<uint32_t, ONI::Frame::Rhs2116DataExtended> multiFrameRawMap;
	std::vector<ONI::Frame::Rhs2116DataExtended> multiFrameBufferRaw;

	

	ONI::Settings::RecordSettings settings;

	std::fstream contextDataStream;
	std::fstream contextTimeStream;
	std::fstream contextStimTypes;
	std::fstream contextStimStream;

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























		//std::map<uint32_t, ONI::Device::BaseDevice*>& devices = ONI::Global::model.getDevices();
		//for(auto& it : devices){
		//	auto dit = deviceChannels.find(it.first);
		//	if(dit == deviceChannels.end()){
		//		deviceChannels[it.first] = new DeviceChannel;
		//		if(it.first == 256 || it.first == 257 || it.first == 512 || it.first == 513){
		//			deviceChannels[it.first]->setup(it.second, RHS2116_SAMPLE_WAIT_TIME_NS); // 33119.999999863810560000560010977
		//		} else{
		//			deviceChannels[it.first]->setup(it.second, 0);
		//		}
		//		
		//	}
		//}

				/*
				auto it = deviceChannels.find((uint32_t)frame_in->dev_idx);

				if(it == deviceChannels.end()){
					LOGERROR("ONI DeviceChannel doesn't exist with idx: %i", frame_in->dev_idx);
				}else{
					it->second->push(frame_in);
					//delete frame_in;
				}

				delete frame_in;
				*/


//std::map<uint32_t, DeviceChannel*> deviceChannels;
//class DeviceChannel{
//
//public:
//
//	~DeviceChannel(){};
//
//	void setup(ONI::Device::BaseDevice* device, const int& waitTimeNanos){
//		this->device = device;
//		this->waitTimeNanos = waitTimeNanos;
//		bThread = true;
//		thread = std::thread(&DeviceChannel::process, this);
//	}
//
//	inline void push(ONI::Frame::Rhs2116DataRaw* frameRaw){
//		const std::lock_guard<std::mutex> lock(mutex);
//		frames.push(new ONI::Frame::Rhs2116DataRaw);
//		std::memcpy(frames.back(), frameRaw, sizeof(ONI::Frame::Rhs2116DataRaw));
//	}
//
//	inline void process(){
//
//		while(bThread){
//
//			mutex.lock();
//
//			if(frames.size() > 0){
//
//				ONI::Frame::Rhs2116DataRaw* frame_in = frames.front();
//
//				oni_frame_t* frame = reinterpret_cast<oni_frame_t*>(frame_in); // this is kinda nasty ==> will it always work
//
//				frame->data = new char[74];
//				memcpy(frame->data, frame_in->data, 74);
//
//				device->process(frame);
//
//				delete[] frame->data;
//				delete frame_in;
//
//				frames.pop();
//
//				using namespace std::chrono;
//				uint64_t elapsed = 0;
//				uint64_t start = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
//				while(elapsed - start < waitTimeNanos){
//					elapsed = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
//					std::this_thread::yield();
//				}
//
//			}else{
//				std::this_thread::yield();
//			}
//			
//			mutex.unlock();
//
//		}
//
//	}
//
//	volatile uint64_t lastAcquireTimeStamp = 0;
//	volatile int waitTimeNanos = 0;
//
//	ONI::Device::BaseDevice * device = nullptr;
//	std::queue<ONI::Frame::Rhs2116DataRaw*> frames;
//
//	volatile bool bThread = false;
//	std::mutex mutex;
//	std::thread thread;
//
//};

