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


#include "ONIConfig.h"
#include "ONIDevice.h"
#include "ONIRegister.h"
#include "ONISettingTypes.h"
#include "ONIUtility.h"

#include "ONIDeviceRhs2116.h"

#pragma once

class Rhs2116MultiInterface;

class Rhs2116MultiDevice : public Rhs2116Device{

public:

	friend class Rhs2116MultiInterface;

	Rhs2116MultiDevice(){
		numProbes = 0;							// default for base ONIProbeDevice
		sampleFrequencyHz = 30.1932367151e3;	// default for base ONIProbeDevice
	};
	
	~Rhs2116MultiDevice(){
		LOGDEBUG("RHS2116Multi Device DTOR");
		//const std::lock_guard<std::mutex> lock(mutex);
	};


	virtual void setup(volatile oni_ctx* context, uint64_t acq_clock_khz){
		LOGDEBUG("Setting up device RHS2116 MULTI");
		ctx = context;
		//deviceType = type;
		deviceType.idx = 999;
		deviceType.id = RHS2116MULTI;
		deviceTypeID = RHS2116MULTI;
		std::ostringstream os; os << ONIDeviceTypeIDStr(deviceTypeID) << " (" << deviceType.idx << ")";
		deviceName = os.str();
		this->acq_clock_khz = acq_clock_khz;
		reset(); // always call device specific setup/reset
		deviceSetup(); // always call device specific setup/reset
	}

	std::map<unsigned int, Rhs2116Device*> devices; 

	void addDevice(Rhs2116Device* device){
		auto it = devices.find(device->getDeviceTableID());
		if(it == devices.end()){
			LOGINFO("Adding device %s", device->getName().c_str());
			devices[device->getDeviceTableID()] = device;
		}else{
			LOGERROR("Device already added: %s", device->getName().c_str());
		}
		std::string processorName = deviceName + " MULTI PROC";
		device->subscribeProcessor(processorName, FrameProcessorType::PRE_FRAME_PROCESSOR, this);
		numProbes += device->getNumProbes();
		channelIDX.resize(numProbes);
		expectDevceIDXNext.push_back(device->getDeviceTableID());
		multiFrameBuffer.resize(multiFrameBuffer.size() + 1);
		multiFrameBufferRaw.resize(multiFrameBufferRaw.size() + 1);
	}

	void deviceSetup(){
		//config.setup(this);
		for(auto it : devices){
			it.second->deviceSetup();
		}
		//config.syncSettings();
	}
	
	inline void gui(){
		
		//config.gui();
		//if(config.applySettings()){
		//	LOGDEBUG("Rhs2116 Multi Device settings changed");
		//	if(settings.dspCutoff != config.getChangedSettings().dspCutoff) setDspCutOff(config.getChangedSettings().dspCutoff);
		//	if(settings.lowCutoff != config.getChangedSettings().lowCutoff) setAnalogLowCutoff(config.getChangedSettings().lowCutoff);
		//	if(settings.lowCutoffRecovery != config.getChangedSettings().lowCutoffRecovery) setAnalogLowCutoffRecovery(config.getChangedSettings().lowCutoffRecovery);
		//	if(settings.highCutoff != config.getChangedSettings().highCutoff) setAnalogHighCutoff(config.getChangedSettings().highCutoff);
		//	if(settings.channelMap != config.getChangedSettings().channelMap){
		//		settings.channelMap = config.getChangedSettings().channelMap;
		//		channelMapToIDX();
		//	}
		//	config.syncSettings();

		//	for(auto it : devices){
		//		it.second->deviceSetup();
		//	}

		//}

	}

	void setChannelMap(const std::vector<size_t>& map){
		if(map.size() == numProbes){
			channelIDX = map;
			for(size_t y = 0; y < numProbes; ++y){
				for(size_t x = 0; x < numProbes; ++x){
					settings.channelMap[y][x] = false;
				}
				size_t xx = channelIDX[y];
				settings.channelMap[y][xx] = true;
			}
			//config.syncSettings();
		}else{
			LOGERROR("Channel IDXs size doesn't equal number of probes");
		}
	}

	void channelMapToIDX(){
		if(settings.channelMap.size() == 0) return;
		for(size_t y = 0; y < numProbes; ++y){
			size_t idx = 0;
			for(size_t x = 0; x < numProbes; ++x){
				if(settings.channelMap[y][x]){
					idx = x;
					break;
				}
			}
			channelIDX[y] = idx;
		}

		ostringstream os;
		for(int i = 0; i < channelIDX.size(); i++){
			os << channelIDX[i] << (i == channelIDX.size() - 1 ? "" : ", ");
		}
		fu::debug << os.str() << fu::endl;
	}

