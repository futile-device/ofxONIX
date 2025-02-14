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
class HeartBeatInterface;
} // namespace Interface

namespace Device{




class HeartBeatDevice : public ONI::Device::BaseDevice{

public:

	friend class ONI::Interface::HeartBeatInterface;

	//HeartBeatDevice(){};

	~HeartBeatDevice(){
		LOGDEBUG("HeartBeat Device DTOR");
	};

	void reset(){
		getFrequencyHz(true);
	}

	inline void process(oni_frame_t* frame){
		//const std::lock_guard<std::mutex> lock(mutex);
		if(postProcessors.size() > 0){
			ONI::Frame::HeartBeatFrame processedFrame(frame, (uint64_t)ONI::Global::model.getAcquireDeltaTimeMicros(frame->time));
			process(processedFrame);
		}
		for(auto& it : preProcessors){
			it.second->process(frame);
		}
	}

	inline void process(ONI::Frame::BaseFrame& frame){
		//const std::lock_guard<std::mutex> lock(mutex);
		for(auto& it : postProcessors){
			it.second->process(frame);
		}
	}

	bool setEnabled(const bool& b){
		bool bOk = writeRegister(ONI::Register::HeartBeat::ENABLE, (unsigned int)b);
		if(bOk) getEnabled(true); // updates the cached value
		return bOk;
	}

	bool getEnabled(const bool& bCheckRegisters = true){
		if(bCheckRegisters) bEnabled = (bool)readRegister(ONI::Register::HeartBeat::ENABLE);
		return bEnabled;
	}

	bool setFrequencyHz(const unsigned int& beatsPerSecond){
		unsigned int clkHz = readRegister(ONI::Register::HeartBeat::CLK_HZ);
		unsigned int clkDv = clkHz / beatsPerSecond;
		bool bOk = writeRegister(ONI::Register::HeartBeat::CLK_DIV, clkDv);
		if(bOk) getFrequencyHz(true); 
		return bOk;
	}

	float getFrequencyHz(const bool& bCheckRegisters = true){
		if(bCheckRegisters){
			unsigned int clkHz = readRegister(ONI::Register::HeartBeat::CLK_HZ);
			unsigned int clkDv = readRegister(ONI::Register::HeartBeat::CLK_DIV);
			settings.frequencyHz = clkHz / clkDv;
			LOGDEBUG("Heartbeat FrequencyHz: %i", settings.frequencyHz);
		}
		return settings.frequencyHz;
	}

	unsigned int readRegister(const ONI::Register::HeartBeatRegister& reg){
		return ONI::Device::BaseDevice::readRegister(reg);
	}

	bool writeRegister(const ONI::Register::HeartBeatRegister& reg, const unsigned int& value){
		return ONI::Device::BaseDevice::writeRegister(reg, value);
	}

	std::string info(){
		return BaseDevice::getName() + "\n" + settings.info();
	}

protected:

	//HeartBeatDeviceConfig config;
	ONI::Settings::HeartBeatDeviceSettings settings;

	bool bEnabled = false;

};


} // namespace Device
} // namespace ONI