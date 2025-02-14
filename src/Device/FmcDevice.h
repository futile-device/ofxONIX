//
//  FmcDevice.h
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
class FmcInterface;
} // namespace Interface

namespace Device{




class FmcDevice : public ONI::Device::BaseDevice{

public:

	friend class ONI::Interface::FmcInterface;

	//FmcDevice(){};

	~FmcDevice(){
		LOGDEBUG("FMC Device DTOR");
	};


	void reset(){
		getPortVoltage(true);
	}


	inline void process(oni_frame_t* frame){}; // nothing
	inline void process(ONI::Frame::BaseFrame& frame){}; // nothing

	bool getLinkState(const bool& bCheckRegisters = true){
		if(bCheckRegisters) bLinkState = (bool)readRegister(ONI::Register::Fmc::LINKSTATE); // HMMM returns 3 when rhs2116 connected?!!
		return bLinkState;
	}

	bool setEnabled(const bool& b){
		bool bOk = writeRegister(ONI::Register::Fmc::ENABLE, (unsigned int)b);
		if(bOk) getEnabled(true); // updates the cached value
		return bOk;
	}

	bool getEnabled(const bool& bCheckRegisters = true){
		if(bCheckRegisters) bEnabled = (bool)readRegister(ONI::Register::Fmc::ENABLE);
		return bEnabled;
	}

	bool setPortVoltage(const float& v){
		writeRegister(ONI::Register::Fmc::PORTVOLTAGE, 0); // have to set to 0 and back to voltage or else won't init properly
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		bool bOk = writeRegister(ONI::Register::Fmc::PORTVOLTAGE, v * 10);
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		if(bOk){ // updates the cached value
			getPortVoltage(true); 
			bContextNeedsRestart = true;
		}
		return bOk;
	}

	const float& getPortVoltage(const bool& bCheckRegisters = true){
		if(bCheckRegisters ||  settings.voltage == 0) settings.voltage = readRegister(ONI::Register::Fmc::PORTVOLTAGE) / 10.0f;
		return settings.voltage;
	}

	unsigned int readRegister(const ONI::Register::FmcRegister& reg){
		return ONI::Device::BaseDevice::readRegister(reg);
	}

	bool writeRegister(const ONI::Register::FmcRegister& reg, const unsigned int& value){
		return ONI::Device::BaseDevice::writeRegister(reg, value);
	}

	std::string info(){
		return BaseDevice::getName() + "\n" + settings.info();
	}

protected:

	//FmcDeviceConfig config;
	ONI::Settings::FmcDeviceSettings settings;

	bool bLinkState = false;
	bool bEnabled = false;

};

} // namespace Device
} // namespace ONI