	bool saveConfig(std::string presetName){
		//return config.save(presetName);
		return false;
	}

	bool loadConfig(std::string presetName){
		//bool bOk = config.load(presetName);
		//if(bOk){
		//	setDspCutOff(settings.dspCutoff);
		//	setAnalogLowCutoff(settings.lowCutoff);
		//	setAnalogLowCutoffRecovery(settings.lowCutoffRecovery);
		//	setAnalogHighCutoff(settings.highCutoff);
		//	channelMapToIDX();
		//	for(auto it : devices){
		//		it.second->deviceSetup();
		//	}
		//	config.syncSettings();
		//}
		//return bOk;
		return false;
	}



	inline void process(oni_frame_t* frame) override {

		//const std::lock_guard<std::mutex> lock(mutex);

		if(postFrameProcessors.size() > 0){

			unsigned int nextDeviceIndex = nextDeviceCounter % devices.size();

			if(frame->dev_idx != expectDevceIDXNext[nextDeviceIndex]){
				LOGERROR("Unexpected frame order");
			}else{

				// push or add the multiframe
				std::memcpy(&multiFrameBufferRaw[nextDeviceIndex], frame->data, frame->data_sz); // copy the data payload including hub clock
				multiFrameBufferRaw[nextDeviceIndex].acqTime = frame->time;						 // copy the acquisition clock
				multiFrameBufferRaw[nextDeviceIndex].deltaTime = ONIDevice::getAcqDeltaTimeMicros(frame->time);
				multiFrameBufferRaw[nextDeviceIndex].devIdx = frame->dev_idx;

				if((nextDeviceCounter + 1) % devices.size() == 0){ // we have chexked off all the expectedIDs in order
					// process the multi frame
					Rhs2116MultiFrame processedFrame(multiFrameBufferRaw, channelIDX);

					for(auto it : postFrameProcessors){
						it.second->process(processedFrame);
					}

				}
				++nextDeviceCounter;
			}
		}

		//for(auto it : preFrameProcessors){
		//	it.second->process(frame);
		//}
		
	}



	inline void process(ONIFrame& frame) override {

		//const std::lock_guard<std::mutex> lock(mutex);

		unsigned int nextDeviceIndex = nextDeviceCounter % devices.size();

		if(frame.getDeviceTableID() != expectDevceIDXNext[nextDeviceIndex]){
			LOGERROR("Unexpected frame order");
		}else{
			// push or add the multiframe
			multiFrameBuffer[nextDeviceIndex] = *reinterpret_cast<Rhs2116Frame*>(&frame);
			//std::swap(multiFrameBuffer[nextDeviceCounter], *reinterpret_cast<Rhs2116Frame*>(&frame));
			if((nextDeviceCounter + 1) % devices.size() == 0){ // we have chexked off all the expectedIDs in order
				// process the multi frame
				Rhs2116MultiFrame processedFrame(multiFrameBuffer, channelIDX);

				for(auto it : postFrameProcessors){
					it.second->process(processedFrame);
				}

			}
			++nextDeviceCounter;
		}

	}

	unsigned int readRegister(const Rhs2116Register& reg) override {
		std::vector<unsigned int> regs; 
		regs.resize(devices.size());
		size_t n = 0;
		for(auto it : devices){
			regs[n] = it.second->readRegister(reg);
			++n;
		}
		for(size_t i = 0; i < regs.size(); ++i){
			for(size_t j = 0; j < regs.size(); ++j){
				if(regs[i] != regs[j]){
					LOGERROR("REGISTER MISMATCH!!!!!!!!!!!!!!!!!!!!!!!");
				}
			}
		}
		return regs[0]; // TODO: we should do something if they mismatch like force setting the register?
	}

	bool writeRegister(const Rhs2116Register& reg, const unsigned int& value) override {
		bool bOk = true;
		for(auto it : devices){
			bOk = it.second->writeRegister(reg, value);
		}
		return bOk;
	}

protected:


private:

	std::vector<Rhs2116Frame> multiFrameBuffer;
	std::vector<Rhs2116RawDataFrame> multiFrameBufferRaw;


	std::vector<unsigned int> expectDevceIDXNext;// = {257,256};
	uint64_t nextDeviceCounter = 0;

	std::vector<size_t> channelIDX;

	bool bEnabled = false;

	Rhs2116Format format;

	Rhs2116DeviceSettings settings;
	//Rhs2116MultiDeviceConfig config;

};
