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


#include "../Device/BaseDevice.h"
#include "../Device/Rhs2116Device.h"

#pragma once

namespace ONI{

namespace Interface{
class Rhs2116MultiInterface;
} // namespace Interface

namespace Processor{
class SpikeProcessor;
class FrameProcessor;
} // namespace Processor

namespace Device{

class Rhs2116StimulusDevice;

class Rhs2116MultiDevice : public ONI::Device::ProbeDevice{

public:

	friend class ONI::Interface::Rhs2116MultiInterface;
	friend class ONI::Device::Rhs2116StimulusDevice;

	friend class ONI::Processor::SpikeProcessor;
	friend class ONI::Processor::FrameProcessor;

	Rhs2116MultiDevice(){
		numProbes = 0;							// default for base ONIProbeDevice
		sampleFrequencyHz = 30.1932367151e3;	// default for base ONIProbeDevice
	};
	
	~Rhs2116MultiDevice(){
		LOGDEBUG("RHS2116Multi Device DTOR");
		//const std::lock_guard<std::mutex> lock(mutex);
	};

	void setup(volatile oni_ctx* context, oni_device_t type, uint64_t acq_clock_khz) = delete; // delete the base class function

	void setup(volatile oni_ctx* context, uint64_t acq_clock_khz){
		LOGDEBUG("Setting up device RHS2116 MULTI");
		ctx = context;
		//deviceType = type;
		deviceType.idx = 999;
		deviceType.id = RHS2116MULTI;
		deviceTypeID = RHS2116MULTI;
		std::ostringstream os; os << ONI::Device::toString(deviceTypeID) << " (" << deviceType.idx << ")";
		BaseProcessor::processorName = os.str();
		this->acq_clock_khz = acq_clock_khz;
		reset(); // always call device specific setup/reset
		//deviceSetup(); // always call device specific setup/reset
	}

	void addDevice(ONI::Device::Rhs2116Device* device){
		auto it = devices.find(device->getDeviceTableID());
		if(it == devices.end()){
			LOGINFO("Adding device %s", device->getName().c_str());
			devices[device->getDeviceTableID()] = device;
		}else{
			LOGERROR("Device already added: %s", device->getName().c_str());
		}
		std::string processorName = BaseProcessor::processorName + " MULTI PROC";
		device->subscribeProcessor(processorName, ONI::Processor::ProcessorType::PRE_PROCESSOR, this);
		numProbes += device->getNumProbes();
		expectDevceIDXNext.push_back(device->getDeviceTableID());
		multiFrameBuffer.resize(multiFrameBuffer.size() + 1);
		multiFrameBufferRaw.resize(multiFrameBufferRaw.size() + 1);
		settings = device->settings; // TODO: this is terrible; the devices could be diferent
		resetChannelMap(); // do this after the terrible settings copy above
	}

	void deviceSetup(){}; // nothing
	
	bool setEnabled(const bool& b){
		for(auto it : devices){
			bool bOk = it.second->setEnabled(b);
			if(!bOk) return false;
		}
		return true;
	}

	bool getEnabled(const bool& bCheckRegisters = true){
		for(auto it : devices){
			bEnabled = getEnabled(bCheckRegisters); // should we check if they are both the same?
		}
		return bEnabled;
	}

	bool setAnalogLowCutoff(ONI::Settings::Rhs2116AnalogLowCutoff lowcut){
		for(auto it : devices){
			bool bOk = it.second->setAnalogLowCutoff(lowcut);
			if(!bOk) return false;
		}
		return true;
	}

	ONI::Settings::Rhs2116AnalogLowCutoff getAnalogLowCutoff(const bool& bCheckRegisters = true){
		for(auto it : devices){
			settings.lowCutoff = getAnalogLowCutoff(bCheckRegisters); // should we check if they are both the same?
		}
		return settings.lowCutoff;
	}

	bool setAnalogLowCutoffRecovery(ONI::Settings::Rhs2116AnalogLowCutoff lowcut){
		for(auto it : devices){
			bool bOk = it.second->setAnalogLowCutoffRecovery(lowcut);
			if(!bOk) return false;
		}
		return true;
	}

	ONI::Settings::Rhs2116AnalogLowCutoff getAnalogLowCutoffRecovery(const bool& bCheckRegisters = true){
		for(auto it : devices){
			settings.lowCutoffRecovery = getAnalogLowCutoffRecovery(bCheckRegisters); // should we check if they are both the same?
		}
		return settings.lowCutoffRecovery;
	}

	bool setAnalogHighCutoff(ONI::Settings::Rhs2116AnalogHighCutoff hicut){
		for(auto it : devices){
			bool bOk = it.second->setAnalogHighCutoff(hicut);
			if(!bOk) return false;
		}
		return true;
	}

	ONI::Settings::Rhs2116AnalogHighCutoff getAnalogHighCutoff(const bool& bCheckRegisters = true){
		for(auto it : devices){
			settings.highCutoff = getAnalogHighCutoff(bCheckRegisters); // should we check if they are both the same?
		}
		return settings.highCutoff;
	}

	bool setFormat(ONI::Settings::Rhs2116Format format){
		for(auto it : devices){
			bool bOk = it.second->setFormat(format);
			if(!bOk) return false;
		}
		return true;
	}

	ONI::Settings::Rhs2116Format getFormat(const bool& bCheckRegisters = true){
		for(auto it : devices){
			settings.format = getFormat(bCheckRegisters); // should we check if they are both the same?
		}
		return settings.format;
	}

