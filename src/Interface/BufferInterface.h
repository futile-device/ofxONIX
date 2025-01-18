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
#include "../Processor/BufferProcessor.h"
#include "../Interface/PlotHelpers.h"

#include "ofxImGui.h"
#include "ofxImPlot.h"
#include "ofxFutilities.h"

#pragma once

namespace ONI{
namespace Interface{

class BufferInterface : public ONI::Interface::BaseInterface{

public:

	//BufferInterface(){}

	~BufferInterface(){};

	void reset(){};
	inline void process(oni_frame_t* frame){}; // nothing
	inline void process(ONI::Frame::BaseFrame& frame){};

	inline void gui(ONI::Processor::BaseProcessor& processor){

		ONI::Processor::BufferProcessor& bp = *reinterpret_cast<ONI::Processor::BufferProcessor*>(&processor);

		//bp.subscribeProcessor(processorName, ONI::Processor::SubscriptionType::POST_PROCESSOR, this);

		//nextSettings = bp.settings;

		ImGui::PushID(bp.getName().c_str());
		ImGui::Text(bp.getName().c_str());

		static bool bBuffersNeedUpdate = false;

		if(bufferSizeTimeMillis == -1 || sparseStepSizeMillis == -1){ // first time
			bufferSizeTimeMillis = bp.settings.getBufferSizeMillis();
			sparseStepSizeMillis = bp.settings.getSparseStepMillis();
		}

		int step = std::floor(sparseStepSizeMillis * bp.settings.getSampleRateHz() / 1000.0f);
		if(step == 0) step = 1;
		int szfine = std::floor(bufferSizeTimeMillis * bp.settings.getSampleRateHz() / 1000.0f);
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
				bp.settings.setBufferSizeMillis(bufferSizeTimeMillis);
				bp.settings.setSparseStepSizeMillis(sparseStepSizeMillis);
				bp.resetBuffers();
				bp.resetProbeData();
				bufferSizeTimeMillis = bp.settings.getBufferSizeMillis();
				sparseStepSizeMillis = bp.settings.getSparseStepMillis();
				bBuffersNeedUpdate = false;
			}

			bAppliedSettings = true;

		}

		if(!bBuffersNeedUpdate && !bAppliedSettings) ImGui::EndDisabled();

		ImGui::Separator();
		const volatile uint64_t& processorTimeNs = bp.getProcessorTimeNs();
		using namespace std::chrono;
		double processorTimeUs =  (double)duration_cast<milliseconds>(duration<double>(1)).count() / (double)duration_cast<nanoseconds>(duration<double>(1)).count() * processorTimeNs;

		ImGui::Text("Processor Time %0.3f (uS)", processorTimeUs);

		ImGui::Separator();
		ImGui::InputInt("Probe Plot Height", &probePlotHeight);

		ImGui::Text("Select Probes to Plot");
		ImGui::NewLine();

		bool bLastSelectAll = bSelectAll;
		size_t selectCount = 0;

		ImGui::Checkbox("Select All", &bSelectAll); //ImGui::SameLine();

		if(bLastSelectAll != bSelectAll && bSelectAll) for(size_t i = 0; i < channelSelect.size(); ++i) channelSelect[i] = true;
		if(bLastSelectAll != bSelectAll && !bSelectAll) for(size_t i = 0; i < channelSelect.size(); ++i) channelSelect[i] = false;

		if(channelSelect.size() != bp.getNumProbes()){
			channelSelect.resize(bp.getNumProbes());
			for(size_t i = 0; i < bp.getNumProbes(); ++i) channelSelect[i] = true;
		}

		for(size_t i = 0; i < bp.getNumProbes(); ++i){
			if(i != 0 && i % 8 != 0) ImGui::SameLine();
			char buf[16];
			std::sprintf(buf, "%02i", i);
			bool b = channelSelect[i];
			ImGui::Checkbox(buf, &b);
			channelSelect[i] = b;
			if(bSelectAll && !b) bSelectAll = false;
			if(b) ++selectCount;
		}

		if(selectCount == bp.getNumProbes()) bSelectAll = true;
		

		bp.processMutex.lock();
		ONI::Frame::Rhs2116ProbeData sparseProbeDataCopy = bp.sparseProbeData;
		bp.processMutex.unlock();

		ONI::Interface::plotCombinedHeatMap("Electrode HeatMap", sparseProbeDataCopy);
		ONI::Interface::plotCombinedLinePlot("AC Combined", sparseProbeDataCopy, channelSelect, ONI::Interface::PLOT_AC_DATA);
		ONI::Interface::plotIndividualLinePlot("AC Probes", sparseProbeDataCopy, channelSelect, ONI::Global::model.getChannelMapProcessor()->getInverseChannelMap(), probePlotHeight, ONI::Interface::PLOT_AC_DATA);
		ONI::Interface::plotCombinedLinePlot("DC Combined", sparseProbeDataCopy, channelSelect, ONI::Interface::PLOT_DC_DATA);
		ONI::Interface::plotIndividualLinePlot("DC Probes", sparseProbeDataCopy, channelSelect, ONI::Global::model.getChannelMapProcessor()->getInverseChannelMap(), probePlotHeight, ONI::Interface::PLOT_DC_DATA);
		

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

	//ONI::Settings::BufferProcessorSettings nextSettings; // inherited from base Rhs2116Interface

	const std::string processorName = "FrameProcessorGui";

};

} // namespace Interface
} // namespace ONI