//
//  ONIContext.h
//
//  Created by Matt Gingold on 12.09.2024.
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

#include "../Type/DataBuffer.h"
#include "../Type/DeviceTypes.h"
#include "../Type/FrameTypes.h"
#include "../Type/RegisterTypes.h"
#include "../Type/SettingTypes.h"
#include "../Type/Log.h"

#include "../Device/BaseDevice.h"
#include "../Device/FmcDevice.h"
#include "../Device/HeartBeatDevice.h"
#include "../Device/Rhs2116Device.h"
#include "../Device/Rhs2116MultiDevice.h"
#include "../Device/Rhs2116StimulusDevice.h"

#include "../Interface/BaseInterface.h"
#include "../Interface/FmcInterface.h"
#include "../Interface/HeartBeatInterface.h"
#include "../Interface/Rhs2116Interface.h"
#include "../Interface/Rhs2116MultiInterface.h"
#include "../Interface/Rhs2116StimulusInterface.h"

#include "../Processor/FrameProcessor.h"
#include "../Processor/SpikeProcessor.h"

#pragma once

// add DELETE copy constructor to contextr and devices
//   noncopyable(const noncopyable&) =delete;
//	 noncopyable& operator=(const noncopyable&) =delete;

inline std::ostream& operator<<(std::ostream& os, const oni_device_t& type) {

	char typeBuffer[256];

	sprintf(typeBuffer, "%05u: 0x%02x.0x%02x\t|%02d\t|%02d\t|%02u\t|%02u\t|%s",
			type.idx,
			(uint8_t)(type.idx >> 8),
			(uint8_t)type.idx,
			type.id,
			type.version,
			type.read_size,
			type.write_size,
			onix_device_str(type.id));

	os << typeBuffer;
	return os;

}

namespace ONI{

namespace Interface{
class ContextInterface; // pre declare for gui/config
} // namespace Interface


class Context {

public:

	friend class ONI::Interface::ContextInterface;


	Context(){};

	~Context(){
		closeContext();
	};

	inline void update(){

		if(!bIsContextSetup) return;

		bool bContextNeedsRestart = false;
		bool bContextNeedsReset = false;

		// check all the devices to see if they require an context restart eg., PORTVOLTAGE

		ONI::Device::BaseDevice* device_changed = NULL;

		for(auto device : oniDevices){
			if(device.second->getContexteNeedsRestart()){
				device_changed = (ONI::Device::BaseDevice*)device.second;
				bContextNeedsRestart = true;
				break;
			}
			if(device.second->getContexteNeedsReset()){
				device_changed = (ONI::Device::BaseDevice*)device.second;
				bContextNeedsReset = true;
				//break;
			}
		}

		if(bContextNeedsRestart){
			LOGDEBUG("Device requires context restart: %s", device_changed->getName().c_str());
			setupContext();

		}

		if(bContextNeedsReset){
			LOGDEBUG("Device requires context reset: %s", device_changed->getName().c_str());
			bool bResetAcquire = bIsAcquiring;
			if(bResetAcquire) stopAcquisition();
			setContextOption(ONI::ContextOption::RESET, 1);
			//setContextOption(ONIContext::BLOCKREADSIZE, blockReadBytes); // WHY? For some reason this needs resetting 
			//setContextOption(ONIContext::BLOCKWRITESIZE, blockWriteBytes);
			if(bResetAcquire){ 
				startAcquisition();
			}else{
				device_changed->reset();
			}
		}

		if(bStopPlaybackThread) stopPlaying();

	}

	bool setup(const std::string& driverName = "riffa", const unsigned int& blockReadBytes = 2048, const unsigned int& blockWriteBytes = 2048){
		this->driverName = driverName;
		this->blockReadBytes = blockReadBytes;
		this->blockWriteBytes = blockWriteBytes;

		return setupContext();
	}

