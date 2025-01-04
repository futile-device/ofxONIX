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




class FmcInterface : public ONI::Interface::BaseInterface{

public:

	~FmcInterface(){};

	inline void process(oni_frame_t* frame){}; // nothing
	inline void process(ONI::Frame::BaseFrame& frame){}; // nothing
	
	inline void gui(ONI::Processor::BaseProcessor& processor){
	
		ONI::Device::FmcDevice& fmc = *reinterpret_cast<ONI::Device::FmcDevice*>(&processor);

		if(!bIsApplying) nextSettings = fmc.settings;

		ImGui::PushID(fmc.getName().c_str());
		ImGui::Text(fmc.getName().c_str());

		ImGui::SetNextItemWidth(200);

		ImGui::InputFloat("Port Voltage", &nextSettings.voltage, 0.1f, 0.1f, "%.1f"); 
		nextSettings.voltage = std::clamp(nextSettings.voltage, 3.0f, 10.0f); // use some way of specifying min and max

		if(nextSettings.voltage != fmc.settings.voltage){

			ImGui::SameLine();

			bIsApplying = true;
			bool bConfirmed = false;

			if(ImGui::Button("Apply")){
				if(nextSettings.voltage >= 5.0f){
					ImGui::OpenPopup("Overvoltage");
				}else{
					bConfirmed = true;
				}
			}

			if(nextSettings.voltage >= 5.0f){
				ImGui::SameLine();
				ImGui::Text("WARNING Voltage exceeds 5.0v");
			}

			// Always center this window when appearing
			ImVec2 center = ImGui::GetMainViewport()->GetCenter();
			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

			if(ImGui::BeginPopupModal("Overvoltage", NULL)){
				ImGui::Text("Voltages above 5.0v may damage the device!");
				ImGui::Text("Do you really want to supply %.1f v to device?", nextSettings.voltage);
				ImGui::Separator();
				if (ImGui::Button("Yes", ImVec2(120, 0))) { bConfirmed = true; bIsApplying = true; ImGui::CloseCurrentPopup(); }
				ImGui::SetItemDefaultFocus();
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120, 0))) {  bConfirmed = false; bIsApplying = false; ImGui::CloseCurrentPopup(); }
				ImGui::EndPopup();
			}

			if(bIsApplying && bConfirmed){
				LOGINFO("Change voltage from %.1f to %.1f", fmc.settings.voltage, nextSettings.voltage);
				fmc.setPortVoltage(nextSettings.voltage);
				bIsApplying = false;
			}

		}

		ImGui::Separator();
		ImGui::Separator();
		ImGui::PopID();

	};

	bool save(std::string presetName){
		return false;
	}

	bool load(std::string presetName){
		return false;
	}

protected:

	ONI::Settings::FmcDeviceSettings nextSettings;
	bool bIsApplying = false;

};

} // namespace Interface
} // namespace ONI