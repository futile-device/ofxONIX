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
#include "../Interface/PlotInterface.h"
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

			const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
			const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();

			static ImGuiTableFlags table_flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_HighlightHoveredColumn;
			static ImGuiTableColumnFlags column_flags = ImGuiTableColumnFlags_AngledHeader | ImGuiTableColumnFlags_WidthFixed;
			
			//ImGui::SliderAngle("style.TableAngledHeadersAngle", &ImGui::GetStyle().TableAngledHeadersAngle, -50.0f, +50.0f);
			ImGui::GetStyle().TableAngledHeadersAngle = -35.0f * (2 * IM_PI) / 360.0f;
			
			if (ImGui::BeginTable("Probe_Map_Table", numProbes + 1, table_flags, ImVec2(0.0f, TEXT_BASE_HEIGHT * 12))){
				
				// Setup angled column headers
				ImGui::TableSetupColumn("RHS/MEA", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoReorder);

				for(int column = 1; column < numProbes + 1; ++column){
					char buf[16];
					std::sprintf(buf, "Electrode %i", column - 1);
					ImGui::TableSetupColumn(buf, column_flags);
				}

				ImGui::TableAngledHeadersRow(); // Draw angled headers for all columns with the ImGuiTableColumnFlags_AngledHeader flag.
				ImGui::TableHeadersRow();       // Draw remaining headers and allow access to context-menu and other functions.

				for (int row = 0; row < numProbes; ++row){
					
					ImGui::PushID(row);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();

					ImGui::Text("Probe %d", row);

					for(int column = 1; column < numProbes + 1; column++){

						if (ImGui::TableSetColumnIndex(column)){

							ImGui::PushID(column);

							size_t x = column - 1;
							size_t y = row;
							bool b = nextSettings.channelMap[y][x];

							ImGui::Checkbox("", &b);
							ImGui::SetItemTooltip("Probe %i to Electrode %i", y, x);

							// always swap the selected mapping for an electrode
							if(b != nextSettings.channelMap[y][x]){
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
						
					ImGui::PopID();
				}
				ImGui::EndTable();
			}
			
			ImGui::NewLine();
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
		ONI::Interface::Plot::plotCombinedProbes("AC Combined", probeData, ONI::Interface::Plot::PLOT_AC_DATA);
		ONI::Interface::Plot::plotIndividualProbes("AC Probes", probeData, ONI::Interface::Plot::PLOT_AC_DATA);
		ONI::Interface::Plot::plotCombinedProbes("DC Combined", probeData, ONI::Interface::Plot::PLOT_DC_DATA);
		ONI::Interface::Plot::plotIndividualProbes("DC Probes", probeData, ONI::Interface::Plot::PLOT_DC_DATA);
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