	const std::vector<oni_device_t>& getDeviceTypes(const bool& bForceList = false){

		if (!bForceList && deviceTypes.size() > 0) return deviceTypes; // is this just dangerous and stupid?

		LOGDEBUG("Enumerating devices...");

		clearDevices();

		if(!bIsContextSetup){
			LOGERROR("Context and driver not setup!");
			return deviceTypes;
		}

		int rc = ONI_ESUCCESS;

		// Examine device table
		oni_size_t num_devs = getContextOption(ONI::ContextOption::NUMDEVICES);

		oni_device_t* devices_t;

		// Get the device table
		size_t devices_sz = sizeof(oni_device_t) * num_devs;
		devices_t = (oni_device_t*)malloc(devices_sz);
		if(devices_t == NULL){
			LOGERROR("No devices found!!");
			free(devices_t);
			return deviceTypes;
		}

		oni_get_opt(ctx, ONI_OPT_DEVICETABLE, devices_t, &devices_sz);

		for(size_t i = 0; i < num_devs; ++i){

			deviceTypes.push_back(devices_t[i]);

			if(devices_t[i].id == ONI::Device::TypeID::FMC){ // 23 FMC Host Device
				LOGDEBUG("Mapping FMC Host Device: %i device idx: %i", devices_t[i].id, devices_t[i].idx);
				ONI::Device::FmcDevice* fmc = new ONI::Device::FmcDevice;
				fmc->setup(&ctx, devices_t[i], acq_clock_khz);
				fmc->reset();
				oniDevices[devices_t[i].idx] = fmc;
			}

			if(devices_t[i].id == ONI::Device::TypeID::HEARTBEAT){ // 12 HeartBeat Device
				LOGDEBUG("Mapping HeartBeat Device: %i device idx: %i", devices_t[i].id, devices_t[i].idx);
				ONI::Device::HeartBeatDevice* hbd = new ONI::Device::HeartBeatDevice;
				hbd->setup(&ctx, devices_t[i], acq_clock_khz);
				hbd->reset();
				oniDevices[devices_t[i].idx] = hbd;
			}

			if(devices_t[i].id == ONI::Device::TypeID::RHS2116){ // 31 RHS2116 Device
				LOGDEBUG("Mapping Rhs2116 Device: %i device idx: %i", devices_t[i].id, devices_t[i].idx);
				ONI::Device::Rhs2116Device* rhd2116 = new ONI::Device::Rhs2116Device;
				rhd2116->setup(&ctx, devices_t[i], acq_clock_khz);
				rhd2116->reset();
				oniDevices[devices_t[i].idx] = rhd2116;
			}

		}

		free(devices_t);

		LOGINFO("Found %i Devices", deviceTypes.size());

		return deviceTypes;

	}

	ONI::Device::BaseDevice* getDevice(const unsigned int& idx){
		auto it = oniDevices.find(idx);
		if(it == oniDevices.end()){
			LOGERROR("ONIDevice doesn't exist with idx: %i", idx);
			return NULL;
		}
		return it->second;
	}

	void printDeviceTable(){

		getDeviceTypes(); // make sure we have the updated list?

		std::ostringstream os;

		os << "   +--------------------+-------+-------+-------+-------+---------------------" << std::endl;
		os << "   |        \t\t|  \t|Firm.\t|Read\t|Wrt. \t|     " << std::endl;
		os << "   |Dev. idx\t\t|ID\t|ver. \t|size\t|size \t|Desc." << std::endl;
		os << "   +--------------------+-------+-------+-------+-------+---------------------" << std::endl;

		for(size_t i = 0; i < deviceTypes.size(); ++i){
			os << std::setfill('0') << std::setw(2) << i << " |" << deviceTypes[i] << std::endl; // see stream operator<< overload for oni_device_t above
		}

		LOGINFO("ONI Device Table\n%s", os.str().c_str());

	}

	oni_size_t getMaxReadSize(){
		assert(maxReadSize > 0);
		return maxReadSize;
	}

	oni_size_t getMaxWriteSize(){
		assert(maxWriteSize > 0);
		return maxWriteSize;
	}

	oni_size_t getAcquireClockKHZ(){
		assert(acq_clock_khz > 0);
		return acq_clock_khz;
	}

