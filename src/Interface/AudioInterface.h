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

class AudioInterface : public ONI::Interface::BaseInterface{

public:

	~AudioInterface(){};

	void reset(){};
	inline void process(oni_frame_t* frame){}; // nothing
	inline void process(ONI::Frame::BaseFrame& frame){}; // nothing
	
	inline void gui(ONI::Processor::BaseProcessor& processor){
	
		ONI::Processor::AudioProcessor& au = *reinterpret_cast<ONI::Processor::AudioProcessor*>(&processor);

		BaseProcessor::numProbes = au.numProbes;

		ImGui::PushID(au.getName().c_str());
		ImGui::Text(au.getName().c_str());
		
		bool bUseAudio = au.bUseAudio;
		ImGui::Checkbox("Process Audio", &bUseAudio);
		au.bUseAudio = bUseAudio;

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