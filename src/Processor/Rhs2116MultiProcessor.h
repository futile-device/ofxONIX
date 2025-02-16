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
#include <set>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <syncstream>


#include "../Processor/BaseProcessor.h"
#include "../Processor/ChannelMapProcessor.h"
#include "../Device/Rhs2116Device.h"

#pragma once

namespace ONI{

namespace Interface{
class Rhs2116MultiInterface;
} // namespace Interface

//namespace Processor{
//class SpikeProcessor;
//class BufferProcessor;
//} // namespace Processor

namespace Processor{

//class Rhs2116StimProcessor;

class Rhs2116MultiProcessor : public ONI::Processor::BaseProcessor{

public:

	friend class ONI::Interface::Rhs2116MultiInterface;
	//friend class ONI::Processor::Rhs2116StimProcessor;
	//friend class ONI::Processor::SpikeProcessor;
	//friend class ONI::Processor::BufferProcessor;

	Rhs2116MultiProcessor(){
		BaseProcessor::processorTypeID = ONI::Processor::TypeID::RHS2116_MULTI_PROCESSOR;
		BaseProcessor::processorName = toString(processorTypeID);
	};
	
	~Rhs2116MultiProcessor(){
		LOGDEBUG("RHS2116Multi Device DTOR");
		//const std::lock_guard<std::mutex> lock(mutex);
	};

	void setup(){
		LOGDEBUG("Setting up RHS2116MULTI Processor");

		reset();
	}

	void reset(){
		for(auto& it : devices) numProbes += it.second->getNumProbes();
		getStepSize(true);
		getDspCutOff(true);
		getAnalogLowCutoff(true);
		getAnalogLowCutoffRecovery(true);
		getAnalogHighCutoff(true);
		//setStepSize(settings.stepSize);
		//setDspCutOff(settings.dspCutoff);
		//setAnalogLowCutoff(settings.lowCutoff);
		//setAnalogLowCutoffRecovery(settings.lowCutoffRecovery);
		//setAnalogHighCutoff(settings.highCutoff);
	}; 

	void addDevice(ONI::Device::Rhs2116Device* device){
		auto& it = devices.find(device->getOnixDeviceTableIDX());
		if(it == devices.end()){
			LOGINFO("Adding device %s", device->getName().c_str());
			devices[device->getOnixDeviceTableIDX()] = device;
		}else{
			LOGERROR("Device already added: %s", device->getName().c_str());
		}
		std::string processorName = BaseProcessor::processorName + " MULTI PROC";
		device->subscribeProcessor(processorName, ONI::Processor::SubscriptionType::PRE_PROCESSOR, this);
		numProbes += device->getNumProbes();
		//expectDevceIDOrdered.push_back(device->getOnixDeviceTableIDX());
		//std::sort(expectDevceIDOrdered.begin(), expectDevceIDOrdered.end()); // sort so device frames get added in ascending order
		//expectDevceIDOrdered = {257, 256, 513, 512};
		multiFrameBuffer.resize(multiFrameBuffer.size() + 1);
		multiFrameBufferRaw.resize(multiFrameBufferRaw.size() + 1);
		settings = device->getSettings(); // TODO: this is terrible; the devices could be diferent
	}
	
	bool setEnabled(const bool& b){
		for(auto& it : devices){
			bool bOk = it.second->setEnabled(b);
			if(!bOk) return false;
		}
		return true;
	}

	bool getEnabled(const bool& bCheckRegisters = true){
		for(auto& it : devices){
			bEnabled = getEnabled(bCheckRegisters); // should we check if they are both the same?
		}
		return bEnabled;
	}

	bool setAnalogLowCutoff(ONI::Settings::Rhs2116AnalogLowCutoff lowcut){
		for(auto& it : devices){
			bool bOk = it.second->setAnalogLowCutoff(lowcut);
			if(!bOk) return false;
		}
		return true;
	}

	ONI::Settings::Rhs2116AnalogLowCutoff getAnalogLowCutoff(const bool& bCheckRegisters = true){
		for(auto& it : devices){
			settings.lowCutoff = it.second->getAnalogLowCutoff(bCheckRegisters); // should we check if they are both the same?
		}
		return settings.lowCutoff;
	}

	bool setAnalogLowCutoffRecovery(ONI::Settings::Rhs2116AnalogLowCutoff lowcut){
		for(auto& it : devices){
			bool bOk = it.second->setAnalogLowCutoffRecovery(lowcut);
			if(!bOk) return false;
		}
		return true;
	}

	ONI::Settings::Rhs2116AnalogLowCutoff getAnalogLowCutoffRecovery(const bool& bCheckRegisters = true){
		for(auto& it : devices){
			settings.lowCutoffRecovery = it.second->getAnalogLowCutoffRecovery(bCheckRegisters); // should we check if they are both the same?
		}
		return settings.lowCutoffRecovery;
	}