	oni_size_t getSysClockKHZ(){
		assert(sys_clock_khz > 0);
		return sys_clock_khz;
	}

	oni_size_t setContextOption(const ONI::ContextOption::BaseOption& option, const oni_size_t& val){
		int rc = ONI_ESUCCESS;
		oni_size_t val_result;
		size_t val_sz = sizeof(val);
		rc = oni_set_opt(ctx, option.getOption(), &val, val_sz);
		oni_get_opt(ctx, option.getOption(), &val_result, &val_sz);
		if(rc && val_result != val){
			LOGERROR("Can't set context option: %s :: %s", option.getName().c_str(), oni_error_str(rc));
			return -1;
		}
		return val;
	}

	oni_size_t getContextOption(const ONI::ContextOption::BaseOption& option){
		int rc = ONI_ESUCCESS;
		oni_size_t val = (oni_size_t)0;
		size_t val_sz = sizeof(val);
		rc = oni_get_opt(ctx, option.getOption(), &val, &val_sz);
		if(rc){ LOGERROR("Can't get context option: %s :: %s", option.getName().c_str(), oni_error_str(rc)); return -1; };
		LOGINFO("Context option: %s == %i", option.getName().c_str(), val);
		return val;
	}

	volatile oni_ctx* getContext(){
		return &ctx;
	}

	void startAcquisition(){
		startFrameRead();
		startContext();
		bIsAcquiring = true;
	}

	void stopAcquisition(){
		stopFrameRead();
		stopContext();
		bIsAcquiring = false;
	}

	void closeContext(){

		LOGDEBUG("Closing ONIContext...");

		if(ctx == NULL) return; // nothing to do

		bIsAcquiring = false;

		stopFrameRead();
		clearDevices();
		stopContext();

		oni_destroy_ctx(ctx);

		LOGINFO("...ONIContext closed");

	}

	void startRecording(){
		if(bIsPlaying) stopPlaying();
		if(bIsRecording) stopRecording();
		if(bIsAcquiring) stopAcquisition();
		contextDataStream = std::fstream(ofToDataPath("data_stream.dat"), std::ios::binary | std::ios::in | std::ios::out | std::ios::app);
		contextTimeStream = std::fstream(ofToDataPath("time_stream.dat"), std::ios::binary | std::ios::in | std::ios::out | std::ios::app);

		using namespace std::chrono;
		uint64_t systemAcquisitionTimeStamp = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
		contextTimeStream.write(reinterpret_cast<char*>(&systemAcquisitionTimeStamp), sizeof(uint64_t));
		if(contextTimeStream.bad()) LOGERROR("Bad init time frame write");

		bIsRecording = true;
		startAcquisition();
	}

	void stopRecording(){
		stopAcquisition();
		contextDataStream.close();
		contextTimeStream.close();
		bIsRecording = false;
	}

	void startPlayng(){
		if(bIsPlaying) stopPlaying();
		if(bIsRecording) stopRecording();
		if(bIsAcquiring) stopAcquisition();
		contextDataStream = std::fstream(ofToDataPath("data_stream.dat"), std::ios::binary | std::ios::in | std::ios::out | std::ios::app);
		contextTimeStream = std::fstream(ofToDataPath("time_stream.dat"), std::ios::binary | std::ios::in | std::ios::out | std::ios::app);
		contextDataStream.seekg(0, ::std::ios::beg);
		contextTimeStream.seekg(0, ::std::ios::beg);
		contextTimeStream.read(reinterpret_cast<char*>(&lastAcquireTimeStamp),  sizeof(uint64_t));
		if(contextTimeStream.bad()) LOGERROR("Bad init time frame read");
		for(auto device : oniDevices) device.second->reset();
		bThread = true;
		bIsPlaying = true;
		thread = std::thread(&Context::playFrames, this);

		//startAcquisition();
	}

	void stopPlaying(){
		if(!bThread) return;
		bThread = false;
		if(thread.joinable()) thread.join();
		contextDataStream.close();
		contextTimeStream.close();
		bIsPlaying = false;
		bStopPlaybackThread = false;
	}

