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

#include "ONIContext.h"
#include "ONIDevice.h"
#include "ONIDeviceFmc.h"
#include "ONISettingTypes.h"
#include "ONIUtility.h"

#pragma once


#include "ofxImGui.h"
#include "ofxImPlot.h"
#include "ofxFutilities.h"

class ONIInterface : public ONIFrameProcessor{

public:

	//ONIInterface(){};
	virtual ~ONIInterface(){};

	// inherited from ONIFrameProcessor
	//virtual inline void process(ONIFrame& frame) = 0;
	//virtual inline void process(oni_frame_t* frame) = 0;
	//void subscribeProcessor(const std::string& processorName, const FrameProcessorType& type, ONIFrameProcessor * processor){...}
	//void unsubscribeProcessor(const std::string& processorName, const FrameProcessorType& type, ONIFrameProcessor * processor){...}
	//std::mutex& getMutex(){...}

	virtual inline void gui(ONIDevice& device) = 0;

	virtual bool save(std::string presetName) = 0;
	virtual bool load(std::string presetName) = 0;

};

class FmcInterface : public ONIInterface{

public:

	~FmcInterface(){};

	inline void process(oni_frame_t* frame){}; // nothing
	inline void process(ONIFrame& frame){}; // nothing
	
	inline void gui(ONIDevice& device){
	
		FmcDevice& fmc = *reinterpret_cast<FmcDevice*>(&device);

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

	FmcDeviceSettings nextSettings;
	bool bIsApplying = false;

};


class HeartBeatInterface : public ONIInterface{

public:

	~HeartBeatInterface(){
	
	};

	inline void process(oni_frame_t* frame){}; // nothing

	inline void process(ONIFrame& frame){
		bHeartBeat = !bHeartBeat;
		deltaTime = frame.getDeltaTime();
	};

	inline void gui(ONIDevice& device){

		HeartBeatDevice& hbd = *reinterpret_cast<HeartBeatDevice*>(&device);

		hbd.subscribeProcessor(processorName, FrameProcessorType::POST_FRAME_PROCESSOR, this);

		ImGui::PushID(hbd.getName().c_str());
		ImGui::Text(hbd.getName().c_str());

		ImGui::RadioButton("HeartBeat", bHeartBeat); ImGui::SameLine();
		ImGui::Text(" %0.3f (ms)", deltaTime / 1000.0f);

		ImGui::Separator();
		ImGui::Separator();

		ImGui::PopID();
		

	}

	bool save(std::string presetName){
		return false;
	}

	bool load(std::string presetName){
		return false;
	}

protected:

	std::atomic_bool bHeartBeat = false;
	std::atomic_uint64_t deltaTime = 0;

	const std::string processorName = "HeartBeatGui";
};


class Rhs2116Interface : public ONIInterface{

public:

	~Rhs2116Interface(){

	};

	inline void process(oni_frame_t* frame){}; // nothing
	inline void process(ONIFrame& frame){}; // nothing

	inline void gui(ONIDevice& device){

		Rhs2116Device& rhs = *reinterpret_cast<Rhs2116Device*>(&device);

		rhs.subscribeProcessor(processorName, FrameProcessorType::POST_FRAME_PROCESSOR, this);

		nextSettings = rhs.settings;

		ImGui::PushID(rhs.getName().c_str());
		ImGui::Text(rhs.getName().c_str());

		deviceGui(rhs.settings);

		if(nextSettings.dspCutoff != rhs.settings.dspCutoff) rhs.setDspCutOff(nextSettings.dspCutoff);
		if(nextSettings.lowCutoff != rhs.settings.lowCutoff) rhs.setAnalogLowCutoff(nextSettings.lowCutoff);
		if(nextSettings.lowCutoffRecovery != rhs.settings.lowCutoffRecovery) rhs.setAnalogLowCutoffRecovery(nextSettings.lowCutoffRecovery);
		if(nextSettings.highCutoff != rhs.settings.highCutoff) rhs.setAnalogHighCutoff(nextSettings.highCutoff);

		ImGui::PopID();

	}

	inline void deviceGui(const Rhs2116DeviceSettings& settings){

		static char * dspCutoffOptions = "Differential\0Dsp3309Hz\0Dsp1374Hz\0Dsp638Hz\0Dsp308Hz\0Dsp152Hz\0Dsp75Hz\0Dsp37Hz\0Dsp19Hz\0Dsp9336mHz\0Dsp4665mHz\0Dsp2332mHz\0Dsp1166mHz\0Dsp583mHz\0Dsp291mHz\0Dsp146mHz\0Off";
		static char * lowCutoffOptions = "Low1000Hz\0Low500Hz\0Low300Hz\0Low250Hz\0Low200Hz\0Low150Hz\0Low100Hz\0Low75Hz\0Low50Hz\0Low30Hz\0Low25Hz\0Low20Hz\0Low15Hz\0Low10Hz\0Low7500mHz\0Low5000mHz\0Low3090mHz\0Low2500mHz\0Low2000mHz\0Low1500mHz\0Low1000mHz\0Low750mHz\0Low500mHz\0Low300mHz\0Low250mHz\0Low100mHz";
		static char * highCutoffOptions = "High20000Hz\0High15000Hz\0High10000Hz\0High7500Hz\0High5000Hz\0High3000Hz\0High2500Hz\0High2000Hz\0High1500Hz\0High1000Hz\0High750Hz\0High500Hz\0High300Hz\0High250Hz\0High200Hz\0High150Hz\0High100Hz";

		ImGui::SetNextItemWidth(200);
		int dspFormatItem = settings.dspCutoff; //format.dspEnable == 1 ? format.dspCutoff : Rhs2116DspCutoff::Off;
		ImGui::Combo("DSP Cutoff", &dspFormatItem, dspCutoffOptions, 5);


		ImGui::SetNextItemWidth(200);
		int lowCutoffItem = settings.lowCutoff; //getAnalogLowCutoff(false);
		ImGui::Combo("Analog Low Cutoff", &lowCutoffItem, lowCutoffOptions, 5);


		ImGui::SetNextItemWidth(200);
		int lowCutoffRecoveryItem = settings.lowCutoffRecovery; //getAnalogLowCutoffRecovery(false);
		ImGui::Combo("Analog Low Cutoff Recovery", &lowCutoffRecoveryItem, lowCutoffOptions, 5);

		ImGui::SetNextItemWidth(200);
		int highCutoffItem = settings.highCutoff; //getAnalogHighCutoff(false);
		ImGui::Combo("Analog High Cutoff", &highCutoffItem, highCutoffOptions, 5);

		nextSettings.dspCutoff = (Rhs2116DspCutoff)dspFormatItem;
		nextSettings.lowCutoff = (Rhs2116AnalogLowCutoff)lowCutoffItem;
		nextSettings.lowCutoffRecovery = (Rhs2116AnalogLowCutoff)lowCutoffRecoveryItem;
		nextSettings.highCutoff = (Rhs2116AnalogHighCutoff)highCutoffItem;

	}

