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
#include "../Interface/Rhs2116Interface.h"

#include "ofxImGui.h"
#include "ofxImPlot.h"
#include "ofxFutilities.h"

#pragma once

namespace ONI{
namespace Interface{

class Rhs2116MultiInterface : public ONI::Interface::Rhs2116Interface{

public:

	//Rhs2116MultiInterface(){}

	~Rhs2116MultiInterface(){};

	void reset(){};
	inline void process(oni_frame_t* frame){}; // nothing
	inline void process(ONI::Frame::BaseFrame& frame){};
	bool bFirstLoad = true;

	inline void gui(ONI::Processor::BaseProcessor& processor){

		ONI::Processor::Rhs2116MultiProcessor& rhsm = *reinterpret_cast<ONI::Processor::Rhs2116MultiProcessor*>(&processor);

		if(bFirstLoad){ // for whatever reason analog high cutoff wasn't updating so let's just force the reset here
			bFirstLoad = false;
			rhsm.reset();
		}

		nextSettings = rhsm.settings;
		
		ImGui::PushID(rhsm.getName().c_str());
		ImGui::Text(rhsm.getName().c_str());

		Rhs2116Interface::deviceGui(rhsm.settings); // draw the common device gui settings

		if(nextSettings.dspCutoff != rhsm.settings.dspCutoff) rhsm.setDspCutOff(nextSettings.dspCutoff);
		if(nextSettings.lowCutoff != rhsm.settings.lowCutoff) rhsm.setAnalogLowCutoff(nextSettings.lowCutoff);
		if(nextSettings.lowCutoffRecovery != rhsm.settings.lowCutoffRecovery) rhsm.setAnalogLowCutoffRecovery(nextSettings.lowCutoffRecovery);
		if(nextSettings.highCutoff != rhsm.settings.highCutoff) rhsm.setAnalogHighCutoff(nextSettings.highCutoff);

		if(rhsm.settings != nextSettings) rhsm.settings = nextSettings;

		ImGui::PopID();
		

	}

	bool save(std::string presetName){
		return false;
	}

	bool load(std::string presetName){
		return false;
	}

protected:

	//Rhs2116DeviceSettings nextSettings; // inherited from base Rhs2116Interface

	const std::string processorName = "Rhs2116MultiGui";

};

} // namespace Interface
} // namespace ONI