	ONI::Device::Rhs2116MultiDevice* getMultiDevice(const bool& bForceNewDevice = false){
		if(rhs2116MultiDevice != nullptr && bForceNewDevice){
			LOGINFO("Deleting old multi device");
			delete rhs2116MultiDevice;
			rhs2116MultiDevice = nullptr;
		}
		if(rhs2116MultiDevice == nullptr){
			LOGINFO("Creating new multi device");
			rhs2116MultiDevice = new ONI::Device::Rhs2116MultiDevice;
			rhs2116MultiDevice->setup(&ctx, acq_clock_khz);
		}
		return rhs2116MultiDevice;
	}

	ONI::Device::Rhs2116StimulusDevice* getStimulusDevice(const bool& bForceNewDevice = false){
		if(rhs2116StimDevice != nullptr && bForceNewDevice){
			LOGINFO("Deleting old stimulus device");
			delete rhs2116StimDevice;
			rhs2116StimDevice = nullptr;
		}
		if(rhs2116StimDevice == nullptr){
			LOGINFO("Creating new stimulus device");
			rhs2116StimDevice = new ONI::Device::Rhs2116StimulusDevice;
			rhs2116StimDevice->setup(rhs2116MultiDevice);
		}
		return rhs2116StimDevice;
	}

	ONI::Processor::SpikeProcessor* getSpikeProcessor(const bool& bForceNewProcessor = false){
		if(spikeProcessor != nullptr && bForceNewProcessor){
			LOGINFO("Deleting old spike processor");
			delete spikeProcessor;
			spikeProcessor = nullptr;
		}
		if(spikeProcessor == nullptr){
			LOGINFO("Creating new spike processor");
			spikeProcessor = new ONI::Processor::SpikeProcessor;
			spikeProcessor->setup(rhs2116MultiDevice);
		}
		return spikeProcessor;
	}

	ONI::Processor::FrameProcessor* getFrameProcessor(const bool& bForceNewProcessor = false){
		if(frameProcessor != nullptr && bForceNewProcessor){
			LOGINFO("Deleting old spike processor");
			delete frameProcessor;
			frameProcessor = nullptr;
		}
		if(frameProcessor == nullptr){
			LOGINFO("Creating new spike processor");
			frameProcessor = new ONI::Processor::FrameProcessor;
			frameProcessor->setup(rhs2116MultiDevice);
		}
		return frameProcessor;
	}

private:

	bool startContext(){
		LOGINFO("Starting ONI context");
		setContextOption(ONI::ContextOption::BLOCKREADSIZE, blockReadBytes);
		setContextOption(ONI::ContextOption::BLOCKWRITESIZE, blockWriteBytes);
		setContextOption(ONI::ContextOption::RESETACQCOUNTER, 2);
		if(getContextOption(ONI::ContextOption::RUNNING) != 1){
			LOGERROR("Could not start hardware"); // necessary?
			return false;
		}
		//startFrameRead();
		return true;
	}

	bool stopContext(){
		LOGINFO("Stopping ONI context");
		//stopFrameRead();
		if(setContextOption(ONI::ContextOption::RUNNING, 0) == -1){
			LOGERROR("Could not stop hardware"); // necessary?
			return false;
		}
		return true;
	}

	//std::ifstream contextPlayStream;
	void startFrameRead(){
		if(bThread) stopFrameRead();
		LOGDEBUG("Starting frame read thread");
		bThread = true;
		for(auto device : oniDevices) device.second->reset();
		thread = std::thread(&Context::readFrames, this);
	}

	void stopFrameRead(){
		LOGDEBUG("Stopping frame read thread");
		if(!bThread) return;
		bThread = false;
		if(thread.joinable()) thread.join();
		//contextPlayStream.close();
	}

