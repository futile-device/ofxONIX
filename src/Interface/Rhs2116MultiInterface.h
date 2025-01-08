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
#include "../Type/DataBuffer.h"

#include "ofxImGui.h"
#include "ofxImPlot.h"
#include "ofxFutilities.h"

#pragma once

namespace ONI{
namespace Interface{

class Rhs2116MultiInterface : public ONI::Interface::Rhs2116Interface{

public:

	//Rhs2116MultiInterface(){}

	~Rhs2116MultiInterface(){};

	inline void process(oni_frame_t* frame){}; // nothing

	inline void process(ONI::Frame::BaseFrame& frame){};

	inline void gui(ONI::Processor::BaseProcessor& processor){

		ONI::Device::Rhs2116MultiDevice& rhsm = *reinterpret_cast<ONI::Device::Rhs2116MultiDevice*>(&processor);

		//rhsm.subscribeProcessor(processorName, ONI::Processor::ProcessorType::POST_PROCESSOR, this);

		nextSettings = rhsm.settings;

		ImGui::PushID(rhsm.getName().c_str());
		ImGui::Text(rhsm.getName().c_str());

		//ImGui::NewLine();

		if(ImGui::Button("Edit Channel Map")){
			ImGui::OpenPopup("Channel Map");
		}

		//ImGui::NewLine();
		ImGui::Separator();
		//ImGui::NewLine();

		Rhs2116Interface::deviceGui(rhsm.settings); // draw the common device gui settings

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
								int swapIDXx = -1;
								int swapIDXy = -1;
								for(size_t xx = 0; xx < numProbes; ++xx) {
									if(nextSettings.channelMap[y][xx] && swapIDXx == -1){
										swapIDXx = xx;  
										LOGDEBUG("From (x,y) (%i,%i) to (%i,%i)", y, swapIDXx, y, x);
									}
									nextSettings.channelMap[y][xx] = false; // toggle off everything in this row
								}
								for(size_t yy = 0; yy < numProbes; ++yy){
									if(nextSettings.channelMap[yy][x] && swapIDXy == -1) {
										swapIDXy = yy;
										LOGDEBUG("Switch (x,y) (%i,%i) to (%i,%i)", swapIDXy, x, swapIDXy, swapIDXx);
									}
									nextSettings.channelMap[yy][x] = false; // toggle off everything in this col
								}
								nextSettings.channelMap[y][x] = true;
								if(swapIDXy != -1 && swapIDXx != -1) nextSettings.channelMap[swapIDXy][swapIDXx] = true;
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

		ImGui::PopID();

	}

	bool save(std::string presetName){
		return false;
	}

	bool load(std::string presetName){
		return false;
	}

protected:

	//Rhs2116DeviceSettings nextSettings; // inherited from base Rhs2116Interface

	const std::string processorName = "Rhs2116MultiGui";

};

} // namespace Interface
} // namespace ONI