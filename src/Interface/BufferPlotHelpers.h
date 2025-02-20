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
/// Plot helpers for AC and DC probe data
///
/////////////////////////////////////////////

enum PlotType{
	PLOT_AC_DATA = 0,
	PLOT_DC_DATA
};

static std::string toString(const ONI::Interface::PlotType& plotType){
	switch(plotType){
	case PLOT_AC_DATA: { return "Plot AC Data"; break; }
	case PLOT_DC_DATA: { return "Plot AC Data"; break; }
	}
};

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



} // namespace Interface
} // namespace ONI