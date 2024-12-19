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
#include "../Interface/Rhs2116Interface.h"
#include "../Interface/PlotHelpers.h"
#include "../Type/DataBuffer.h"

#include "ofxImGui.h"
#include "ofxImPlot.h"
#include "ofxFutilities.h"

#pragma once

namespace ONI{
namespace Interface{

class Rhs2116MultiInterface : public ONI::Interface::Rhs2116Interface{

public:

	Rhs2116MultiInterface(){
		bufferSizeTimeMillis = 5000;
		bufferSampleTimeMillis = 10;
		resetBuffers();
		bThread = true;
		thread = std::thread(&ONI::Interface::Rhs2116MultiInterface::processProbeData, this);
	}

	~Rhs2116MultiInterface(){
	
		bThread = false;
		if(thread.joinable()) thread.join();

		buffer.clear();
		probeData.acProbeStats.clear();
		probeData.dcProbeStats.clear();
		probeData.acProbeVoltages.clear();
		probeData.dcProbeVoltages.clear();
		probeData.probeTimeStamps.clear();

	};

	inline void process(oni_frame_t* frame){}; // nothing

	inline void process(ONI::Frame::BaseFrame& frame){
		buffer.push(*reinterpret_cast<ONI::Frame::Rhs2116MultiFrame*>(&frame));
	};

	inline void gui(ONI::Device::BaseDevice& device){

		ONI::Device::Rhs2116MultiDevice& rhsm = *reinterpret_cast<ONI::Device::Rhs2116MultiDevice*>(&device);

		rhsm.subscribeProcessor(processorName, ONI::Device::FrameProcessorType::POST_FRAME_PROCESSOR, this);

		nextSettings = rhsm.settings;

		ImGui::PushID(rhsm.getName().c_str());
		ImGui::Text(rhsm.getName().c_str());

		if(ImGui::Button("Edit Channel Map")){
			ImGui::OpenPopup("Channel Map");
		}

		ImGui::Separator();

		Rhs2116Interface::deviceGui(rhsm.settings); // draw the common device gui settings

		static bool bBuffersNeedUpdate = false;
		if(ImGui::InputInt("Buffer Size (ms)", &bufferSizeTimeMillis)){
			bBuffersNeedUpdate = true;
			//resetBuffers();
		}

		if(ImGui::InputInt("Buffer Sample Time (ms)", &bufferSampleTimeMillis)){
			bBuffersNeedUpdate = true;
			//if(bufferSampleTimeMillis == 0) bufferSampleTimeMillis = 1;
			//resetBuffers();
		}

		if(!bBuffersNeedUpdate) ImGui::BeginDisabled();

		bool bAppliedSettings = false;
		if(ImGui::Button("Apply Settings")){

			if(bBuffersNeedUpdate) {
				resetBuffers();
				bBuffersNeedUpdate = false;
			}

			bAppliedSettings = true;

		}

		if(!bBuffersNeedUpdate && !bAppliedSettings) ImGui::EndDisabled();

		//ImGui::SameLine();

		//if(ImGui::Button("Save Config")){
		//	save("default");
		//}

		bool unused = true;
		bool bChannelMapNeedsUpdate = false;

		if(ImGui::BeginPopupModal("Channel Map", &unused, ImGuiWindowFlags_AlwaysAutoResize)){

			const size_t& numProbes = rhsm.getNumProbes();

			ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5, 0.5));
			for(size_t y = 0; y < numProbes; ++y){
				for(size_t x = 0; x < numProbes; ++x){
					if (x > 0) ImGui::SameLine();
					ImGui::PushID(y * numProbes + x);
					char buf[16];
					std::sprintf(buf, "%02dx%02d", x, y % 16);
					if (ImGui::Selectable(buf, nextSettings.channelMap[y][x] != 0, ImGuiSelectableFlags_NoAutoClosePopups, ImVec2(20, 20))){
						size_t swapIDXx, swapIDXy = 0;
						for(size_t xx = 0; xx < numProbes; ++xx) {
							if(nextSettings.channelMap[y][xx]){
								swapIDXx = xx;  
								LOGDEBUG("From (x,y) (%i,%i) to (%i,%i)", y, swapIDXx, y, x);
							}
							nextSettings.channelMap[y][xx] = false; // toggle off everything in this row
						}
						for(size_t yy = 0; yy < numProbes; ++yy){
							if(nextSettings.channelMap[yy][x]) {
								swapIDXy = yy;
								LOGDEBUG("Switch (x,y) (%i,%i) to (%i,%i)", swapIDXy, x, swapIDXy, swapIDXx);
							}
							nextSettings.channelMap[yy][x] = false; // toggle off everything in this col
						}
						nextSettings.channelMap[y][x] = true;
						nextSettings.channelMap[swapIDXy][swapIDXx] = true;
						bChannelMapNeedsUpdate = true;

					}
					ImGui::PopID();
				}
			}
			ImGui::PopStyleVar();
			ImGui::Separator();
			if (ImGui::Button("Done", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); } ImGui::SameLine();
			if (ImGui::Button("Reset", ImVec2(120, 0))) { 
				rhsm.resetChannelMap(); 
				nextSettings.channelMap = rhsm.settings.channelMap;
				bChannelMapNeedsUpdate = true; 
			};
			ImGui::EndPopup();
		}

