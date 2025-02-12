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
		
		lowCut = fp.settings.lowCut;
		highCut = fp.settings.highCut;

		ImGui::InputInt("Low Cut", &lowCut);
		ImGui::InputInt("High Cut", &highCut);

		if(fp.settings.lowCut != lowCut || fp.settings.highCut != highCut){
			fp.setBandPass(lowCut, highCut);
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

	int lowCut = -1;
	int highCut = -1;

};

} // namespace Interface
} // namespace ONI