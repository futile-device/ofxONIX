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

#include "ofxImGui.h"
#include "ofxImPlot.h"
#include "ofxFutilities.h"

#pragma once

namespace ONI{
namespace Interface{



/////////////////////////////////////////////
///
/// Nice Sparkline Plot for multi-plotting
///
/////////////////////////////////////////////

static inline void Sparkline(const char* id, const float* values, int count, float min_v, float max_v, int offset, const ImVec4& col, const ImVec2& size, bool bLabelYAxis = false) {

	ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0,0));

	if (ImPlot::BeginPlot(id, size, ImPlotFlags_CanvasOnly)){
		bLabelYAxis ? ImPlot::SetupAxes(nullptr, nullptr) : ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_None);
		ImPlot::SetupAxesLimits(0, count, min_v, max_v, ImGuiCond_Always);

		ImPlot::SetNextLineStyle(col);
		ImPlot::PlotLine(id, values, count, 1, 0, ImPlotLineFlags_None, offset); //ImPlotLineFlags_Shaded

		ImPlot::EndPlot();
	}
	ImPlot::PopStyleVar();
}

static std::vector<float> frameTv;

static inline void Sparkline2(const char* id, const float* values, const float* values2, int count, float min_v, float max_v, int offset, const ImVec4& col, const ImVec2& size, bool bLabelYAxis = false) {

	ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0,0));
	if(frameTv.size() != count){ // TODO: make this better
		frameTv.resize(count);
		for(size_t i = 0; i < count; ++i) frameTv[i] = i;
	}
	if (ImPlot::BeginPlot(id, size, ImPlotFlags_CanvasOnly)){

		bLabelYAxis ? ImPlot::SetupAxes(nullptr, nullptr) : ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_None);

		ImPlot::SetupAxesLimits(0, count, min_v, max_v, ImGuiCond_Always);

		ImPlot::SetNextLineStyle(col);
		ImPlot::PlotLine("##prob", values, count, 1, 0, ImPlotLineFlags_None, offset); //ImPlotLineFlags_Shaded
		
		ImPlot::SetNextLineStyle(col, 0.0);
		ImPlot::SetNextFillStyle(col, 0.6);

		ImPlot::PlotDigital("##stim", &frameTv[0], values2, count, ImPlotLineFlags_None, offset);

		ImPlot::EndPlot();
	}
	ImPlot::PopStyleVar();
}

static inline void SparklineTimes(const char* id, const float* values, const float* timeStamps, int count, float min_v, float max_v, int offset, const ImVec4& col, const ImVec2& size, bool bLabelYAxis = false) {

	ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0,0));

	if (ImPlot::BeginPlot(id, size, ImPlotFlags_CanvasOnly)){

		//ImPlot::SetupAxisFormat(ImAxis_X1, "%g ms");
		ImPlot::SetupAxisFormat(ImAxis_Y1, "%.3f");

		bLabelYAxis ? ImPlot::SetupAxes(nullptr, nullptr) : ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_None);
		//ImPlot::SetupAxesLimits(0, count - 1, min_v, max_v, ImGuiCond_Always);

		ImPlot::SetupAxesLimits(0, timeStamps[count - 1], min_v, max_v, ImGuiCond_Always);

		ImPlot::SetNextLineStyle(col);
		ImPlot::PlotLine("##probe", timeStamps, values, count, ImPlotLineFlags_None, offset);

		ImPlot::EndPlot();
	}
	ImPlot::PopStyleVar();
}




/////////////////////////////////////////////
///
/// Plot helpers for Stimulus data
///
/////////////////////////////////////////////