	bool setDspCutOff(ONI::Settings::Rhs2116DspCutoff cutoff){
		for(auto it : devices){
			bool bOk = it.second->setDspCutOff(cutoff);
			if(!bOk) return false;
		}
		return true;

	}

	const ONI::Settings::Rhs2116DspCutoff& getDspCutOff(const bool& bCheckRegisters = true){
		getFormat(bCheckRegisters);
		return settings.dspCutoff;
	}

	bool setStepSize(ONI::Settings::Rhs2116StimulusStep stepSize){
		for(auto it : devices){
			bool bOk = it.second->setStepSize(stepSize);
			if(!bOk) return false;
		}
		return true;
	}

	ONI::Settings::Rhs2116StimulusStep getStepSize(const bool& bCheckRegisters = true){
		for(auto it : devices){
			settings.stepSize = getStepSize(bCheckRegisters); // should we check if they are both the same?
		}
		return settings.stepSize;
	}

	void resetChannelMap(){

		settings.channelMap.clear();
		settings.channelMap.resize(numProbes);
		for(size_t y = 0; y < numProbes; ++y){
			settings.channelMap[y].resize(numProbes);
			for(size_t x = 0; x < numProbes; ++x){
				settings.channelMap[y][x] = 0;
				if(x == y) settings.channelMap[y][x] = 1;
			}
		}

		channelMapToIDX();

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
		}else{
			LOGERROR("Channel IDXs size doesn't equal number of probes");
		}
	}

	void channelMapToIDX(){

		if(settings.channelMap.size() == 0) return;

		channelIDX.resize(numProbes);
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

		//ostringstream os;
		//for(int i = 0; i < channelIDX.size(); i++){
		//	os << channelIDX[i] << (i == channelIDX.size() - 1 ? "" : ", ");
		//}
		//fu::debug << os.str() << fu::endl;
	}

	inline void process(oni_frame_t* frame) override {

		//const std::lock_guard<std::mutex> lock(mutex);

		if(postProcessors.size() > 0){

			unsigned int nextDeviceIndex = nextDeviceCounter % devices.size();

			if(frame->dev_idx != expectDevceIDXNext[nextDeviceIndex]){
				LOGERROR("Unexpected frame order");
			}else{

				// push or add the multiframe
				std::memcpy(&multiFrameBufferRaw[nextDeviceIndex], frame->data, frame->data_sz); // copy the data payload including hub clock
				multiFrameBufferRaw[nextDeviceIndex].acqTime = frame->time;						 // copy the acquisition clock
				multiFrameBufferRaw[nextDeviceIndex].deltaTime = ONI::Device::BaseDevice::getAcqDeltaTimeMicros(frame->time);
				multiFrameBufferRaw[nextDeviceIndex].devIdx = frame->dev_idx;

				if((nextDeviceCounter + 1) % devices.size() == 0){ // we have chexked off all the expectedIDs in order
					// process the multi frame
					ONI::Frame::Rhs2116MultiFrame processedFrame(multiFrameBufferRaw, channelIDX);

					for(auto it : postProcessors){
						it.second->process(processedFrame);
					}

				}
				++nextDeviceCounter;
			}
		}

		//for(auto it : preProcessors){
		//	it.second->process(frame);
		//}
		
	}



	inline void process(ONI::Frame::BaseFrame& frame) override {

		//const std::lock_guard<std::mutex> lock(mutex);

		unsigned int nextDeviceIndex = nextDeviceCounter % devices.size();

		if(frame.getDeviceTableID() != expectDevceIDXNext[nextDeviceIndex]){
			LOGERROR("Unexpected frame order");
		}else{
			// push or add the multiframe
			multiFrameBuffer[nextDeviceIndex] = *reinterpret_cast<ONI::Frame::Rhs2116Frame*>(&frame);
			//std::swap(multiFrameBuffer[nextDeviceCounter], *reinterpret_cast<Rhs2116Frame*>(&frame));
			if((nextDeviceCounter + 1) % devices.size() == 0){ // we have chexked off all the expectedIDs in order
				// process the multi frame
				ONI::Frame::Rhs2116MultiFrame processedFrame(multiFrameBuffer, channelIDX);

				for(auto it : postProcessors){
					it.second->process(processedFrame);
				}

			}
			++nextDeviceCounter;
		}

	}

	Rhs2116Device* getDevice(const unsigned int& deviceTableIDX){
		auto it = devices.find(deviceTableIDX);
		if(it == devices.end()){
			LOGERROR("No device found for IDX: %i", deviceTableIDX);
			return nullptr;
		}else{
			return it->second;
		}
	}

	std::map<unsigned int, Rhs2116Device*>& getDevices(){
		return devices;
	}

protected:


private:

	std::map<unsigned int, Rhs2116Device*> devices; 

	std::vector<ONI::Frame::Rhs2116Frame> multiFrameBuffer;
	std::vector<ONI::Frame::Rhs2116DataExtended> multiFrameBufferRaw;

	std::vector<unsigned int> expectDevceIDXNext;// = {257,256};
	uint64_t nextDeviceCounter = 0;

	std::vector<size_t> channelIDX;

	bool bEnabled = false;

	ONI::Settings::Rhs2116Format format;
	ONI::Settings::Rhs2116DeviceSettings settings;
	//Rhs2116MultiDeviceConfig config;

};


} // namespace Device
} // namespace ONI