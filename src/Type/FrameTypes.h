//
//  BaseDevice.h
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

#include "../Type/Log.h"

#pragma once


#define MAX_NUM_MULTIDEVICES 4
#define MAX_NUM_MULTIPROBES MAX_NUM_MULTIDEVICES * 16


namespace ONI{
namespace Frame{

class BaseFrame{

public:

	virtual ~BaseFrame(){};

	virtual inline void convert(oni_frame_t* frame, const uint64_t& deltaTime = 0) = 0;

	inline const uint32_t& getOnixDeviceTableIDX(){ return deviceTableID; };
	inline const size_t& getNumProbes() const { return numProbes; };
	inline const uint64_t& getDeltaTime() const { return deltaTime; };
	inline const uint64_t& getAcquisitionTime() const { return acqTime; };

protected:


	unsigned int deviceTableID = 0;
	uint64_t deltaTime = 0;
	uint64_t acqTime = 0;
	size_t numProbes = 0;
	
};

struct acquisition_clock_compare {
	inline bool operator() (const ONI::Frame::BaseFrame& lhs, const ONI::Frame::BaseFrame& rhs){
		return (lhs.getAcquisitionTime() < rhs.getAcquisitionTime());
	}
};



struct ProbeStatistics{
	float sum = 0;
	float mean = 0;
	float ss = 0;
	float variance = 0;
	float deviation = 0;
};

struct Rhs2116ProbeData{

	inline void reset(){

		size_t numProbes = acProbeVoltages.size();

		if(numProbes == 0){
			LOGERROR("ProbeData numProbes is not setup - can't reset");
			return;
		}

		size_t numFrames = acProbeVoltages[0].size();

		if(numFrames == 0){
			LOGERROR("ProbeData numFrames is not setup - can't reset");
			return;
		}
		
		resize(numProbes, numFrames, true);

	}

	inline void resize(size_t numProbes, size_t numFrames, bool bForceReset = false){

		if(bForceReset) clear();

		if(acProbeVoltages.size() == numProbes){
			if(acProbeVoltages[0].size() == numFrames){
				return;
			}
		}

		acProbeVoltages.resize(numProbes);
		dcProbeVoltages.resize(numProbes);
		acProbeStats.resize(numProbes);
		dcProbeStats.resize(numProbes);
		probeTimeStamps.resize(numProbes);
		stimProbeData.resize(numProbes);

		for(size_t probe = 0; probe < numProbes; ++probe){
			acProbeVoltages[probe].resize(numFrames);
			dcProbeVoltages[probe].resize(numFrames);
			probeTimeStamps[probe].resize(numFrames);
			stimProbeData[probe].resize(numFrames);
			acProbeStats[probe].deviation = dcProbeStats[probe].deviation = 0;
			acProbeStats[probe].mean = dcProbeStats[probe].mean = 0;
			acProbeStats[probe].ss = dcProbeStats[probe].ss = 0;
			acProbeStats[probe].sum = dcProbeStats[probe].sum = 0;
			acProbeStats[probe].variance = dcProbeStats[probe].variance = 0;
		}

	}

	void clear(){
		acProbeStats.clear();
		dcProbeStats.clear();
		acProbeVoltages.clear();
		dcProbeVoltages.clear();
		probeTimeStamps.clear();
	}

	std::vector< std::vector<float> > acProbeVoltages;
	std::vector< std::vector<float> > dcProbeVoltages;
	std::vector< std::vector<float> > stimProbeData;
	std::vector< std::vector<float> > probeTimeStamps;
	std::vector<ProbeStatistics> acProbeStats;
	std::vector<ProbeStatistics> dcProbeStats;
	float millisPerStep = 0;
};

//// onix.h Frame type
//typedef struct {
//	const oni_fifo_time_t time;     // Frame time (ACQCLKHZ)
//	const oni_fifo_dat_t dev_idx;   // Device index that produced or accepts the frame
//	const oni_fifo_dat_t data_sz;   // Size in bytes of data buffer
//	char *data;                     // Raw data block
//
//} oni_frame_t;

#pragma pack(push, 1)
struct Rhs2116DataExtended{
	uint64_t hubTime;
	uint16_t ac[16];
	uint16_t dc[16];
	uint16_t unused;
	uint64_t acqTime;
	long double deltaTime;
	uint32_t devIdx;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct Rhs2116DataRaw{
	uint64_t time;
	uint32_t dev_idx;
	uint32_t data_sz;
	char data[74];

};
#pragma pack(pop)


class Rhs2116Frame : public ONI::Frame::BaseFrame{

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
		for(size_t probe = 0; probe < numProbes; ++probe){
			ac_mV[probe] = 0.195f * (rawFrame.ac[probe] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
			dc_mV[probe] = -19.23 * (rawFrame.dc[probe] - 512) / 1000.0f;   // -19.23 mV × (ADC result – 512) divide by 1000 for V?
		}
	}

	ONI::Frame::Rhs2116DataExtended rawFrame;

	float ac_mV[16];
	float dc_mV[16];
	bool stim = false;

