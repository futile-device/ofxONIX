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
#include "../Processor/ChannelMapProcessor.h"

#include "ofxImGui.h"
#include "ofxImPlot.h"
#include "ofxFutilities.h"

#pragma once

namespace ONI{
namespace Interface{




class ChannelMapInterface : public ONI::Interface::BaseInterface{

public:

	~ChannelMapInterface(){};

	void reset(){};
	inline void process(oni_frame_t* frame){}; // nothing
	inline void process(ONI::Frame::BaseFrame& frame){}; // nothing
	
	inline void gui(ONI::Processor::BaseProcessor& processor){
	
		ONI::Processor::ChannelMapProcessor& cm = *reinterpret_cast<ONI::Processor::ChannelMapProcessor*>(&processor);

		//if(nextChannelMapBoolMatrix.size() != cm.getNumProbes()){
		//	nextChannelMapBoolMatrix = cm.getChannelMapBoolMatrix();
		//}

		nextChannelMapBoolMatrix = cm.getChannelMapBoolMatrix(); // not efficient but it works when updating

		ImGui::PushID(cm.getName().c_str());
		ImGui::Text(cm.getName().c_str());

		ImGui::NewLine();

		if(ImGui::Button("Edit Channel Map")){
			ImGui::OpenPopup("Channel Map");
		}

		ImGui::NewLine();

		bool unused = true;
		bool bChannelMapNeedsUpdate = false;

		if(ImGui::BeginPopupModal("Channel Map", &unused, ImGuiWindowFlags_AlwaysAutoResize)){

			const size_t& numProbes = cm.getNumProbes();

			const float TEXT_BASE_WIDTH = ImGui::CalcTextSize("A").x;
			const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();

			static ImGuiTableFlags table_flags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_HighlightHoveredColumn;
			static ImGuiTableColumnFlags column_flags = ImGuiTableColumnFlags_AngledHeader | ImGuiTableColumnFlags_WidthFixed;

			//ImGui::SliderAngle("style.TableAngledHeadersAngle", &ImGui::GetStyle().TableAngledHeadersAngle, -50.0f, +50.0f);
			ImGui::GetStyle().TableAngledHeadersAngle = -35.0f * (2 * IM_PI) / 360.0f;

			if (ImGui::BeginTable("Probe_Map_Table", numProbes + 1, table_flags, ImVec2(0.0f, TEXT_BASE_HEIGHT * 12))){

				// Setup angled column headers
				ImGui::TableSetupColumn("MEA/RHS", ImGuiTableColumnFlags_NoHide | ImGuiTableColumnFlags_NoReorder);

				for(int column = 1; column < numProbes + 1; ++column){
					char buf[16];
					std::sprintf(buf, "RHS Probe %i", column - 1);
					ImGui::TableSetupColumn(buf, column_flags);
				}

				ImGui::TableAngledHeadersRow(); // Draw angled headers for all columns with the ImGuiTableColumnFlags_AngledHeader flag.
				ImGui::TableHeadersRow();       // Draw remaining headers and allow access to context-menu and other functions.

				for (int row = 0; row < numProbes; ++row){

					ImGui::PushID(row);
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::AlignTextToFramePadding();

					ImGui::Text("MEA Electrode %d", row);

					for(size_t column = 1; column < numProbes + 1; column++){

						if (ImGui::TableSetColumnIndex(column)){

							ImGui::PushID(column);

							size_t x = column - 1;
							size_t y = row;
							bool b = nextChannelMapBoolMatrix[x][y];

							ImGui::Checkbox("", &b);
							ImGui::SetItemTooltip("Probe %i to Electrode %i", y, x);

							// always swap the selected mapping for an electrode
							if(b != nextChannelMapBoolMatrix[x][y]){
								int swapIDXx = -1;
								int swapIDXy = -1;
								for(size_t xx = 0; xx < numProbes; ++xx) {
									if(nextChannelMapBoolMatrix[xx][y] && swapIDXx == -1){
										swapIDXx = xx;  
										LOGDEBUG("From (x,y) (%i,%i) to (%i,%i)", y, swapIDXx, y, x);
									}
									nextChannelMapBoolMatrix[xx][y] = false; // toggle off everything in this row
								}
								for(size_t yy = 0; yy < numProbes; ++yy){
									if(nextChannelMapBoolMatrix[x][yy] && swapIDXy == -1) {
										swapIDXy = yy;
										LOGDEBUG("Switch (x,y) (%i,%i) to (%i,%i)", swapIDXy, x, swapIDXy, swapIDXx);
									}
									nextChannelMapBoolMatrix[x][yy] = false; // toggle off everything in this col
								}
								nextChannelMapBoolMatrix[x][y] = true;
								if(swapIDXy != -1 && swapIDXx != -1) nextChannelMapBoolMatrix[swapIDXx][swapIDXy] = true;
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
				cm.reset(); 
				nextChannelMapBoolMatrix = cm.getChannelMapBoolMatrix();
				//bChannelMapNeedsUpdate = true; 
			};
			ImGui::EndPopup();
		}


		if(bChannelMapNeedsUpdate){
			cm.setChannelMapFromBoolMatrix(nextChannelMapBoolMatrix);
			nextChannelMapBoolMatrix = cm.getChannelMapBoolMatrix();
		}

		ImGui::PopID();

	};

	bool save(std::string presetName){
		return false;
	}

	bool load(std::string presetName){
		return false;
	}

protected:

	std::vector< std::vector<bool> > nextChannelMapBoolMatrix;
	//ONI::Settings::ChannelMapProcessorSettings nextSettings;

};

} // namespace Interface
} // namespace ONI