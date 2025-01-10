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
#include "../Interface/PlotHelpers.h"

#include "ofxImGui.h"
#include "ofxImPlot.h"
#include "ofxFutilities.h"

#pragma once

namespace ONI{
namespace Interface{

class Rhs2116StimulusInterface : public ONI::Interface::BaseInterface{

public:

	~Rhs2116StimulusInterface(){

	};

	inline void process(oni_frame_t* frame){}; // nothing
	inline void process(ONI::Frame::BaseFrame& frame){};// nothing

	inline void refreshStimuliData(ONI::Device::Rhs2116StimulusDevice& std){

		bool bResetUploadedStimuli = std.isChannelMapChanged();

		stimuli = std.getProbeStimuliMapped();
		maxStimulusLength = std.getMaxLengthSamples(stimuli);
		allStimulusAmplitudes = std.getAllStimulusAmplitudes(stimuli);

		maxMicroAmps = -INFINITY;
		minMicroAmps = INFINITY;

		// conform all amplitude plots to the same length
		for(size_t i = 0; i < stimuli.size(); ++i){
			if(-stimuli[i].actualAnodicAmplitudeMicroAmps < minMicroAmps) minMicroAmps = -stimuli[i].actualAnodicAmplitudeMicroAmps;
			if(stimuli[i].actualCathodicAmplitudeMicroAmps > maxMicroAmps) maxMicroAmps = stimuli[i].actualCathodicAmplitudeMicroAmps;
			
			size_t sampleDiff = maxStimulusLength - allStimulusAmplitudes[i].size();
			for(size_t j = 0; j < sampleDiff; ++j) allStimulusAmplitudes[i].push_back(0);
		}

		allTimeStamps.resize(maxStimulusLength);
		for(size_t j = 0; j < allTimeStamps.size(); ++j) allTimeStamps[j] = j / 30.1932367151e3 * 1000.0f;

		currentStimulusStepSize = std.conformStepSize(currentStimulus);
		currentStimulusStepSizeMicroAmps = ONI::Settings::Rhs2116StimulusStepMicroAmps[currentStimulusStepSize];
		currentStimulusAmplitudes = std.getStimulusAmplitudes(currentStimulus);

		currentTimeStamps.resize(currentStimulusAmplitudes.size());
		for(size_t j = 0; j < currentTimeStamps.size(); ++j) currentTimeStamps[j] = j / 30.1932367151e3 * 1000.0f;

		// SURELY THERE IS A NICER WAY TO DO THIS!!!
		for(size_t i = 0; i < probeDropDownChar.size(); ++i){
			delete [] probeDropDownChar[i];
		}

		probeDropDownChar.clear();
		probeDropDownChar.resize(std.getNumProbes());
		for(size_t i = 0; i < std.getNumProbes(); ++i){
			probeDropDownChar[i] = new char[16];
			std::sprintf(probeDropDownChar[i], "Probe %02i", i);
		}

		if(bResetUploadedStimuli){
			LOGDEBUG("Updating the stimuli upload cos channel map changed -->> DO THIS BETTER WITH EVENT LISTENER");
			ONI::Settings::Rhs2116StimulusStep  stepSize = std.conformStepSize(stimuli);
			std.setStimulusSequence(stimuli, stepSize);
		}

	}
	
	
	inline void gui(ONI::Processor::BaseProcessor& processor){

		ONI::Device::Rhs2116StimulusDevice& std = *reinterpret_cast<ONI::Device::Rhs2116StimulusDevice*>(&processor);

		static bool bUseBurstFrequency = false;
		static float burstFrequency = 1;
		static float burstDurationMs = 1;

		static bool bDeleted = false;
		static bool bSelectAll = false;
		bool bLastSelectAll = bSelectAll;
		
		char buf[64];

		//ImGui::Begin(std.getName().c_str());
		ImGui::PushID(std.getName().c_str());
		ImGui::Text(std.getName().c_str());


		if(stimuli.size() == 0 || std.isChannelMapChanged()){ // ...then this is the first time we are drawing the gui
			refreshStimuliData(std);
		}

		/////////////////////////////////////////////////////////////////////
		///
		/// TRIGGER SEQUENCE
		/// 
		/////////////////////////////////////////////////////////////////////

		
		ImGui::NewLine();

		//ImGui::Text("Stimulus Trigger");

		if(ImGui::Button("Trigger Stimulus")){
			std.triggerStimulusSequence();
		}

		/////////////////////////////////////////////////////////////////////
		///
		/// PLOTTING OPTIONS
		/// 
		/////////////////////////////////////////////////////////////////////

		ImGui::NewLine();
		ImGui::Separator();
		ImGui::NewLine();

		ImGui::Text("Stimulus Plot Options");

		ImGui::NewLine();

		static bool bPlotTimes = true;
		ImGui::Checkbox("Plot Times", &bPlotTimes); ImGui::SameLine();

		static bool bShowOnlySetStimuli = true;
		ImGui::Checkbox("Plot Only Set Stimuli", &bShowOnlySetStimuli); ImGui::SameLine();

		static bool bPlotRelativeuA = true;
		ImGui::Checkbox("Plot Relative uA", &bPlotRelativeuA); //ImGui::SameLine();

		/////////////////////////////////////////////////////////////////////
		///
		/// SELECT PROBE TO LOAD OR CLEAR
		/// 
		/////////////////////////////////////////////////////////////////////

		ImGui::NewLine();
		ImGui::Separator();
		ImGui::NewLine();

		ImGui::Text("Stimulus Load/Delete");
		ImGui::NewLine();


		ImGui::SetNextItemWidth(200);
		static int probeItemIDX = 0; //format.dspEnable == 1 ? format.dspCutoff : Rhs2116DspCutoff::Off;
		ImGui::Combo("##", &probeItemIDX, &probeDropDownChar[0], probeDropDownChar.size(), 5);

		if(stimuli[probeItemIDX].numberOfStimuli == 0)  ImGui::BeginDisabled();
		ImGui::SameLine();
		if(ImGui::Button("Load")){
			lastStimulus = std.defaultStimulus;
			currentStimulus = stimuli[probeItemIDX];
			bLastSelectAll = bSelectAll = false;
			for(size_t i = 0; i < channelSelect.size(); ++i) channelSelect[i] = false;
			channelSelect[probeItemIDX] = true;
			bUseBurstFrequency = false;
		}

		static std::vector<size_t> toDeleteIDX;

		ImGui::SameLine();
		if(ImGui::Button("Delete")){ // let's load it just in case?
			lastStimulus = std.defaultStimulus;
			currentStimulus = stimuli[probeItemIDX];
			bLastSelectAll = bSelectAll = false;
			for(size_t i = 0; i < channelSelect.size(); ++i) channelSelect[i] = false;
			channelSelect[probeItemIDX] = true;
			toDeleteIDX.push_back(probeItemIDX);
		}

		if(stimuli[probeItemIDX].numberOfStimuli == 0)  ImGui::EndDisabled();

		ImGui::SameLine();
		if(ImGui::Button("Delete All")){ // let's load it just in case?
			lastStimulus = std.defaultStimulus;
			currentStimulus = stimuli[probeItemIDX];
			bLastSelectAll = bSelectAll = false;
			for(size_t i = 0; i < channelSelect.size(); ++i) channelSelect[i] = false;
			channelSelect[probeItemIDX] = true;
			for(size_t i = 0; i < stimuli.size(); ++i) if(stimuli[i].numberOfStimuli > 0) toDeleteIDX.push_back(i);
		}

		if(toDeleteIDX.size() > 0){
			ImGui::OpenPopup("Confirm Delete");
		}

		/////////////////////////////////////////////////////////////////////
		///
		/// CONFIRM DELETE POP UP
		/// 
		/////////////////////////////////////////////////////////////////////

		bool unused0 = true;
		if(ImGui::BeginPopupModal("Confirm Delete", &unused0, ImGuiWindowFlags_AlwaysAutoResize)){
			std::ostringstream os;
			os << "Delete Probe" << (toDeleteIDX.size() > 1 ? "s: " : ": ");
			for(size_t i = 0; i < toDeleteIDX.size(); ++i) os << toDeleteIDX[i] << " ";
			os << "?";
			ImGui::Text(os.str().c_str());
			ImGui::Separator();
			if (ImGui::Button("Cancel", ImVec2(120, 0))){ 
				toDeleteIDX.clear();
				ImGui::CloseCurrentPopup();
			} 
			ImGui::SameLine();
			if (ImGui::Button("OK", ImVec2(120, 0))){ 
				for(size_t i = 0; i < toDeleteIDX.size(); ++i) std.deleteProbeStimulus(toDeleteIDX[i]);
				toDeleteIDX.clear();
				refreshStimuliData(std);
				bDeleted = true;
				bUseBurstFrequency = false;
				ImGui::CloseCurrentPopup(); 
			} 
			ImGui::EndPopup();
		}

		ImGui::NewLine();
		ImGui::Separator();
		ImGui::NewLine();

		/////////////////////////////////////////////////////////////////////
		///
		/// STMULUS EDITOR
		/// 
		/////////////////////////////////////////////////////////////////////

		ImGui::Text("Stimulus Editor");
		ImGui::NewLine();

		if(lastStimulus != currentStimulus){ // ... then this stimulus has been edited
			refreshStimuliData(std);
			lastStimulus = currentStimulus;
		}

		//float anodicAmplitudeMicroAmps = currentStimulus.actualAnodicAmplitudeMicroAmps / currentStimulusStepSizeMicroAmps;
		//float cathodicAmplitudeMicroAmps = currentStimulus.actualCathodicAmplitudeMicroAmps / currentStimulusStepSizeMicroAmps;
		float delaySamplesMs = currentStimulus.delaySamples / std.sampleFrequencyHz * 1000.0f;
		float anodicWidthSamplesMs = currentStimulus.anodicWidthSamples / std.sampleFrequencyHz * 1000.0f;
		float cathodicWidthSamplesMs = currentStimulus.cathodicWidthSamples / std.sampleFrequencyHz * 1000.0f;
		float dwellSamplesMs = currentStimulus.dwellSamples / std.sampleFrequencyHz * 1000.0f;
		float interStimulusIntervalSamplesMs = currentStimulus.interStimulusIntervalSamples / std.sampleFrequencyHz * 1000.0f;
		int numberOfStimuli = currentStimulus.numberOfStimuli;



		ImGui::Checkbox("Burst Frequency", &bUseBurstFrequency); ImGui::SameLine();
		if(bUseBurstFrequency){
			ImGui::BeginDisabled();
			currentStimulus.biphasic = true;
		}
		ImGui::Checkbox("Biphasic", &currentStimulus.biphasic); ImGui::SameLine();
		if(bUseBurstFrequency) ImGui::EndDisabled();
		ImGui::Checkbox("Anodic First", &currentStimulus.anodicFirst); //ImGui::SameLine();
		

		ImGui::PushItemWidth(200);
		ImGui::InputFloat("Delay Time (ms)", &delaySamplesMs, 0.03f, 1.0f);

		if(!currentStimulus.biphasic){

			ImGui::InputFloat("Anodic Time (ms)", &anodicWidthSamplesMs, 0.03f, 1.0f);
			ImGui::InputFloat("Cathodic Time (ms)", &cathodicWidthSamplesMs, 0.03f, 1.0f);

		}else{

			if(bUseBurstFrequency){
				ImGui::InputFloat("Burst Frequency (Hz)", &burstFrequency, 0.1f, 1.0f); 
				burstFrequency = std::clamp(burstFrequency, 0.1f, 1000.0f);
				ImGui::InputFloat("Burst Time (ms)", &burstDurationMs, 0.03f, 1.0f); 
				anodicWidthSamplesMs = 1000.0 / burstFrequency / 2.0f;
			}else{
				ImGui::InputFloat("Pulse Time (ms)", &anodicWidthSamplesMs, 0.03f, 1.0f); 
			}
			
			ImGui::BeginDisabled();
			ImGui::InputFloat("Cathodic Time (ms)", &cathodicWidthSamplesMs, 0.03f, 1.0f); 
			ImGui::EndDisabled();
			cathodicWidthSamplesMs = anodicWidthSamplesMs;
		}

		ImGui::InputFloat("Dwell Time (ms)", &dwellSamplesMs, 0.03f, 1.0f);
		ImGui::InputFloat("Inter Stimulus Time (ms)", &interStimulusIntervalSamplesMs, 0.03f, 1.0f);

		if(bUseBurstFrequency){
			ImGui::BeginDisabled();
			numberOfStimuli = std::clamp((int)std::floor(burstDurationMs / anodicWidthSamplesMs / 2.0f), 1, 256);
		}

		ImGui::InputInt("Number of Stimuli", &numberOfStimuli, 1, 512);
		if(bUseBurstFrequency) ImGui::EndDisabled();

		currentStimulus.delaySamples = std::round(delaySamplesMs * std.sampleFrequencyHz / 1000.0f);
		currentStimulus.anodicWidthSamples = std::round(anodicWidthSamplesMs * std.sampleFrequencyHz / 1000.0f);
		currentStimulus.cathodicWidthSamples = std::round(cathodicWidthSamplesMs * std.sampleFrequencyHz / 1000.0f);
		currentStimulus.dwellSamples = std::round(dwellSamplesMs * std.sampleFrequencyHz / 1000.0f);
		currentStimulus.interStimulusIntervalSamples = std::round(interStimulusIntervalSamplesMs * std.sampleFrequencyHz / 1000.0f);
		currentStimulus.numberOfStimuli = std::clamp(numberOfStimuli, 1, 256);

		//if(currentStimulus.delaySamples == 0) currentStimulus.delaySamples = 1;
		if(currentStimulus.anodicAmplitudeSteps == 0) currentStimulus.anodicAmplitudeSteps = 1;
		if(currentStimulus.cathodicAmplitudeSteps == 0) currentStimulus.cathodicAmplitudeSteps = 1;
		if(currentStimulus.dwellSamples == 0) currentStimulus.dwellSamples = 1;
		if(currentStimulus.interStimulusIntervalSamples == 0) currentStimulus.interStimulusIntervalSamples = 1;

		ImGui::Text("Step Size (uA) %0.3f", currentStimulusStepSizeMicroAmps);

		if(!currentStimulus.biphasic){
			ImGui::InputFloat("Requested Anodic Amplitude (uA)", &currentStimulus.requestedAnodicAmplitudeMicroAmps, 0.01f, 1.0f); //ImGui::SameLine();
			ImGui::Text("Actual Amplitude (uA) %0.3f", currentStimulus.actualAnodicAmplitudeMicroAmps);
			ImGui::InputFloat("Requested Cathodic Amplitude (uA)", &currentStimulus.requestedCathodicAmplitudeMicroAmps, 0.01f, 1.0f); //ImGui::SameLine();
			ImGui::Text("Actual Amplitude (uA) %0.3f", currentStimulus.actualCathodicAmplitudeMicroAmps);
		}else{
			ImGui::InputFloat("Requested Pulse Amplitude (uA)", &currentStimulus.requestedAnodicAmplitudeMicroAmps, 0.01f, 1.0f);
			currentStimulus.requestedCathodicAmplitudeMicroAmps = currentStimulus.requestedAnodicAmplitudeMicroAmps;
			ImGui::Text("Actual Amplitude (uA) %0.3f", currentStimulus.actualAnodicAmplitudeMicroAmps);
			ImGui::BeginDisabled();
			ImGui::InputFloat("Requested Cathodic Amplitude (uA)", &currentStimulus.requestedCathodicAmplitudeMicroAmps, 0.01f, 1.0f); //ImGui::SameLine();
			ImGui::Text("Requested Actual Amplitude (uA) %0.3f", currentStimulus.actualCathodicAmplitudeMicroAmps);
			ImGui::EndDisabled();
		}

		currentStimulus.requestedAnodicAmplitudeMicroAmps = std::clamp(currentStimulus.requestedAnodicAmplitudeMicroAmps, 0.01f, 2550.0f);
		currentStimulus.requestedCathodicAmplitudeMicroAmps = std::clamp(currentStimulus.requestedCathodicAmplitudeMicroAmps, 0.01f, 2550.0f);

		ImGui::PopItemWidth();

		ImGui::NewLine();
		if(ImGui::Button("Reset Stimulus Editor")){
			currentStimulus = std.defaultStimulus;
		}

		/////////////////////////////////////////////////////////////////////
		///
		/// SELECTION OF PROBES TO APPLY STIMULUS
		/// 
		/////////////////////////////////////////////////////////////////////

		ImGui::NewLine();
		ImGui::Separator();
		ImGui::NewLine();

		ImGui::Text("Select Probes to Apply Stimulus");
		ImGui::NewLine();

		size_t selectCount = 0;

		ImGui::Checkbox("Select All", &bSelectAll); //ImGui::SameLine();

		if(bLastSelectAll != bSelectAll && bSelectAll) for(size_t i = 0; i < channelSelect.size(); ++i) channelSelect[i] = true;
		if(bLastSelectAll != bSelectAll && !bSelectAll) for(size_t i = 0; i < channelSelect.size(); ++i) channelSelect[i] = false;

		if(channelSelect.size() == 0){
			channelSelect.resize(stimuli.size());
			for(size_t i = 0; i < stimuli.size(); ++i) channelSelect[i] = false;
		}

		for(size_t i = 0; i < stimuli.size(); ++i){
			if(i != 0 && i % 8 != 0) ImGui::SameLine();
			std::sprintf(buf, "%02i", i);
			bool b = channelSelect[i];
			ImGui::Checkbox(buf, &b);
			channelSelect[i] = b;
			if(bSelectAll && !b) bSelectAll = false;
			if(b) ++selectCount;
		}

		if(selectCount == stimuli.size()) bSelectAll = true;



		/////////////////////////////////////////////////////////////////////
		///
		/// APPLY LOGIC
		/// 
		/////////////////////////////////////////////////////////////////////


		bool bStimuliNeedConfirmation = false;
		bool bApplyStimulus = false;

		ONI::Settings::Rhs2116StimulusStep stagedStepSize = ONI::Settings::Rhs2116StimulusStep::Step10nA;

		ImGui::NewLine();

		if(ImGui::Button("Apply Stimulus to Selected") || bDeleted){

			stagedStimuli = stimuli;

			size_t stepCount = 0;
			for(size_t i = 0; i < stimuli.size(); ++i){
				//size_t channelMappedIDX = std.multi->getChannelMapIDX()[i]; // make sure we set things to the mapped IDX!!
				if(channelSelect[i] && !bDeleted){
					stagedStimuli[i] = currentStimulus;
				}
				stepCount += stagedStimuli[i].numberOfStimuli; // count the number of stimuli
			}

			bDeleted = false;

			if(stepCount <= 255){ // CHECK IF NUMBER OF STIMULI FIT INTO RAM

				// IF THEY DO, CHECK IF WE NEED TO MODIFY STEP SIZES ACROSS STUMULI

				stagedStepSize = std.conformStepSize(stagedStimuli);

				sequenceMessage = "The following stimuli will be changed:\n";

				for(size_t i = 0; i < stagedStimuli.size(); ++i){

					if(stagedStimuli[i].requestedAnodicAmplitudeMicroAmps != stagedStimuli[i].actualAnodicAmplitudeMicroAmps){

						bStimuliNeedConfirmation = true;

						char buf[256];
						std::sprintf(buf, "Probe %i anodic uA will change from %0.3f to %0.3f: \n", i, stagedStimuli[i].requestedAnodicAmplitudeMicroAmps, stagedStimuli[i].actualAnodicAmplitudeMicroAmps);
						sequenceMessage.append(buf);
						std::sprintf(buf, "Probe %i cathod uA will change from %0.3f to %0.3f: \n", i, stagedStimuli[i].requestedCathodicAmplitudeMicroAmps, stagedStimuli[i].actualCathodicAmplitudeMicroAmps);
						sequenceMessage.append(buf);
						LOGALERT("Probe %i anodic uA will change from %0.3f to %0.3f: ", i, stagedStimuli[i].requestedAnodicAmplitudeMicroAmps, stagedStimuli[i].actualAnodicAmplitudeMicroAmps);
						LOGALERT("Probe %i cathod uA will change from %0.3f to %0.3f: ", i, stagedStimuli[i].requestedCathodicAmplitudeMicroAmps, stagedStimuli[i].actualCathodicAmplitudeMicroAmps);
					
					}
				}

				if(bStimuliNeedConfirmation){ // STIMULI STEP SIZES WILL CHANGE SO LETS ASK WITH A POPUP
					ImGui::OpenPopup("Apply Sequence");
					bApplyStimulus = false;
				}else{
					bApplyStimulus = true;
				}

			}else{

				// NUMBER OF STIMULI WON'T FIT INTO RAM!!
				std::ostringstream os; 
				os << "Stimulus does not fit into memory " << currentStimulus.numberOfStimuli << " + " << stepCount - currentStimulus.numberOfStimuli << " >= 255 ==> " << stepCount << std::endl;
				os << "Try reducing number of stimuli to: " << 255 - stepCount - currentStimulus.numberOfStimuli;
				sequenceMessage = os.str();
				ImGui::OpenPopup("Error Sequence");
				bApplyStimulus = false;

			}

		}

		ImGui::NewLine();

		/////////////////////////////////////////////////////////////////////
		///
		/// POP UP TO NOTIFY THAT THE NUMBER OF STIMULI EXCEEDS RAM
		/// 
		/////////////////////////////////////////////////////////////////////

		bool unused1 = true;
		if(ImGui::BeginPopupModal("Error Sequence", &unused1, ImGuiWindowFlags_AlwaysAutoResize)){
			ImGui::Text(sequenceMessage.c_str());
			ImGui::Separator();
			if (ImGui::Button("OK", ImVec2(120, 0))){ 
				ImGui::CloseCurrentPopup();
			} 
		}

		/////////////////////////////////////////////////////////////////////
		///
		/// POP UP TO NOTIFY OF CHANGES IN STEP SIZE FOR STIMULI
		/// 
		/////////////////////////////////////////////////////////////////////

		bool unused2 = true;
		if(ImGui::BeginPopupModal("Apply Sequence", &unused2, ImGuiWindowFlags_AlwaysAutoResize)){
			ImGui::Text(sequenceMessage.c_str());
			ImGui::Separator();
			if (ImGui::Button("Accept", ImVec2(120, 0))){ 
				ImGui::CloseCurrentPopup(); bApplyStimulus = true;
			} 
			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0))){ 
				ImGui::CloseCurrentPopup(); bApplyStimulus = false;

			};
			ImGui::EndPopup();
		}

