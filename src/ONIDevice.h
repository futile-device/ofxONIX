//
//  ONIDevice.h
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

#include "ONIDataBuffer.h"
#include "ONIRegister.h"
#include "ONISettingTypes.h"
#include "ONIFrameTypes.h"
#include "ONIUtility.h"

#pragma once

//class ONIContext; // pre-declare for friend class?

enum ONIDeviceTypeID{
	HEARTBEAT		= 12,
	FMC				= 23,
	RHS2116			= 31,
	RHS2116MULTI	= 666,
	NONE		= 2048
};

static std::string ONIDeviceTypeIDStr(const ONIDeviceTypeID& typeID){
	switch(typeID){
	case HEARTBEAT: {return "HeartBeat Device"; break;}
	case FMC: {return "FMC Device"; break;}
	case RHS2116: {return "RHS2116 Device"; break;}
	case RHS2116MULTI: {return "RHS2116MULTI Device"; break;}
	case NONE: {return "No Device"; break;}
	}
};


class ONIFrameProcessor{

public:

	virtual ~ONIFrameProcessor(){};

	virtual inline void process(ONIFrame& frame) = 0;

	void subscribeProcessor(const std::string& processorName, ONIFrameProcessor * processor){
		const std::lock_guard<std::mutex> lock(mutex);
		auto it = processors.find(processorName);
		if(it == processors.end()){
			LOGINFO("Adding processor %s", processorName.c_str());
			processors[processorName] = processor;
		}else{
			LOGALERT("Processor %s already exists", processorName.c_str());
		}
	}

	void unsubscribeProcessor(const std::string& processorName, ONIFrameProcessor * processor){
		const std::lock_guard<std::mutex> lock(mutex);
		auto it = processors.find(processorName);
		if(it == processors.end()){
			LOGALERT("No processor %s", processorName.c_str());
		}else{
			LOGINFO("Deleting processor %s", processorName.c_str());
			processors.erase(it);
		}
	}

	std::mutex& getMutex(){
		return mutex;
	}

protected:

	std::mutex mutex;

	std::map<std::string, ONIFrameProcessor*> processors;

};


class ONIDevice : public ONIFrameProcessor {

public:

	//ONIDevice(){};
	virtual ~ONIDevice(){};

	virtual void setup(volatile oni_ctx* context, oni_device_t type, uint64_t acq_clock_khz){
		LOGDEBUG("Setting up device: %s", onix_device_str(type.id));
		ctx = context;
		deviceType = type;
		deviceTypeID = (ONIDeviceTypeID)type.id;
		std::ostringstream os; os << ONIDeviceTypeIDStr(deviceTypeID) << " (" << type.idx << ")";
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

	inline const ONIDeviceTypeID& getDeviceTypeID(){
		return deviceTypeID;
	}

	inline const unsigned int& getDeviceTableID(){
		return deviceType.idx;
	}

	virtual inline void gui() = 0;
	virtual bool saveConfig(std::string presetName) = 0;
	virtual bool loadConfig(std::string presetName) = 0;

	virtual inline void process(oni_frame_t* frame) = 0;

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

	unsigned int readRegister(const ONIRegister& reg){

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

	bool writeRegister(const ONIRegister& reg, const unsigned int& value){

		assert(ctx != NULL && deviceTypeID != -1, "ONIContext and ONIDevice must be setup");
		LOGDEBUG("%s write register: %s == dec(%05i) bin(%016llX) hex(%#06x)", deviceName.c_str(), reg.getName().c_str(), value, uint16_to_bin16(value), value);

		int rc = ONI_ESUCCESS;

		rc = oni_write_reg(*ctx, deviceType.idx, reg.getAddress(), value);
		
		if(rc){
			LOGERROR("Could not write to register: %s", oni_error_str(rc));
			return false;
		}

		// double check it set correctly
		unsigned int t_value = 0;
		rc = oni_read_reg(*ctx, deviceType.idx, reg.getAddress(), &t_value);
		if(rc || value != t_value){
			LOGERROR("Write does not match read values");
			return false;
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

	ONIDeviceTypeID deviceTypeID = ONIDeviceTypeID::NONE;

	std::string deviceName = "UNDEFINED";

	long double firstFrameTime = -1;
	uint64_t acq_clock_khz = -1; // 250000000

	

};

//template<typename FrameDataType>
class ONIProbeDevice : public ONIDevice{

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