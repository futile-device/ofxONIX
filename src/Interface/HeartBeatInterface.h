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




class HeartBeatInterface : public ONI::Interface::BaseInterface{

public:

	~HeartBeatInterface(){
	
	};

	inline void process(oni_frame_t* frame){}; // nothing

	inline void process(ONI::Frame::BaseFrame& frame){
		bHeartBeat = !bHeartBeat;
		deltaTime = frame.getDeltaTime();
	};

	inline void gui(ONI::Processor::BaseProcessor& processor){

		ONI::Device::HeartBeatDevice& hbd = *reinterpret_cast<ONI::Device::HeartBeatDevice*>(&processor);

		hbd.subscribeProcessor(processorName, ONI::Processor::ProcessorType::POST_PROCESSOR, this);

		ImGui::PushID(hbd.getName().c_str());
		ImGui::Text(hbd.getName().c_str());

		ImGui::RadioButton("HeartBeat", bHeartBeat); ImGui::SameLine();
		ImGui::Text(" %0.3f (ms)", deltaTime / 1000.0f);

		ImGui::Separator();
		ImGui::Separator();

		ImGui::PopID();
		

	}

	bool save(std::string presetName){
		return false;
	}

	bool load(std::string presetName){
		return false;
	}

protected:

	std::atomic_bool bHeartBeat = false;
	std::atomic_uint64_t deltaTime = 0;

	const std::string processorName = "HeartBeatGui";

};

} // namespace Interface
} // namespace ONI