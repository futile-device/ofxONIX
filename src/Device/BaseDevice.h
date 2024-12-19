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

#pragma once

namespace ONI{
namespace Device{




class FrameProcessor{

public:

	virtual ~FrameProcessor(){};

	virtual inline void process(oni_frame_t* frame) = 0;
	virtual inline void process(ONI::Frame::BaseFrame& frame) = 0;

	inline void subscribeProcessor(const std::string& processorName, const FrameProcessorType& type, FrameProcessor * processor){
		const std::lock_guard<std::mutex> lock(mutex);
		std::map<std::string, FrameProcessor*>& processors = (type == PRE_FRAME_PROCESSOR ? preFrameProcessors : postFrameProcessors);
		auto it = processors.find(processorName);
		if(it == processors.end()){
			LOGINFO("Adding processor %s", processorName.c_str());
			processors[processorName] = processor;
		}else{
			//LOGALERT("Processor %s already exists", processorName.c_str());
		}
	}

	inline void unsubscribeProcessor(const std::string& processorName, const FrameProcessorType& type, FrameProcessor * processor){
		const std::lock_guard<std::mutex> lock(mutex);
		std::map<std::string, FrameProcessor*>& processors = (type == PRE_FRAME_PROCESSOR ? preFrameProcessors : postFrameProcessors);
		auto it = processors.find(processorName);
		if(it == processors.end()){
			LOGALERT("No processor %s", processorName.c_str());
		}else{
			LOGINFO("Deleting processor %s", processorName.c_str());
			processors.erase(it);
		}
	}

	//std::mutex& getMutex(){
	//	return mutex;
	//}

protected:

	std::mutex mutex;

	std::map<std::string, FrameProcessor*> preFrameProcessors;
	std::map<std::string, FrameProcessor*> postFrameProcessors;

};

class BaseDevice : public FrameProcessor {

public:

	//BaseDevice(){};
	virtual ~BaseDevice(){};

	void setup(volatile oni_ctx* context, oni_device_t type, uint64_t acq_clock_khz){
		LOGDEBUG("Setting up device: %s", onix_device_str(type.id));
		ctx = context;
		deviceType = type;
		deviceTypeID = (ONI::Device::TypeID)type.id;
		std::ostringstream os; os << ONI::Device::toString(deviceTypeID) << " (" << type.idx << ")";
		deviceName = os.str();
		this->acq_clock_khz = acq_clock_khz;
		reset(); // always call device specific setup/reset
		deviceSetup(); // always call device specific setup/reset
	}

	virtual void reset(){ // derived class should always call base clase reset()
		firstFrameTime = -1;
		bContextNeedsRestart = false;
		bContextNeedsReset = false;
	}

	virtual void deviceSetup() = 0; // this should always set the config name and do whatever else is device specific for setup/reset

	inline const std::string& getName(){
		return deviceName;
	}

	inline const ONI::Device::TypeID& getDeviceTypeID(){
		return deviceTypeID;
	}

	inline const unsigned int& getDeviceTableID(){
		return deviceType.idx;
	}

	//virtual inline void gui() = 0;
	//virtual bool saveConfig(std::string presetName) = 0;
	//virtual bool loadConfig(std::string presetName) = 0;

	const inline bool& getContexteNeedsRestart(){
		return bContextNeedsRestart;
	}

	const inline bool& getContexteNeedsReset(){
		return bContextNeedsReset;
	}

	inline long double getAcqDeltaTimeMicros(const uint64_t& t){
		if(firstFrameTime == -1) firstFrameTime = t;
		return (t - firstFrameTime) / (long double)acq_clock_khz * 1000000; // 250000000
	}

protected:

	unsigned int readRegister(const ONI::Register::BaseRegister& reg){

		assert(ctx != NULL && deviceTypeID != -1, "ONIContext and ONIDevice must be setup");

		int rc = ONI_ESUCCESS;

		unsigned int value = 0;
		rc = oni_read_reg(*ctx, deviceType.idx, reg.getAddress(), &value);
		if(rc){
			LOGERROR("Could not read read register: %i %s", reg, oni_error_str(rc));
			//assert(false); //???
			return 0;
		}

		LOGDEBUG("%s  read register: %s == dec(%05i) bin(%016llX) hex(%#06x)", deviceName.c_str(), reg.getName().c_str(), value, uint16_to_bin16(value), value);

		return value;

	}

	bool writeRegister(const ONI::Register::BaseRegister& reg, const unsigned int& value){

		assert(ctx != NULL && deviceTypeID != -1, "ONIContext and ONIDevice must be setup");
		LOGDEBUG("%s write register: %s == dec(%05i) bin(%016llX) hex(%#06x)", deviceName.c_str(), reg.getName().c_str(), value, uint16_to_bin16(value), value);

		int rc = ONI_ESUCCESS;

		rc = oni_write_reg(*ctx, deviceType.idx, reg.getAddress(), value);

		if(rc){
			LOGERROR("Could not write to register: %s", oni_error_str(rc));
			return false;
		}

		if(reg.getAddress() != ONI::Register::Fmc::SAVEVOLTAGE &&
		   reg.getAddress() != ONI::Register::Rhs2116::DELTAIDXTIME &&
		   reg.getAddress() != ONI::Register::Rhs2116::DELTAPOLEN &&
		   reg.getAddress() != ONI::Register::Rhs2116::TRIGGER &&
		   reg.getAddress() != ONI::Register::Rhs2116::RESPECTSTIMACTIVE &&
		   reg.getAddress() != ONI::Register::Rhs2116::TRIGGER){			/// READ ONLY REGISTERS!!
			// double check it set correctly
			unsigned int t_value = 0;
			rc = oni_read_reg(*ctx, deviceType.idx, reg.getAddress(), &t_value);
			if(rc || value != t_value){
				LOGERROR("Write does not match read values: %s", oni_error_str(rc));
				return false;
			}
		}

		bContextNeedsReset = true;
		return true;
	}

	//friend class ONIDeviceConfig<FmcDeviceSettings>; // pre declare firend for direct member access
	//friend class ONIDeviceConfig<HeartBeatDeviceSettings>; // pre declare firend for direct member access

	bool bContextNeedsRestart = false;
	bool bContextNeedsReset = false;

	volatile oni_ctx* ctx = NULL;
	oni_device_t deviceType;

	ONI::Device::TypeID deviceTypeID = ONI::Device::TypeID::NONE;

	std::string deviceName = "UNDEFINED";

	long double firstFrameTime = -1;
	uint64_t acq_clock_khz = -1; // 250000000



};

//template<typename FrameDataType>
class ProbeDevice : public BaseDevice{

public:

	inline const size_t& getNumProbes(){
		return numProbes;
	}

	inline const long double& getSampleFrequencyHz(){
		return sampleFrequencyHz;
	}

protected:

	size_t numProbes = 16;
	long double sampleFrequencyHz = 30.1932367151e3;

};

} // namespace Device
} // namespace ONI










