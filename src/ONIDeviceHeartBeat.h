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

#include "ONIDevice.h"
#include "ONIDeviceConfig.h"
#include "ONIRegister.h"
#include "ONIUtility.h"

#pragma once

// later we can set which device config to use with specific #define for USE_IMGUI etc
typedef _HeartBeatDeviceConfig HeartBeatDeviceConfig;

class HeartBeatRegister : public ONIRegister{
public:
	HeartBeatRegister(unsigned int val, std::string name) { this->addr = val; this->name = name; this->devn = "HeartBeatDevice"; };
};

class HeartBeatDevice : public ONIDevice{

public:

	static inline const HeartBeatRegister ENABLE	= HeartBeatRegister(0, "ENABLE");	// Enable the heartbeat
	static inline const HeartBeatRegister CLK_DIV	= HeartBeatRegister(1, "CLK_DIV");	// Heartbeat clock divider ratio. Default results in 10 Hz heartbeat. Values less than CLK_HZ / 10e6 Hz will result in 1kHz.
	static inline const HeartBeatRegister CLK_HZ	= HeartBeatRegister(2, "CLK_HZ");	// The frequency parameter, CLK_HZ, used in the calculation of CLK_DIV

	//HeartBeatDevice(){};

	~HeartBeatDevice(){
		LOGDEBUG("HeartBeat Device DTOR");
		//readRegister(ENABLE); // FOR SOME REASON IF WE DISABLE WE HAVE A PROBLEM
		//writeRegister(ENABLE, 0);
	};

	void deviceSetup(){
		//config.setup(deviceName);
	}

	inline void gui(){
		config.gui();
		if(config.apply()){
			LOGDEBUG("HeartBeat Device settings changed");
			setFrequencyHz(config.last().frequencyHz);
			config.last() = config.get();
		}
	}

	bool saveConfig(std::string filePath){
		getFrequencyHz(true);
		return config.save(filePath);
	}

	bool loadConfig(std::string filePath){
		bool bOk = config.load(filePath);
		setFrequencyHz(config.get().frequencyHz);
		return bOk;
	}

	void reset(){
		ONIDevice::reset();
		getFrequencyHz(true);
	}

	inline void process(oni_frame_t* frame, const uint64_t & sampleIDX){
		//config.process(frame);
		config.bHeartBeat = !config.bHeartBeat;
		config.deltaTime = (uint64_t)ONIDevice::getAcqDeltaTimeMicros(frame->time);
		//LOGDEBUG("HEARTBEAT: %i %i %i %s", frame->dev_idx, (uint64_t)ONIDevice::getAcqDeltaTimeMicros(frame->time), frame->data_sz, deviceName.c_str());
		//fu::debug << (uint64_t)ONIDevice::getAcqDeltaTimeMicros(frame->time) << fu::endl;
	}

	bool setEnabled(const bool& b){
		bool bOk = writeRegister(HeartBeatDevice::ENABLE, (unsigned int)b);
		if(bOk) getEnabled(true); // updates the cached value
		return bOk;
	}

	bool getEnabled(const bool& bCheckRegisters = true){
		if(bCheckRegisters) config.bEnabled = (bool)readRegister(HeartBeatDevice::ENABLE);
		return config.bEnabled;
	}

	bool setFrequencyHz(const unsigned int& beatsPerSecond){
		unsigned int clkHz = readRegister(HeartBeatDevice::CLK_HZ);
		unsigned int clkDv = clkHz / beatsPerSecond;
		bool bOk = writeRegister(HeartBeatDevice::CLK_DIV, clkDv);
		if(bOk) getFrequencyHz(true); 
		return bOk;
	}

	float getFrequencyHz(const bool& bCheckRegisters = true){
		if(bCheckRegisters){
			unsigned int clkHz = readRegister(HeartBeatDevice::CLK_HZ);
			unsigned int clkDv = readRegister(HeartBeatDevice::CLK_DIV);
			config.get().frequencyHz = clkHz / clkDv;
			LOGDEBUG("Heartbeat FrequencyHz: %i", config.get().frequencyHz);
		}
		return config.get().frequencyHz;
	}

	unsigned int readRegister(const HeartBeatRegister& reg){
		return ONIDevice::readRegister(reg);
	}

	bool writeRegister(const HeartBeatRegister& reg, const unsigned int& value){
		return ONIDevice::writeRegister(reg, value);
	}

protected:

	HeartBeatDeviceConfig config;
	//unsigned int frequencyHz = 0;
	//bool bEnabled = false;
	//std::atomic_bool bHeartBeat = false;
	//std::atomic_uint64_t deltaTime = 0;

};