		/////////////////////////////////////////////////////////////////////
		///
		/// IF EVERYTHING IS OK, ACTUALLY APPLY THE SEQUENCE
		/// 
		/////////////////////////////////////////////////////////////////////

		if(bApplyStimulus){
			stimuli = stagedStimuli;
			std.setStimulusSequence(stimuli, stagedStepSize);
			refreshStimuliData(std);
		}


		/////////////////////////////////////////////////////////////////////
		///
		/// PLOT THE CURRENTLY EDITING STIMULUS
		/// 
		/////////////////////////////////////////////////////////////////////


		ImGui::Begin("Stimulus Preview");
		ImGui::PushID("##StimulusPreviewPlot");

		if(ImPlot::BeginPlot("Stimulus Preview", ImVec2(-1, -1))){

			std::string unitsStr = bPlotTimes ? "mS" : "samples";
			ImPlot::SetupAxes(unitsStr.c_str(), "uA");

			ImGui::PushID(0);
			ImPlot::SetNextLineStyle(ImPlot::GetColormapColor(8));

			int offset = 0;

			float min = bPlotRelativeuA ? -currentStimulus.actualAnodicAmplitudeMicroAmps - 0.01 : std::min(minMicroAmps -  0.01, -currentStimulus.actualAnodicAmplitudeMicroAmps - 0.01);
			float max = bPlotRelativeuA ?  currentStimulus.actualCathodicAmplitudeMicroAmps + 0.01 : std::max(maxMicroAmps + 0.01, currentStimulus.actualCathodicAmplitudeMicroAmps + 0.01);
			
			if(currentStimulusAmplitudes.size() > 0){
				if(bPlotTimes){
					ImPlot::SetupAxesLimits(0, currentTimeStamps[currentTimeStamps.size() - 1], min, max, ImGuiCond_Always);
					ImPlot::PlotLine("##stimulus", &currentTimeStamps[0], &currentStimulusAmplitudes[0], currentStimulusAmplitudes.size(), ImPlotLineFlags_None, offset);

				}else{
					ImPlot::SetupAxesLimits(0, currentStimulusAmplitudes.size(), min, max, ImGuiCond_Always);
					ImPlot::PlotLine("##stimulus", &currentStimulusAmplitudes[0], currentStimulusAmplitudes.size(), 1, 0, ImPlotLineFlags_None, offset);
				}

			}
			ImGui::PopID();

			ImPlot::EndPlot();
		}

