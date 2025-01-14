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
#include "../Type/GlobalTypes.h"
#include "../Type/FrameTypes.h"
#include "../Type/RegisterTypes.h"
#include "../Type/SettingTypes.h"
#include "../Type/Log.h"

#include "../Device/BaseDevice.h"
#include "../Device/FmcDevice.h"
#include "../Device/HeartBeatDevice.h"
#include "../Device/Rhs2116Device.h"
#include "../Device/Rhs2116StimDevice.h"

#include "../Processor/BufferProcessor.h"
#include "../Processor/ChannelMapProcessor.h"
#include "../Processor/SpikeProcessor.h"
#include "../Processor/Rhs2116MultiProcessor.h"
#include "../Processor/Rhs2116StimProcessor.h"

#include "../Interface/BaseInterface.h"
#include "../Interface/FmcInterface.h"
#include "../Interface/HeartBeatInterface.h"
#include "../Interface/Rhs2116Interface.h"
#include "../Interface/Rhs2116MultiInterface.h"
#include "../Interface/Rhs2116StimInterface.h"



// TODO:: Refactor ONIContext to Context and ofxOnix.h
// TODO:: Add BurstFrequency type to the stimulus settings
// DONE:: Visualise/and functionilise suppressing spike detection during stimulation
// DONE:: Refactor Processor and Device base classes
// DONE:: Implement global/model for ctx, clocks, sample rates, channel map etc
// DONE:: Implement re-usable global device and processor factory/model
// TODO:: Heatmap Plots for AC and DC channel mapped probes
// TODO:: Implment audio output of frame buffers
// TODO:: Finish off recorder interface: time, play, pause, load, save, export
// TODO:: Implement spike detector/classifier


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
		std::map<uint32_t, ONI::Device::BaseDevice*>& devices = ONI::Global::model.getDevices();

		for(auto& it : devices){
			ONI::Device::BaseDevice* device = it.second;
			if(device->getContexteNeedsRestart()){ // for now let's auto reset these
				LOGDEBUG("Device requires context restart: %s", device->getName().c_str());
				bContextNeedsRestart = true;
			}
			if(device->getContexteNeedsReset()){ // for now let's auto reset these
				LOGDEBUG("Device requires context reset: %s", device->getName().c_str());
				bContextNeedsReset = true;
			}
		}

		//for(auto it : ONI::Global::model.getProcessors()){
		//	ONI::Device::BaseProcessor* processor = it.second;
		//	if(processor->getContexteNeedsRestart()){
		//		LOGDEBUG("Processor requires context restart: %s", processor->getName().c_str());
		//		bContextNeedsRestart = true;
		//	}
		//	if(processor->getContexteNeedsReset()){
		//		LOGDEBUG("Processor requires context reset: %s", processor->getName().c_str());
		//		bContextNeedsReset = true;
		//	}
		//}

		if(bContextNeedsRestart){
			setupContext();
		}

		if(bContextNeedsReset){
			
			bool bResetAcquire = bIsAcquiring;
			if(bResetAcquire) stopAcquisition();
			setContextOption(ONI::ContextOption::RESET, 1);
			if(bResetAcquire) startAcquisition();
			//deviceRequestedChange->reset();
		}

		if(bStopPlaybackThread) stopPlaying();

	}

	bool setup(const std::string& driverName = "riffa", const uint32_t& blockReadBytes = 2048, const uint32_t& blockWriteBytes = 2048){
		this->driverName = driverName;
		this->blockReadBytes = blockReadBytes;
		this->blockWriteBytes = blockWriteBytes;
		return setupContext();
	}

	

	ONI::Device::BaseDevice* getDevice(const uint32_t& idx){
		std::map<uint32_t, ONI::Device::BaseDevice*>& devices = ONI::Global::model.getDevices();
		auto it = devices.find(idx);
		if(it == devices.end()){
			LOGERROR("ONIDevice doesn't exist with idx: %i", idx);
			return nullptr;
		}
		return it->second;
	}

	void printDeviceTable(){

		//enumerateOnixDeviceTypes(); // make sure we have the updated list?

		if(onixDeviceTypes.size() == 0){
			LOGERROR("No ONIX devices found - perhaps you haven't setup the context?");
			return;
		}

		std::ostringstream os;

		os << "   +--------------------+-------+-------+-------+-------+---------------------" << std::endl;
		os << "   |        \t\t|  \t|Firm.\t|Read\t|Wrt. \t|     " << std::endl;
		os << "   |Dev. idx\t\t|ID\t|ver. \t|size\t|size \t|Desc." << std::endl;
		os << "   +--------------------+-------+-------+-------+-------+---------------------" << std::endl;

		for(size_t i = 0; i < onixDeviceTypes.size(); ++i){
			os << std::setfill('0') << std::setw(2) << i << " |" << onixDeviceTypes[i] << std::endl; // see stream operator<< overload for oni_device_t above
		}

		LOGINFO("ONI Device Table\n%s", os.str().c_str());

	}

	oni_size_t setContextOption(const ONI::ContextOption::BaseOption& option, const oni_size_t& val){
		int rc = ONI_ESUCCESS;
		oni_size_t val_result;
		size_t val_sz = sizeof(val);
		volatile oni_ctx* ctx = ONI::Global::model.getOnixContext();
		rc = oni_set_opt(*ctx, option.getOption(), &val, val_sz);
		oni_get_opt(*ctx, option.getOption(), &val_result, &val_sz);
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
		volatile oni_ctx* ctx = ONI::Global::model.getOnixContext();
		rc = oni_get_opt(*ctx, option.getOption(), &val, &val_sz);
		if(rc){ LOGERROR("Can't get context option: %s :: %s", option.getName().c_str(), oni_error_str(rc)); return -1; };
		LOGINFO("Context option: %s == %i", option.getName().c_str(), val);
		return val;
	}

	//volatile oni_ctx* getContext(){
	//	return &ctx;
	//}

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
		LOGDEBUG("Closing ONIX Context...");
		volatile oni_ctx* ctx = ONI::Global::model.getOnixContext();
		if(ctx == nullptr) return; // nothing to do
		bIsAcquiring = false;
		stopFrameRead();
		onixDeviceTypes.clear();
		stopContext();
		oni_destroy_ctx(*ctx);
		LOGINFO("...ONIX Context closed");
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
		for(auto& device : ONI::Global::model.getDevices()) device.second->reset();
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

	ONI::Processor::BufferProcessor* createBufferProcessor(){
		//std::map<ONI::Processor::TypeID, ONI::Processor::BaseProcessor*>& processors = ONI::Global::model.getProcessors();
		if(ONI::Global::model.bufferProcessor != nullptr){
			LOGINFO("Deleting old BufferProcessor");
			delete ONI::Global::model.bufferProcessor;
			ONI::Global::model.bufferProcessor = nullptr;
		}
		if(ONI::Global::model.bufferProcessor == nullptr){
			LOGINFO("Creating new BufferProcessor");
			ONI::Global::model.bufferProcessor = new ONI::Processor::BufferProcessor;
		}
		return ONI::Global::model.bufferProcessor;
	}

	ONI::Processor::ChannelMapProcessor* createChannelMapProcessor(){
		//ONI::Processor::ChannelMapProcessor * channelMapProcessor = ONI::Global::model.channelMapProcessor;
		if(ONI::Global::model.channelMapProcessor != nullptr){
			LOGINFO("Deleting old ChannelMapProcessor");
			delete ONI::Global::model.channelMapProcessor;
			ONI::Global::model.channelMapProcessor = nullptr;
		}
		if(ONI::Global::model.channelMapProcessor == nullptr){
			LOGINFO("Creating new ChannelMapProcessor");
			ONI::Global::model.channelMapProcessor = new ONI::Processor::ChannelMapProcessor;
			//ONI::Global::model.setChannelMapProcessor(channelMapProcessor);
		}
		return ONI::Global::model.channelMapProcessor;
	}

	ONI::Processor::SpikeProcessor* createSpikeProcessor(){
		//ONI::Processor::SpikeProcessor * spikeProcessor = ONI::Global::model.spikeProcessor;
		if(ONI::Global::model.spikeProcessor != nullptr){
			LOGINFO("Deleting old SpikeProcessor");
			delete ONI::Global::model.spikeProcessor;
			ONI::Global::model.spikeProcessor = nullptr;
		}
		if(ONI::Global::model.spikeProcessor == nullptr){
			LOGINFO("Creating new SpikeProcessor");
			ONI::Global::model.spikeProcessor = new ONI::Processor::SpikeProcessor;
		}
		return ONI::Global::model.spikeProcessor;
	}

	ONI::Processor::Rhs2116MultiProcessor* createRhs2116MultiProcessor(){
		//ONI::Processor::Rhs2116MultiProcessor * rhs2116MultiProcessor = ONI::Global::model.rhs2116MultiProcessor;
		if(ONI::Global::model.rhs2116MultiProcessor != nullptr){
			LOGINFO("Deleting old Rhs2116MultiProcessor");
			delete ONI::Global::model.rhs2116MultiProcessor;
			ONI::Global::model.rhs2116MultiProcessor = nullptr;
		}
		if(ONI::Global::model.rhs2116MultiProcessor == nullptr){
			LOGINFO("Creating new Rhs2116MultiProcessor");
			ONI::Global::model.rhs2116MultiProcessor = new ONI::Processor::Rhs2116MultiProcessor;
		}
		return ONI::Global::model.rhs2116MultiProcessor;
	}

	ONI::Processor::Rhs2116StimProcessor* createRhs2116StimProcessor(){
		ONI::Processor::Rhs2116StimProcessor * rhs2116StimProcessor = ONI::Global::model.rhs2116StimProcessor;
		if(ONI::Global::model.rhs2116StimProcessor != nullptr){
			LOGINFO("Deleting old Rhs2116StimProcessor");
			delete ONI::Global::model.rhs2116StimProcessor;
			ONI::Global::model.rhs2116StimProcessor = nullptr;
		}
		if(ONI::Global::model.rhs2116StimProcessor == nullptr){
			LOGINFO("Creating new Rhs2116StimProcessor");
			ONI::Global::model.rhs2116StimProcessor = new ONI::Processor::Rhs2116StimProcessor;
		}
		return ONI::Global::model.rhs2116StimProcessor;
	}

	ONI::Processor::BufferProcessor* getBufferProcessor(){
		assert(ONI::Global::model.getBufferProcessor() != nullptr, "User must create the BufferProcessor first!");
		return ONI::Global::model.getBufferProcessor();
	}

	ONI::Processor::ChannelMapProcessor* getChannelMapProcessor(){
		assert(ONI::Global::model.getChannelMapProcessor() != nullptr, "User must create the ChannelMapProcessor first!");
		return ONI::Global::model.getChannelMapProcessor();
	}

	ONI::Processor::SpikeProcessor* getSpikeProcessor(){
		assert(ONI::Global::model.getSpikeProcessor() != nullptr, "User must create the SpikeProcessor first!");
		return ONI::Global::model.getSpikeProcessor();
	}

	ONI::Processor::Rhs2116MultiProcessor* getRhs2116MultiProcessor(){
		assert(ONI::Global::model.getRhs2116MultiProcessor() != nullptr, "User must create the Rhs2116MultiProcessor first!");
		return  ONI::Global::model.getRhs2116MultiProcessor();
	}

	ONI::Processor::Rhs2116StimProcessor* getRhs2116StimProcessor(){
		assert(ONI::Global::model.getRhs2116StimProcessor() != nullptr, "User must create the Rhs2116StimProcessor first!");
		return ONI::Global::model.getRhs2116StimProcessor();
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

	void startFrameRead(){
		if(bThread) stopFrameRead();
		LOGDEBUG("Starting frame read thread");
		bThread = true;
		for(auto& device : ONI::Global::model.getDevices()) device.second->reset();
		ONI::Global::model.resetAcquireTimeStart();
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

		// create context
		volatile oni_ctx* ctx = ONI::Global::model.getOnixContext();
		*ctx = oni_create_ctx(driverName.c_str());

		if(ctx == nullptr){
			LOGERROR("ONI context create: FAIL with driver: %s", driverName.c_str());
			return false;
		}

		LOGINFO("ONI context create: OK with driver: %s", driverName.c_str());

		// Print the driver translator informaiton
		char driver_version[128];
		const oni_driver_info_t* di = oni_get_driver_info(*ctx);
		if(di->pre_release == NULL){
			LOGINFO("Driver version patch: %s v%d.%d.%d", di->name, di->major, di->minor, di->patch);
		}else{
			LOGINFO("Driver version pre-release: %s v%d.%d.%d-%s", di->name, di->major, di->minor, di->patch, di->pre_release);
		}

		// Initialize context and discover hardware
		rc = oni_init_ctx(*ctx, host_idx);
		if(rc){
			LOGERROR("Hardware init: FAIL ==> %s", oni_error_str(rc));
			return false;
		}else{
			LOGINFO("Hardware init: OK");
		}

		// there's a firmware bug which requires two context inits 
		// for PORTVOLTAGE to be set, so let's just always do it!
		oni_destroy_ctx(*ctx);
		*ctx = oni_create_ctx(driverName.c_str());
		rc = oni_init_ctx(*ctx, host_idx);

		if(rc){
			LOGERROR("Hardware re init: FAIL ==> %s", oni_error_str(rc));
			return false;
		}else{
			LOGINFO("Hardware re init: OK");
		}

		ONI::Global::model.maxReadSize = getContextOption(ONI::ContextOption::MAXREADFRAMESIZE);
		ONI::Global::model.maxWriteSize = getContextOption(ONI::ContextOption::MAXWRITEFRAMESIZE);
		ONI::Global::model.acq_clock_khz = getContextOption(ONI::ContextOption::ACQCLKHZ);
		ONI::Global::model.sys_clock_khz = getContextOption(ONI::ContextOption::SYSCLKHZ);

		if(ONI::Global::model.maxReadSize == -1) return false;
		if(ONI::Global::model.maxWriteSize == -1) return false;
		if(ONI::Global::model.acq_clock_khz == -1) return false;
		if(ONI::Global::model.sys_clock_khz == -1) return false;

		//blockReadBytes = setContextOption(BLOCKREADSIZE, blockReadSize);
		//blockWriteBytes = setContextOption(BLOCKWRITESIZE, blockWriteSize);

		//if(blockReadBytes == -1) return false;
		//if(blockWriteBytes == -1) return false;

		bIsContextSetup = startContext(); // is this ok? or should the user do it!?

		if(enumerateOnixDeviceTypes()){
			createOnixDevices();
			return true;
		}

		return false;

	}

	void createOnixDevices(){

		std::map<uint32_t, ONI::Device::BaseDevice*>& devices = ONI::Global::model.getDevices();

		//devices.clear(); // or should we leave them and check if they exist?

		for(auto& onixDeviceType : onixDeviceTypes){

			auto it = devices.find(onixDeviceType.idx);

			if(it == devices.end()){ // let's create an ofxONIX device that matches the oni_device_t

				LOGINFO("Checking ONIX device type: %s", onix_device_str(onixDeviceType.id));

				if(onixDeviceType.id == ONI::Processor::TypeID::FMC_DEVICE){ // 23 FMC Host Device
					LOGINFO("Mapping FMC Host Device: %i device idx: %i", onixDeviceType.id, onixDeviceType.idx);
					ONI::Device::FmcDevice* fmc = new ONI::Device::FmcDevice;
					fmc->setup(onixDeviceType);
					devices[onixDeviceType.idx] = fmc;
				}

				if(onixDeviceType.id == ONI::Processor::TypeID::HEARTBEAT_DEVICE){ // 12 HeartBeat Device
					LOGINFO("Mapping HeartBeat Device: %i device idx: %i", onixDeviceType.id, onixDeviceType.idx);
					ONI::Device::HeartBeatDevice* hbd = new ONI::Device::HeartBeatDevice;
					hbd->setup(onixDeviceType);
					devices[onixDeviceType.idx] = hbd;
				}

				if(onixDeviceType.id == ONI::Processor::TypeID::RHS2116_DEVICE){ // 31 RHS2116 Device
					LOGINFO("Mapping Rhs2116 Device: %i device idx: %i",onixDeviceType.id, onixDeviceType.idx);
					ONI::Device::Rhs2116Device* rhd2116 = new ONI::Device::Rhs2116Device;
					rhd2116->setup(onixDeviceType);
					devices[onixDeviceType.idx] = rhd2116;
				}

				if(onixDeviceType.id == ONI::Processor::TypeID::RHS2116STIM_DEVICE){ // 32 RHS2116 Stim Device
					LOGINFO("Mapping Rhs2116 Stim Device: %i device idx: %i", onixDeviceType.id, onixDeviceType.idx);
					ONI::Device::Rhs2116StimDevice* rhd2116Stim = new ONI::Device::Rhs2116StimDevice;
					rhd2116Stim->setup(onixDeviceType);
					devices[onixDeviceType.idx] = rhd2116Stim;
				}

			}else{
				LOGDEBUG("Re setting up onix device type: %s", onix_device_str(onixDeviceType.id));
				it->second->reset();
			}
		}

		// delete any ofxONIX devices if they are no longer in the onix_device_t tree
		for (auto it = devices.begin(); it != devices.end();){
			bool bDeleteDevice = true;
			for(auto& onixDeviceType : onixDeviceTypes){
				if(onixDeviceType.idx == it->first){
					bDeleteDevice = false;
					break;
				}
			}
			if (bDeleteDevice){
				LOGINFO("Deleting Device: %s %i", it->second->getName(), it->second->getOnixDeviceTableIDX());
				it = devices.erase(it);
			}else{
				it++;
			}
		}

	}

	bool enumerateOnixDeviceTypes(){

		//if (!bForceList && onixDeviceTypes.size() > 0) return onixDeviceTypes; // is this just dangerous and stupid?

		LOGDEBUG("Enumerating devices...");

		onixDeviceTypes.clear();

		if(!bIsContextSetup){
			LOGERROR("Context and driver not setup!");
			return false;
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
			return false;
		}

		volatile oni_ctx* ctx = ONI::Global::model.getOnixContext();

		oni_get_opt(*ctx, ONI_OPT_DEVICETABLE, devices_t, &devices_sz);

		for(size_t i = 0; i < num_devs; ++i){
			onixDeviceTypes.push_back(devices_t[i]);
		}

		free(devices_t);

		LOGINFO("Found %i Devices", onixDeviceTypes.size());

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

			std::map<uint32_t, ONI::Device::BaseDevice*>& devices = ONI::Global::model.getDevices();

			auto it = devices.find((uint32_t)frame->dev_idx);

			if(it == devices.end()){
				LOGERROR("ONI Device doesn't exist with idx: %i", frame->dev_idx);
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
			volatile oni_ctx* ctx = ONI::Global::model.getOnixContext();
			rc = oni_read_frame(*ctx, &frame);

			if(rc < 0){
				LOGERROR("Frame read error: %s", oni_error_str(rc));
			}else{

				std::map<uint32_t, ONI::Device::BaseDevice*>& devices = ONI::Global::model.getDevices();

				auto it = devices.find((uint32_t)frame->dev_idx);

				if(it == devices.end()){
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

	//void clearDevices(){

	//	onixDeviceTypes.clear();

	//	for(auto device : oniDevices){
	//		delete device.second;
	//	}

	//	oniDevices.clear();

	//	delete frameProcessor;
	//	frameProcessor = nullptr;

	//	delete spikeProcessor;
	//	spikeProcessor = nullptr;

	//	delete rhs2116StimDevice;
	//	rhs2116StimDevice = nullptr;

	//	delete rhs2116MultiDevice;
	//	rhs2116MultiDevice = nullptr;

	//}

private:

	//ONI::Processor::Rhs2116MultiProcessor * rhs2116MultiDevice = nullptr; // TODO: this better!!
	//ONI::Processor::Rhs2116StimProcessor * rhs2116StimDevice = nullptr;

	//ONI::Processor::BufferProcessor * frameProcessor = nullptr;
	//ONI::Processor::SpikeProcessor * spikeProcessor = nullptr;

	

	uint32_t blockReadBytes = 0;
	uint32_t blockWriteBytes = 0;

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

	//volatile oni_ctx ctx = NULL;

	//std::map<uint32_t, ONI::Device::BaseDevice*> oniDevices;
	std::vector<oni_device_t> onixDeviceTypes = {};

	int host_idx = -1;
	std::string driverName = "riffa";

	//uint32_t maxReadSize = -1;
	//uint32_t maxWriteSize = -1;
	//uint32_t acq_clock_khz = -1;
	//uint32_t sys_clock_khz = -1;


};



} // namespace ONI