	bool save(std::string presetName){
		return false;
	}

	bool load(std::string presetName){
		return false;
	}

protected:

	Rhs2116DeviceSettings nextSettings;

	const std::string processorName = "Rhs2116Gui";
};


static void Sparkline(const char* id, const float* values, int count, float min_v, float max_v, int offset, const ImVec4& col, const ImVec2& size) {
	ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0,0));
	if (ImPlot::BeginPlot(id, size, ImPlotFlags_CanvasOnly)){
		ImPlot::SetupAxes(nullptr, nullptr);//, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_None);
		ImPlot::SetupAxesLimits(0, count - 1, min_v, max_v, ImGuiCond_Always);
		ImPlot::SetNextLineStyle(col);
		//ImPlot::SetNextFillStyle(col, 0.25);
		ImPlot::PlotLine(id, values, count, 1, 0, ImPlotLineFlags_None, offset); //ImPlotLineFlags_Shaded
		ImPlot::EndPlot();
	}
	ImPlot::PopStyleVar();
}

class Rhs2116MultiInterface : public Rhs2116Interface{

public:

	Rhs2116MultiInterface(){
		bufferSizeTimeMillis = 5000;
		bufferSampleTimeMillis = 10;
		resetBuffers();
		bThread = true;
		thread = std::thread(&Rhs2116MultiInterface::processProbeData, this);
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

	inline void process(ONIFrame& frame){
		buffer.push(*reinterpret_cast<Rhs2116MultiFrame*>(&frame));
	};

	inline void gui(ONIDevice& device){

		Rhs2116MultiDevice& rhsm = *reinterpret_cast<Rhs2116MultiDevice*>(&device);

		rhsm.subscribeProcessor(processorName, FrameProcessorType::POST_FRAME_PROCESSOR, this);

		nextSettings = rhsm.settings;

		ImGui::PushID(rhsm.getName().c_str());
		ImGui::Text(rhsm.getName().c_str());

		Rhs2116Interface::deviceGui(rhsm.settings); // draw the common device gui settings

		if(ImGui::InputInt("Buffer Size (ms)", &bufferSizeTimeMillis)){
			resetBuffers();
		}

		if(ImGui::InputInt("Buffer Sample Time (ms)", &bufferSampleTimeMillis)){
			//if(bufferSampleTimeMillis == 0) bufferSampleTimeMillis = 1;
			resetBuffers();
		}

		ImGui::Separator();

		if(ImGui::Button("Channel Map")){
			ImGui::OpenPopup("Channel Map");
		}

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
	std::vector<Rhs2116MultiFrame> lastFrameBuffer;
	void processProbeData(){

		while(bThread){

			if(buffer.isFrameNew()){ // is this helping at all? Or just locking up threads?!

				std::vector<Rhs2116MultiFrame> frameBuffer = buffer.getBuffer();
				std::sort(frameBuffer.begin(), frameBuffer.end(), acquisition_clock_compare());

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

		static bool anim = true;
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

			Sparkline("##spark", &p.dcProbeVoltages[probe][0], frameCount, -10.0f, 10.0f, offset, ImPlot::GetColormapColor(probe), ImVec2(-1, 120));

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

	ONIDataBuffer<Rhs2116MultiFrame> buffer;

	std::thread thread;
	std::mutex mutex;

	std::atomic_bool bThread = false;

	ProbeData probeData;

	const std::string processorName = "Rhs2116MultiGui";
};



class ContextInterface : public ONIInterface{

public:

	~ContextInterface(){};

	inline void process(ONIFrame& frame){}; // nothing
	inline void process(oni_frame_t* frame){}; // nothing
	inline void gui(ONIDevice& device){}; //nothing

	inline void gui(ONIContext& context){

		static bool bOpenOnFirstStart = true;

		ImGui::Begin("ONI Context");
		ImGui::PushID("ONI Context");

		if(context.bIsContextSetup){

			if(ImGui::Button("Setup Context")){
				context.setupContext();
			}

			ImGui::SameLine();

			if(ImGui::Button("List Devices")){
				context.printDeviceTable();
			}

			ImGui::SameLine();

			std::string acqString = (context.bIsAcquiring ? "Stop Acquisition" : "Start Acquisition");

			if(ImGui::Button(acqString.c_str())){
				if(context.bIsAcquiring){
					context.stopAcquisition();
				}else{
					context.startAcquisition();
				}
			}

			ImGui::NewLine();

			static ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable | ImGuiTableFlags_Hideable;

			ImVec2 outer_size = ImVec2(0.0f, 400.0f);

			if(bOpenOnFirstStart) ImGui::SetNextItemOpen(true);

			if(ImGui::CollapsingHeader("Device List")){
				if (ImGui::BeginTable("Device Types", 6, flags, outer_size)){
					ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
					ImGui::TableSetupColumn("Device IDX", ImGuiTableColumnFlags_WidthFixed, 50);
					ImGui::TableSetupColumn("Hard ID", ImGuiTableColumnFlags_WidthFixed, 25);
					ImGui::TableSetupColumn("Firmware", ImGuiTableColumnFlags_WidthFixed, 20);
					ImGui::TableSetupColumn("Read Size", ImGuiTableColumnFlags_WidthFixed, 20);
					ImGui::TableSetupColumn("Write Size", ImGuiTableColumnFlags_WidthFixed, 20);
					ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_None);
					ImGui::TableHeadersRow();

					// Demonstrate using clipper for large vertical lists
					ImGuiListClipper clipper;
					clipper.Begin(context.deviceTypes.size());
					while (clipper.Step()){
						for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++){

							const oni_device_t& type = context.deviceTypes[row];
							ImGui::TableNextRow();

							ImGui::TableSetColumnIndex(0); // Device IDX
							ImGui::Text("%02i|%05u: 0x%02x.0x%02x", row, type.idx, (uint8_t)(type.idx >> 8), (uint8_t)type.idx);

							ImGui::TableSetColumnIndex(1); // Device Type ID
							ImGui::Text("%02d", type.id);

							ImGui::TableSetColumnIndex(2); // Firmware Version
							ImGui::Text("%02d", type.version);

							ImGui::TableSetColumnIndex(3); // Read Size
							ImGui::Text("%02u", type.read_size);

							ImGui::TableSetColumnIndex(4); // Write Size
							ImGui::Text("%02u", type.write_size);

							ImGui::TableSetColumnIndex(5); // Description
							ImGui::Text("%s", onix_device_str(type.id));

						}
					}
					ImGui::EndTable();
				}
			}

		}

		for(auto device : context.oniDevices){
			if(bOpenOnFirstStart) ImGui::SetNextItemOpen(bOpenOnFirstStart);
			if(ImGui::CollapsingHeader(device.second->getName().c_str(), true)){
				//device.second->gui()
				// ;
				switch(device.first){
				case 0: // heart beat device
				{
					heartBeatInterface.gui(*device.second);
					break;
				}
				case 1: // fmc devices
				case 2:
				{
					fmcInterface[device.first - 1].gui(*device.second);
					break;
				}
				case 256:
				case 257:
				{
					if(context.rhs2116MultiDevice != nullptr) ImGui::BeginDisabled();
					rhsInterface[device.first - 256].gui(*device.second);
					if(context.rhs2116MultiDevice != nullptr) ImGui::EndDisabled();
					break;
				}
				}
			}
		}

		//if(context.rhs2116MultiDevice == nullptr){
		//	context.rhs2116MultiDevice = new Rhs2116MultiDevice;
		//	context.rhs2116MultiDevice->setup(&context.ctx, context.acq_clock_khz);
		//	for(auto device : context.oniDevices){
		//		if(device.second->getDeviceTypeID() == RHS2116){
		//			context.rhs2116MultiDevice->addDevice(reinterpret_cast<Rhs2116Device*>(device.second));
		//			//rhs2116MultiDevice->devices[device.second->getDeviceTableID()] = reinterpret_cast<Rhs2116Device*>(device.second);
		//		}
		//	}
		//	//rhs2116MultiDevice->setDspCutOff(Rhs2116DspCutoff::Dsp308Hz);
		//	//rhs2116MultiDevice->setAnalogLowCutoff(Rhs2116AnalogLowCutoff::Low100mHz);
		//	//rhs2116MultiDevice->setAnalogLowCutoffRecovery(Rhs2116AnalogLowCutoff::Low250Hz);
		//	//rhs2116MultiDevice->setAnalogHighCutoff(Rhs2116AnalogHighCutoff::High10000Hz);
		//	//rhs2116MultiDevice->saveConfig("default");
		//	//context.rhs2116MultiDevice->loadConfig("default");
		//	std::vector<size_t> t = {31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,6,7,5,4,3,2,1,0};
		//	//context.rhs2116MultiDevice->resetChannelMap();
		//	context.rhs2116MultiDevice->setChannelMap(t);
		//	//rhs2116MultiDevice->saveConfig("default");
		//	//startAcquisition();
		//}

		if(context.rhs2116MultiDevice != nullptr){
			if(bOpenOnFirstStart) ImGui::SetNextItemOpen(bOpenOnFirstStart);
			if(ImGui::CollapsingHeader("Rhs2116Multi", true)) multiInterface.gui(*context.rhs2116MultiDevice); //context.rhs2116MultiDevice->gui();
		}

		ImGui::PopID();
		ImGui::End();

		bOpenOnFirstStart = false;
	
	};

	bool save(std::string presetName){
		return false;
	}

	bool load(std::string presetName){
		return false;
	}

protected:

	FmcInterface fmcInterface[2];
	HeartBeatInterface heartBeatInterface;
	Rhs2116Interface rhsInterface[2];
	Rhs2116MultiInterface multiInterface;

};

typedef Singleton<ContextInterface> ContextInterfaceSingleton;
static ContextInterface& ONIGui = ContextInterfaceSingleton::Instance();

/*
class _ContextConfig : public ONIContextConfig<ContextSettings> {

public:

	_ContextConfig(){};
	~_ContextConfig(){};

	inline void process(ONIFrame& frame){
		//nothing
	}

	inline void gui(){

	}

	bool save(std::string presetName){
		//LOGINFO("Saving Context config: %s", filePath.c_str());
		return true;
	}

	bool load(std::string presetName){
		//LOGINFO("Loading Context config: %s", filePath.c_str());
		return true;
	}

protected:

	friend class boost::serialization::access;
	template<class Archive>
	void serialize(Archive & ar, const unsigned int version){
		ar & BOOST_SERIALIZATION_NVP(settings);
	}

};


template<class Archive>
void serialize(Archive & ar, FmcDeviceSettings & settings, const unsigned int version) {
	ar & BOOST_SERIALIZATION_NVP(settings.voltage);
};

class _FmcDeviceConfig : public ONIDeviceConfig<FmcDeviceSettings> {

public:

	_FmcDeviceConfig(){};
	~_FmcDeviceConfig(){};

	void deviceConfigSetup(){
		// nothing
	}
	inline void process(oni_frame_t* frame){}; // nothing
	inline void process(ONIFrame& frame){};    // nothing

	inline void gui(){

		ImGui::PushID(device->getName().c_str());
		ImGui::Text(device->getName().c_str());

		ImGui::SetNextItemWidth(200);
		ImGui::InputFloat("Port Voltage", &changedSettings.voltage, 0.1f, 0.1f, "%.1f"); 

		changedSettings.voltage = std::clamp(changedSettings.voltage, 3.0f, 10.0f); // use some way of specifying min and max

		if(changedSettings.voltage != currentSettings.voltage){
			ImGui::SameLine();
			bool bApplyVoltage = false;
			if(ImGui::Button("Apply")){
				if(changedSettings.voltage >= 5.0f){
					bApplyVoltage = false;
					ImGui::OpenPopup("Overvoltage");
				}else{
					bApplyVoltage = true;
				}
			}

			if(changedSettings.voltage >= 5.0f){
				ImGui::SameLine();
				ImGui::Text("WARNING Voltage exceeds 5.0v");
			}

			// Always center this window when appearing
			ImVec2 center = ImGui::GetMainViewport()->GetCenter();
			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

			if(ImGui::BeginPopupModal("Overvoltage", NULL)){
				ImGui::Text("Voltages above 5.0v may damage the device!");
				ImGui::Text("Do you really want to supply %.1f v to device?", changedSettings.voltage);
				ImGui::Separator();
				if (ImGui::Button("Yes", ImVec2(120, 0))) { bApplyVoltage = true; ImGui::CloseCurrentPopup(); }
				ImGui::SetItemDefaultFocus();
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(120, 0))) { bApplyVoltage = false; ImGui::CloseCurrentPopup(); }
				ImGui::EndPopup();
			}

			if(bApplyVoltage){
				LOGINFO("Change voltage from %.1f to %.1f", currentSettings.voltage, changedSettings.voltage);
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

	bool save(std::string presetName){
		std::string filePath = getPresetFilePath(presetName, device->getName());
		LOGINFO("Saving FMC config: %s", filePath.c_str());
		return fu::Serializer.saveClass(filePath, *this, ARCHIVE_XML);
	}

	bool load(std::string presetName){
		std::string filePath = getPresetFilePath(presetName, device->getName());
		LOGINFO("Loading FMC config: %s", filePath.c_str());
		return fu::Serializer.loadClass(filePath, *this, ARCHIVE_XML);
	}

	//bool bLinkState = false;
	//bool bEnabled = false;

protected:

	friend class boost::serialization::access;
	template<class Archive>
	void serialize(Archive & ar, const unsigned int version){
		ar & BOOST_SERIALIZATION_NVP(currentSettings);
	}

};

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



template<class Archive>
void serialize(Archive & ar, HeartBeatDeviceSettings & settings, const unsigned int version) {
	ar & BOOST_SERIALIZATION_NVP(settings.frequencyHz);
};

class _HeartBeatDeviceConfig : public ONIDeviceConfig<HeartBeatDeviceSettings> {

public:

	_HeartBeatDeviceConfig(){};
	~_HeartBeatDeviceConfig(){
		//ONIProbeDevice* probeDevice = reinterpret_cast<ONIProbeDevice*>(device);
		std::string processorName = device->getName() + " GUI PROC";
		device->unsubscribeProcessor(processorName, FrameProcessorType::POST_FRAME_PROCESSOR, this);
	};

	void deviceConfigSetup(){
		//ONIProbeDevice* probeDevice = reinterpret_cast<ONIProbeDevice*>(device);
		std::string processorName = device->getName() + " GUI PROC";
		device->subscribeProcessor(processorName, FrameProcessorType::POST_FRAME_PROCESSOR, this);
	}

	inline void process(oni_frame_t* frame){}; // nothing

	inline void process(ONIFrame& frame){
		bHeartBeat = !bHeartBeat;
		deltaTime = frame.getDeltaTime();
	}

	inline void gui(){

		ImGui::PushID(device->getName().c_str());
		ImGui::Text(device->getName().c_str());

		ImGui::RadioButton("HeartBeat", bHeartBeat); ImGui::SameLine();
		ImGui::Text(" %0.3f (ms)", deltaTime / 1000.0f);

		ImGui::Separator();
		ImGui::Separator();

		ImGui::PopID();

	}

	bool save(std::string presetName){
		std::string filePath = getPresetFilePath(presetName, device->getName());
		LOGINFO("Saving HeartBeat config: %s", filePath.c_str());
		return fu::Serializer.saveClass(filePath, *this, ARCHIVE_XML);
	}

	bool load(std::string presetName){
		std::string filePath = getPresetFilePath(presetName, device->getName());
		LOGINFO("Loading HeartBeat config: %s", filePath.c_str());
		return fu::Serializer.loadClass(filePath, *this, ARCHIVE_XML);;
	}

	//bool bEnabled = false;

protected:

	std::atomic_bool bHeartBeat = false;
	std::atomic_uint64_t deltaTime = 0;

	friend class boost::serialization::access;
	template<class Archive>
	void serialize(Archive & ar, const unsigned int version){
		ar & BOOST_SERIALIZATION_NVP(currentSettings);
	}

};

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++



template<class Archive>
void serialize(Archive & ar, Rhs2116DeviceSettings & settings, const unsigned int version) {
	ar & BOOST_SERIALIZATION_NVP(settings.dspCutoff);
	ar & BOOST_SERIALIZATION_NVP(settings.lowCutoff);
	ar & BOOST_SERIALIZATION_NVP(settings.lowCutoffRecovery);
	ar & BOOST_SERIALIZATION_NVP(settings.highCutoff);
	ar & BOOST_SERIALIZATION_NVP(settings.channelMap);
};

class _Rhs2116DeviceConfig : public ONIDeviceConfig<Rhs2116DeviceSettings> {

public:

	_Rhs2116DeviceConfig(){};
	~_Rhs2116DeviceConfig(){
		//if(device == nullptr) return;
		//ONIProbeDevice* probeDevice = reinterpret_cast<ONIProbeDevice*>(device);
		//std::string processorName = device->getName() + " GUI PROC";
		//device->unsubscribeProcessor(processorName, FrameProcessorType::POST_FRAME_PROCESSOR, this);
	};

	void deviceConfigSetup(){
		//if(device == nullptr) return;
		//ONIProbeDevice* probeDevice = reinterpret_cast<ONIProbeDevice*>(device);
		std::string processorName = device->getName() + " GUI PROC";
		device->subscribeProcessor(processorName, FrameProcessorType::POST_FRAME_PROCESSOR, this);
	}

	void defaults(){

		bufferSize = 2 * 30000; // round off for display!! reinterpret_cast<ONIProbeDevice*>(device)->getSampleFrequencyHz();
		bufferStepSize = 100;

		changedSettings.dspCutoff = Rhs2116DspCutoff::Dsp308Hz;
		changedSettings.lowCutoff = Rhs2116AnalogLowCutoff::Low100mHz;
		changedSettings.lowCutoffRecovery = Rhs2116AnalogLowCutoff::Low250Hz;
		changedSettings.highCutoff = Rhs2116AnalogHighCutoff::High10000Hz;

		//changedSettings.bufferStepSize = 100;
		//buffer.resize(currentSettings.bufferSize);

	}

	inline void process(oni_frame_t* frame){

	}

	inline void process(ONIFrame& frame){

	}

	inline void gui(){

		//ImGui::Begin(deviceName.c_str());
		ImGui::PushID(device->getName().c_str());

		static char * dspCutoffOptions = "Differential\0Dsp3309Hz\0Dsp1374Hz\0Dsp638Hz\0Dsp308Hz\0Dsp152Hz\0Dsp75Hz\0Dsp37Hz\0Dsp19Hz\0Dsp9336mHz\0Dsp4665mHz\0Dsp2332mHz\0Dsp1166mHz\0Dsp583mHz\0Dsp291mHz\0Dsp146mHz\0Off";
		static char * lowCutoffOptions = "Low1000Hz\0Low500Hz\0Low300Hz\0Low250Hz\0Low200Hz\0Low150Hz\0Low100Hz\0Low75Hz\0Low50Hz\0Low30Hz\0Low25Hz\0Low20Hz\0Low15Hz\0Low10Hz\0Low7500mHz\0Low5000mHz\0Low3090mHz\0Low2500mHz\0Low2000mHz\0Low1500mHz\0Low1000mHz\0Low750mHz\0Low500mHz\0Low300mHz\0Low250mHz\0Low100mHz";
		static char * highCutoffOptions = "High20000Hz\0High15000Hz\0High10000Hz\0High7500Hz\0High5000Hz\0High3000Hz\0High2500Hz\0High2000Hz\0High1500Hz\0High1000Hz\0High750Hz\0High500Hz\0High300Hz\0High250Hz\0High200Hz\0High150Hz\0High100Hz";

		ImGui::SetNextItemWidth(200);
		int dspFormatItem = currentSettings.dspCutoff; //format.dspEnable == 1 ? format.dspCutoff : Rhs2116DspCutoff::Off;
		ImGui::Combo("DSP Cutoff", &dspFormatItem, dspCutoffOptions, 5);


		ImGui::SetNextItemWidth(200);
		int lowCutoffItem = currentSettings.lowCutoff; //getAnalogLowCutoff(false);
		ImGui::Combo("Analog Low Cutoff", &lowCutoffItem, lowCutoffOptions, 5);


		ImGui::SetNextItemWidth(200);
		int lowCutoffRecoveryItem = currentSettings.lowCutoffRecovery; //getAnalogLowCutoffRecovery(false);
		ImGui::Combo("Analog Low Cutoff Recovery", &lowCutoffRecoveryItem, lowCutoffOptions, 5);

		ImGui::SetNextItemWidth(200);
		int highCutoffItem = currentSettings.highCutoff; //getAnalogHighCutoff(false);
		ImGui::Combo("Analog High Cutoff", &highCutoffItem, highCutoffOptions, 5);
		
		changedSettings.dspCutoff = (Rhs2116DspCutoff)dspFormatItem;
		changedSettings.lowCutoff = (Rhs2116AnalogLowCutoff)lowCutoffItem;
		changedSettings.lowCutoffRecovery = (Rhs2116AnalogLowCutoff)lowCutoffRecoveryItem;
		changedSettings.highCutoff = (Rhs2116AnalogHighCutoff)highCutoffItem;

		if(changedSettings != currentSettings) bApplySettings = true;

		//mutex.lock();
		//ImGui::Text("Frame delta: %i", frameDeltaSFX);
		//ImGui::Text("Sample cont: %i", sampleCounter);
		//ImGui::Text("Div delta  : %f", div);
		//mutex.unlock();
		ImGui::PopID();
		//ImGui::End();

	}

	bool save(std::string presetName){
		std::string filePath = getPresetFilePath(presetName, device->getName());
		LOGINFO("Saving Rhs2116 config: %s", filePath.c_str());
		return fu::Serializer.saveClass(filePath, *this, ARCHIVE_XML);;
	}

	bool load(std::string presetName){
		std::string filePath = getPresetFilePath(presetName, device->getName());
		LOGINFO("Loading Rhs2116 config: %s", filePath.c_str());
		bool bOk = fu::Serializer.loadClass(filePath, *this, ARCHIVE_XML);
		if(!bOk){
			LOGALERT("Problem loading config - reset to defaults");
			defaults();
			return save(presetName);
		}
		//buffer.resize(currentSettings.bufferSize);
		return bOk;
	}


protected:

	
	uint64_t bufferFrameCounter = 0;
	int bufferSize = 0;
	int bufferStepSize = 0;

	ONIDataBuffer<Rhs2116Frame> buffer;

	friend class boost::serialization::access;
	template<class Archive>
	void serialize(Archive & ar, const unsigned int version){
		ar & BOOST_SERIALIZATION_NVP(bufferSize);
		ar & BOOST_SERIALIZATION_NVP(bufferStepSize);
		ar & BOOST_SERIALIZATION_NVP(currentSettings);
	}

};

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++




class _Rhs2116MultiDeviceConfig : public _Rhs2116DeviceConfig {

public:

	_Rhs2116MultiDeviceConfig(){};
	~_Rhs2116MultiDeviceConfig(){
		//ONIProbeDevice* probeDevice = reinterpret_cast<ONIProbeDevice*>(device);
		std::string processorName = device->getName() + " GUI PROC";
		device->unsubscribeProcessor(processorName, FrameProcessorType::POST_FRAME_PROCESSOR, this);
		bThread = false;
		if(thread.joinable()) thread.join();
	};

	void deviceConfigSetup(){
		//ONIProbeDevice* probeDevice = reinterpret_cast<ONIProbeDevice*>(device);
		std::string processorName = device->getName() + " GUI PROC";
		device->subscribeProcessor(processorName, FrameProcessorType::POST_FRAME_PROCESSOR, this);
		//buffer.resize(currentSettings.bufferSize);
		bThread = true;
		thread = std::thread(&_Rhs2116MultiDeviceConfig::processBufferThread, this);

	}

	void defaults(){

		changedSettings.dspCutoff = Rhs2116DspCutoff::Dsp308Hz;
		changedSettings.lowCutoff = Rhs2116AnalogLowCutoff::Low100mHz;
		changedSettings.lowCutoffRecovery = Rhs2116AnalogLowCutoff::Low250Hz;
		changedSettings.highCutoff = Rhs2116AnalogHighCutoff::High10000Hz;

		bufferSize = 5 * 30000; // round off for display!! reinterpret_cast<ONIProbeDevice*>(device)->getSampleFrequencyHz();
		bufferStepSize = 300;
		bufferProcessDelay = 0;
		resetBuffer();
		// do I need to resize? always assume square?

		resetChannelMap();

		currentSettings = changedSettings;
		bApplySettings = true;
		
	}

	inline void process(oni_frame_t* frame){}; // nothing
	
	inline void process(ONIFrame& frame){
		const std::lock_guard<std::mutex> lock(mutex);
		if(buffer.size() == 0) return;
		if(bufferFrameCounter % bufferStepSize == 0) buffer.push(*reinterpret_cast<Rhs2116MultiFrame*>(&frame));
		++bufferFrameCounter;
		//if(bufferFrameCounter >= buffer.size() * bufferStepSize) bStartedAcquire = true;
	}

	inline void gui(){

		_Rhs2116DeviceConfig::gui();

		ImGui::PushID(device->getName().c_str());

		ImGui::Separator();

		size_t sampleFrequencyHz = 30000;//reinterpret_cast<ONIProbeDevice*>(device)->getSampleFrequencyHz();

		int bufferSizeSeconds = bufferSize / sampleFrequencyHz;

		if(ImGui::InputInt("Buffer Size (s)", &bufferSizeSeconds)){
			bufferSizeSeconds = std::clamp(bufferSizeSeconds, 0, 10);
			bufferSize = sampleFrequencyHz * bufferSizeSeconds;
			//LOGINFO("Resize frame buffer: %i", bufferSize);
			resetBuffer();
		}
		
		if(ImGui::InputInt("Buffer Step Size (frames)", &bufferStepSize)){
			bufferStepSize = std::clamp(bufferStepSize, 1, (int)(sampleFrequencyHz * bufferSizeSeconds));
			resetBuffer();
		}
		

		if(ImGui::InputInt("Buffer Process Delay (ms)", &bufferProcessDelay)){
			bufferProcessDelay = std::clamp(bufferProcessDelay, 0, 20);
			frameTimer.start<fu::millis>(bufferProcessDelay);
		}
		
		ImGui::Separator();

		if(ImGui::Button("Channel Map")){
			ImGui::OpenPopup("Channel Map");
		}

		ImGui::SameLine();

		if(ImGui::Button("Save Config")){
			save("default");
		}

		bool unused = true;
		if(ImGui::BeginPopupModal("Channel Map", &unused, ImGuiWindowFlags_AlwaysAutoResize)){
			const size_t& numProbes = reinterpret_cast<ONIProbeDevice*>(device)->getNumProbes();
			ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5, 0.5));
			for(size_t y = 0; y < numProbes; ++y){
				for(size_t x = 0; x < numProbes; ++x){
					if (x > 0) ImGui::SameLine();
					ImGui::PushID(y * numProbes + x);
					char buf[16];
					std::sprintf(buf, "%02dx%02d", x, y % 16);
					if (ImGui::Selectable(buf, changedSettings.channelMap[y][x] != 0, ImGuiSelectableFlags_NoAutoClosePopups, ImVec2(20, 20))){
						size_t swapIDXx, swapIDXy = 0;
						for(size_t xx = 0; xx < numProbes; ++xx) {
							//if(changedSettings.channelMap[y][xx]) swapIDXx = xx; 
							changedSettings.channelMap[y][xx] = false; // toggle off everything in this row
						}
						//for(size_t yy = 0; yy < numProbes; ++yy){
						//	if(changedSettings.channelMap[yy][x]) swapIDXy = yy;
						//	changedSettings.channelMap[yy][x] = false; // toggle off everything in this col
						//}
						changedSettings.channelMap[y][x] = true;
						//changedSettings.channelMap[swapIDXy][swapIDXx] = true;
					}
					ImGui::PopID();
				}
			}
			ImGui::PopStyleVar();
			ImGui::Separator();
			if (ImGui::Button("Done", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); } ImGui::SameLine();
			if (ImGui::Button("Reset", ImVec2(120, 0))) {resetChannelMap(); };
			ImGui::EndPopup();
		}

		ImGui::PopID();

		drawMutex.lock();
		std::swap(probeData, frontProbeDataBuffers);
		drawMutex.unlock();
		bufferPlot();
		probePlot();

		if(changedSettings != currentSettings) bApplySettings = true;

	}
	std::mutex drawMutex;
	//--------------------------------------------------------------
	void bufferPlot(){
		
		//const std::lock_guard<std::mutex> lock(mutex);

		const size_t& numProbes = reinterpret_cast<ONIProbeDevice*>(device)->getNumProbes();
		size_t frameCount = buffer.size();

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
			ImPlot::SetupAxesLimits(0, frontProbeDataBuffers.probeTimeStamps[probe][frameCount - 1] - 1, -5.0f, 5.0f, ImGuiCond_Always);
			ImPlot::PlotLine("##probe", &frontProbeDataBuffers.probeTimeStamps[probe][0], &frontProbeDataBuffers.acProbeVoltages[probe][0], frameCount, ImPlotLineFlags_None, offset);

			//ImPlot::SetupAxesLimits(0, frameCount - 1, -6.0f, 6.0f, ImGuiCond_Always);
			//ImPlot::PlotLine("###probe", data, frameCount, 1, 0, ImPlotLineFlags_None, offset);


			ImGui::PopID();

		}

		ImPlot::EndPlot();
		ImGui::PopID();
		ImGui::End();

	}

	//--------------------------------------------------------------
	static void Sparkline(const char* id, const float* values, int count, float min_v, float max_v, int offset, const ImVec4& col, const ImVec2& size) {
		ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0,0));
		if (ImPlot::BeginPlot(id, size, ImPlotFlags_CanvasOnly)){
			ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_None);
			ImPlot::SetupAxesLimits(0, count - 1, min_v, max_v, ImGuiCond_Always);
			ImPlot::SetNextLineStyle(col);
			//ImPlot::SetNextFillStyle(col, 0.25);
			ImPlot::PlotLine(id, values, count, 1, 0, ImPlotLineFlags_None, offset); //ImPlotLineFlags_Shaded
			ImPlot::EndPlot();
		}
		ImPlot::PopStyleVar();
	}

	//--------------------------------------------------------------
	void probePlot(){

		//const std::lock_guard<std::mutex> lock(mutex);

		const size_t& numProbes = reinterpret_cast<ONIProbeDevice*>(device)->getNumProbes();
		size_t frameCount = buffer.size();

		if(frameCount == 0) return;

		ImGui::Begin("Probes");
		ImGui::PushID("Probe Plot");
		static ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
			ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable;

		static bool anim = true;
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
			ImGui::Text("%.3f mV \n%.3f avg \n%.3f dev \n%i N", frontProbeDataBuffers.acProbeVoltages[probe][offset], frontProbeDataBuffers.acProbeStats[probe].mean, frontProbeDataBuffers.acProbeStats[probe].deviation, frameCount);
			ImGui::TableSetColumnIndex(2);

			ImGui::PushID(probe);

			Sparkline("##spark", &frontProbeDataBuffers.acProbeVoltages[probe][0], frameCount, -5.0f, 5.0f, offset, ImPlot::GetColormapColor(probe), ImVec2(-1, 120));

			ImGui::PopID();

		}

		ImPlot::PopColormap();
		ImGui::EndTable();
		ImGui::PopID();
		ImGui::End();

	}


	void resetChannelMap(){

		const size_t& numProbes = reinterpret_cast<ONIProbeDevice*>(device)->getNumProbes();

		changedSettings.channelMap.clear();
		changedSettings.channelMap.resize(numProbes);
		for(size_t y = 0; y < numProbes; ++y){
			changedSettings.channelMap[y].resize(numProbes);
		}

		for(size_t y = 0; y < numProbes; ++y){
			for(size_t x = 0; x < numProbes; ++x){
				changedSettings.channelMap[y][x] = 0;
				if(x == y) changedSettings.channelMap[y][x] = 1;
			}
		}

	}

	bool save(std::string presetName) override {
		std::string filePath = getPresetFilePath(presetName, device->getName());
		LOGINFO("Saving Rhs2116Multi config: %s", filePath.c_str());
		return fu::Serializer.saveClass(filePath, *this, ARCHIVE_XML);;
	}

	bool load(std::string presetName) override {
		std::string filePath = getPresetFilePath(presetName, device->getName());
		LOGINFO("Loading Rhs2116Multi config: %s", filePath.c_str());
		bool bOk = fu::Serializer.loadClass(filePath, *this, ARCHIVE_XML);
		if(!bOk){
			LOGALERT("Problem loading config - reset to defaults");
			defaults();
			return save(presetName);
		}
		resetBuffer();
		return bOk;
	}

	void resetBuffer(){
		const std::lock_guard<std::mutex> lock(mutex);
		bStartedAcquire = false;
		size_t frameCount = bufferSize / bufferStepSize;
		buffer.resize(frameCount);
		bufferFrameCounter = 0;
		frameTimer.start<fu::millis>(bufferProcessDelay);

		drawMutex.lock();
		const size_t& numProbes = reinterpret_cast<ONIProbeDevice*>(device)->getNumProbes();
		frontProbeDataBuffers.acProbeVoltages.resize(numProbes);
		frontProbeDataBuffers.dcProbeVoltages.resize(numProbes);
		frontProbeDataBuffers.acProbeStats.resize(numProbes);
		frontProbeDataBuffers.dcProbeStats.resize(numProbes);
		frontProbeDataBuffers.probeTimeStamps.resize(numProbes);

		for(size_t probe = 0; probe < numProbes; ++probe){
			frontProbeDataBuffers.acProbeVoltages[probe].resize(frameCount);
			frontProbeDataBuffers.dcProbeVoltages[probe].resize(frameCount);
			frontProbeDataBuffers.probeTimeStamps[probe].resize(frameCount);
		}

		probeData = frontProbeDataBuffers;
		drawMutex.unlock();

	}

	fu::Timer frameTimer;
	std::atomic_bool bStartedAcquire = false;
	void processBufferThread(){
		while(bThread){
			if(true){ //frameTimer.finished()
				//frameTimer.restart();
				mutex.lock();
				std::vector<Rhs2116MultiFrame> frameBuffer = buffer.getBuffer();
				mutex.unlock();
				if(frameBuffer.size() != 0){
					drawMutex.lock();
					const size_t& numProbes = reinterpret_cast<ONIProbeDevice*>(device)->getNumProbes();
					size_t frameCount = frameBuffer.size();

					std::sort(frameBuffer.begin(), frameBuffer.end(), acquisition_clock_compare());

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

					drawMutex.unlock();
					
				}
				ofSleepMillis(bufferProcessDelay);
			}

		}
		
	}
	struct ONIProbeStatistics{
		float sum;
		float mean;
		float ss;
		float variance;
		float deviation;
	};

	struct ProbeDataBuffers{
		std::vector< std::vector<float> > acProbeVoltages;
		std::vector< std::vector<float> > dcProbeVoltages;
		std::vector< std::vector<float> > probeTimeStamps;
		std::vector<ONIProbeStatistics> acProbeStats;
		std::vector<ONIProbeStatistics> dcProbeStats;
	};

	ProbeDataBuffers frontProbeDataBuffers;
	ProbeDataBuffers probeData;

protected:

	std::thread thread;
	std::mutex mutex;
	std::atomic_bool bThread = false;

	uint64_t bufferFrameCounter = 0;
	int bufferSize = 0;
	int bufferStepSize = 300;
	int bufferProcessDelay = 0;

	ONIDataBuffer<Rhs2116MultiFrame> buffer;
	ONIDataBuffer<Rhs2116MultiFrame> drawBuffer;

	friend class boost::serialization::access;
	template<class Archive>
	void serialize(Archive & ar, const unsigned int version){
		ar & BOOST_SERIALIZATION_NVP(bufferSize);
		ar & BOOST_SERIALIZATION_NVP(bufferStepSize);
		ar & BOOST_SERIALIZATION_NVP(bufferProcessDelay);
		ar & BOOST_SERIALIZATION_NVP(currentSettings);
	}

};


// later we can set which device config to use with specific #define for USE_IMGUI etc
typedef _ContextConfig ContextConfig;

typedef _FmcDeviceConfig FmcDeviceConfig;
typedef _HeartBeatDeviceConfig HeartBeatDeviceConfig;
typedef _Rhs2116DeviceConfig Rhs2116DeviceConfig;
typedef _Rhs2116MultiDeviceConfig Rhs2116MultiDeviceConfig;

*/