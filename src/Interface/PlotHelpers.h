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
		ImPlot::PushColormap(ImPlotColormap_Cool);

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

		ImPlot::PopColormap();
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
static inline void plotCombinedProbes(const std::string plotName, const ONI::Frame::Rhs2116ProbeData& p, const std::vector<bool>& channelSelect, const ONI::Interface::PlotType& plotType){

	//const std::lock_guard<std::mutex> lock(mutex);

	size_t numProbes = 0;
	size_t frameCount = 0;
	const std::vector<std::vector<float>>* voltages = nullptr;
	std::string unitStr = "";
	float voltageRange = 0;

	switch(plotType){

	case PLOT_AC_DATA: {
		numProbes = p.acProbeStats.size();
		frameCount = p.acProbeVoltages[0].size();
		voltages = &p.acProbeVoltages;
		voltageRange = 5.0f;
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

	ImGui::Begin(plotName.c_str());
	ImGui::PushID("##CombinedProbePlot");

	if(ImPlot::BeginPlot("Probe Voltages", ImVec2(-1, -1))){

		ImPlot::SetupAxes("mS", unitStr.c_str());

		for (int probe = 0; probe < numProbes; probe++) {

			if(!channelSelect[probe]) continue; // only show selected probes

			ImGui::PushID(probe);
			ImPlot::SetNextLineStyle(ImPlot::GetColormapColor(probe));

			int offset = 0;

			//ImPlot::SetupAxesLimits(time[0], time[frameCount - 1] - 1, -5.0f, 5.0f, ImGuiCond_Always);
			ImPlot::SetupAxesLimits(0, p.probeTimeStamps[probe][frameCount - 1] - 1, -voltageRange, voltageRange, ImGuiCond_Always);
			ImPlot::PlotLine("##probe", &p.probeTimeStamps[probe][0], &(*voltages)[probe][0], frameCount, ImPlotLineFlags_None, offset);

			//ImPlot::SetupAxesLimits(0, frameCount - 1, -6.0f, 6.0f, ImGuiCond_Always);
			//ImPlot::PlotLine("###probe", data, frameCount, 1, 0, ImPlotLineFlags_None, offset);


			ImGui::PopID();

		}

		ImPlot::EndPlot();
	}

	ImGui::PopID();
	ImGui::End();

}


// Plot Individual AC or DC probe data
static inline void plotIndividualProbes(const std::string plotName, 
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
		voltageRange = 5.0f;
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

	ImGui::Begin(plotName.c_str());
	ImGui::PushID("##AllProbePlot");
	static ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg;

	static int offset = 0;

	if(ImGui::BeginTable("##probetable", 3, flags, ImVec2(-1, 0))){

		

		ImGui::TableSetupColumn("Probe", ImGuiTableColumnFlags_WidthFixed, 75.0f);
		ImGui::TableSetupColumn("Metrics", ImGuiTableColumnFlags_WidthFixed, 75.0f);
		ImGui::TableSetupColumn(voltageStr.c_str());

		ImGui::TableHeadersRow();
		ImPlot::PushColormap(ImPlotColormap_Cool);

		for (int probe = 0; probe < numProbes; probe++){


			if(!channelSelect[probe]) continue; // only show selected probes

			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Probe %d (%d)", probe, inverseChannelIDX[probe]);
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%.3f %s \n%.3f avg \n%.3f dev \n%i N", (*voltages)[probe][offset], unitStr.c_str(), (*statistics)[probe].mean, (*statistics)[probe].deviation, frameCount);
			ImGui::TableSetColumnIndex(2);

			ImGui::PushID(probe);

			ONI::Interface::Sparkline("##spark", &(*voltages)[probe][0], frameCount, -voltageRange, voltageRange, offset, ImPlot::GetColormapColor(probe), ImVec2(-1, probePlotHeight));

			ImGui::PopID();

		}

		ImPlot::PopColormap();
		ImGui::EndTable();
	}

	ImGui::PopID();
	ImGui::End();

}


} // namespace Interface
} // namespace ONI