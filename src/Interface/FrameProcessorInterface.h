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
#include "../Processor/FrameProcessor.h"
#include "../Interface/PlotHelpers.h"

#include "ofxImGui.h"
#include "ofxImPlot.h"
#include "ofxFutilities.h"

#pragma once

namespace ONI{
namespace Interface{

class FrameProcessorInterface : public ONI::Interface::BaseInterface{

public:

	//FrameProcessorInterface(){}

	~FrameProcessorInterface(){};

	inline void process(oni_frame_t* frame){}; // nothing

	inline void process(ONI::Frame::BaseFrame& frame){};

	inline void gui(ONI::Processor::BaseProcessor& processor){

		ONI::Processor::FrameProcessor& fp = *reinterpret_cast<ONI::Processor::FrameProcessor*>(&processor);

		//fp.subscribeProcessor(processorName, ONI::Processor::ProcessorType::POST_PROCESSOR, this);

		//nextSettings = fp.settings;

		ImGui::PushID(fp.getName().c_str());
		ImGui::Text(fp.getName().c_str());

		if(bufferSizeTimeMillis == -1 || sparseStepSizeMillis == -1){ // first time
			bufferSizeTimeMillis = fp.settings.getBufferSizeMillis();
			sparseStepSizeMillis = fp.settings.getSparseStepMillis();
		}

		static bool bBuffersNeedUpdate = false;
		
		ImGui::SetNextItemWidth(200);
		if(ImGui::InputInt("Buffer Size (ms)", &bufferSizeTimeMillis)){
			bBuffersNeedUpdate = true;
			//resetBuffers();
		}

		ImGui::SetNextItemWidth(200);
		
		if(ImGui::InputInt("Buffer Sample Time (ms)", &sparseStepSizeMillis)){
			bBuffersNeedUpdate = true;
			//if(sparseStepSizeMillis == 0) sparseStepSizeMillis = 1;
			//resetBuffers();
		}

		if(!bBuffersNeedUpdate) ImGui::BeginDisabled();

		bool bAppliedSettings = false;
		if(ImGui::Button("Apply Settings")){

			if(bBuffersNeedUpdate){
				fp.settings.setBufferSizeMillis(bufferSizeTimeMillis);
				fp.settings.setSparseStepSizeMillis(sparseStepSizeMillis);
				fp.resetBuffers();
				fp.resetProbeData();
				bufferSizeTimeMillis = fp.settings.getBufferSizeMillis();
				sparseStepSizeMillis = fp.settings.getSparseStepMillis();
				bBuffersNeedUpdate = false;
			}

			bAppliedSettings = true;

		}

		if(!bBuffersNeedUpdate && !bAppliedSettings) ImGui::EndDisabled();

		//if(fp.settings != nextSettings) {
		//	fp.settings = nextSettings;
		//}

		

		fp.mutex.lock();
		ONI::Interface::plotCombinedProbes("AC Combined", fp.probeData, ONI::Interface::PLOT_AC_DATA);
		ONI::Interface::plotIndividualProbes("AC Probes", fp.probeData, ONI::Interface::PLOT_AC_DATA);
		ONI::Interface::plotCombinedProbes("DC Combined", fp.probeData, ONI::Interface::PLOT_DC_DATA);
		ONI::Interface::plotIndividualProbes("DC Probes", fp.probeData, ONI::Interface::PLOT_DC_DATA);
		fp.mutex.unlock();

		ImGui::PopID();

	}


	bool save(std::string presetName){
		return false;
	}

	bool load(std::string presetName){
		return false;
	}

protected:

	int bufferSizeTimeMillis = -1;
	int sparseStepSizeMillis = -1;

	//ONI::Settings::FrameProcessorSettings nextSettings; // inherited from base Rhs2116Interface

	const std::string processorName = "FrameProcessorGui";

};

} // namespace Interface
} // namespace ONI