	bool setAnalogHighCutoff(ONI::Settings::Rhs2116AnalogHighCutoff hicut){
		for(auto& it : devices){
			bool bOk = it.second->setAnalogHighCutoff(hicut);
			if(!bOk) return false;
		}
		return true;
	}

	ONI::Settings::Rhs2116AnalogHighCutoff getAnalogHighCutoff(const bool& bCheckRegisters = true){
		for(auto& it : devices){
			settings.highCutoff = it.second->getAnalogHighCutoff(bCheckRegisters); // should we check if they are both the same?
		}
		return settings.highCutoff;
	}

	bool setFormat(ONI::Settings::Rhs2116Format format){
		for(auto& it : devices){
			bool bOk = it.second->setFormat(format);
			if(!bOk) return false;
		}
		return true;
	}

	ONI::Settings::Rhs2116Format getFormat(const bool& bCheckRegisters = true){
		for(auto& it : devices){
			settings.format = it.second->getFormat(bCheckRegisters); // should we check if they are both the same?
		}
		if(settings.format.dspEnable == 0){
			settings.dspCutoff = ONI::Settings::Rhs2116DspCutoff::Off; // is this a bug? that seems to be default status
		}else{
			settings.dspCutoff = (ONI::Settings::Rhs2116DspCutoff)settings.format.dspCutoff;
		}
		return settings.format;
	}

	bool setDspCutOff(ONI::Settings::Rhs2116DspCutoff cutoff){
		for(auto& it : devices){
			bool bOk = it.second->setDspCutOff(cutoff);
			if(bOk) getDspCutOff(true);
			if(!bOk) return false;
		}
		return true;

	}

	const ONI::Settings::Rhs2116DspCutoff& getDspCutOff(const bool& bCheckRegisters = true){
		getFormat(bCheckRegisters);
		return settings.dspCutoff;
	}

	bool setStepSize(ONI::Settings::Rhs2116StimulusStep stepSize, const bool& bSetWithoutCheck){
		for(auto& it : devices){
			bool bOk = it.second->setStepSize(stepSize, bSetWithoutCheck);
			if(!bOk) return false;
		}
		return true;
	}

	ONI::Settings::Rhs2116StimulusStep getStepSize(const bool& bCheckRegisters = true){
		for(auto& it : devices){
			settings.stepSize = it.second->getStepSize(bCheckRegisters); // should we check if they are both the same?
		}
		return settings.stepSize;
	}

