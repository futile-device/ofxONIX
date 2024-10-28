//
//  ONIDevice.h
//
//  Created by Matt Gingold on 13.09.2024.
//

#include "oni.h"
#include "onix.h"

//#include "ofMain.h"
//#include "ofxFutilities.h"

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

#include "ONIUtility.h"

#pragma once

#pragma pack(push, 1)
struct Rhs2116RawDataFrame{
	uint64_t hubTime;
	uint16_t ac[16];
	uint16_t dc[16];
	uint16_t unused;
	uint64_t acqTime;
	//long double deltaTime;
	//float ac_uV[16];
	//float dc_mV[16];
	//uint64_t sampleIDX;
};
#pragma pack(pop)


class ONIFrame{

public:

	//ONIFrame(oni_frame_t* frame, const uint64_t& deltaTime = 0){
	//	convert(frame, deltaTime);
	//}
	virtual ~ONIFrame(){};

	virtual inline void convert(oni_frame_t* frame, const uint64_t& deltaTime = 0) = 0;

	inline const unsigned int& getDeviceTableID(){ return deviceTableID; };
	inline const size_t& getNumProbes() const { return numProbes; };
	inline const uint64_t& getDeltaTime() const { return deltaTime; };
	inline const uint64_t& getAcquisitionTime() const { return acqTime; };

protected:

	
	unsigned int deviceTableID = 0;
	uint64_t deltaTime = 0;
	uint64_t acqTime = 0;
	size_t numProbes = 0;

};

class Rhs2116Frame : public ONIFrame{

public:

	Rhs2116Frame(){ numProbes = 16; };
	Rhs2116Frame(oni_frame_t* frame, const uint64_t& deltaTime = 0){
		numProbes = 16;
		convert(frame, deltaTime); 
	}
	~Rhs2116Frame(){};

	inline void convert(oni_frame_t* frame, const uint64_t& deltaTime = 0){
		memcpy(&rawFrame, frame->data, frame->data_sz);  // copy the data payload including hub clock
		this->acqTime = rawFrame.acqTime = frame->time;					 // copy the acquisition clock
		this->deltaTime = deltaTime;
		this->deviceTableID = frame->dev_idx;
		//for(size_t probe = 0; probe < numProbes; ++probe){
		//	ac_uV[probe] = 0.195f * (rawFrame.ac[probe] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
		//	dc_mV[probe] = -19.23 * (rawFrame.dc[probe] - 512) / 1000.0f;   // -19.23 mV × (ADC result – 512) divide by 1000 for V?
		//}
	}

	Rhs2116RawDataFrame rawFrame;

	float ac_uV[16];
	float dc_mV[16];

	inline bool operator==(const Rhs2116Frame& other){ 
		for(size_t probe = 0; probe < numProbes; ++probe){
			if(ac_uV[probe] != other.ac_uV[probe] || dc_mV[probe] != other.ac_uV[probe]){
				return false;
			}
		}
		return (numProbes == other.numProbes && deltaTime == other.deltaTime);
	}
	inline bool operator!=(const Rhs2116Frame& other) { return !(this == &other); }

};

#define MAX_NUM_MULTIPROBES 64

class Rhs2116MultiFrame : public ONIFrame{

public:

	Rhs2116MultiFrame(){ 
		numProbes = 0; 
	};
	Rhs2116MultiFrame(const std::vector<Rhs2116Frame>& frames, const std::vector<size_t>& channelMap){
		//numProbes = 32;//16 * frames.size();
		convert(frames, channelMap); 
	}
	~Rhs2116MultiFrame(){};

