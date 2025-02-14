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

enum Command{
	PLAY,
	RECORD,
	PAUSE,
	STOP,
	OPEN,
	NONE
};


class RecordInterface : public ONI::Interface::BaseInterface{

public:

	~RecordInterface(){};

	void reset(){};
	inline void process(oni_frame_t* frame){}; // nothing
	inline void process(ONI::Frame::BaseFrame& frame){}; // nothing
	
	inline void gui(ONI::Processor::BaseProcessor& processor){
	
		ONI::Processor::RecordProcessor& rp = *reinterpret_cast<ONI::Processor::RecordProcessor*>(&processor);

		BaseInterface::numProbes = rp.getNumProbes();
		//if(!bIsApplying) nextSettings = rp.settings;

		ImGui::PushID(rp.getName().c_str());
		ImGui::Text(rp.getName().c_str());

		if(ImGui::Button("Open")) open();
		ImGui::SameLine();
		if(ImGui::Button("Play")) play();
		ImGui::SameLine();
		if(ImGui::Button("Record")) record();
		ImGui::SameLine();
		if(ImGui::Button("Stop")) stop();

		ImGui::Text("Record Time: %s", rp.getTime().c_str());

		


		switch(nextCommand)
		{
		case ONI::Interface::PLAY:
		{
			rp.play();
			break;
		}
		
		case ONI::Interface::RECORD:
		{
			ImGui::OpenPopup("Record Description");
			break;
		}
		case ONI::Interface::PAUSE:
		{
			rp.pause();
			nextCommand = ONI::Interface::NONE;
			break;
		}
		case ONI::Interface::STOP:
		{
			rp.stop();
			nextCommand = ONI::Interface::NONE;
			break;
		}
		case ONI::Interface::OPEN:
		{
			if(listFiles(rp)){
				ImGui::OpenPopup("Select File");
			}else{
				nextCommand = ONI::Interface::NONE;
			}
			
			break;
		}
		case ONI::Interface::NONE:
		break;
		default:
		break;
		}

		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f)); // centre pop up
		ImGui::SetNextWindowSize(ImVec2(400, 800));
		if(ImGui::BeginPopupModal("Record Description", NULL)){
			nextCommand = Command::NONE;
			static char description[512];
			ImGui::InputTextMultiline("Description", description, 512);
			ImGui::Separator();
			ImGui::SetItemDefaultFocus();
			if(ImGui::Button("Start", ImVec2(120, 0))) { rp.settings.description = std::string(description); rp.record(); ImGui::CloseCurrentPopup(); }
			ImGui::SameLine();
			if(ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}

		if(ImGui::BeginPopupModal("Select File", NULL)){
			nextCommand = Command::NONE;

			ImGui::SetNextItemWidth(200);
			if(ImFui::ListBox("##", &fileIDX, files)){
				rp.getStreamNamesFromFolder(folders[fileIDX]);
			}
			ImGui::SameLine();
			ImGui::Text("%s", rp.settings.info.c_str());

			ImGui::SetItemDefaultFocus();
			if(fileIDX == -1) ImGui::BeginDisabled();
			if(ImGui::Button("Open", ImVec2(120, 0))) { 
				rp.play();
				ImGui::CloseCurrentPopup();
			}
			if(fileIDX == -1) ImGui::EndDisabled();
			ImGui::SameLine();
			if(ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}

		ImGui::PopID();

	};

	bool save(std::string presetName){
		return false;
	}

	bool load(std::string presetName){
		return false;
	}

	void record(){
		nextCommand = Command::RECORD;
	}

	void play(){
		nextCommand = Command::PLAY;
	}

	void pause(){
		nextCommand = Command::PAUSE;
	}

	void stop(){
		nextCommand = Command::STOP;
	}

	void open(){
		nextCommand = Command::OPEN;
	}

private:



	bool listFiles(ONI::Processor::RecordProcessor& rp){
		folders.clear();
		fileIDX = -1;
		folders = rp.getAllRecordingFolders();
		if(folders.size() == 0){
			LOGERROR("No recording files");
			return false;
		}
		files.clear();
		for(const std::string& path : folders){
			std::string exp = "experiment_";
			files.push_back(path.substr(path.find(exp) + exp.size()));
		}
		return true;
	}

protected:

	int fileIDX = -1;
	std::vector<std::string> folders;
	std::vector<std::string> files;
	
	Command nextCommand = NONE;

};

} // namespace Interface
} // namespace ONI