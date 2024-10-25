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
#include "ONISettingTypes.h"
#include "ONIUtility.h"

#pragma once


template<typename ONIDeviceTypeSettings>
class ONIConfig{

public:

	virtual ~ONIConfig(){};

	virtual inline void gui() = 0;

	virtual bool save(std::string presetName) = 0;
	virtual bool load(std::string presetName) = 0;

	//std::string getName(){ return deviceName; };

	inline bool applySettings(const bool& bAutoReset = true){
		if(bApplySettings && bAutoReset){
			bApplySettings = false;
			return true;
		}
		return false;
	}

	virtual void set(const ONIDeviceTypeSettings& settings){
		this->currentSettings = this->changedSettings = settings;
	}

	ONIDeviceTypeSettings& getSettings(){
		return currentSettings;
	}

	ONIDeviceTypeSettings& getChangedSettings(){
		return changedSettings;
	}

	inline void syncSettings(){
		changedSettings = currentSettings;
	}

protected:

	ONIDeviceTypeSettings currentSettings;
	ONIDeviceTypeSettings changedSettings;

	bool bApplySettings = false;

};

template<typename ONIDeviceTypeSettings>
class ONIDeviceConfig : public ONIConfig<ONIDeviceTypeSettings>, public ONIFrameProcessor{

public:

	virtual ~ONIDeviceConfig(){};

	virtual void setup(ONIDevice* device){ 
		this->device = device;
		deviceConfigSetup();
		syncSettings();
	};

	virtual void deviceConfigSetup() = 0; // always called during the setup

	//virtual inline void process(oni_frame_t* frame) = 0; // in ONIFrameProcessor

protected:

	ONIDevice * device;

};

template<typename ONIDeviceTypeSettings>
class ONIContextConfig : public ONIConfig<ONIDeviceTypeSettings>{

public:

	virtual ~ONIContextConfig(){};

	//virtual void setup(std::vector<ONIDevice*>& devices){ this->devices = devices; };

	std::map<unsigned int, ONIDevice*>& devices(){ return oniDevices; }

protected:

	std::map<unsigned int, ONIDevice*> oniDevices;

};


#include "ONIConfigImGui.h"

// later we can set which device config to use with specific #define for USE_IMGUI etc
typedef _ContextConfig ContextConfig;
typedef _FmcDeviceConfig FmcDeviceConfig;
typedef _HeartBeatDeviceConfig HeartBeatDeviceConfig;