	inline void process(oni_frame_t* frame) override {

		//const std::lock_guard<std::mutex> lock(mutex);

		if(postProcessors.size() > 0){

			size_t nextDeviceIndex = nextDeviceCounter % devices.size();
			//LOGDEBUG("Dev-IDX: %i", frame->dev_idx);
			ONI::Frame::Rhs2116DataExtended& frameRaw = multiFrameRawMap[frame->dev_idx];
			std::memcpy(&frameRaw, frame->data, frame->data_sz); // copy the data payload including hub clock
			frameRaw.acqTime = frame->time;						 // copy the acquisition clock
			frameRaw.deltaTime = ONI::Global::model.getAcquireDeltaTimeMicros(frame->time);
			frameRaw.devIdx = frame->dev_idx;

			if(nextDeviceCounter > 0 && nextDeviceCounter % devices.size() == 0){

				if(multiFrameRawMap.size() == devices.size()){

					if(bBadFrame){
						LOGDEBUG("Pickup  multiframe %i (%llu) ==> %i", frame->dev_idx, frameRaw.hubTime, nextDeviceCounter);
						bBadFrame = false;
					}
					
					//lastMultiFrameRawMap = multiFrameRawMap;

					// order the frames by ascending device idx
					for(size_t i = 0; i < devices.size(); ++i){
						multiFrameBufferRaw[i] = std::move(multiFrameRawMap[ONI::Global::model.getRhs2116DeviceOrderIDX()[i]]);
					}

					// process the multi frame
					ONI::Frame::Rhs2116MultiFrame processedFrame(multiFrameBufferRaw, ONI::Global::model.getChannelMapProcessor()->getChannelMap());
					for(auto& it : postProcessors){
						it.second->process(processedFrame);
					}

					// clear the map to keep tracking device idx
					multiFrameRawMap.clear();

				}else{

					//LOGERROR("Out of order for multiframe %i ==> %i", frame->dev_idx, nextDeviceCounter);
					//size_t t = multiFrameRawMap.size();
					//std::memcpy(&errorFrameRaw, frame->data, frame->data_sz); // copy the data payload including hub clock
					//errorFrameRaw.acqTime = frame->time;						 // copy the acquisition clock
					//errorFrameRaw.deltaTime = ONI::Device::BaseDevice::getAcqDeltaTimeMicros(frame->time);
					//errorFrameRaw.devIdx = frame->dev_idx;
					
					LOGDEBUG("Dropped multiframe %i (%llu) ==> %i", frame->dev_idx, reinterpret_cast<ONI::Frame::Rhs2116DataExtended*>(frame->data)->hubTime, nextDeviceCounter);

					bBadFrame = true;
					--nextDeviceCounter; // decrease the device id counter so we can check if this gets corrected on next frame
				}
			}

			++nextDeviceCounter;

			//LOGDEBUG
			// ("Frame order: %i ==> %i || %i", frame->dev_idx, expectDevceIDOrdered[nextDeviceIndex], nextDeviceIndex);
			//if(frame->dev_idx != expectDevceIDOrdered[nextDeviceIndex]){
			//	LOGERROR("Unexpected frame order: %i ==> %i", frame->dev_idx, nextDeviceIndex);
			//}else{

			//	// push or add the multiframe
				//std::memcpy(&multiFrameBufferRaw[nextDeviceIndex], frame->data, frame->data_sz); // copy the data payload including hub clock
				//multiFrameBufferRaw[nextDeviceIndex].acqTime = frame->time;						 // copy the acquisition clock
				//multiFrameBufferRaw[nextDeviceIndex].deltaTime = ONI::Device::BaseDevice::getAcqDeltaTimeMicros(frame->time);
				//multiFrameBufferRaw[nextDeviceIndex].devIdx = frame->dev_idx;

			//	if((nextDeviceCounter + 1) % devices.size() == 0){ // we have chexked off all the expectedIDs in order
			//		// process the multi frame
			//		ONI::Frame::Rhs2116MultiFrame processedFrame(multiFrameBufferRaw, channelIDX);
			//		for(auto& it : postProcessors){
			//			it.second->process(processedFrame);
			//		}

			//	}
			//	++nextDeviceCounter;
			//}
		}

		//for(auto& it : preProcessors){
		//	it.second->process(frame);
		//}
		
	}



	inline void process(ONI::Frame::BaseFrame& frame) override {

		//const std::lock_guard<std::mutex> lock(mutex);

		uint32_t nextDeviceIndex = nextDeviceCounter % devices.size();

		if(frame.getOnixDeviceTableIDX() != ONI::Global::model.getRhs2116DeviceOrderIDX()[nextDeviceIndex]){
			LOGERROR("Unexpected frame order");
		}else{
			// push or add the multiframe
			multiFrameBuffer[nextDeviceIndex] = *reinterpret_cast<ONI::Frame::Rhs2116Frame*>(&frame);
			//std::swap(multiFrameBuffer[nextDeviceCounter], *reinterpret_cast<Rhs2116Frame*>(&frame));
			if((nextDeviceCounter + 1) % devices.size() == 0){ // we have chexked off all the expectedIDs in order
				// process the multi frame
				ONI::Frame::Rhs2116MultiFrame processedFrame(multiFrameBuffer, ONI::Global::model.getChannelMapProcessor()->getChannelMap());

				for(auto& it : postProcessors){
					it.second->process(processedFrame);
				}

			}
			++nextDeviceCounter;
		}

	}

	ONI::Device::Rhs2116Device* getDevice(const uint32_t& deviceTableIDX){
		auto& it = devices.find(deviceTableIDX);
		if(it == devices.end()){
			LOGERROR("No device found for IDX: %i", deviceTableIDX);
			return nullptr;
		}else{
			return it->second;
		}
	}

	std::map<uint32_t, ONI::Device::Rhs2116Device*>& getDevices(){
		return devices;
	}

	std::string info(){
		return getDevice(0)->info();
	}

protected:


private:

	std::map<uint32_t, ONI::Device::Rhs2116Device*> devices;

	std::vector<ONI::Frame::Rhs2116Frame> multiFrameBuffer;
	std::vector<ONI::Frame::Rhs2116DataExtended> multiFrameBufferRaw;

	ONI::Frame::Rhs2116DataExtended errorFrameRaw;
	std::map<uint32_t, ONI::Frame::Rhs2116DataExtended> lastMultiFrameRawMap;
	std::map<uint32_t, ONI::Frame::Rhs2116DataExtended> multiFrameRawMap;
	bool bBadFrame = false; // for tracking out of order frame idx with multiple rhs2116 devices

	uint64_t nextDeviceCounter = 0;

	bool bEnabled = false;

	ONI::Settings::Rhs2116Format format;
	ONI::Settings::Rhs2116DeviceSettings settings;

};


} // namespace Device
} // namespace ONI