		ImGui::PopID();
		ImGui::End();

		/////////////////////////////////////////////////////////////////////
		///
		/// PLOT ALL THE INDIVIDUAL STIMULI
		/// 
		/////////////////////////////////////////////////////////////////////

		//ONI::Interface::Plot::plotIndividualStimulus(maxStimulusLength, allStimulusAmplitudes, stimuli);

		static ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_RowBg;

		static int offset = 0;

		ImGui::Begin("Stimuli");
		ImGui::PushID("##IndividualStimulusPlot");

		if(ImGui::BeginTable("##table", 3, flags, ImVec2(-1,0))){

			ImGui::TableSetupColumn("Electrode", ImGuiTableColumnFlags_WidthFixed, 75.0f);
			ImGui::TableSetupColumn("Stimulus", ImGuiTableColumnFlags_WidthFixed, 75.0f);
			ImGui::TableSetupColumn("AC Signal (uA)");

			ImGui::TableHeadersRow();
			ImPlot::PushColormap(ImPlotColormap_Cool);

			for(size_t i = 0; i < allStimulusAmplitudes.size(); ++i){

				size_t probe_idx = std.multi->getInverseChannelMapIDX()[i]; ///HMMM WTF HEH

				if(bShowOnlySetStimuli && stimuli[probe_idx].numberOfStimuli == 0) continue;

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Probe %d (%d)", i, probe_idx); //std.multi->getChannelMapIDX()[i]
				ImGui::TableSetColumnIndex(1);
				//ImGui::Text("  delay-t %.3f ms", stimuli[i].delaySamples / 30.1932367151e3 * 1000.0f);
				//ImGui::Text("  anode-t %.3f ms", stimuli[i].anodicWidthSamples / 30.1932367151e3 * 1000.0f);
				//ImGui::Text("  dwell-t %.3f ms", stimuli[i].dwellSamples / 30.1932367151e3 * 1000.0f);
				//ImGui::Text("cathode-t %.3f ms", stimuli[i].anodicWidthSamples / 30.1932367151e3 * 1000.0f);
				//ImGui::Text("  inter-t %.3f ms", stimuli[i].interStimulusIntervalSamples / 30.1932367151e3 * 1000.0f);
				//ImGui::Text("   anodic %.3f uA", stimuli[i].actualAnodicAmplitudeMicroAmps);
				//ImGui::Text(" cathodic %.3f uA", stimuli[i].actualCathodicAmplitudeMicroAmps);
				ImGui::TableSetColumnIndex(2);

				ImGui::PushID(i);

				const std::vector<float>& plotAmplitudes = allStimulusAmplitudes[probe_idx];

				float min = bPlotRelativeuA ? -stimuli[probe_idx].actualAnodicAmplitudeMicroAmps - 0.01 : minMicroAmps - 0.01;
				float max = bPlotRelativeuA ?  stimuli[probe_idx].actualCathodicAmplitudeMicroAmps + 0.01 : maxMicroAmps + 0.01;

				if(bPlotTimes){
					ONI::Interface::SparklineTimes("##spark", &plotAmplitudes[0], &allTimeStamps[0], plotAmplitudes.size(), min, max, offset, ImPlot::GetColormapColor(i), ImVec2(-1, 80), true);
				}else{
					ONI::Interface::Sparkline("##spark", &plotAmplitudes[0], plotAmplitudes.size(), min, max, offset, ImPlot::GetColormapColor(i), ImVec2(-1, 80), true);

				}
				//


				ImGui::PopID();

			}

			ImPlot::PopColormap();
			ImGui::EndTable();
		}

		ImGui::PopID();
		ImGui::End();


		ImGui::PopID();
		//ImGui::End();

	}

	bool save(std::string presetName){
		return false;
	}

	bool load(std::string presetName){
		return false;
	}

protected:

	float maxMicroAmps = 0;
	float minMicroAmps = 0;

	std::vector<ONI::Rhs2116StimulusData> stimuli;
	
	std::vector<char*> probeDropDownChar;

	std::vector<std::vector<float>> allStimulusAmplitudes;
	std::vector<float> allTimeStamps;

	size_t maxStimulusLength = 0;

	ONI::Rhs2116StimulusData currentStimulus;
	ONI::Rhs2116StimulusData lastStimulus;

	std::vector<float> currentStimulusAmplitudes;
	std::vector<float> currentTimeStamps;

	ONI::Settings::Rhs2116StimulusStep currentStimulusStepSize = ONI::Settings::Rhs2116StimulusStep::Step10nA;
	float currentStimulusStepSizeMicroAmps = 0.01;

	std::vector<ONI::Rhs2116StimulusData> stagedStimuli;
	std::vector<bool> channelSelect;

	std::string sequenceMessage = "";

	const std::string processorName = "Rhs2116StimulusGui";

};

} // namespace Interface
} // namespace ONI