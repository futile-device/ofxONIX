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




class SpikeInterface : public ONI::Interface::BaseInterface{

public:

	~SpikeInterface(){};

	void reset(){};
	inline void process(oni_frame_t* frame){}; // nothing
	inline void process(ONI::Frame::BaseFrame& frame){}; // nothing
	
	inline void gui(ONI::Processor::BaseProcessor& processor){
	
		ONI::Processor::SpikeProcessor& sp = *reinterpret_cast<ONI::Processor::SpikeProcessor*>(&processor);

		size_t frameCount = sp.spikeSampleLength;
		numProbes = sp.numProbes;

		ImGui::PushID(sp.getName().c_str());
		ImGui::Text(sp.getName().c_str());
		
		static std::vector<float> timeStamps;
		static std::vector< std::vector<float>> devP;
		static std::vector< std::vector<float>> devN;
		
		if(frameCount != 0 && numProbes != 0){
			if(devP.size() != numProbes) devP.resize(numProbes);
			if(devN.size() != numProbes) devN.resize(numProbes);
			if(devP[0].size() != frameCount) for(size_t i = 0; i < numProbes; ++i) devP[i].resize(frameCount);
			if(devN[0].size() != frameCount) for(size_t i = 0; i < numProbes; ++i) devN[i].resize(frameCount);
			for(size_t probe = 0; probe < numProbes; ++probe) {
				for(size_t frame = 0; frame < frameCount; ++frame){
					devP[probe][frame] = ONI::Global::model.getSpikeProcessor()->getStDev(probe) * 3;
					devN[probe][frame] = -devP[probe][frame];
				}
			}
		}


		ImGui::Begin("SpikeDetection");
		ImGui::PushID("##SpikeDetection");
		static ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg;

		static int offset = 0;

		if(ImGui::BeginTable("##spiketable", 8, flags, ImVec2(-1, 0))){



			//ImGui::TableSetupColumn("Probe", ImGuiTableColumnFlags_WidthFixed, 75.0f);
			//ImGui::TableSetupColumn("Metrics", ImGuiTableColumnFlags_WidthFixed, 75.0f);
			//ImGui::TableSetupColumn(voltageStr.c_str());

			ImGui::TableHeadersRow();
			//ImPlot::PushColormap(ImPlotColormap_Cool);

			for(int probe = 0; probe < sp.numProbes; probe++){

				ImVec4 col = ImPlot::GetColormapColor(probe);

				//if(!channelSelect[probe]) continue; // only show selected probes

				if(probe % 8 == 0) ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(probe % 8);
				//ImGui::TableSetColumnIndex(0);
				////ImGui::Text("Probe %d (%d)", probe, inverseChannelIDX[probe]);
				//ImGui::TableSetColumnIndex(1);
				////ImGui::Text("%.3f %s \n%.3f avg \n%.3f dev \n%i N", (*voltages)[probe][offset], unitStr.c_str(), (*statistics)[probe].mean, (*statistics)[probe].deviation, frameCount);
				//ImGui::TableSetColumnIndex(2);

				ImGui::PushID(probe);
				float voltageRange = 0.06;
				
				ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0, 0));

				if(ImPlot::BeginPlot("##spark", ImVec2(-1, 240), ImPlotFlags_CanvasOnly)){
					static ImPlotAxisFlags flags = ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickLabels;
					ImPlot::SetupAxes(nullptr, nullptr, flags, flags);
					ImPlot::SetupAxesLimits(0, sp.spikes[probe][0].rawWaveform.size(), -voltageRange, voltageRange, ImGuiCond_Always);
					ImVec4 col = ImPlot::GetColormapColor(probe);
					for(size_t j = 0; j < sp.spikes[probe].size(); ++j){
						col.w = (float)j / sp.spikes[probe].size() + 0.2;
						ImPlot::SetNextLineStyle(col);
						ImPlot::PlotLine("##spark", &sp.spikes[probe][j].rawWaveform[0], sp.spikes[probe][j].rawWaveform.size(), 1, 0, ImPlotLineFlags_None, offset); //ImPlotLineFlags_Shaded
						if(frameCount != 0 && numProbes != 0){
							ImPlot::SetNextLineStyle(ImVec4(0.5, 0, 0, 0.2), 0.2);
							ImPlot::PlotLine("##devP", &devP[probe][0], frameCount, 1, 0, ImPlotLineFlags_None, offset); //ImPlotLineFlags_Shaded
							ImPlot::SetNextLineStyle(ImVec4(0, 0.5, 0, 0.2), 0.2);
							ImPlot::PlotLine("##devN", &devN[probe][0], frameCount, 1, 0, ImPlotLineFlags_None, offset); //ImPlotLineFlags_Shaded
						}

					}
					ImPlot::EndPlot();
				}

				ImPlot::PopStyleVar();
				//ONI::Interface::Sparkline("##spark", &sp.spikes[probe][j].rawWaveform[0], sp.spikes[probe][j].rawWaveform.size(), -voltageRange, voltageRange, offset, ImPlot::GetColormapColor(probe), ImVec2(-1, 60));

				
				


				ImGui::PopID();

			}

			//ImPlot::PopColormap();
			ImGui::EndTable();
		}

		ImGui::PopID();
		ImGui::End();


		for(size_t probe = 0; probe < sp.numProbes; ++probe){
			
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