		if(nextSettings.dspCutoff != rhsm.settings.dspCutoff) rhsm.setDspCutOff(nextSettings.dspCutoff);
		if(nextSettings.lowCutoff != rhsm.settings.lowCutoff) rhsm.setAnalogLowCutoff(nextSettings.lowCutoff);
		if(nextSettings.lowCutoffRecovery != rhsm.settings.lowCutoffRecovery) rhsm.setAnalogLowCutoffRecovery(nextSettings.lowCutoffRecovery);
		if(nextSettings.highCutoff != rhsm.settings.highCutoff) rhsm.setAnalogHighCutoff(nextSettings.highCutoff);

		if(bChannelMapNeedsUpdate){
			rhsm.settings.channelMap = nextSettings.channelMap; 
			rhsm.channelMapToIDX();
		}

		if(rhsm.settings != nextSettings) rhsm.settings = nextSettings;

		resetProbeData(rhsm.getNumProbes(), buffer.size());

		mutex.lock();
		bufferPlot(probeData);
		probePlot(probeData);
		mutex.unlock();

		ImGui::PopID();

	}

	void processProbeData(){

		while(bThread){

			if(buffer.isFrameNew()){ // is this helping at all? Or just locking up threads?!

				std::vector<ONI::Frame::Rhs2116MultiFrame> frameBuffer = buffer.getBuffer();
				std::sort(frameBuffer.begin(), frameBuffer.end(), ONI::Frame::acquisition_clock_compare());

				mutex.lock();

				size_t numProbes = probeData.acProbeStats.size();
				size_t frameCount = frameBuffer.size();

				for(size_t probe = 0; probe < numProbes; ++probe){

					probeData.acProbeStats[probe].sum = 0;
					probeData.dcProbeStats[probe].sum = 0;

					for(size_t frame = 0; frame < frameCount; ++frame){

						probeData.probeTimeStamps[probe][frame] =  uint64_t((frameBuffer[frame].getDeltaTime() - frameBuffer[0].getDeltaTime()) / 1000);
						probeData.acProbeVoltages[probe][frame] = frameBuffer[frame].ac_uV[probe]; //0.195f * (frames1[frame].ac[probe     ] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
						probeData.dcProbeVoltages[probe][frame] = frameBuffer[frame].dc_mV[probe]; //-19.23 * (frames1[frame].dc[probe     ] - 512) / 1000.0f;   // -19.23 mV × (ADC result – 512) divide by 1000 for V?

						probeData.acProbeStats[probe].sum += probeData.acProbeVoltages[probe][frame];
						probeData.dcProbeStats[probe].sum += probeData.dcProbeVoltages[probe][frame];

					}

					probeData.acProbeStats[probe].mean = probeData.acProbeStats[probe].sum / frameCount;
					probeData.dcProbeStats[probe].mean = probeData.dcProbeStats[probe].sum / frameCount;

					probeData.acProbeStats[probe].ss = 0;
					probeData.dcProbeStats[probe].ss = 0;

					for(size_t frame = 0; frame < frameCount; ++frame){
						float acdiff = probeData.acProbeVoltages[probe][frame] - probeData.acProbeStats[probe].mean;
						float dcdiff = probeData.dcProbeVoltages[probe][frame] - probeData.dcProbeStats[probe].mean;
						probeData.acProbeStats[probe].ss += acdiff * acdiff; 
						probeData.dcProbeStats[probe].ss += dcdiff * dcdiff;
					}

					probeData.acProbeStats[probe].variance = probeData.acProbeStats[probe].ss / (frameCount - 1);  // use population (N) or sample (n-1) deviation?
					probeData.acProbeStats[probe].deviation = sqrt(probeData.acProbeStats[probe].variance);

					probeData.dcProbeStats[probe].variance = probeData.dcProbeStats[probe].ss / (frameCount - 1);  // use population (N) or sample (n-1) deviation?
					probeData.dcProbeStats[probe].deviation = sqrt(probeData.dcProbeStats[probe].variance);

				}

				mutex.unlock();
			}else{
				std::this_thread::yield(); // if we don't yield then the thread spins too fast while waiting for a buffer.isFrameNew()
			}

			

		}

	}

