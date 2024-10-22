//
//  ONIDevice.h
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

#pragma once

//class ONIDevice; // forward declaration

template<typename ONIDeviceTypeSettings>
class ONIDeviceConfig{

public:

	virtual ~ONIDeviceConfig(){};

	virtual bool setup(std::string deviceName) = 0;
	virtual inline void gui() = 0;
	virtual bool save(std::string filePath) = 0;
	virtual bool load(std::string filePath) = 0;

	std::string getName(){ return deviceName; };

	inline bool apply(){
		if(bApplySettings){
			bApplySettings = false;
			return true;
		}
		return false;
	}

	virtual void set(const ONIDeviceTypeSettings& settings){
		this->settings = this->lastSettings = settings;
	}

	ONIDeviceTypeSettings& get(){
		return settings;
	}

	ONIDeviceTypeSettings& last(){
		return lastSettings;
	}

	//friend class ONIDevice;

protected:

	std::string deviceName = "UNKNOWN";
	ONIDeviceTypeSettings settings;
	ONIDeviceTypeSettings lastSettings;

	bool bApplySettings = false;

};

struct FmcDeviceSettings{
	float voltage = 0;
};

inline bool operator==(const FmcDeviceSettings& lhs, const FmcDeviceSettings& rhs){
	return (lhs.voltage == rhs.voltage);
}
inline bool operator!=(const FmcDeviceSettings& lhs, const FmcDeviceSettings& rhs) { return !(lhs == rhs); }

// later let's abstract this to a specific ImGui/fu/bost header, but for now let's just go for it

#include "ofxImGui.h"

class _FmcDeviceConfig : public ONIDeviceConfig<FmcDeviceSettings> {

public:

	_FmcDeviceConfig(){};
	~_FmcDeviceConfig(){};

	bool setup(std::string deviceName){
		this->deviceName = deviceName;
		return true;
	}

	inline void gui(){

		ImGui::PushID(deviceName.c_str());
		ImGui::Text(deviceName.c_str());

		ImGui::SetNextItemWidth(200);
		ImGui::InputFloat("Port Voltage", &lastSettings.voltage, 0.1f, 0.1f, "%.1f"); 


		lastSettings.voltage = std::clamp(lastSettings.voltage, 3.0f, 10.0f); // use some way of specifying min and max

		if(lastSettings.voltage != settings.voltage){
			ImGui::SameLine();
			bool bApplyVoltage = false;
			if(ImGui::Button("Apply")){
				if(lastSettings.voltage >= 5.0f){
					bApplyVoltage = false;
					ImGui::OpenPopup("Overvoltage");
				}else{
					bApplyVoltage = true;
				}
			}

			if(lastSettings.voltage >= 5.0f){
				ImGui::SameLine();
				ImGui::Text("WARNING Voltage exceeds 5.0v");
			}

			// Always center this window when appearing
			ImVec2 center = ImGui::GetMainViewport()->GetCenter();
			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

			if(ImGui::BeginPopupModal("Overvoltage", NULL)){
				ImGui::Text("Voltages above 5.0v may damage the device!");
				ImGui::Text("Do you really want to supply %.1f v to device?", lastSettings.voltage);
				ImGui::Separator();
				if (ImGui::Button("Yes", ImVec2(120, 0))) { bApplyVoltage = true; ImGui::CloseCurrentPopup(); }
				ImGui::SetItemDefaultFocus();
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120, 0))) { bApplyVoltage = false; ImGui::CloseCurrentPopup(); }
				ImGui::EndPopup();
			}

			if(bApplyVoltage){
				LOGINFO("Change voltage from %.1f to %.1f", settings.voltage, lastSettings.voltage);
				bApplySettings = true;
			}else{
				bApplySettings = false;  
			}


		}

		ImGui::Separator();
		ImGui::Separator();
		ImGui::PopID();
		//ImGui::End();

	}

	//void gui(){
	//	//LOGDEBUG("FMC GUI");
	//	//ImGui::Begin(deviceName.c_str());
	//	ImGui::PushID(deviceName.c_str());

	//	//getPortVoltage(false);

	//	ImGui::Text(deviceName.c_str());

	//	ImGui::SetNextItemWidth(200);
	//	ImGui::InputFloat("Port Voltage", &lastSettings.voltage, 0.1f, 0.1f, "%.1f"); 


	//	lastSettings.voltage = std::clamp(lastSettings.voltage, 3.0f, 10.0f); // use some way of specifying min and max

	//	if(lastSettings.voltage != settings.voltage){
	//		ImGui::SameLine();
	//		bool bApplyVoltage = true;
	//		if(ImGui::Button("Apply")){
	//			if(lastSettings.voltage >= 5.0f){
	//				bApplyVoltage = false;
	//				ImGui::OpenPopup("Overvoltage");
	//			}else{
	//				bApplyVoltage = true;
	//			}
	//		}

	//		if(lastSettings.voltage >= 5.0f){
	//			ImGui::SameLine();
	//			ImGui::Text("WARNING Voltage exceeds 5.0v");
	//		}

	//		// Always center this window when appearing
	//		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	//		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

	//		if(ImGui::BeginPopupModal("Overvoltage", NULL)){
	//			ImGui::Text("Voltages above 5.0v may damage the device!");
	//			ImGui::Text("Do you really want to supply %.1f v to device?", lastSettings.voltage);
	//			ImGui::Separator();
	//			if (ImGui::Button("Yes", ImVec2(120, 0))) { bApplyVoltage = true; ImGui::CloseCurrentPopup(); }
	//			ImGui::SetItemDefaultFocus();
	//			ImGui::SameLine();
	//			if (ImGui::Button("Cancel", ImVec2(120, 0))) { bApplyVoltage = false;  ImGui::CloseCurrentPopup(); } //getPortVoltage(true);
	//			ImGui::EndPopup();
	//		}

	//		if(bApplyVoltage){
	//			LOGINFO("Change voltage from %.1f to %.1f", settings.voltage, lastSettings.voltage);
	//			//setPortVoltage(gvoltage);
	//			//getPortVoltage(true);
	//		}else{
	//			LOGINFO("Cancel change voltage from %.1f to %.1f", settings.voltage, lastSettings.voltage);
	//			lastSettings.voltage = settings.voltage;
	//		}

	//	}

	//	ImGui::Separator();
	//	ImGui::Separator();
	//	ImGui::PopID();
	//	//ImGui::End();
	//	
	//}

	bool save(std::string filePath){
		return true;
	}

	bool load(std::string filePath){
		return true;
	}

private:



};