	inline void convert(oni_frame_t* frame, const uint64_t& deltaTime = 0){}; // nothing
	inline void convert(const std::vector<Rhs2116Frame>& frames, const std::vector<size_t>& channelMap){
		numProbes = frames.size() * 16;
		//this->frames = frames;
		//ac_uV.resize(numProbes);
		//dc_mV.resize(numProbes);
		size_t deviceCount = 0;
		for(const Rhs2116Frame& frame : frames){
			for(size_t probe = 0; probe < frame.getNumProbes(); ++probe){
				size_t thisChannelIDX = probe + deviceCount * frame.getNumProbes();
				size_t thisProbeIDX = channelMap[thisChannelIDX];
				ac_uV[thisProbeIDX] = frame.ac_uV[probe];
				dc_mV[thisProbeIDX] = frame.dc_mV[probe];
			}
			++deviceCount;
		}
		this->deltaTime = frames[0].getDeltaTime(); // for now just use device 1??
		this->acqTime = frames[0].getAcquisitionTime();
		this->deviceTableID = 999;
	}

	//Rhs2116Frame frames;

	float ac_uV[MAX_NUM_MULTIPROBES];
	float dc_mV[MAX_NUM_MULTIPROBES];

	// copy assignment (copy-and-swap idiom)
	Rhs2116MultiFrame& Rhs2116MultiFrame::operator=(Rhs2116MultiFrame other) noexcept{
		std::swap(deltaTime, other.deltaTime);
		std::swap(acqTime, other.acqTime);
		std::swap(deviceTableID, other.deviceTableID);
		std::swap(numProbes, other.numProbes);
		std::swap(ac_uV, other.ac_uV);
		std::swap(dc_mV, other.dc_mV);
		return *this;
	}

	inline bool operator==(const Rhs2116MultiFrame& other){ 
		for(size_t probe = 0; probe < numProbes; ++probe){
			if(ac_uV[probe] != other.ac_uV[probe] || dc_mV[probe] != other.ac_uV[probe]){
				return false;
			}
		}
		return (numProbes == other.numProbes && deltaTime == other.deltaTime);
	}
	inline bool operator!=(const Rhs2116MultiFrame& other) { return !(this == &other); }

};

struct acquisition_clock_compare {
	inline bool operator() (const ONIFrame& lhs, const ONIFrame& rhs){
		return (lhs.getAcquisitionTime() < rhs.getAcquisitionTime());
	}
};

class HeartBeatFrame : public ONIFrame{

public:

	HeartBeatFrame(){ numProbes = 0; };
	HeartBeatFrame(oni_frame_t* frame, const uint64_t& deltaTime = 0){
		numProbes = 0; convert(frame, deltaTime); 
	}
	~HeartBeatFrame(){};

	inline void convert(oni_frame_t* frame, const uint64_t& deltaTime = 0){
		//memcpy(&rawFrame, frame->data, frame->data_sz);  // copy the data payload including hub clock
		//rawFrame.acqTime = frame->time;					 // copy the acquisition clock
		this->acqTime = frame->time;
		this->deltaTime = deltaTime;
	}

	//Rhs2116RawDataFrame rawFrame;

	inline bool operator==(const HeartBeatFrame& other){ 
		return (numProbes == other.numProbes && deltaTime == other.deltaTime);
	}
	inline bool operator!=(const HeartBeatFrame& other) { return !(this == &other); }

};

//class FmcFrame : public ONIFrame{
//
//	FmcFrame(){ numProbes = 0; };
//	~FmcFrame(){};
//
//	inline void convert(oni_frame_t* frame, const uint64_t& deltaTime = 0){
//		// nothing ??
//	}
//
//	Rhs2116RawDataFrame rawFrame;
//
//	float ac_uV[16];
//	float dc_mV[16];
//
//	inline bool operator==(const Rhs2116Frame& other){ 
//		for(size_t probe = 0; probe < numProbes; ++probe){
//			if(ac_uV[probe] != other.ac_uV[probe] || dc_mV[probe] != other.ac_uV[probe]){
//				return false;
//			}
//		}
//		return (numProbes == other.numProbes);
//	}
//	inline bool operator!=(const Rhs2116Frame& other) { return !(this == &other); }
//
//};