	bool setupContext(){

		LOGINFO("Setting up ONI context");

		if(bIsContextSetup){
			LOGDEBUG("Context already exists, destroying...");
			closeContext();
		}

		bIsContextSetup = false; // TODO: check and warn/tear down if it's already setup?

		int rc = ONI_ESUCCESS;

		// Generate context
		ctx = oni_create_ctx(driverName.c_str());

		if(!ctx){
			LOGERROR("ONI context create: FAIL with driver: %s", driverName.c_str());
			return false;
		}

		LOGINFO("ONI context create: OK with driver: %s", driverName.c_str());

		// Print the driver translator informaiton
		char driver_version[128];
		const oni_driver_info_t* di = oni_get_driver_info(ctx);
		if(di->pre_release == NULL){
			LOGINFO("Driver version patch: %s v%d.%d.%d", di->name, di->major, di->minor, di->patch);
		}else{
			LOGINFO("Driver version pre-release: %s v%d.%d.%d-%s", di->name, di->major, di->minor, di->patch, di->pre_release);
		}

		// Initialize context and discover hardware
		rc = oni_init_ctx(ctx, host_idx);
		if(rc){
			LOGERROR("Hardware init: FAIL ==> %s", oni_error_str(rc));
			return false;
		}else{
			LOGINFO("Hardware init: OK");
		}

		// there's a firmware bug which requires two context inits 
		// for PORTVOLTAGE to be set, so let's just always do it!
		oni_destroy_ctx(ctx);
		ctx = oni_create_ctx(driverName.c_str());
		rc = oni_init_ctx(ctx, host_idx);

		if(rc){
			LOGERROR("Hardware re init: FAIL ==> %s", oni_error_str(rc));
			return false;
		}else{
			LOGINFO("Hardware re init: OK");
		}

		maxReadSize = getContextOption(ONI::ContextOption::MAXREADFRAMESIZE);
		maxWriteSize = getContextOption(ONI::ContextOption::MAXWRITEFRAMESIZE);
		acq_clock_khz = getContextOption(ONI::ContextOption::ACQCLKHZ);
		sys_clock_khz = getContextOption(ONI::ContextOption::SYSCLKHZ);

		if(maxReadSize == -1) return false;
		if(maxWriteSize == -1) return false;
		if(acq_clock_khz == -1) return false;
		if(sys_clock_khz == -1) return false;

		//blockReadBytes = setContextOption(BLOCKREADSIZE, blockReadSize);
		//blockWriteBytes = setContextOption(BLOCKWRITESIZE, blockWriteSize);

		//if(blockReadBytes == -1) return false;
		//if(blockWriteBytes == -1) return false;

		bIsContextSetup = startContext(); // is this ok? or should the user do it!?

		getDeviceTypes(true); // hmm leave this to the user?

		return true;

	}

	void recordFrame(oni_frame_t* frame){

		if(!bIsRecording) return;

		if(frame->dev_idx == 256 || frame->dev_idx == 257){

			using namespace std::chrono;
			uint64_t systemAcquisitionTimeStamp = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();

			ONI::Frame::Rhs2116DataRaw * frame_out = new ONI::Frame::Rhs2116DataRaw;

			frame_out->time = frame->time;
			frame_out->dev_idx = frame->dev_idx;
			frame_out->data_sz = frame->data_sz;

			std::memcpy(frame_out->data, frame->data, frame->data_sz);

			contextDataStream.write(reinterpret_cast<char*>(frame_out), sizeof(ONI::Frame::Rhs2116DataRaw));
			if(contextDataStream.bad()) LOGERROR("Bad data frame write");

			contextTimeStream.write(reinterpret_cast<char*>(&systemAcquisitionTimeStamp), sizeof(uint64_t));
			if(contextTimeStream.bad()) LOGERROR("Bad time frame write");

			delete frame_out;

		}

	}

