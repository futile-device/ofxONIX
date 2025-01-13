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

#pragma once

namespace ONI{
namespace Device{

class BaseDevice : public ONI::Processor::BaseProcessor {

public:

	//BaseDevice(){};
	virtual ~BaseDevice(){};

	void setup(oni_device_t type){
		LOGDEBUG("Setting up device: %s", onix_device_str(type.id));
		onixDeviceType = type;
		BaseProcessor::processorTypeID = (ONI::Processor::TypeID)type.id;
		std::ostringstream os; os << ONI::Processor::toString(processorTypeID) << " (" << type.idx << ")";
		BaseProcessor::processorName = os.str();
		reset(); // device specific defaults and setup
	}

	inline const unsigned int& getOnixDeviceTableIDX(){
		return onixDeviceType.idx;
	}

	inline bool getContexteNeedsRestart(){ //  for now let's auto reset these flags bc only the context checks once every update
		if(bContextNeedsRestart){
			bContextNeedsRestart = false;
			return true;
		}
		return false;
	}

	inline bool getContexteNeedsReset(){
		if(bContextNeedsReset){
			bContextNeedsReset = false;
			return true;
		}
		return false;
	}

	//inline long double getAcqDeltaTimeMicros(const uint64_t& t){
	//	if(firstFrameTime == -1) firstFrameTime = t;
	//	return (t - firstFrameTime) / (long double)acq_clock_khz * 1000000; // 250000000
	//}

protected:

	unsigned int readRegister(const ONI::Register::BaseRegister& reg){

		volatile oni_ctx* ctx = ONI::Global::model.getOnixContext();

		assert(ctx != nullptr && processorTypeID != ONI::Processor::TypeID::UNDEFINED, "ONIContext and ONIDevice must be setup");

		int rc = ONI_ESUCCESS;

		unsigned int value = 0;
		rc = oni_read_reg(*ctx, onixDeviceType.idx, reg.getAddress(), &value);
		if(rc){
			LOGERROR("Could not read read register: %i %s", reg, oni_error_str(rc));
			//assert(false); //???
			return 0;
		}

		LOGDEBUG("%s  read register: %s == dec(%05i) bin(%016llX) hex(%#06x)", BaseProcessor::processorName.c_str(), reg.getName().c_str(), value, uint16_to_bin16(value), value);

		return value;

	}

	bool writeRegister(const ONI::Register::BaseRegister& reg, const unsigned int& value, const bool& bSetWithoutCheck = false){

		volatile oni_ctx* ctx = ONI::Global::model.getOnixContext();

		assert(ctx != nullptr && processorTypeID != ONI::Processor::TypeID::UNDEFINED, "ONIContext and ONIDevice must be setup");
		LOGDEBUG("%s write register: %s == dec(%05i) bin(%016llX) hex(%#06x)", BaseProcessor::processorName.c_str(), reg.getName().c_str(), value, uint16_to_bin16(value), value);

		int rc = ONI_ESUCCESS;

		rc = oni_write_reg(*ctx, onixDeviceType.idx, reg.getAddress(), value);

		if(rc){
			LOGERROR("Could not write to register: %s", oni_error_str(rc));
			return false;
		}

		if(!bSetWithoutCheck &&
		   reg.getAddress() != ONI::Register::Fmc::SAVEVOLTAGE &&
		   reg.getAddress() != ONI::Register::Rhs2116::DELTAIDXTIME &&
		   reg.getAddress() != ONI::Register::Rhs2116::DELTAPOLEN &&
		   reg.getAddress() != ONI::Register::Rhs2116::TRIGGER &&
		   reg.getAddress() != ONI::Register::Rhs2116::RESPECTSTIMACTIVE &&
		   reg.getAddress() != ONI::Register::Rhs2116Stimulus::TRIGGER){			/// READ ONLY REGISTERS!!
			// double check it set correctly
			unsigned int t_value = 0;
			rc = oni_read_reg(*ctx, onixDeviceType.idx, reg.getAddress(), &t_value);
			if(rc || value != t_value){
				LOGERROR("Write does not match read values: %s", oni_error_str(rc));
				LOGERROR("	Attempt: %s == dec(%05i) bin(%016llX) hex(%#06x)", reg.getName().c_str(), value, uint16_to_bin16(value), value);
				LOGERROR("	Actual : %s == dec(%05i) bin(%016llX) hex(%#06x)", reg.getName().c_str(), t_value, uint16_to_bin16(t_value), t_value);
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

	//volatile oni_ctx* ctx = NULL;
	oni_device_t onixDeviceType;

	

	//long double firstFrameTime = -1;
	//uint64_t acq_clock_khz = -1; // 250000000



};

//template<typename FrameDataType>
//class ProbeDevice : public BaseDevice{
//
//public:
//
//	inline const size_t& getNumProbes(){
//		return numProbes;
//	}
//
//	inline const long double& getSampleFrequencyHz(){
//		return sampleFrequencyHz;
//	}
//
//protected:
//
//	size_t numProbes = 16;
//	long double sampleFrequencyHz = 30.1932367151e3;
//
//};

} // namespace Device
} // namespace ONI










