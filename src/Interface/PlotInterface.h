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
namespace Plot{

static void Sparkline(const char* id, const float* values, int count, float min_v, float max_v, int offset, const ImVec4& col, const ImVec2& size, bool bLabelYAxis = true) {
	ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0,0));
	if (ImPlot::BeginPlot(id, size, ImPlotFlags_CanvasOnly)){
		bLabelYAxis ? ImPlot::SetupAxes(nullptr, nullptr) : ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_None);
		ImPlot::SetupAxesLimits(0, count - 1, min_v, max_v, ImGuiCond_Always);
		ImPlot::SetNextLineStyle(col);
		//ImPlot::SetNextFillStyle(col, 0.25);
		ImPlot::PlotLine(id, values, count, 1, 0, ImPlotLineFlags_None, offset); //ImPlotLineFlags_Shaded
		ImPlot::EndPlot();
	}
	ImPlot::PopStyleVar();
}

enum PlotType{
	PLOT_AC_DATA = 0,
	PLOT_DC_DATA
};

static std::string toString(const ONI::Interface::Plot::PlotType& plotType){
	switch(plotType){
		case PLOT_AC_DATA: {return "Plot AC Data"; break;}
		case PLOT_DC_DATA: {return "Plot AC Data"; break;}
	}
};

static inline void combinedProbePlot(const std::string plotName, const ProbeData& p, const ONI::Interface::Plot::PlotType& plotType){

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

	if(ImPlot::BeginPlot("Data Frames", ImVec2(-1, -1))){
		ImPlot::SetupAxes("mS",unitStr.c_str());

		for (int probe = 0; probe < numProbes; probe++) {

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



//--------------------------------------------------------------
static inline void individualProbePlot(const std::string plotName, const ProbeData& p, const ONI::Interface::Plot::PlotType& plotType){

	//const std::lock_guard<std::mutex> lock(mutex);

	size_t numProbes = 0;
	size_t frameCount = 0;
	const std::vector<std::vector<float>>* voltages = nullptr;
	const std::vector<ONI::ProbeStatistics>* statistics = nullptr;
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
			voltageStr = "AC Voltages";
			unitStr = "mV";
			break;
		}
		case PLOT_DC_DATA: {
			numProbes = p.dcProbeStats.size();
			frameCount = p.dcProbeVoltages[0].size();
			voltages = &p.dcProbeVoltages;
			statistics = &p.dcProbeStats;
			voltageRange = 8.0f;
			voltageStr = "DC Voltages";
			unitStr = "V";
			break;
		}
	}

	if(frameCount == 0) return;

	ImGui::Begin(plotName.c_str());
	ImGui::PushID("##AllProbePlot");
	static ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
		ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable;

	static int offset = 0;

	if(ImGui::BeginTable("##probetable", 3, flags, ImVec2(-1, 0))){
		ImGui::TableSetupColumn("Probe", ImGuiTableColumnFlags_WidthFixed, 75.0f);
		ImGui::TableSetupColumn("Voltage", ImGuiTableColumnFlags_WidthFixed, 75.0f);
		ImGui::TableSetupColumn(voltageStr.c_str());

		ImGui::TableHeadersRow();
		ImPlot::PushColormap(ImPlotColormap_Cool);

		for (int probe = 0; probe < numProbes; probe++){

			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Probe %d", probe);
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%.3f %s \n%.3f avg \n%.3f dev \n%i N", (*voltages)[probe][offset], unitStr.c_str(), (*statistics)[probe].mean, (*statistics)[probe].deviation, frameCount);
			ImGui::TableSetColumnIndex(2);

			ImGui::PushID(probe);

			ONI::Interface::Plot::Sparkline("##spark", &(*voltages)[probe][0], frameCount, -voltageRange, voltageRange, offset, ImPlot::GetColormapColor(probe), ImVec2(-1, 120));

			ImGui::PopID();

		}

		ImPlot::PopColormap();
		ImGui::EndTable();
	}
	
	ImGui::PopID();
	ImGui::End();

}

} // namespace Plot
} // namespace Interface
} // namespace ONI