	void playFrames(){

		while(bThread){

			uint64_t systemAcquisitionTimeStamp = 0;

			contextTimeStream.read(reinterpret_cast<char*>(&systemAcquisitionTimeStamp),  sizeof(uint64_t));
			if(contextTimeStream.bad()) LOGERROR("Bad time frame read");

			ONI::Frame::Rhs2116DataRaw * frame_in = new ONI::Frame::Rhs2116DataRaw;

			contextDataStream.read(reinterpret_cast<char*>(frame_in),  sizeof(ONI::Frame::Rhs2116DataRaw));
			if(contextDataStream.bad()) LOGERROR("Bad data frame read");

			if(contextDataStream.eof()) break;

			oni_frame_t * frame = reinterpret_cast<oni_frame_t*>(frame_in); // this is kinda nasty ==> will it always work

			frame->data = new char[74];
			memcpy(frame->data, frame_in->data, 74);

			auto it = oniDevices.find((unsigned int)frame->dev_idx);

			if(it == oniDevices.end()){
				LOGERROR("ONIDevice doesn't exist with idx: %i", frame->dev_idx);
			}else{

				auto device = it->second;
				device->process(frame);	

			}

			delete [] frame->data;
			delete frame_in;

			uint64_t deltaTimeStamp = systemAcquisitionTimeStamp - lastAcquireTimeStamp;

			using namespace std::chrono;
			uint64_t elapsed = 0;
			uint64_t start = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();

			while(elapsed < deltaTimeStamp){
				elapsed = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count() - start;
				std::this_thread::yield();
			}

			//LOGDEBUG("delta: %i || actual: %i", deltaTimeStamp, elapsed);

			lastAcquireTimeStamp = systemAcquisitionTimeStamp;



		}

		LOGINFO("Playback EOF");
		bStopPlaybackThread = true;

	}

	void readFrames(){

		while(bThread){

			int rc = ONI_ESUCCESS;

			oni_frame_t *frame = NULL;
			rc = oni_read_frame(ctx, &frame);

			if(rc < 0){
				LOGERROR("Frame read error: %s", oni_error_str(rc));
			}else{

				auto it = oniDevices.find((unsigned int)frame->dev_idx);

				if(it == oniDevices.end()){
					LOGERROR("ONIDevice doesn't exist with idx: %i", frame->dev_idx);
				}else{

					auto device = it->second;

					recordFrame(frame);
					device->process(frame);	

				}

			}

			oni_destroy_frame(frame);

		}

	}

	void clearDevices(){

		deviceTypes.clear();

		for(auto device : oniDevices){
			delete device.second;
		}

		oniDevices.clear();

		delete frameProcessor;
		frameProcessor = nullptr;

		delete spikeProcessor;
		spikeProcessor = nullptr;

		delete rhs2116StimDevice;
		rhs2116StimDevice = nullptr;

		delete rhs2116MultiDevice;
		rhs2116MultiDevice = nullptr;

	}

private:

	ONI::Device::Rhs2116MultiDevice * rhs2116MultiDevice = nullptr; // TODO: this better!!
	ONI::Device::Rhs2116StimulusDevice * rhs2116StimDevice = nullptr;

	ONI::Processor::FrameProcessor * frameProcessor = nullptr;
	ONI::Processor::SpikeProcessor * spikeProcessor = nullptr;

	

	unsigned int blockReadBytes = 0;
	unsigned int blockWriteBytes = 0;

	std::atomic_bool bIsContextSetup = false;
	std::atomic_bool bIsAcquiring = false;

	std::fstream contextDataStream;
	std::fstream contextTimeStream;

	uint64_t lastAcquireTimeStamp = 0;

	std::atomic_bool bIsRecording = false;
	std::atomic_bool bIsPlaying = false;

	std::atomic_bool bStopPlaybackThread = false;
	std::atomic_bool bThread = false;

	std::thread thread;
	std::mutex mutex;

	volatile oni_ctx ctx = NULL;

	std::map<unsigned int, ONI::Device::BaseDevice*> oniDevices;
	std::vector<oni_device_t> deviceTypes = {};

	int host_idx = -1;
	std::string driverName = "riffa";

	uint32_t maxReadSize = -1;
	uint32_t maxWriteSize = -1;
	uint32_t acq_clock_khz = -1;
	uint32_t sys_clock_khz = -1;


};



} // namespace ONI