// Plot individual Stimulus data for all probes
static inline void plotIndividualStimulus(const size_t& maxStimulusLength, 
										  const std::vector<std::vector<float>>& allStimulusAmplitudes, 
										  const std::vector<ONI::Rhs2116StimulusData>& deviceStimuli){

	static ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg;

	static int offset = 0;

	ImGui::Begin("Stimuli");
	ImGui::PushID("##IndividualStimulusPlot");

	if(ImGui::BeginTable("##table", 3, flags, ImVec2(-1,0))){

		ImGui::TableSetupColumn("Electrode", ImGuiTableColumnFlags_WidthFixed, 75.0f);
		ImGui::TableSetupColumn("Stimulus", ImGuiTableColumnFlags_WidthFixed, 75.0f);
		ImGui::TableSetupColumn("AC Signal (uA)");

		ImGui::TableHeadersRow();
		//ImPlot::PushColormap(ImPlotColormap_Cool);

		for(size_t i = 0; i < allStimulusAmplitudes.size(); ++i){

			std::vector<float> plotAmplitudes = allStimulusAmplitudes[i];

			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Probe %d", i);
			ImGui::TableSetColumnIndex(1);
			//ImGui::Text("  delay-t %.3f ms", stimuli[i].delaySamples / 30.1932367151e3 * 1000.0f);
			//ImGui::Text("  anode-t %.3f ms", stimuli[i].anodicWidthSamples / 30.1932367151e3 * 1000.0f);
			//ImGui::Text("  dwell-t %.3f ms", stimuli[i].dwellSamples / 30.1932367151e3 * 1000.0f);
			//ImGui::Text("cathode-t %.3f ms", stimuli[i].anodicWidthSamples / 30.1932367151e3 * 1000.0f);
			//ImGui::Text("  inter-t %.3f ms", stimuli[i].interStimulusIntervalSamples / 30.1932367151e3 * 1000.0f);
			//ImGui::Text("   anodic %.3f uA", stimuli[i].actualAnodicAmplitudeMicroAmps);
			//ImGui::Text(" cathodic %.3f uA", stimuli[i].actualCathodicAmplitudeMicroAmps);
			ImGui::TableSetColumnIndex(2);

			ImGui::PushID(i);


			// make all stimulus plots the same length
			size_t sampleDiff = maxStimulusLength - plotAmplitudes.size();
			for(size_t j = 0; j < sampleDiff; ++j) plotAmplitudes.push_back(0);

			//std::vector<float> allTimeStamps(plotAmplitudes.size());
			//for(size_t j = 0; j < allTimeStamps.size(); ++j) allTimeStamps[j] = j / 30.1932367151e3 * 1000.0f;

			//ONI::Interface::Plot::SparklineTimes("##spark", &plotAmplitudes[0], &allTimeStamps[0], plotAmplitudes.size(), -stimuli[i].actualAnodicAmplitudeMicroAmps - 0.01, stimuli[i].actualCathodicAmplitudeMicroAmps + 0.01, offset, ImPlot::GetColormapColor(i), ImVec2(-1, 80), true);
			ONI::Interface::Sparkline("##spark", &plotAmplitudes[0], plotAmplitudes.size(), -deviceStimuli[i].actualAnodicAmplitudeMicroAmps - 0.01, deviceStimuli[i].actualCathodicAmplitudeMicroAmps + 0.01, offset, ImPlot::GetColormapColor(i), ImVec2(-1, 80), true);



			ImGui::PopID();

		}

		//ImPlot::PopColormap();
		ImGui::EndTable();
	}

	ImGui::PopID();
	ImGui::End();

}



/////////////////////////////////////////////
///
/// Plot helpers for AC and DC probe data
///
/////////////////////////////////////////////

enum PlotType{
	PLOT_AC_DATA = 0,
	PLOT_DC_DATA
};

static std::string toString(const ONI::Interface::PlotType& plotType){
	switch(plotType){
	case PLOT_AC_DATA: {return "Plot AC Data"; break;}
	case PLOT_DC_DATA: {return "Plot AC Data"; break;}
	}
};


