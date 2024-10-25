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

#include "ONIConfig.h"
#include "ONIDevice.h"
#include "ONIRegister.h"
#include "ONISettingTypes.h"
#include "ONIUtility.h"

#pragma once


class HeartBeatDevice : public ONIDevice{

public:

	//HeartBeatDevice(){};

	~HeartBeatDevice(){
		LOGDEBUG("HeartBeat Device DTOR");
		//readRegister(ENABLE); // FOR SOME REASON IF WE DISABLE WE HAVE A PROBLEM
		//writeRegister(ENABLE, 0);
	};

	void deviceSetup(){
		config.setup(this);
		getFrequencyHz(true);
		//setFrequencyHz(config.getSettings().frequencyHz);
		config.syncSettings();
	}

	inline void gui(){
		config.gui();
		if(config.applySettings()){
			LOGDEBUG("HeartBeat Device settings changed");
			setFrequencyHz(config.getChangedSettings().frequencyHz);
			config.syncSettings();
		}
	}

	bool saveConfig(std::string presetName){
		//getFrequencyHz(true);
		return config.save(presetName);
	}

	bool loadConfig(std::string presetName){
		bool bOk = config.load(presetName);
		if(bOk){
			setFrequencyHz(config.getSettings().frequencyHz);
			config.syncSettings();
		}
		return bOk;
	}

	void reset(){
		ONIDevice::reset();
		getFrequencyHz(true);
	}

	inline void process(oni_frame_t* frame){
		const std::lock_guard<std::mutex> lock(mutex);
		for(auto it : processors){
			it.second->process(frame);
		}
		//config.process(frame);
		//config.bHeartBeat = !config.bHeartBeat;
		//config.deltaTime = (uint64_t)ONIDevice::getAcqDeltaTimeMicros(frame->time);
		//LOGDEBUG("HEARTBEAT: %i %i %i %s", frame->dev_idx, (uint64_t)ONIDevice::getAcqDeltaTimeMicros(frame->time), frame->data_sz, deviceName.c_str());
		//fu::debug << (uint64_t)ONIDevice::getAcqDeltaTimeMicros(frame->time) << fu::endl;
	}

	bool setEnabled(const bool& b){
		bool bOk = writeRegister(HEARTBEAT_REG::ENABLE, (unsigned int)b);
		if(bOk) getEnabled(true); // updates the cached value
		return bOk;
	}

	bool getEnabled(const bool& bCheckRegisters = true){
		if(bCheckRegisters) bEnabled = (bool)readRegister(HEARTBEAT_REG::ENABLE);
		return bEnabled;
	}

	bool setFrequencyHz(const unsigned int& beatsPerSecond){
		unsigned int clkHz = readRegister(HEARTBEAT_REG::CLK_HZ);
		unsigned int clkDv = clkHz / beatsPerSecond;
		bool bOk = writeRegister(HEARTBEAT_REG::CLK_DIV, clkDv);
		if(bOk) getFrequencyHz(true); 
		return bOk;
	}

	float getFrequencyHz(const bool& bCheckRegisters = true){
		if(bCheckRegisters){
			unsigned int clkHz = readRegister(HEARTBEAT_REG::CLK_HZ);
			unsigned int clkDv = readRegister(HEARTBEAT_REG::CLK_DIV);
			config.getSettings().frequencyHz = clkHz / clkDv;
			LOGDEBUG("Heartbeat FrequencyHz: %i", config.getSettings().frequencyHz);
		}
		return config.getSettings().frequencyHz;
	}

	unsigned int readRegister(const HeartBeatRegister& reg){
		return ONIDevice::readRegister(reg);
	}

	bool writeRegister(const HeartBeatRegister& reg, const unsigned int& value){
		return ONIDevice::writeRegister(reg, value);
	}

protected:

	HeartBeatDeviceConfig config;

	bool bEnabled = false;

};

