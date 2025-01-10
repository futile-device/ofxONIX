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

		static bool bBuffersNeedUpdate = false;

		if(bufferSizeTimeMillis == -1 || sparseStepSizeMillis == -1){ // first time
			bufferSizeTimeMillis = fp.settings.getBufferSizeMillis();
			sparseStepSizeMillis = fp.settings.getSparseStepMillis();
		}

		int step = std::floor(sparseStepSizeMillis * fp.settings.getSampleRateHz() / 1000.0f);
		if(step == 0) step = 1;
		int szfine = std::floor(bufferSizeTimeMillis * fp.settings.getSampleRateHz() / 1000.0f);
		int szstep = std::floor((float)szfine / (float)step);
		

		ImGui::SetNextItemWidth(200);
		if(ImGui::InputInt("Buffer Size (ms)", &bufferSizeTimeMillis)) bBuffersNeedUpdate = true;
		ImGui::SameLine();
		ImGui::Text("Samples: %i Sparse: %i", szfine, szstep);

		ImGui::SetNextItemWidth(200);
		if(ImGui::InputInt("Buffer Step (ms)", &sparseStepSizeMillis)) bBuffersNeedUpdate = true;
		ImGui::SameLine();
		ImGui::Text("Samples: %i", step);

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

		ImGui::Separator();
		ImGui::InputInt("Probe Plot Height", &probePlotHeight);

		ImGui::Text("Select Probes to Plot");
		ImGui::NewLine();

		bool bLastSelectAll = bSelectAll;
		size_t selectCount = 0;

		ImGui::Checkbox("Select All", &bSelectAll); //ImGui::SameLine();

		if(bLastSelectAll != bSelectAll && bSelectAll) for(size_t i = 0; i < channelSelect.size(); ++i) channelSelect[i] = true;
		if(bLastSelectAll != bSelectAll && !bSelectAll) for(size_t i = 0; i < channelSelect.size(); ++i) channelSelect[i] = false;

		if(channelSelect.size() != fp.multi->getNumProbes()){
			channelSelect.resize(fp.multi->getNumProbes());
			for(size_t i = 0; i < fp.multi->getNumProbes(); ++i) channelSelect[i] = true;
		}

		for(size_t i = 0; i < fp.multi->getNumProbes(); ++i){
			if(i != 0 && i % 8 != 0) ImGui::SameLine();
			char buf[16];
			std::sprintf(buf, "%02i", i);
			bool b = channelSelect[i];
			ImGui::Checkbox(buf, &b);
			channelSelect[i] = b;
			if(bSelectAll && !b) bSelectAll = false;
			if(b) ++selectCount;
		}

		if(selectCount == fp.multi->getNumProbes()) bSelectAll = true;
		
		fp.processMutex.lock();
		ONI::Interface::plotCombinedProbes("AC Combined", fp.sparseProbeData, channelSelect, ONI::Interface::PLOT_AC_DATA);
		ONI::Interface::plotIndividualProbes("AC Probes", fp.sparseProbeData, channelSelect, fp.multi->getInverseChannelMapIDX(), probePlotHeight, ONI::Interface::PLOT_AC_DATA);
		ONI::Interface::plotCombinedProbes("DC Combined", fp.sparseProbeData, channelSelect, ONI::Interface::PLOT_DC_DATA);
		ONI::Interface::plotIndividualProbes("DC Probes", fp.sparseProbeData, channelSelect, fp.multi->getInverseChannelMapIDX(), probePlotHeight, ONI::Interface::PLOT_DC_DATA);
		fp.processMutex.unlock();

		ImGui::PopID();

	}


	bool save(std::string presetName){
		return false;
	}

	bool load(std::string presetName){
		return false;
	}

protected:

	std::vector<bool> channelSelect;

	bool bSelectAll = true;
	
	int probePlotHeight = 60;

	int bufferSizeTimeMillis = -1;
	int sparseStepSizeMillis = -1;

	//ONI::Settings::FrameProcessorSettings nextSettings; // inherited from base Rhs2116Interface

	const std::string processorName = "FrameProcessorGui";

};

} // namespace Interface
} // namespace ONI