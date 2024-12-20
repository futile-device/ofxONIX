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
#include "../Interface/PlotInterface.h"

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

	inline void gui(ONI::Device::BaseDevice& device){

		ONI::Device::Rhs2116StimulusDevice& std = *reinterpret_cast<ONI::Device::Rhs2116StimulusDevice*>(&device);

		//std.subscribeProcessor(processorName, FrameProcessorType::POST_FRAME_PROCESSOR, this);

		ImGui::PushID(std.getName().c_str());
		ImGui::Text(std.getName().c_str());

		if(ImGui::Button("Stimulus Editor")){
			ImGui::OpenPopup("Stimulus Editor");
		}

		ImGui::SameLine();

		if(ImGui::Button("Trigger Stimulus")){
			std.triggerStimulusSequence();
		}

		bool unused = true;
		bool bStimulusNeedsUpdate = false;
		if(ImGui::BeginPopupModal("Stimulus Editor", &unused, ImGuiWindowFlags_AlwaysAutoResize)){

			std::vector<ONI::Rhs2116StimulusData> stimuli = std.getProbeStimuliMapped();

			// SELECTION OF PROBES

			static bool bSelectAll = false;
			bool bLastSelectAll = bSelectAll;

			ImGui::Checkbox("Select All", &bSelectAll);
			if(bLastSelectAll != bSelectAll && bSelectAll) for(size_t i = 0; i < channelSelect.size(); ++i) channelSelect[i] = true;
			if(bLastSelectAll != bSelectAll && !bSelectAll) for(size_t i = 0; i < channelSelect.size(); ++i) channelSelect[i] = false;

			if(channelSelect.size() == 0){
				channelSelect.resize(stimuli.size());
				for(size_t i = 0; i < stimuli.size(); ++i) channelSelect[i] = false;
			}

			size_t lastSelected = -1;
			size_t selectCount = 0;
			for(size_t i = 0; i < stimuli.size(); ++i){
				if(i != 0 && i % 16 != 0) ImGui::SameLine();
				char buf[64];
				std::sprintf(buf, "Probe #%03i", i);
				bool b = channelSelect[i];
				ImGui::Checkbox(buf, &b);
				channelSelect[i] = b;
				if(bSelectAll && !b) bSelectAll = false;
				if(b){
					lastSelected = i;
					++selectCount;
				}
			}

			if(selectCount == stimuli.size()) bSelectAll = true;



			// EDITING STIMULUS

			float currentStimulusStepSizeMicroAmps = ONI::Settings::Rhs2116StimulusStepMicroAmps[std.conformStepSize(currentStimulus)];
			//float anodicAmplitudeMicroAmps = currentStimulus.actualAnodicAmplitudeMicroAmps / currentStimulusStepSizeMicroAmps;
			//float cathodicAmplitudeMicroAmps = currentStimulus.actualCathodicAmplitudeMicroAmps / currentStimulusStepSizeMicroAmps;
			float delaySamplesMs = currentStimulus.delaySamples / std.sampleFrequencyHz * 1000.0f;
			float anodicWidthSamplesMs = currentStimulus.anodicWidthSamples / std.sampleFrequencyHz * 1000.0f;
			float cathodicWidthSamplesMs = currentStimulus.cathodicWidthSamples / std.sampleFrequencyHz * 1000.0f;
			float dwellSamplesMs = currentStimulus.dwellSamples / std.sampleFrequencyHz * 1000.0f;
			float interStimulusIntervalSamplesMs = currentStimulus.interStimulusIntervalSamples / std.sampleFrequencyHz * 1000.0f;
			int numberOfStimuli = currentStimulus.numberOfStimuli;

			ImGui::InputFloat("Delay Time (ms)", &delaySamplesMs, 0.03f, 1.0f);
			ImGui::InputFloat("Anodic Time (ms)", &anodicWidthSamplesMs, 0.03f, 1.0f);
			ImGui::InputFloat("Dwell Time (ms)", &dwellSamplesMs, 0.03f, 1.0f);
			ImGui::InputFloat("Cathodic Time (ms)", &cathodicWidthSamplesMs, 0.03f, 1.0f);
			ImGui::InputFloat("Inter Stimulus Time (ms)", &interStimulusIntervalSamplesMs, 0.03f, 1.0f);

			currentStimulus.delaySamples = std::round(delaySamplesMs * std.sampleFrequencyHz / 1000.0f);
			currentStimulus.anodicWidthSamples = std::round(anodicWidthSamplesMs * std.sampleFrequencyHz / 1000.0f);
			currentStimulus.cathodicWidthSamples = std::round(cathodicWidthSamplesMs * std.sampleFrequencyHz / 1000.0f);
			currentStimulus.dwellSamples = std::round(dwellSamplesMs * std.sampleFrequencyHz / 1000.0f);
			currentStimulus.interStimulusIntervalSamples = std::round(interStimulusIntervalSamplesMs * std.sampleFrequencyHz / 1000.0f);

			//if(currentStimulus.delaySamples == 0) currentStimulus.delaySamples = 1;
			if(currentStimulus.anodicAmplitudeSteps == 0) currentStimulus.anodicAmplitudeSteps = 1;
			if(currentStimulus.cathodicAmplitudeSteps == 0) currentStimulus.cathodicAmplitudeSteps = 1;
			if(currentStimulus.dwellSamples == 0) currentStimulus.dwellSamples = 1;
			if(currentStimulus.interStimulusIntervalSamples == 0) currentStimulus.interStimulusIntervalSamples = 1;

			ImGui::Text("Step Size (uA) %0.3f", currentStimulusStepSizeMicroAmps);

			ImGui::InputFloat("Anodic Amplitude (uA)", &currentStimulus.requestedAnodicAmplitudeMicroAmps, 0.01f, 1.0f); ImGui::SameLine();
			ImGui::Text("   Actual (uA) %0.3f", currentStimulus.actualAnodicAmplitudeMicroAmps);
			ImGui::InputFloat("Cathodic Amplitude (uA)", &currentStimulus.requestedCathodicAmplitudeMicroAmps, 0.01f, 1.0f); ImGui::SameLine();
			ImGui::Text("   Actual (uA) %0.3f", currentStimulus.actualCathodicAmplitudeMicroAmps);

			char buf[64];

			std::sprintf(buf, "Load Stimulus #%03i", lastSelected);
			if(ImGui::Button(buf)){
				currentStimulus = stimuli[lastSelected];
			}

			ImGui::SameLine();

			if(ImGui::Button("Clear")){
				currentStimulus = std.defaultStimulus;
			}

			ImGui::SameLine();

			ONI::Settings::Rhs2116StimulusStep stepSize = ONI::Settings::Rhs2116StimulusStep::Step10nA;
			bool bStimuliNeedConfirmation = false;
			bool bApplyStimulus = false;

			

			if(ImGui::Button("Apply")){

				stagedStimuli = stimuli;

				for(size_t i = 0; i < stimuli.size(); ++i){
					if(channelSelect[i]){
						stagedStimuli[i] = currentStimulus;
					}
				}

				stepSize = std.conformStepSize(stagedStimuli);

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
				
				

				if(bStimuliNeedConfirmation){
					ImGui::OpenPopup("Apply Sequence");
					bApplyStimulus = false;
				}else{
					bApplyStimulus = true;
				}


			}

			// APPLY SEQUENCE

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


			if(bApplyStimulus){
				stimuli = stagedStimuli;
				std.setStimulusSequence(stimuli, stepSize);
			}

			// SHOW STIMULUS ERRORS
			


			// PLOTTING STIMULI

			static ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
				ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable;

			static int offset = 0;

			ImGui::BeginTable("##table", 3, flags, ImVec2(-1,0));
			ImGui::TableSetupColumn("Electrode", ImGuiTableColumnFlags_WidthFixed, 75.0f);
			ImGui::TableSetupColumn("Stimulus", ImGuiTableColumnFlags_WidthFixed, 75.0f);
			ImGui::TableSetupColumn("AC Signal");

			ImGui::TableHeadersRow();
			ImPlot::PushColormap(ImPlotColormap_Cool);

			size_t maxStimulusLength = std.getMaxLengthSamples(stimuli);

			for(size_t i = 0; i < stimuli.size(); ++i){

				std::vector<float> plotAmplitudes = std.getStimulusAmplitudePlot(stimuli[i]);

				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Probe %d", i);
				ImGui::TableSetColumnIndex(1);
				//ImGui::Text("%.3f mV \n%.3f avg \n%.3f dev \n%i N", p.acProbeVoltages[probe][offset], p.acProbeStats[probe].mean, p.acProbeStats[probe].deviation, frameCount);
				ImGui::TableSetColumnIndex(2);

				ImGui::PushID(i);

				if(plotAmplitudes.size() == 4 && plotAmplitudes[0] == 0 && plotAmplitudes[1] == 0 && plotAmplitudes[2] == 0 && plotAmplitudes[3] == 0){
					// nothing
				}else{

					// make all stimulus plots the same length
					size_t sampleDiff = maxStimulusLength - plotAmplitudes.size();
					for(size_t k = 0; k < sampleDiff; ++k) plotAmplitudes.push_back(0);

					ONI::Interface::Plot::Sparkline("##spark", &plotAmplitudes[0], plotAmplitudes.size(), -stimuli[i].actualAnodicAmplitudeMicroAmps - 0.01, stimuli[i].actualCathodicAmplitudeMicroAmps + 0.01, offset, ImPlot::GetColormapColor(i), ImVec2(-1, 80));
				
				
				}
				
				ImGui::PopID();

			}

			ImPlot::PopColormap();
			ImGui::EndTable();

			

			ImGui::Separator();
			if (ImGui::Button("Done", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); } ImGui::SameLine();
			if (ImGui::Button("Reset", ImVec2(120, 0))) { 
				channelSelect.resize(std.getNumProbes());
				for(size_t i = 0; i < channelSelect.size(); ++i) channelSelect[i] = false;
			};
			ImGui::EndPopup();
		}


		ImGui::PopID();


	}

	bool save(std::string presetName){
		return false;
	}

	bool load(std::string presetName){
		return false;
	}

protected:

	ONI::Rhs2116StimulusData currentStimulus;

	std::vector<ONI::Rhs2116StimulusData> stagedStimuli;
	std::vector<bool> channelSelect;

	std::string sequenceMessage = "";

	const std::string processorName = "Rhs2116StimulusGui";

};

} // namespace Interface
} // namespace ONI