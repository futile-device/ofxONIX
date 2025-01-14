//
//  HeartBeatDevice.h
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


#include "../Device/BaseDevice.h"

#pragma once

namespace ONI{

namespace Interface{
class Rhs2116StimDeviceInterface;
} // namespace Interface

namespace Device{




class Rhs2116StimDevice : public ONI::Device::BaseDevice{

public:

	friend class ONI::Interface::Rhs2116StimDeviceInterface;

	//HeartBeatDevice(){};

	~Rhs2116StimDevice(){
		LOGDEBUG("Rhs2116Stim Device DTOR");
	};

	void reset(){
		numProbes = 0;
		getEnabled(true);
		getTriggerSource(true);
	}

	inline void process(oni_frame_t* frame){}
	inline void process(ONI::Frame::BaseFrame& frame){}

	bool setEnabled(const bool& b){
		bool bOk = writeRegister(ONI::Register::Rhs2116Stimulus::ENABLE, (unsigned int)b);
		if(bOk) getEnabled(true); // updates the cached value
		return bOk;
	}

	bool getEnabled(const bool& bCheckRegisters = true){
		if(bCheckRegisters) bEnabled = (bool)readRegister(ONI::Register::Rhs2116Stimulus::ENABLE);
		return bEnabled;
	}

	bool setTriggerSource(const bool& bTriggerSource){
		bool bOk = writeRegister(ONI::Register::Rhs2116Stimulus::TRIGGERSOURCE, uint32_t(!bTriggerSource));
		if(bOk) getTriggerSource(true);
		return bOk;
	}

	bool getTriggerSource(const bool& bCheckRegisters = true){
		if(bCheckRegisters) settings.bTriggerDevice = !(bool)readRegister(ONI::Register::Rhs2116Stimulus::TRIGGERSOURCE);
		return settings.bTriggerDevice;
	}

	inline bool triggerStimulus(){
		if(settings.bTriggerDevice) return writeRegister(ONI::Register::Rhs2116Stimulus::TRIGGER, 1);
		return false;
	}

	unsigned int readRegister(const ONI::Register::Rhs2116StimulusRegister& reg){
		return ONI::Device::BaseDevice::readRegister(reg);
	}

	bool writeRegister(const ONI::Register::Rhs2116StimulusRegister& reg, const unsigned int& value){
		return ONI::Device::BaseDevice::writeRegister(reg, value);
	}

protected:

	bool bEnabled = false;

	ONI::Settings::Rhs2116StimDeviceSettings settings;

};


} // namespace Device
} // namespace ONI