	void bufferPlot(const ProbeData& p){

		//const std::lock_guard<std::mutex> lock(mutex);

		size_t numProbes = p.acProbeStats.size();
		size_t frameCount = p.acProbeVoltages[0].size();

		if(frameCount == 0) return;

		ImGui::Begin("Buffered");
		ImGui::PushID("BufferPlot");

		ImPlot::BeginPlot("Data Frames", ImVec2(-1,-1));
		ImPlot::SetupAxes("mS","mV");


		for (int probe = 0; probe < numProbes; probe++) {

			ImGui::PushID(probe);
			ImPlot::SetNextLineStyle(ImPlot::GetColormapColor(probe));

			int offset = 0;

			//ImPlot::SetupAxesLimits(time[0], time[frameCount - 1] - 1, -5.0f, 5.0f, ImGuiCond_Always);
			ImPlot::SetupAxesLimits(0, p.probeTimeStamps[probe][frameCount - 1] - 1, -5.0f, 5.0f, ImGuiCond_Always);
			ImPlot::PlotLine("##probe", &p.probeTimeStamps[probe][0], &p.acProbeVoltages[probe][0], frameCount, ImPlotLineFlags_None, offset);

			//ImPlot::SetupAxesLimits(0, frameCount - 1, -6.0f, 6.0f, ImGuiCond_Always);
			//ImPlot::PlotLine("###probe", data, frameCount, 1, 0, ImPlotLineFlags_None, offset);


			ImGui::PopID();

		}

		ImPlot::EndPlot();
		ImGui::PopID();
		ImGui::End();

	}



	//--------------------------------------------------------------
	void probePlot(const ProbeData& p){

		//const std::lock_guard<std::mutex> lock(mutex);

		size_t numProbes = p.acProbeStats.size();
		size_t frameCount = p.acProbeVoltages[0].size();

		if(frameCount == 0) return;

		ImGui::Begin("Probes");
		ImGui::PushID("Probe Plot");
		static ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
			ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable;

		static int offset = 0;

		ImGui::BeginTable("##table", 3, flags, ImVec2(-1,0));
		ImGui::TableSetupColumn("Electrode", ImGuiTableColumnFlags_WidthFixed, 75.0f);
		ImGui::TableSetupColumn("Voltage", ImGuiTableColumnFlags_WidthFixed, 75.0f);
		ImGui::TableSetupColumn("AC Signal");

		ImGui::TableHeadersRow();
		ImPlot::PushColormap(ImPlotColormap_Cool);

		for (int probe = 0; probe < numProbes; probe++){

			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("Probe %d", probe);
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%.3f mV \n%.3f avg \n%.3f dev \n%i N", p.acProbeVoltages[probe][offset], p.acProbeStats[probe].mean, p.acProbeStats[probe].deviation, frameCount);
			ImGui::TableSetColumnIndex(2);

			ImGui::PushID(probe);

			ONI::Interface::Plot::Sparkline("##spark", &p.dcProbeVoltages[probe][0], frameCount, -10.0f, 10.0f, offset, ImPlot::GetColormapColor(probe), ImVec2(-1, 120));

			ImGui::PopID();

		}

		ImPlot::PopColormap();
		ImGui::EndTable();
		ImGui::PopID();
		ImGui::End();

	}

	void resetBuffers(){

		bufferSizeTimeMillis = std::clamp(bufferSizeTimeMillis, 1, 60000);
		bufferSampleTimeMillis = std::clamp(bufferSampleTimeMillis, 0, bufferSizeTimeMillis);
		buffer.resize(bufferSizeTimeMillis, bufferSampleTimeMillis, sampleFrequencyHz);

	}

	void resetProbeData(const size_t& numProbes, const size_t& bufferSizeFrames){

		const std::lock_guard<std::mutex> lock(mutex);

		if(probeData.acProbeVoltages.size() == numProbes){
			if(probeData.acProbeVoltages[0].size() == bufferSizeFrames){
				return;
			}
		}

		probeData.acProbeVoltages.resize(numProbes);
		probeData.dcProbeVoltages.resize(numProbes);
		probeData.acProbeStats.resize(numProbes);
		probeData.dcProbeStats.resize(numProbes);
		probeData.probeTimeStamps.resize(numProbes);

		for(size_t probe = 0; probe < numProbes; ++probe){
			probeData.acProbeVoltages[probe].resize(bufferSizeFrames);
			probeData.dcProbeVoltages[probe].resize(bufferSizeFrames);
			probeData.probeTimeStamps[probe].resize(bufferSizeFrames);
		}

	}

	bool save(std::string presetName){
		return false;
	}

	bool load(std::string presetName){
		return false;
	}

protected:

	//Rhs2116DeviceSettings nextSettings; // inherited from base Rhs2116Interface

	int bufferSizeTimeMillis = 5000;
	int bufferSampleTimeMillis = 10;

	long double sampleFrequencyHz = 30000; //30.1932367151e3;

	ONI::DataBuffer<ONI::Frame::Rhs2116MultiFrame> buffer;

	std::thread thread;
	std::mutex mutex;

	std::atomic_bool bThread = false;

	ProbeData probeData;

	const std::string processorName = "Rhs2116MultiGui";

};

} // namespace Interface
} // namespace ONI