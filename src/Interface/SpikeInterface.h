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
			sp.spikeMutex.lock();
			nextSettings = sp.settings;
			sp.spikeMutex.unlock();
			bFirstLoad = false;
		}
		

		if(ImGui::InputFloat("Positive Deviation Multiplier", &nextSettings.positiveDeviationMultiplier)) bNeedsUpdate = true;
		if(ImGui::InputFloat("Negative Deviation Multiplier", &nextSettings.negativeDeviationMultiplier)) bNeedsUpdate = true;

		static char* spikeDetectionTypes = "FALLING EDGE\0RISING EDGE\0BOTH EDGES\0EITHER EDGE";
		if(ImGui::Combo("Detection Type", (int*)&nextSettings.spikeEdgeDetectionType, spikeDetectionTypes, 3)) bNeedsUpdate = true;

		if(ImGui::InputFloat("Spike Wave Length (ms)", &nextSettings.spikeWaveformLengthMs)) bNeedsUpdate = true;;
		nextSettings.spikeWaveformLengthMs = std::clamp(nextSettings.spikeWaveformLengthMs, 0.02f, 1000.0f);
		nextSettings.spikeWaveformLengthSamples = nextSettings.spikeWaveformLengthMs * RHS2116_SAMPLES_PER_MS;

		if(ImGui::InputInt("Spike Detection Buffer Size", &nextSettings.shortSpikeBufferSize)) bNeedsUpdate = true;
		if(ImGui::InputInt("Spike Min Search Offset (samples)", &nextSettings.minSampleOffset)) bNeedsUpdate = true;

		if(ImGui::Checkbox("Align Falling Edges to Peak", &nextSettings.bFallingAlignMax)) bNeedsUpdate = true;
		if(ImGui::Checkbox("Align Rising Edges to Trough", &nextSettings.bRisingAlignMin)) bNeedsUpdate = true;

		ImGui::InputFloat("Voltage Range", &voltageRange);

		if(!bNeedsUpdate) ImGui::BeginDisabled();

		bool bAppliedSettings = false;
		if(ImGui::Button("Apply Settings")){
			sp.spikeMutex.lock();
			sp.settings = nextSettings;
			sp.spikeMutex.unlock();
			sp.reset();
			bNeedsUpdate = false;
			bAppliedSettings = true;

		}

		if(!bNeedsUpdate && !bAppliedSettings) ImGui::EndDisabled();

		ImGui::SameLine();
		if(ImGui::Button("Reset")){
			sp.reset();
		}

		plotCombinedBursts(sp);

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

				if(ONI::Global::model.getChannelSelect().size()){
					bool b = ONI::Global::model.getChannelSelect()[probe];
					//ImGui::Selectable("###probeid", &b);
					ImU32 cell_bg_color = ImGui::GetColorU32(ImVec4(col.x, col.y, col.z, 0.3f));
					if(b) ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, cell_bg_color);
				}

				ImGui::Text("Bursts/s: %0.5f [%0.5f]", sp.burstsPerWindowAvg[probe], sp.burstBuffer.getCurrentBurstRatePSA(probe, 30000));

				ImGui::PushID(probe);

				ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0, 0));

				if(ImPlot::BeginPlot("##spark", ImVec2(-1, 240), ImPlotFlags_CanvasOnly)){

					size_t displaySpikeBufferSize = sp.settings.shortSpikeBufferSize;
					size_t spikeWaveformSize = sp.settings.spikeWaveformLengthSamples;

					

					static ImPlotAxisFlags flags = ImPlotAxisFlags_NoLabel | ImPlotAxisFlags_NoTickLabels;
					
					ImPlot::SetupAxes(nullptr, nullptr, flags, flags);
					ImPlot::SetupAxesLimits(0, spikeWaveformSize, -voltageRange, voltageRange, ImGuiCond_Always);

					sp.spikeMutex.lock();
					size_t count = sp.spikeBuffer.getMinCount(probe, displaySpikeBufferSize);
					sp.spikeMutex.unlock();

					for(size_t j = 0; j < count; ++j){
						
						sp.spikeMutex.lock();
						int spikeIndex = sp.spikeBuffer.getLastIndex(probe) - j;
						ONI::Spike spike = sp.spikeBuffer.getSpikeAt(probe, spikeIndex);
						sp.spikeMutex.unlock();

						//if(spike.rawWaveform.size() == 0) continue;
						
						ImVec4 colf = col; colf.w = (float)(j + 1) / (count % displaySpikeBufferSize);
						ImPlot::SetNextLineStyle(colf);

						ImPlot::PlotLine("##spark", &spike.rawWaveform[0], spikeWaveformSize, 1, 0, ImPlotLineFlags_None, offset); //ImPlotLineFlags_Shaded

						if(frameCount != 0 && numProbes != 0){
							ImPlot::SetNextLineStyle(ImVec4(1, 0, 0, 0.5));
							ImPlot::PlotLine("##devP", &timeP[0], &devP[probe][0], 2, ImPlotLineFlags_None, offset);
							ImPlot::SetNextLineStyle(ImVec4(0, 1, 0, 0.5));
							ImPlot::PlotLine("##devN", &timeP[0], &devN[probe][0], 2, ImPlotLineFlags_None, offset);
						}

					}

					

					if(ImGui::IsItemClicked()){
						ONI::Global::model.clickChannelSelect(probe, ImGui::IsKeyDown(ImGuiKey::ImGuiKey_LeftShift));
						
						if(ImGui::IsKeyDown(ImGuiKey::ImGuiKey_LeftShift)){
							LOGDEBUG("Spike shift clicked: ", probe);
						} else{
							LOGDEBUG("Spike clicked: ", probe);
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

	// Plot Combined AC or DC probe data
	inline void plotCombinedBursts(ONI::Processor::SpikeProcessor& sp){


		size_t numProbes = sp.numProbes;
		size_t frameCount = std::min(sp.burstBuffer.size(), sp.burstBuffer.getBufferCount());

		if(frameCount == 0) return;

		ImGui::Begin("Burst Plot");
		ImGui::PushID("##CombinedBurstPlot");

		if(ImPlot::BeginPlot("Bursts", ImVec2(-1, -1))){

			ImPlot::SetupAxes("mS", "");

			//ImPlot::SetupAxesLimits(0, frameCount, 0, 400, ImGuiCond_Always);
			//ImPlot::SetupAxisLimits(ImAxis_X1, x_min, x_max, cond);
			ImPlot::SetupAxisLimits(ImAxis_X1, 0, frameCount, ImGuiCond_Always);
			ImPlot::SetupAxisLimits(ImAxis_Y1, 0, 50, ImGuiCond_Once);

			//for(int probe = 0; probe < numProbes; probe++) {

				//if(!ONI::Global::model.getChannelSelect()[probe]) continue; // only show selected probes

				//ImGui::PushID(probe);
				ImVec4 col = ImPlot::GetColormapColor(1);

				int offset = 0;



				ImPlot::SetNextLineStyle(col);
				//ImPlot::SetNextFillStyle(col);
				float* rawBurst = sp.burstBuffer.getRawSPSA(sp.burstBuffer.getCurrentIndex() - frameCount);
				ImPlot::PlotLine("##burstrate", rawBurst, frameCount, 1, 0, ImPlotLineFlags_None, offset);

				//bp.dataMutex[SPARSE_MUTEX].lock();

				//float* voltages = bp.sparseBuffer.getAcuVFloatRaw(probe, bp.sparseBuffer.getCurrentIndex());
				//float* spikeV = bp.sparseBuffer.getSpikeFloatRaw(probe, bp.sparseBuffer.getCurrentIndex());
				////ImPlot::PlotLine("##spike", &bp.sparseTimeStamps[0], spikeV, frameCount, ImPlotLineFlags_None, offset);
				//bool bHighLight = ONI::Global::model.getChannelSelect()[probe];
				//ImPlot::PushStyleVar(ImPlotStyleVar_FillAlpha, bHighLight ? 1.0f : 0.25f);
				//ImVec4 cl = col;
				//col.w = bHighLight ? 1 : 0.8;
				//cl.w = bHighLight ? 0.9 : 0.4;
				//ImPlot::SetNextMarkerStyle(ImPlotMarker_Square, 4, cl, -1, col);
				//ImPlot::PlotScatter("##spikeS", &bp.sparseTimeStamps[0], spikeV, frameCount, ImPlotLineFlags_None, offset);
				////ImPlot::PlotDigital("##spike2", &bp.sparseTimeStamps[0], spikeV, frameCount, ImPlotLineFlags_None, offset);

				//bp.dataMutex[SPARSE_MUTEX].unlock();

				//ImGui::PopID();

			//}

			ImPlot::EndPlot();

		}

		ImGui::PopID();
		ImGui::End();

	}

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