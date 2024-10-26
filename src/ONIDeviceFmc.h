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

class FmcDevice : public ONIDevice{

public:

	//FmcDevice(){};

	~FmcDevice(){
		LOGDEBUG("FMC Device DTOR");
		//readRegister(ENABLE);
		//writeRegister(ENABLE, 0);
	};

	//void reset(){
	//	ONIDevice::reset();
	//	
	//}

	void deviceSetup(){
		config.setup(this);
		getPortVoltage(true);
		//setPortVoltage(config.getSettings().voltage);
		config.syncSettings();
	}

	inline void gui(){
		config.gui();
		if(config.applySettings()){
			LOGDEBUG("FMC Device settings changed");
			setPortVoltage(config.getChangedSettings().voltage);
			config.syncSettings();
		}
	}

	bool saveConfig(std::string presetName){
		//getPortVoltage(true);
		return config.save(presetName);
	}

	bool loadConfig(std::string presetName){
		bool bOk = config.load(presetName);
		if(bOk){
			setPortVoltage(config.getSettings().voltage);
			config.syncSettings();
		}
		return bOk;
	}

	inline void process(oni_frame_t* frame){
		//const std::lock_guard<std::mutex> lock(mutex);

	}

	inline void process(ONIFrame& frame){
		//for(auto it : processors){
		//	it.second->process(frame);
		//}
	}

	bool getLinkState(const bool& bCheckRegisters = true){
		if(bCheckRegisters) bLinkState = (bool)readRegister(FMC_REG::LINKSTATE); // HMMM returns 3 when rhs2116 connected?!!
		return bLinkState;
	}

	bool setEnabled(const bool& b){
		bool bOk = writeRegister(FMC_REG::ENABLE, (unsigned int)b);
		if(bOk) getEnabled(true); // updates the cached value
		return bOk;
	}

	bool getEnabled(const bool& bCheckRegisters = true){
		if(bCheckRegisters) bEnabled = (bool)readRegister(FMC_REG::ENABLE);
		return bEnabled;
	}

	bool setPortVoltage(const float& v){
		writeRegister(FMC_REG::PORTVOLTAGE, 0); // have to set to 0 and back to voltage or else won't init properly
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		//ofSleepMillis(500); // needs to pause
		bool bOk = writeRegister(FMC_REG::PORTVOLTAGE, v * 10);
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		//ofSleepMillis(500); // needs to pause
		if(bOk){ // updates the cached value
			getPortVoltage(true); 
			bContextNeedsRestart = true;
		}
		return bOk;
	}

	const float& getPortVoltage(const bool& bCheckRegisters = true){
		if(bCheckRegisters ||  config.getSettings().voltage == 0) config.getSettings().voltage = readRegister(FMC_REG::PORTVOLTAGE) / 10.0f;
		return config.getSettings().voltage;
	}

	unsigned int readRegister(const FmcRegister& reg){
		return ONIDevice::readRegister(reg);
	}

	bool writeRegister(const FmcRegister& reg, const unsigned int& value){
		return ONIDevice::writeRegister(reg, value);
	}

protected:

	FmcDeviceConfig config;

	bool bLinkState = false;
	bool bEnabled = false;

};
