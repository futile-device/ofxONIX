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

enum ShuttleCommand{
	OPEN,
	PLAY,
	STOP,
	RECORD,
	NONE
};

class RecordInterface : public ONI::Interface::BaseInterface{

public:

	RecordInterface(){};
	~RecordInterface(){};

	void reset(){};
	inline void process(oni_frame_t* frame){}; // nothing
	inline void process(ONI::Frame::BaseFrame& frame){}; // nothing


	inline void gui(ONI::Processor::BaseProcessor& processor){
	
		ONI::Processor::RecordProcessor& rp = *reinterpret_cast<ONI::Processor::RecordProcessor*>(&processor);

		if(bFirstLoad){
			listFiles(rp);
			rp.getStreamNamesFromFolder(folders[fileIDX]);
			std::string info = rp.getInfoFromFolder(folders[fileIDX]);
			std::sprintf(descriptionReadBuf, "%s", info.c_str());
			flashTimer.start<fu::millis>(750);
			bFirstLoad = false;
		}

		if(flashTimer.finished()) flashTimer.restart();

		ImGui::PushID(rp.getName().c_str());
		ImGui::Begin(rp.getName().c_str());

		ImGui::PushFont(fu::IconsSymbol);
		//ImGuiStyle& style = ImGui::GetStyle();
		//float avail = ImGui::GetContentRegionAvail().x;
		//ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 300);

		if(openButton()) open();
		ImGui::SameLine();
		if(playButton()) play();
		ImGui::SameLine();
		if(recordButton()) record();
		ImGui::SameLine();
		if(stopButton()) stop();
		ImGui::SameLine();
		ImGui::Text("  [%s]", rp.getCurrentTimeStamp().c_str()); 
		ImGui::SameLine();
		ImGui::Text("   ||   %s", rp.settings.timeStamp.c_str());
		ImGui::SameLine();
		ImGui::Text("   ||   [%s]", rp.settings.fileLengthTimeStamp.c_str());
		ImGui::NewLine();
		ImGui::PopFont();

		

		switch(nextCommand)
		{
		case ONI::Interface::PLAY:
		{
			rp.play();
			nextCommand = ONI::Interface::NONE;
			break;
		}
		
		case ONI::Interface::RECORD:
		{
			ImGui::OpenPopup("Record Description");
			memset(descriptionWriteBuf, 0, 512);
			break;
		}
		//case ONI::Interface::PAUSE:
		//{
		//	rp.pause();
		//	nextCommand = ONI::Interface::NONE;
		//	break;
		//}
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
		ImGui::SetNextWindowSize(ImVec2(250, 200));
		if(ImGui::BeginPopupModal("Record Description", NULL)){
			nextCommand = ShuttleCommand::NONE;
			
			ImGui::SetNextItemWidth(200);
			ImGui::InputTextMultiline("##write", descriptionWriteBuf, 1024);

			ImGui::NewLine();
			ImGui::SetItemDefaultFocus();
			if(ImGui::Button("Start", ImVec2(120, 0))) { 
				rp.settings.description = std::string(descriptionWriteBuf); 
				//memset(descriptionWriteBuf, 0, 1024);
				rp.record(); 
				listFiles(rp);
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();
			if(ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}

		ImGui::SetNextWindowSize(ImVec2(400, 490));
		if(ImGui::BeginPopupModal("Select File", NULL)){

			nextCommand = ShuttleCommand::NONE;

			
			ImGui::SetNextItemWidth(200);
			ImGuiID lastItemClicked = 0;
			if(ImFui::ListBox("##f", &fileIDX, files, 25)){
				std::string info = rp.getInfoFromFolder(folders[fileIDX]);
				std::sprintf(descriptionReadBuf, "%s", info.c_str());
				lastItemClicked = ImGui::GetHoveredID();	// store the selected item ID to confirm double click is hovered over the same item
			}

			static bool bDoubleClicked = false;
			if(ImGui::IsMouseDoubleClicked(0)) bDoubleClicked = true;  // check if we are double clicked NB: seems like click is mouse up, so we cache the state till next pass...

			if(bDoubleClicked && ImGui::IsItemHovered() && lastItemClicked == ImGui::GetHoveredID()){ // ... and as long as the last pass remains hovered on the same item, lets double click. yawn
				bDoubleClicked = false;
				rp.getStreamNamesFromFolder(folders[fileIDX]);
				rp.play();
				ImGui::CloseCurrentPopup();
			}

			

			ImGui::SameLine();

			ImGui::SetNextItemWidth(180);
			// TODO: make info/description editable
			ImGui::InputTextMultiline("##read", descriptionReadBuf, 1024, ImVec2(180, 29 * ImGui::GetFontSize()), ImGuiInputTextFlags_ReadOnly);

			ImGui::NewLine();
			ImGui::SetItemDefaultFocus();

			if(ImGui::Button("Play File", ImVec2(120, 0))) {
				rp.getStreamNamesFromFolder(folders[fileIDX]);
				rp.play(); 
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine();

			if(ImGui::Button("Refresh List", ImVec2(120, 0))) {
				listFiles(rp);
			}
			ImGui::SameLine();
			if(ImGui::Button("Export Audio", ImVec2(120, 0))) {
				rp.getStreamNamesFromFolder(folders[fileIDX]);
				rp.exportToAudio();
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();
			if(ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}
		ImGui::End();
		ImGui::PopID();

	};

	bool save(std::string presetName){
		return false;
	}

	bool load(std::string presetName){
		return false;
	}

	void record(){
		nextCommand = ShuttleCommand::RECORD;
	}

	void play(){
		nextCommand = ShuttleCommand::PLAY;
	}

	//void pause(){
	//	nextCommand = Command::PAUSE;
	//}

	void stop(){
		nextCommand = ShuttleCommand::STOP;
	}

	void open(){
		nextCommand = ShuttleCommand::OPEN;
	}

private:

	inline bool openButton(){

		ImGui::PushID(ICON_MD_FOLDER_OPEN);
		ImGuiStyle& style = ImGui::GetStyle();
		if(nextCommand == ShuttleCommand::OPEN) ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);

		bool bPressed = ImGui::Button(ICON_MD_FOLDER_OPEN, buttonSize);

		if(nextCommand == ShuttleCommand::OPEN) ImGui::PopStyleColor(1);
		ImGui::PopID();
		return bPressed;
	}

	inline bool playButton(){

		ImGui::PushID(ICON_MD_PLAY_CIRCLE);
		ImGuiStyle& style = ImGui::GetStyle();

		if(ONI::Global::model.getRecordProcessor()->isPlaying()){
			ImVec4 mod = style.Colors[ImGuiCol_Button];
			mod.y += 2 * (1 - flashTimer.easePct(fu::EASE_INOUTEXPO));
			ImGui::PushStyleColor(ImGuiCol_Button, mod);
		} else if(nextCommand == ShuttleCommand::PLAY){
			ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
		} else{
			ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_Button]);
		}

		bool bPressed = ImGui::Button(ICON_MD_PLAY_CIRCLE, buttonSize);

		ImGui::PopStyleColor();
		ImGui::PopID();
		return bPressed;
	}

	inline bool recordButton(){

		ImGui::PushID(ICON_MD_RADIO_BUTTON_CHECKED);
		ImGuiStyle& style = ImGui::GetStyle();

		if(ONI::Global::model.getRecordProcessor()->isRecording()){
			ImVec4 mod = style.Colors[ImGuiCol_Button];
			mod.x += 2 * (1 - flashTimer.easePct(fu::EASE_INOUTEXPO));
			ImGui::PushStyleColor(ImGuiCol_Button, mod);
		} else if(nextCommand == ShuttleCommand::RECORD){
			ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
		} else{
			ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_Button]);
		}

		bool bPressed = ImGui::Button(ICON_MD_RADIO_BUTTON_CHECKED, buttonSize);

		ImGui::PopStyleColor(1);
		ImGui::PopID();
		return bPressed;
	}

	inline bool stopButton(){
		ImGui::PushID(ICON_MD_STOP_CIRCLE);
		ImGuiStyle& style = ImGui::GetStyle();
		if(nextCommand == ShuttleCommand::STOP) ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
		bool bPressed = ImGui::Button(ICON_MD_STOP_CIRCLE, buttonSize);
		if(nextCommand == ShuttleCommand::STOP) ImGui::PopStyleColor(1);
		ImGui::PopID();
		return bPressed;
	}

	bool listFiles(ONI::Processor::RecordProcessor& rp){
		folders.clear();
		//if(fileIDX == -1) fileIDX = 0;
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
		std::string info = rp.getInfoFromFolder(folders[fileIDX]);
		std::sprintf(descriptionReadBuf, "%s", info.c_str());
		return true;
	}

protected:

	constexpr ImVec2 buttonSize = ImVec2(64, 60);
	fu::Timer flashTimer;
	
	char descriptionReadBuf[4096];
	char descriptionWriteBuf[512];

	int fileIDX = 0;
	std::vector<std::string> folders;
	std::vector<std::string> files;

	ShuttleCommand nextCommand = NONE;
	bool bFirstLoad = true;

};

} // namespace Interface
} // namespace ONI