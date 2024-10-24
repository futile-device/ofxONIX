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
typedef _FmcDeviceConfig FmcDeviceConfig;

class FmcRegister : public ONIRegister{
public:
	FmcRegister(unsigned int val, std::string name) { this->addr = val; this->name = name; this->devn = "FMCDevice"; };
};

class FmcDevice : public ONIDevice{

public:

	static inline const FmcRegister ENABLE		= FmcRegister(0, "ENABLE");			// The LSB is used to enable or disable the device data stream
	static inline const FmcRegister GPOSTATE	= FmcRegister(1, "GPOSTATE");		// GPO output state (bits 31 downto 3: ignore. bits 2 downto 0: ‘1’ = high, ‘0’ = low)
	static inline const FmcRegister DESPWR		= FmcRegister(2, "DESPWR");			// Set link deserializer PDB state, 0 = deserializer power off else on. Does not affect port voltage.
	static inline const FmcRegister PORTVOLTAGE = FmcRegister(3, "PORTVOLTAGE");	// 10 * link voltage
	static inline const FmcRegister SAVEVOLTAGE = FmcRegister(4, "SAVEVOLTAGE");	// Save link voltage to non-volatile EEPROM if greater than 0. This voltage will be applied after POR.
	static inline const FmcRegister LINKSTATE	= FmcRegister(5, "LINKSTATE");		// bit 1 pass; bit 0 lock

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
		config.setup(deviceName);
		getPortVoltage(true);
		config.last() = config.get();
	}

	inline void gui(){
		config.gui();
		if(config.apply()){
			LOGDEBUG("FMC Device settings changed");
			setPortVoltage(config.last().voltage);
			config.last() = config.get();
		}
	}

	bool saveConfig(std::string filePath){
		getPortVoltage(true);
		return config.save(filePath);
	}

	bool loadConfig(std::string filePath){
		bool bOk = config.load(filePath);
		setPortVoltage(config.get().voltage);
		return bOk;
	}

	inline void process(oni_frame_t* frame, const uint64_t & sampleIDX){
		// nothing
	}

	bool getLinkState(const bool& bCheckRegisters = true){
		if(bCheckRegisters) config.bLinkState = (bool)readRegister(FmcDevice::LINKSTATE); // HMMM returns 3 when rhs2116 connected?!!
		return config.bLinkState;
	}

	bool setEnabled(const bool& b){
		bool bOk = writeRegister(FmcDevice::ENABLE, (unsigned int)b);
		if(bOk) getEnabled(true); // updates the cached value
		return bOk;
	}

	bool getEnabled(const bool& bCheckRegisters = true){
		if(bCheckRegisters) config.bEnabled = (bool)readRegister(FmcDevice::ENABLE);
		return config.bEnabled;
	}

	bool setPortVoltage(const float& v){
		writeRegister(FmcDevice::PORTVOLTAGE, 0); // have to set to 0 and back to voltage or else won't init properly
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		//ofSleepMillis(500); // needs to pause
		bool bOk = writeRegister(FmcDevice::PORTVOLTAGE, v * 10);
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		//ofSleepMillis(500); // needs to pause
		if(bOk){ // updates the cached value
			getPortVoltage(true); 
			bContextNeedsRestart = true;
		}
		return bOk;
	}

	const float& getPortVoltage(const bool& bCheckRegisters = true){
		if(bCheckRegisters ||  config.get().voltage == 0) config.get().voltage = readRegister(FmcDevice::PORTVOLTAGE) / 10.0f;
		return config.get().voltage;
	}

	unsigned int readRegister(const FmcRegister& reg){
		return ONIDevice::readRegister(reg);
	}

	bool writeRegister(const FmcRegister& reg, const unsigned int& value){
		return ONIDevice::writeRegister(reg, value);
	}

protected:

	FmcDeviceConfig config;

	//float voltage = 0;
	//float gvoltage = 0; // voltage for gui
	//bool bLinkState = false;
	//bool bEnabled = false;

};
