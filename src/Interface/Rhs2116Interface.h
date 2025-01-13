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

#include "../Interface/BaseInterface.h"

#include "ofxImGui.h"
#include "ofxImPlot.h"
#include "ofxFutilities.h"

#pragma once

namespace ONI{
namespace Interface{

class Rhs2116Interface : public ONI::Interface::BaseInterface{

public:

	~Rhs2116Interface(){};

	void reset(){};
	inline void process(oni_frame_t* frame){}; // nothing
	inline void process(ONI::Frame::BaseFrame& frame){}; // nothing

	inline void gui(ONI::Processor::BaseProcessor& processor){

		ONI::Device::Rhs2116Device& rhs = *reinterpret_cast<ONI::Device::Rhs2116Device*>(&processor);

		rhs.subscribeProcessor(processorName, ONI::Processor::FrameProcessorType::POST_PROCESSOR, this);

		nextSettings = rhs.settings;

		ImGui::PushID(rhs.getName().c_str());
		ImGui::Text(rhs.getName().c_str());

		deviceGui(rhs.settings);

		if(nextSettings.dspCutoff != rhs.settings.dspCutoff) rhs.setDspCutOff(nextSettings.dspCutoff);
		if(nextSettings.lowCutoff != rhs.settings.lowCutoff) rhs.setAnalogLowCutoff(nextSettings.lowCutoff);
		if(nextSettings.lowCutoffRecovery != rhs.settings.lowCutoffRecovery) rhs.setAnalogLowCutoffRecovery(nextSettings.lowCutoffRecovery);
		if(nextSettings.highCutoff != rhs.settings.highCutoff) rhs.setAnalogHighCutoff(nextSettings.highCutoff);

		ImGui::PopID();

	}

	inline void deviceGui(const ONI::Settings::Rhs2116DeviceSettings& settings){

		static char * dspCutoffOptions = "Differential\0Dsp3309Hz\0Dsp1374Hz\0Dsp638Hz\0Dsp308Hz\0Dsp152Hz\0Dsp75Hz\0Dsp37Hz\0Dsp19Hz\0Dsp9336mHz\0Dsp4665mHz\0Dsp2332mHz\0Dsp1166mHz\0Dsp583mHz\0Dsp291mHz\0Dsp146mHz\0Off";
		static char * lowCutoffOptions = "Low1000Hz\0Low500Hz\0Low300Hz\0Low250Hz\0Low200Hz\0Low150Hz\0Low100Hz\0Low75Hz\0Low50Hz\0Low30Hz\0Low25Hz\0Low20Hz\0Low15Hz\0Low10Hz\0Low7500mHz\0Low5000mHz\0Low3090mHz\0Low2500mHz\0Low2000mHz\0Low1500mHz\0Low1000mHz\0Low750mHz\0Low500mHz\0Low300mHz\0Low250mHz\0Low100mHz";
		static char * highCutoffOptions = "High20000Hz\0High15000Hz\0High10000Hz\0High7500Hz\0High5000Hz\0High3000Hz\0High2500Hz\0High2000Hz\0High1500Hz\0High1000Hz\0High750Hz\0High500Hz\0High300Hz\0High250Hz\0High200Hz\0High150Hz\0High100Hz";

		ImGui::SetNextItemWidth(200);
		int dspFormatItem = settings.dspCutoff; //format.dspEnable == 1 ? format.dspCutoff : Rhs2116DspCutoff::Off;
		ImGui::Combo("DSP Cutoff", &dspFormatItem, dspCutoffOptions, 5);


		ImGui::SetNextItemWidth(200);
		int lowCutoffItem = settings.lowCutoff; //getAnalogLowCutoff(false);
		ImGui::Combo("Analog Low Cutoff", &lowCutoffItem, lowCutoffOptions, 5);


		ImGui::SetNextItemWidth(200);
		int lowCutoffRecoveryItem = settings.lowCutoffRecovery; //getAnalogLowCutoffRecovery(false);
		ImGui::Combo("Analog Low Cutoff Recovery", &lowCutoffRecoveryItem, lowCutoffOptions, 5);

		ImGui::SetNextItemWidth(200);
		int highCutoffItem = settings.highCutoff; //getAnalogHighCutoff(false);
		ImGui::Combo("Analog High Cutoff", &highCutoffItem, highCutoffOptions, 5);

		nextSettings.dspCutoff = (ONI::Settings::Rhs2116DspCutoff)dspFormatItem;
		nextSettings.lowCutoff = (ONI::Settings::Rhs2116AnalogLowCutoff)lowCutoffItem;
		nextSettings.lowCutoffRecovery = (ONI::Settings::Rhs2116AnalogLowCutoff)lowCutoffRecoveryItem;
		nextSettings.highCutoff = (ONI::Settings::Rhs2116AnalogHighCutoff)highCutoffItem;

	}

	bool save(std::string presetName){
		return false;
	}

	bool load(std::string presetName){
		return false;
	}

protected:

	ONI::Settings::Rhs2116DeviceSettings nextSettings;

	const std::string processorName = "Rhs2116Gui";
};

} // namespace Interface
} // namespace ONI