// Plot Combined AC or DC probe data
static inline void plotCombinedHeatMap(const std::string plotName, 
									   const ONI::Frame::Rhs2116ProbeData& p){

	//const std::lock_guard<std::mutex> lock(mutex);

	size_t numProbes = p.acProbeVoltages.size();
	size_t frameCount = p.acProbeVoltages[0].size();


	if(frameCount == 0) return;

	ONI::Processor::Rhs2116StimProcessor* stim = ONI::Global::model.getRhs2116StimProcessor();



	ImGui::Begin(plotName.c_str());
	ImGui::PushID("##HeatMapProbePlot");

	static ImPlotColormap map = ImPlotColormap_Cool; //ImPlotColormap_Plasma
	if (ImPlot::ColormapButton(ImPlot::GetColormapName(map), ImVec2(225, 0), map)) {
		map = (map + 1) % ImPlot::GetColormapCount();
		// We bust the color cache of our plots so that item colors will
		// resample the new colormap in the event that they have already
		// been created. See documentation in implot.h.
		//BustColorCache("AC Electrodes");
		//BustColorCache("DC Electrodes");
	}

	static ImPlotAxisFlags axes_flags = ImPlotAxisFlags_Lock | ImPlotAxisFlags_NoGridLines | ImPlotAxisFlags_NoTickMarks;

	ImPlot::PushColormap(map);

	float scW = ImGui::GetWindowWidth() > ImGui::GetWindowHeight() ? 120 : 60;
	//float divW = ImGui::GetWindowWidth() > ImGui::GetWindowHeight() ? 2 : 1;
	float dimW = ImGui::GetWindowWidth() > ImGui::GetWindowHeight() ? ImGui::GetWindowWidth() : ImGui::GetWindowHeight();
	float width = (dimW - scW) / 2; //std::min(ImGui::GetWindowWidth(), ImGui::GetWindowHeight())

	static float hm[8][8]; // TODO: this should be calculated or passed in depending on actual num probes

	static const char* ylabels[] = { "8","7","6","5","4","3","2","1" };
	static const char* xlabels[] = { "A","B","C","D","E","F","G","H" };

	for (size_t i = 0; i < numProbes; ++i) {
		size_t col = i % 8;
		size_t row = std::floor(i / 8);
		hm[row][col] = (p.acProbeVoltages)[i][(p.acProbeVoltages)[i].size() - 1];
	}

	float voltageRange = 5.0f;

	if (ImPlot::BeginPlot("AC Electrodes", ImVec2(width, width), ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
		ImPlot::SetupAxes(nullptr, nullptr, axes_flags, axes_flags);
		ImPlot::SetupAxisTicks(ImAxis_X1, 0 + 1.0 / 16.0, 1 - 1.0 / 16.0, 8, xlabels);
		ImPlot::SetupAxisTicks(ImAxis_Y1, 1 - 1.0 / 16.0, 0 + 1.0 / 16.0, 8, ylabels);
		ImPlot::PlotHeatmap("AC Voltages", hm[0], 8, 8, -voltageRange, voltageRange, "", ImPlotPoint(0, 0), ImPlotPoint(1, 1), ImPlotHeatmapFlags_None);
		ImPlot::EndPlot();
	}

	ImGui::SameLine();
	ImPlot::ColormapScale("mV", -voltageRange, voltageRange, ImVec2(60, width));

	if (ImGui::GetWindowWidth() > ImGui::GetWindowHeight()) ImGui::SameLine();

	for (size_t i = 0; i < numProbes; ++i) {
		size_t col = i % 8;
		size_t row = std::floor(i / 8);
		hm[row][col] = (p.dcProbeVoltages)[i][(p.dcProbeVoltages)[i].size() - 1];
	}

	voltageRange = 5.0f;

	if (ImPlot::BeginPlot("DC Electrodes", ImVec2(width, width), ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
		ImPlot::SetupAxes(nullptr, nullptr, axes_flags, axes_flags);
		ImPlot::SetupAxisTicks(ImAxis_X1, 0 + 1.0 / 16.0, 1 - 1.0 / 16.0, 8, xlabels);
		ImPlot::SetupAxisTicks(ImAxis_Y1, 1 - 1.0 / 16.0, 0 + 1.0 / 16.0, 8, ylabels);
		ImPlot::PlotHeatmap("DC Voltages", hm[0], 8, 8, -voltageRange, voltageRange, "", ImPlotPoint(0, 0), ImPlotPoint(1, 1), ImPlotHeatmapFlags_None);
		ImPlot::EndPlot();
	}

	ImGui::SameLine();
	ImPlot::ColormapScale("V", -voltageRange, voltageRange, ImVec2(60, width));

	ImPlot::PopColormap();

	ImGui::PopID();
	ImGui::End();

}

static std::vector<float> timeStamps;

// Plot Combined AC or DC probe data
static inline void plotCombinedLinePlot(const std::string plotName,
	const ONI::Frame::Rhs2116ProbeData& p,
	const std::vector<bool>& channelSelect,
	const ONI::Interface::PlotType& plotType) {

	//const std::lock_guard<std::mutex> lock(mutex);

	size_t numProbes = 0;
	size_t frameCount = 0;
	const std::vector<std::vector<float>>* voltages = nullptr;
	std::string unitStr = "";
	float voltageRange = 0;

	switch (plotType) {

	case PLOT_AC_DATA: {
		numProbes = p.acProbeStats.size();
		frameCount = p.acProbeVoltages[0].size();
		voltages = &p.acProbeVoltages;
#ifdef USE_FRAMEBUFFER
		//if (timeStamps.size() != frameCount){
			timeStamps.resize(frameCount);
			for(size_t frame = 0; frame < frameCount; ++frame) timeStamps[frame] = frame * p.millisPerStep;
		//}
#endif
		voltageRange = 6.0f;
		unitStr = "mV";
		break;
	}

	case PLOT_DC_DATA: {
		numProbes = p.dcProbeStats.size();
		frameCount = p.dcProbeVoltages[0].size();
		voltages = &p.dcProbeVoltages;
		voltageRange = 8.0f;
		unitStr = "V";
		break;
	}

	}

	if(frameCount == 0) return;

	ONI::Processor::Rhs2116StimProcessor* stim = ONI::Global::model.getRhs2116StimProcessor();
	//std::vector<ONI::Rhs2116StimulusData> stimData = stim->getStimuli();
	//ONI::Rhs2116StimulusData defaultStimulus;

	ImGui::Begin(plotName.c_str());
	ImGui::PushID("##CombinedProbePlot");

	if(ImPlot::BeginPlot("Probe Voltages", ImVec2(-1, -1))){

		ImPlot::SetupAxes("mS", unitStr.c_str());

		for (int probe = 0; probe < numProbes; probe++) {

			if(!channelSelect[probe]) continue; // only show selected probes

			ImGui::PushID(probe);
			ImVec4 col = ImPlot::GetColormapColor(probe);
			

			int offset = 0;

#ifdef USE_FRAMEBUFFER
			ImPlot::SetupAxesLimits(0, timeStamps[frameCount - 1] - 1, -voltageRange, voltageRange, ImGuiCond_Always);
			ImPlot::SetNextLineStyle(col);
			ImPlot::PlotLine("##probe", &timeStamps[0], &(*voltages)[probe][0], frameCount, ImPlotLineFlags_None, offset);
			if (stim->isStimulusOnDevice(probe)) {
				ImPlot::SetNextLineStyle(col, 0.0);
				ImPlot::SetNextFillStyle(col, 0.6);
				//ImPlot::PlotShaded("##pshaded", &p.probeTimeStamps[probe][0], &p.stimProbeData[probe][0], frameCount, -INFINITY, ImPlotLineFlags_None, offset);
				ImPlot::PlotDigital("##stim", &timeStamps[0], &p.stimProbeData[probe][0], frameCount, ImPlotLineFlags_None, offset);
			}
#else
			ImPlot::SetupAxesLimits(0, p.probeTimeStamps[probe][frameCount - 1] - 1, -voltageRange, voltageRange, ImGuiCond_Always);
			ImPlot::SetNextLineStyle(col);
			ImPlot::PlotLine("##probe", &p.probeTimeStamps[probe][0], &(*voltages)[probe][0], frameCount, ImPlotLineFlags_None, offset);
			if (stim->isStimulusOnDevice(probe)) {
				ImPlot::SetNextLineStyle(col, 0.0);
				ImPlot::SetNextFillStyle(col, 0.6);
				//ImPlot::PlotShaded("##pshaded", &p.probeTimeStamps[probe][0], &p.stimProbeData[probe][0], frameCount, -INFINITY, ImPlotLineFlags_None, offset);
				ImPlot::PlotDigital("##stim", &p.probeTimeStamps[probe][0], &p.stimProbeData[probe][0], frameCount, ImPlotLineFlags_None, offset);
			}
#endif
			
			



			ImGui::PopID();

		}

		ImPlot::EndPlot();
	}

	ImGui::PopID();
	ImGui::End();

}


// Plot Individual AC or DC probe data
static inline void plotIndividualLinePlot(const std::string plotName,
										  const ONI::Frame::Rhs2116ProbeData& p,
										  const std::vector<bool>& channelSelect,
										  const std::vector<size_t>& inverseChannelIDX,
										  const int& probePlotHeight,
										  const ONI::Interface::PlotType& plotType){

	//const std::lock_guard<std::mutex> lock(mutex);

	size_t numProbes = 0;
	size_t frameCount = 0;
	const std::vector<std::vector<float>>* voltages = nullptr;
	const std::vector<ONI::Frame::ProbeStatistics>* statistics = nullptr;
	std::string voltageStr = "";
	std::string unitStr = "";
	float voltageRange = 0;

	switch(plotType){
	case PLOT_AC_DATA: {
		numProbes = p.acProbeStats.size();
		frameCount = p.acProbeVoltages[0].size();
		voltages = &p.acProbeVoltages;
		statistics = &p.acProbeStats;
		voltageRange = 6.0f;
		voltageStr = "AC Voltages (mV)";
		unitStr = "mV";
		break;
	}
	case PLOT_DC_DATA: {
		numProbes = p.dcProbeStats.size();
		frameCount = p.dcProbeVoltages[0].size();
		voltages = &p.dcProbeVoltages;
		statistics = &p.dcProbeStats;
		voltageRange = 8.0f;
		voltageStr = "DC Voltages (V)";
		unitStr = "V";
		break;
	}
	}

	if(frameCount == 0) return;

	ONI::Processor::Rhs2116StimProcessor* stim = ONI::Global::model.getRhs2116StimProcessor();
	//std::vector<ONI::Rhs2116StimulusData> stimData = stim->getStimuli();
	//ONI::Rhs2116StimulusData defaultStimulus;

	ImGui::Begin(plotName.c_str());
	ImGui::PushID("##AllProbePlot");
	static ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg;

	static int offset = 0;

	if(ImGui::BeginTable("##probetable", 3, flags, ImVec2(-1, 0))){

		

		ImGui::TableSetupColumn("Probe", ImGuiTableColumnFlags_WidthFixed, 75.0f);
		ImGui::TableSetupColumn("Metrics", ImGuiTableColumnFlags_WidthFixed, 75.0f);
		ImGui::TableSetupColumn(voltageStr.c_str());

		ImGui::TableHeadersRow();
		//ImPlot::PushColormap(ImPlotColormap_Cool);

		for (int probe = 0; probe < numProbes; probe++){

			ImVec4 col = ImPlot::GetColormapColor(probe);

			if(!channelSelect[probe]) continue; // only show selected probes

			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Probe %d (%d)", probe, inverseChannelIDX[probe]);
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%.3f %s \n%.3f avg \n%.3f dev \n%i N", (*voltages)[probe][offset], unitStr.c_str(), (*statistics)[probe].mean, (*statistics)[probe].deviation, frameCount);
			ImGui::TableSetColumnIndex(2);

			ImGui::PushID(probe);

			if(stim->isStimulusOnDevice(probe)){
				ONI::Interface::Sparkline2("##spark", &(*voltages)[probe][0], &p.stimProbeData[probe][0], frameCount, -voltageRange, voltageRange, offset, col, ImVec2(-1, probePlotHeight));
			}else{
				ONI::Interface::Sparkline("##spark", &(*voltages)[probe][0], frameCount, -voltageRange, voltageRange, offset, ImPlot::GetColormapColor(probe), ImVec2(-1, probePlotHeight));
			}

			ImGui::PopID();

		}

		//ImPlot::PopColormap();
		ImGui::EndTable();
	}

	ImGui::PopID();
	ImGui::End();

}


} // namespace Interface
} // namespace ONI