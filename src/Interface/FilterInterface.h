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




class FilterInterface : public ONI::Interface::BaseInterface{

public:

	~FilterInterface(){};

	void reset(){};
	inline void process(oni_frame_t* frame){}; // nothing
	inline void process(ONI::Frame::BaseFrame& frame){}; // nothing
	
	inline void gui(ONI::Processor::BaseProcessor& processor){
	
		ONI::Processor::FilterProcessor& fp = *reinterpret_cast<ONI::Processor::FilterProcessor*>(&processor);

		BaseProcessor::numProbes = fp.numProbes;

		ImGui::PushID(fp.getName().c_str());
		ImGui::Text(fp.getName().c_str());
		
		///////////////////////////////////////////////
		/// BAND STOP FILTER
		///////////////////////////////////////////////

		ImGui::Checkbox("Use Band Stop", &fp.settings.bUseBandStopFilter);

		int bandStopFrequency = fp.settings.bandStopFrequency;
		int bandStopWidth = fp.settings.bandStopWidth;

		ImGui::InputInt("Band Frequency", &bandStopFrequency);
		ImGui::InputInt("Width", &bandStopWidth);

		if(bandStopFrequency < 1) bandStopFrequency = 1;
		if(bandStopWidth < 1) bandStopWidth = 1;

		if(fp.settings.lowBandPassFrequency != bandStopFrequency || fp.settings.highBandPassFrequency != bandStopWidth){
			fp.setBandStop(bandStopFrequency, bandStopWidth);
		}

		
		///////////////////////////////////////////////
		/// LOW SHELFTER
		///////////////////////////////////////////////

		ImGui::PushID("LowShelf");

		ImGui::Checkbox("Use Low Shelf", &fp.settings.bUseLowShelf);

		int lowShelfFrequency = fp.settings.lowShelfFrequency;
		float lowShelfGain = fp.settings.lowShelfGain;
		float lowShelfRipple = fp.settings.lowShelfRipple;

		ImGui::InputInt("Frequency", &lowShelfFrequency);
		ImGui::InputFloat("Gain", &lowShelfGain);
		ImGui::InputFloat("Ripple", &lowShelfRipple);

		if(lowShelfFrequency < 1) lowShelfFrequency = 1;
		if(lowShelfRipple < 0.01) lowShelfRipple = 0.01;

		if(fp.settings.lowShelfFrequency != lowShelfFrequency || fp.settings.lowShelfGain != lowShelfGain || fp.settings.lowShelfRipple != lowShelfRipple){
			fp.setLowShelf(lowShelfFrequency, lowShelfGain, lowShelfRipple);
		}
		
		ImGui::PopID();

		///////////////////////////////////////////////
		/// HIGH SHELF FILTER
		///////////////////////////////////////////////

		ImGui::PushID("HighShelf");

		ImGui::Checkbox("Use High Shelf", &fp.settings.bUseHighShelf);

		int highPassFrequency = fp.settings.highShelfFrequency;
		float highShelfGain = fp.settings.highShelfGain;
		float highShelfRipple = fp.settings.highShelfRipple;

		ImGui::InputInt("Frequency", &highPassFrequency);
		ImGui::InputFloat("Gain", &highShelfGain);
		ImGui::InputFloat("Ripple", &highShelfRipple);

		if(highPassFrequency < 1) highPassFrequency = 1;
		if(highShelfRipple < 0.01) highShelfRipple = 0.01;

		if(fp.settings.highShelfFrequency != highPassFrequency || fp.settings.highShelfGain != highShelfGain || fp.settings.highShelfRipple != highShelfRipple){
			fp.setHighShelf(highPassFrequency, highShelfGain, highShelfRipple);
		}
		
		ImGui::PopID();

		///////////////////////////////////////////////
		/// BAND PASS FILTER
		///////////////////////////////////////////////

		ImGui::Checkbox("Use Band Pass", &fp.settings.bUseBandPassFilter);

		int lowBandPassFrequency = fp.settings.lowBandPassFrequency;
		int highBandPassFrequency = fp.settings.highBandPassFrequency;

		ImGui::InputInt("Low Cut", &lowBandPassFrequency);
		ImGui::InputInt("High Cut", &highBandPassFrequency);

		if(lowBandPassFrequency < 1) lowBandPassFrequency = 1;
		if(highBandPassFrequency < 1) highBandPassFrequency = 1;

		highBandPassFrequency = std::max(lowBandPassFrequency + 1, highBandPassFrequency);

		if(fp.settings.lowBandPassFrequency != lowBandPassFrequency || fp.settings.highBandPassFrequency != highBandPassFrequency){
			fp.setBandPass(lowBandPassFrequency, highBandPassFrequency);
		}

		ImGui::PopID();

	};

	bool save(std::string presetName){
		return false;
	}

	bool load(std::string presetName){
		return false;
	}

protected:

};

} // namespace Interface
} // namespace ONI