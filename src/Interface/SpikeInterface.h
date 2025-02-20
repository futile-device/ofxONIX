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

		size_t frameCount = sp.settings.spikeWaveformLengthSamples;
		numProbes = sp.numProbes;

		ImGui::PushID(sp.getName().c_str());
		ImGui::Text(sp.getName().c_str());
		
		static std::vector< std::vector<float>> devP;
		static std::vector< std::vector<float>> devN;
		std::vector<float> timeP;

		if(devP.size() != numProbes) devP.resize(numProbes);
		if(devN.size() != numProbes) devN.resize(numProbes);
		if(timeP.size() != 2) {
			timeP.resize(2);
			timeP[0] = 0;
		}
		if(devP[0].size() != 2) for(size_t i = 0; i < numProbes; ++i) devP[i].resize(2);
		if(devN[0].size() != 2) for(size_t i = 0; i < numProbes; ++i) devN[i].resize(2);

		timeP[1] = frameCount * sp.getMillisPerStep();
		for(size_t probe = 0; probe < numProbes; ++probe) {
			for(size_t frame = 0; frame < 2; ++frame){
				devP[probe][frame] = sp.getPosStDevMultiplied(probe);
				devN[probe][frame] = sp.getNegStDevMultiplied(probe);
			}
		}

		static bool bNeedsUpdate = false;
		static bool bFirstLoad = true;
		if(bFirstLoad){
			nextSettings = sp.settings;
			bFirstLoad = false;
		}
		

		if(ImGui::InputFloat("Positive Deviation Multiplier", &nextSettings.positiveDeviationMultiplier)) bNeedsUpdate = true;
		if(ImGui::InputFloat("Negative Deviation Multiplier", &nextSettings.negativeDeviationMultiplier)) bNeedsUpdate = true;

		static char* spikeDetectionTypes = "FALLING EDGE\0RISING EDGE\0BOTH EDGES\0EITHER EDGE";
		if(ImGui::Combo("Detection Type", (int*)&nextSettings.spikeEdgeDetectionType, spikeDetectionTypes, 3)) bNeedsUpdate = true;

		if(ImGui::InputFloat("Spike Wave Length (ms)", &nextSettings.spikeWaveformLengthMs)) bNeedsUpdate = true;;
		nextSettings.spikeWaveformLengthMs = std::clamp(nextSettings.spikeWaveformLengthMs, 0.02f, 1000.0f);
		nextSettings.spikeWaveformLengthSamples = nextSettings.spikeWaveformLengthMs * RHS2116_SAMPLE_FREQUENCY_MS;

		if(ImGui::InputInt("Spike Detection Buffer Size", &nextSettings.spikeWaveformBufferSize)) bNeedsUpdate = true;
		if(ImGui::InputInt("Spike Min Search Offset (samples)", &nextSettings.minSampleOffset)) bNeedsUpdate = true;

		if(ImGui::Checkbox("Align Falling Edges to Peak", &nextSettings.bFallingAlignMax)) bNeedsUpdate = true;
		if(ImGui::Checkbox("Align Rising Edges to Trough", &nextSettings.bRisingAlignMin)) bNeedsUpdate = true;

		ImGui::InputFloat("Voltage Range", &voltageRange);

		if(!bNeedsUpdate) ImGui::BeginDisabled();

		bool bAppliedSettings = false;
		if(ImGui::Button("Apply Settings")){

			sp.settings = nextSettings;
			sp.reset();
			bNeedsUpdate = false;
			bAppliedSettings = true;

		}

		if(!bNeedsUpdate && !bAppliedSettings) ImGui::EndDisabled();

		ImGui::SameLine();
		if(ImGui::Button("Reset")){
			sp.reset();
		}

		ImGui::Begin("SpikeDetection");
		ImGui::PushID("##SpikeDetection");
		static ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg;

		static int offset = 0;

		if(ImGui::BeginTable("##spiketable", 8, flags, ImVec2(-1, 0))){


			ImGui::TableHeadersRow();

			for(int probe = 0; probe < sp.numProbes; probe++){

				ImVec4 col = ImPlot::GetColormapColor(probe);

				if(probe % 8 == 0) ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(probe % 8);


				ImGui::PushID(probe);
				
				
				ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0, 0));

				if(ImPlot::BeginPlot("##spark", ImVec2(-1, 240), ImPlotFlags_CanvasOnly)){
					
					static ImPlotAxisFlags flags = ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickLabels;
					
					ImPlot::SetupAxes(nullptr, nullptr, flags, flags);
					ImPlot::SetupAxesLimits(0, sp.spikes[probe][0].rawWaveform.size(), -voltageRange, voltageRange, ImGuiCond_Always);
					ImVec4 col = ImPlot::GetColormapColor(probe);
					
					for(size_t j = 0; j < sp.spikes[probe].size(); ++j){

						col.w = (float)(j + 1) / (sp.spikeCounts[probe] % sp.spikes[probe].size());
						ImPlot::SetNextLineStyle(col);

						ImPlot::PlotLine("##spark", &sp.spikes[probe][j].rawWaveform[0], sp.spikes[probe][j].rawWaveform.size(), 1, 0, ImPlotLineFlags_None, offset); //ImPlotLineFlags_Shaded
						
						if(frameCount != 0 && numProbes != 0){
							ImPlot::SetNextLineStyle(ImVec4(1, 0, 0, 0.5));
							ImPlot::PlotLine("##devP", &timeP[0], &devP[probe][0], 2, ImPlotLineFlags_None, offset);
							ImPlot::SetNextLineStyle(ImVec4(0, 1, 0, 0.5));
							ImPlot::PlotLine("##devN", &timeP[0], &devN[probe][0], 2, ImPlotLineFlags_None, offset);
						}

					}
					ImPlot::EndPlot();
				}

				ImPlot::PopStyleVar();

				ImGui::PopID();

			}

			ImGui::EndTable();
		}

		ImGui::PopID();
		ImGui::End();

		ImGui::PopID();

	};

	bool save(std::string presetName){
		return false;
	}

	bool load(std::string presetName){
		return false;
	}

protected:

	ONI::Settings::SpikeSettings nextSettings;

	bool bUseFallingEdge = true;
	bool bUseRisingEdge = false;
	float voltageRange = 0.04;

};

} // namespace Interface
} // namespace ONI