	// copy assignment (copy-and-swap idiom)
	Rhs2116Frame& Rhs2116Frame::operator=(Rhs2116Frame other) noexcept{
		std::swap(deltaTime, other.deltaTime);
		std::swap(acqTime, other.acqTime);
		std::swap(deviceTableID, other.deviceTableID);
		std::swap(numProbes, other.numProbes);
		std::swap(ac_mV, other.ac_mV);
		std::swap(dc_mV, other.dc_mV);
		std::swap(stim, other.stim);
		std::swap(rawFrame, other.rawFrame);
		return *this;
	}

	inline bool operator==(const Rhs2116Frame& other){ 
		for(size_t probe = 0; probe < numProbes; ++probe){
			if(ac_mV[probe] != other.ac_mV[probe] || dc_mV[probe] != other.ac_mV[probe]){
				return false;
			}
		}
		return (numProbes == other.numProbes && deltaTime == other.deltaTime && stim == other.stim);
	}
	inline bool operator!=(const Rhs2116Frame& other) { return !(this == &other); }

};



class Rhs2116MultiFrame : public ONI::Frame::BaseFrame{

public:

	Rhs2116MultiFrame(){ 
		numProbes = 0;
		for(size_t i = 0; i < MAX_NUM_MULTIPROBES; ++i){
			ac_uV[i] = 0;
			dc_mV[i] = 0;
		}
	};
	Rhs2116MultiFrame(const std::vector<ONI::Frame::Rhs2116DataExtended>& frames, 
					  const std::vector<size_t>& channelMap){
		convert(frames, channelMap); 
	}

	Rhs2116MultiFrame(const std::vector<ONI::Frame::Rhs2116Frame>& frames, const std::vector<size_t>& channelMap){
		//numProbes = 32;//16 * multiFrameBuffer.size();
		convert(frames, channelMap); 
	}
	~Rhs2116MultiFrame(){};

	inline void convert(oni_frame_t* frame, const uint64_t& deltaTime = 0){}; // nothing
	inline void convert(const std::vector<ONI::Frame::Rhs2116Frame>& frames, const std::vector<size_t>& channelMap){
		numProbes = frames.size() * 16;
		//this->multiFrameBuffer = multiFrameBuffer;
		//ac_mV.resize(numProbes);
		//dc_mV.resize(numProbes);
		size_t deviceCount = 0;
		for(const ONI::Frame::Rhs2116Frame& frame : frames){
			for(size_t probe = 0; probe < frame.getNumProbes(); ++probe){
				size_t thisChannelIDX = probe + deviceCount * frame.getNumProbes();
				size_t thisProbeIDX = channelMap[thisChannelIDX];
				ac_uV[thisProbeIDX] = frame.ac_mV[probe];
				dc_mV[thisProbeIDX] = frame.dc_mV[probe];
			}
			++deviceCount;
		}
		this->deltaTime = frames[0].getDeltaTime(); // for now just use device 1??
		this->acqTime = frames[0].getAcquisitionTime();
		this->deviceTableID = 999;
	}

	inline void convert(const std::vector<ONI::Frame::Rhs2116DataExtended>& frames, const std::vector<size_t>& channelMap){

		numProbes = frames.size() * 16;

		for(int i = 0; i < frames.size(); ++i){

			const ONI::Frame::Rhs2116DataExtended& frame = frames[i];

			size_t frameProbe = 0;

			for(size_t probe = i * 16; probe < i * 16 + 16; ++probe){

				size_t mappedProbeIDX = channelMap[probe];

				ac_uV[mappedProbeIDX] = 0.195f * (frame.ac[frameProbe] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
				dc_mV[mappedProbeIDX] = -19.23 * (frame.dc[frameProbe] - 512) / 1000.0f;   // -19.23 mV × (ADC result – 512) divide by 1000 for V?

				++frameProbe;
			}

		}

		this->acqTime = frames[0].acqTime;					 // copy the acquisition clock from the first device?? maybe set sync time?
		this->deltaTime = frames[0].deltaTime;
		this->deviceTableID = frames[0].devIdx;

	}

	float ac_uV[MAX_NUM_MULTIPROBES];
	float dc_mV[MAX_NUM_MULTIPROBES];
	bool stim = false;

	// copy assignment (copy-and-swap idiom)
	Rhs2116MultiFrame& Rhs2116MultiFrame::operator=(Rhs2116MultiFrame other) noexcept{
		std::swap(deltaTime, other.deltaTime);
		std::swap(acqTime, other.acqTime);
		std::swap(deviceTableID, other.deviceTableID);
		std::swap(numProbes, other.numProbes);
		std::swap(ac_uV, other.ac_uV);
		std::swap(dc_mV, other.dc_mV);
		std::swap(stim, other.stim);
		return *this;
	}

	inline bool operator==(const ONI::Frame::Rhs2116MultiFrame& other){ 
		for(size_t probe = 0; probe < numProbes; ++probe){
			if(ac_uV[probe] != other.ac_uV[probe] || dc_mV[probe] != other.ac_uV[probe]){
				return false;
			}
		}
		return (numProbes == other.numProbes && deltaTime == other.deltaTime && stim == other.stim);
	}
	inline bool operator!=(const ONI::Frame::Rhs2116MultiFrame& other) { return !(this == &other); }


};



class HeartBeatFrame : public ONI::Frame::BaseFrame{

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



} // namespace Frame
} // namespace ONI










