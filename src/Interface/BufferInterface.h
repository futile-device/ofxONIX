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
#include "../Interface/BufferPlotHelpers.h"

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

		int step = std::floor(sparseStepSizeMillis * RHS2116_SAMPLE_FREQUENCY_MS);
		if(step == 0) step = 1;
		int szfine = std::floor(bufferSizeTimeMillis * RHS2116_SAMPLE_FREQUENCY_MS);
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
		
		using namespace std::chrono;
		double interfaceTimeMs =  (double)duration_cast<milliseconds>(duration<double>(1)).count() / (double)duration_cast<nanoseconds>(duration<double>(1)).count() * interfaceTimeNs;

		ImGui::Text("Interface Time %0.3f (mS)", interfaceTimeMs);

		ImGui::Separator();
		ImGui::InputFloat("AC Voltage Range", &acVoltageRange); 
		
		if(ImGui::Button("Force Calculate Std")) bp.calculateThresholds();
		ImGui::SameLine();
		ImGui::Checkbox("Auto Calculate Std", &bp.settings.bUseAutoThreshold);
		int t = bp.settings.autoThresholdMs;
		ImGui::InputInt("Auto Refresh Time (ms)", &t); if(t < 0) t = 1;
		bp.settings.autoThresholdMs = t;
		
		ImGui::Text("Select Probes to Plot");
		ImGui::NewLine();

		bool bLastSelectAll = bSelectAll;
		size_t selectCount = 0;

		ImGui::Checkbox("Select All", &bSelectAll); //ImGui::SameLine();

		if(bLastSelectAll != bSelectAll && bSelectAll) for(size_t i = 0; i < channelSelect.size(); ++i) channelSelect[i] = true;
		if(bLastSelectAll != bSelectAll && !bSelectAll) for(size_t i = 0; i < channelSelect.size(); ++i) channelSelect[i] = false;

		if(channelSelect.size() != bp.getNumProbes()){
			channelSelect.resize(bp.getNumProbes());
			allSelect.resize(bp.getNumProbes());
			for (size_t i = 0; i < bp.getNumProbes(); ++i) {
				channelSelect[i] = true;
				allSelect[i] = true;
			}
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
		
		
		volatile uint64_t startTime = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();


		//plotCombinedLinePlot(bp, ONI::Interface::PLOT_DC_DATA);
		plotCombinedLinePlot(bp, ONI::Interface::PLOT_AC_DATA);

		plotIndividualLinePlot(bp, ONI::Interface::PLOT_DC_DATA);
		plotIndividualLinePlot(bp, ONI::Interface::PLOT_AC_DATA);
		
		plotCombinedHeatMap(bp);


		interfaceTimeNs = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count() - startTime;
		

		ImGui::PopID();

	}
	

	bool save(std::string presetName){
		return false;
	}

	bool load(std::string presetName){
		return false;
	}

private:

	// Plot Combined AC or DC probe data
	inline void plotCombinedHeatMap(ONI::Processor::BufferProcessor& bp){

		//const std::lock_guard<std::mutex> lock(mutex);

		std::string plotName = "Electrode HeatMap";
		size_t numProbes = bp.numProbes;
		size_t frameCount = bp.sparseBuffer.size();


		if(frameCount == 0) return;

		//ONI::Processor::Rhs2116StimProcessor* stim = ONI::Global::model.getRhs2116StimProcessor();



		ImGui::Begin(plotName.c_str());
		ImGui::PushID("##HeatMapProbePlot");

		static ImPlotColormap map = ImPlotColormap_Cool; //ImPlotColormap_Plasma
		if(ImPlot::ColormapButton(ImPlot::GetColormapName(map), ImVec2(225, 0), map)) {
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

		static const char* ylabels[] = {"8","7","6","5","4","3","2","1"};
		static const char* xlabels[] = {"A","B","C","D","E","F","G","H"};

		for(size_t probe = 0; probe < numProbes; ++probe) {
			size_t col = probe % 8;
			size_t row = std::floor(probe / 8);
			bp.dataMutex[SPARSE_MUTEX].lock();
			float* voltages = bp.sparseBuffer.getAcuVFloatRaw(probe, bp.sparseBuffer.getCurrentIndex() - 1);
			hm[row][col] = voltages[0];
			bp.dataMutex[SPARSE_MUTEX].unlock();
		}

		float voltageRange = acVoltageRange;

		if(ImPlot::BeginPlot("AC Electrodes", ImVec2(width, width), ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
			ImPlot::SetupAxes(nullptr, nullptr, axes_flags, axes_flags);
			ImPlot::SetupAxisTicks(ImAxis_X1, 0 + 1.0 / 16.0, 1 - 1.0 / 16.0, 8, xlabels);
			ImPlot::SetupAxisTicks(ImAxis_Y1, 1 - 1.0 / 16.0, 0 + 1.0 / 16.0, 8, ylabels);
			ImPlot::PlotHeatmap("AC Voltages", hm[0], 8, 8, -voltageRange, voltageRange, "", ImPlotPoint(0, 0), ImPlotPoint(1, 1), ImPlotHeatmapFlags_None);
			ImPlot::EndPlot();
		}

		ImGui::SameLine();
		ImPlot::ColormapScale("mV", -voltageRange, voltageRange, ImVec2(60, width));

		if(ImGui::GetWindowWidth() > ImGui::GetWindowHeight()) ImGui::SameLine();

		for(size_t probe = 0; probe < numProbes; ++probe) {
			size_t col = probe % 8;
			size_t row = std::floor(probe / 8);
			bp.dataMutex[SPARSE_MUTEX].lock();
			float* voltages = bp.sparseBuffer.getDcmVFloatRaw(probe, bp.sparseBuffer.getCurrentIndex() - 1);
			hm[row][col] = voltages[0];
			bp.dataMutex[SPARSE_MUTEX].unlock();
		}

		voltageRange = 5.0f;

		if(ImPlot::BeginPlot("DC Electrodes", ImVec2(width, width), ImPlotFlags_NoLegend | ImPlotFlags_NoMouseText)) {
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


	// Plot Individual AC or DC probe data
	inline void plotIndividualLinePlot(ONI::Processor::BufferProcessor& bp, const ONI::Interface::PlotType& plotType){

		std::string plotName = "";
		size_t numProbes = bp.numProbes;
		size_t frameCount = bp.sparseBuffer.size();
		std::string unitStr = "";
		std::string voltageStr = "";
		float voltageRange = 0;

		switch(plotType){
		case PLOT_AC_DATA:
		{
			plotName = "AC Individual";
			voltageRange = acVoltageRange;//6.0f;
			voltageStr = "AC Voltages (mV)";
			unitStr = "mV";
			break;
		}
		case PLOT_DC_DATA:
		{
			plotName = "DC Individual";
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
			//ImPlot::PushColormap(ImPlotColormap_Cool);

			for(int probe = 0; probe < numProbes; probe++){

				ImVec4 col = ImPlot::GetColormapColor(probe);

				//if(!channelSelect[probe]) continue; // only show selected probes

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Probe %d (%d)", probe, ONI::Global::model.getChannelMapProcessor()->getInverseChannelMap()[probe]);
				ImGui::TableSetColumnIndex(1);
				bp.dataMutex[SPARSE_MUTEX].lock();
				if(plotType == PLOT_AC_DATA) ImGui::Text("%.3f avg \n%.3f dev \n%i N", bp.probeStats[probe].mean, bp.probeStats[probe].deviation, frameCount);
				bp.dataMutex[SPARSE_MUTEX].unlock();
				ImGui::TableSetColumnIndex(2);

				ImGui::PushID(probe);

				bp.dataMutex[SPARSE_MUTEX].lock();
				float* voltages = nullptr;
				if(plotType == PLOT_AC_DATA) voltages = bp.sparseBuffer.getAcuVFloatRaw(probe, bp.sparseBuffer.getCurrentIndex());
				if(plotType == PLOT_DC_DATA) voltages = bp.sparseBuffer.getDcmVFloatRaw(probe, bp.sparseBuffer.getCurrentIndex());
				ONI::Processor::Rhs2116StimProcessor* stim = ONI::Global::model.getRhs2116StimProcessor();
				if(stim->isStimulusOnDevice(probe)){
					float* stimV = bp.sparseBuffer.getStimFloatRaw(probe, bp.sparseBuffer.getCurrentIndex());
					ONI::Interface::Sparkline2("##spark", voltages, stimV, frameCount, -voltageRange, voltageRange, offset, col, ImVec2(-1, 60));
				} else{
					ONI::Interface::Sparkline("##spark", voltages, frameCount, -voltageRange, voltageRange, offset, ImPlot::GetColormapColor(probe), ImVec2(-1, 60));
				}
				bp.dataMutex[SPARSE_MUTEX].unlock();


				ImGui::PopID();

			}

			ImGui::EndTable();
		}

		ImGui::PopID();
		ImGui::End();

	}

	// Plot Combined AC or DC probe data
	inline void plotCombinedLinePlot(ONI::Processor::BufferProcessor& bp, const ONI::Interface::PlotType& plotType) {

		std::string plotName = "";
		size_t numProbes = bp.numProbes;
		size_t frameCount = bp.sparseBuffer.size();
		std::string unitStr = "";
		float voltageRange = 0;

		static std::vector< std::vector<float>> devP;
		static std::vector< std::vector<float>> devN;
		std::vector<float> timeP;
		
		switch(plotType) {

		case PLOT_AC_DATA:
		{

			plotName = "AC Combined";
			voltageRange = acVoltageRange;//6.0f;
			unitStr = "mV";

			if(bShowStdDev){
				if(devP.size() != numProbes) devP.resize(numProbes);
				if(devN.size() != numProbes) devN.resize(numProbes);
				if(timeP.size() != 2) {
					timeP.resize(2);
					timeP[0] = 0;
				}
				if(devP[0].size() != 2) for(size_t i = 0; i < numProbes; ++i) devP[i].resize(2);
				if(devN[0].size() != 2) for(size_t i = 0; i < numProbes; ++i) devN[i].resize(2);

				timeP[1] = frameCount * bp.sparseBuffer.getMillisPerStep();
				for(size_t probe = 0; probe < numProbes; ++probe) {
					for(size_t frame = 0; frame < 2; ++frame){
						devP[probe][frame] = ONI::Global::model.getSpikeProcessor()->getPosStDevMultiplied(probe);
						devN[probe][frame] = ONI::Global::model.getSpikeProcessor()->getNegStDevMultiplied(probe);
					}
				}
			}


			break;
		}

		case PLOT_DC_DATA:
		{
			plotName = "DC Combined";
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

			for(int probe = 0; probe < numProbes; probe++) {

				if(!channelSelect[probe]) continue; // only show selected probes

				ImGui::PushID(probe);
				ImVec4 col = ImPlot::GetColormapColor(probe);


				int offset = 0;

				ImPlot::SetupAxesLimits(0, bp.sparseTimeStamps[frameCount - 1] - 1, -voltageRange, voltageRange, ImGuiCond_Always);
				ImPlot::SetNextLineStyle(col);

				bp.dataMutex[SPARSE_MUTEX].lock();
				float* voltages = nullptr;
				if(plotType == PLOT_AC_DATA) voltages = bp.sparseBuffer.getAcuVFloatRaw(probe, bp.sparseBuffer.getCurrentIndex());
				if(plotType == PLOT_DC_DATA) voltages = bp.sparseBuffer.getDcmVFloatRaw(probe, bp.sparseBuffer.getCurrentIndex());
				ImPlot::PlotLine("##probe", &bp.sparseTimeStamps[0], voltages, frameCount, ImPlotLineFlags_None, offset);
				bp.dataMutex[SPARSE_MUTEX].unlock();

				if(bShowStdDev && plotType == PLOT_AC_DATA){
					ImPlot::SetNextLineStyle(ImVec4(1, 0, 0, 0.5));
					ImPlot::PlotLine("##devP", &timeP[0], &devP[probe][0], 2, ImPlotLineFlags_None, offset);
					ImPlot::SetNextLineStyle(ImVec4(0, 1, 0, 0.5));
					ImPlot::PlotLine("##devN", &timeP[0], &devN[probe][0], 2, ImPlotLineFlags_None, offset);
				}

				ONI::Processor::Rhs2116StimProcessor* stim = ONI::Global::model.getRhs2116StimProcessor();
				if(stim->isStimulusOnDevice(probe)) {
					ImPlot::SetNextLineStyle(col, 0.0);
					ImPlot::SetNextFillStyle(col, 0.6);
					bp.dataMutex[SPARSE_MUTEX].lock();
					float * stim = bp.sparseBuffer.getStimFloatRaw(probe, bp.sparseBuffer.getCurrentIndex());
					//ImPlot::PlotShaded("##pshaded", &bp.sparseTimeStamps[0], &stim[0], frameCount, -INFINITY, ImPlotLineFlags_None, offset);
					ImPlot::PlotDigital("##stim", &bp.sparseTimeStamps[0], &stim[0], frameCount, ImPlotLineFlags_None, offset);
					bp.dataMutex[SPARSE_MUTEX].unlock();

				}


				ImGui::PopID();

			}

			ImPlot::EndPlot();
		}

		ImGui::PopID();
		ImGui::End();

	}

protected:

	bool bShowStdDev = true;

	std::atomic_uint64_t interfaceTimeNs = 0;

	std::vector<bool> channelSelect;
	std::vector<bool> allSelect;

	bool bSelectAll = true;
	
	float acVoltageRange = 0.06f;

	int bufferSizeTimeMillis = -1;
	int sparseStepSizeMillis = -1;

	//ONI::Settings::BufferProcessorSettings nextSettings; // inherited from base Rhs2116Interface

	const std::string processorName = "FrameProcessorGui";

};

} // namespace Interface
} // namespace ONI