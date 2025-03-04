//
//  FrameBuffer.h
//
//  Created by Matt Gingold on 09.02.2024.
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

#include "../Type/SettingTypes.h"
#include "../Type/FrameTypes.h"
#include "../Type/Log.h"

#pragma once

namespace ONI{


class FrameBuffer{

public:

	virtual ~FrameBuffer(){
		rawSpikeBuffer.clear();
	}

	size_t resizeBySamples(const size_t& size, const size_t& step, const size_t& numProbes){

		rawSpikeBuffer.clear();
		bufferSize = size;
		this->numProbes = numProbes;
		rawSpikeBuffer.resize(bufferSize * 3);

		acProbeVoltages.resize(numProbes);
		dcProbeVoltages.resize(numProbes);
		stimProbeData.resize(numProbes);
		spikeProbeData.resize(numProbes);

		for(size_t probe = 0; probe < numProbes; ++probe) {
			acProbeVoltages[probe].resize(bufferSize * 3);
			dcProbeVoltages[probe].resize(bufferSize * 3);
			stimProbeData[probe].resize(bufferSize * 3);
			spikeProbeData[probe].assign(bufferSize * 3, -10);
		}

		this->bufferSampleRateStep = step;
		currentBufferIndex = 0;
		bufferSampleCount = 0;
		bIsFrameNew = false;
		millisPerStep = step * framesPerMillis;
		return bufferSize;

	}

	size_t resizeByMillis(const int& bufferSizeMillis, const int& bufferStepMillis,const size_t& numProbes){

		size_t bufferStepFrameSize = bufferStepMillis / framesPerMillis;
		if(bufferStepMillis == 0) bufferStepFrameSize = 1;
		size_t bufferFrameSizeRequired = bufferSizeMillis / framesPerMillis / bufferStepFrameSize;
		return resizeBySamples(bufferFrameSizeRequired, bufferStepFrameSize, numProbes);
	}

	void clear(){
		rawSpikeBuffer.clear();
		acProbeVoltages.clear();
		dcProbeVoltages.clear();
		stimProbeData.clear();
		spikeProbeData.clear();
	}

	inline bool push(const ONI::Frame::Rhs2116MultiFrame& dataFrame){
		//const std::lock_guard<std::mutex> lock(mutex);

		if(bufferSampleCount % bufferSampleRateStep == 0){

			rawSpikeBuffer[currentBufferIndex] = rawSpikeBuffer[currentBufferIndex + bufferSize] = rawSpikeBuffer[currentBufferIndex + bufferSize * 2] = dataFrame;

			for (size_t probe = 0; probe < numProbes; ++probe) {

				acProbeVoltages[probe][currentBufferIndex] = dataFrame.ac_uV[probe]; //0.195f * (frames1[frame].ac[probe     ] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
				dcProbeVoltages[probe][currentBufferIndex] = dataFrame.dc_mV[probe]; //-19.23 * (frames1[frame].dc[probe     ] - 512) / 1000.0f;   // -19.23 mV × (ADC result – 512) divide by 1000 for V?
				stimProbeData[probe][currentBufferIndex] = (float)dataFrame.stimulation; //frameBuffer[frame].stim ? 8.0f : 0.0f;
				spikeProbeData[probe][currentBufferIndex] = dataFrame.spikes[probe] ? (float)(64 - probe) * 10.0 : -10.0f;
				//if(spikeProbeData[probe][currentBufferIndex] && currentBufferIndex > 0) spikeProbeData[probe][currentBufferIndex - 1] = spikeProbeData[probe][currentBufferIndex];

				acProbeVoltages[probe][currentBufferIndex + bufferSize] = acProbeVoltages[probe][currentBufferIndex + bufferSize * 2] = acProbeVoltages[probe][currentBufferIndex];
				dcProbeVoltages[probe][currentBufferIndex + bufferSize] = dcProbeVoltages[probe][currentBufferIndex + bufferSize * 2] = dcProbeVoltages[probe][currentBufferIndex];
				stimProbeData[probe][currentBufferIndex + bufferSize] = stimProbeData[probe][currentBufferIndex + bufferSize * 2] = stimProbeData[probe][currentBufferIndex];
				spikeProbeData[probe][currentBufferIndex + bufferSize] = spikeProbeData[probe][currentBufferIndex + bufferSize * 2] = spikeProbeData[probe][currentBufferIndex];

			}

			currentBufferIndex = (currentBufferIndex + 1) % bufferSize;
			bIsFrameNew = true;
			++bufferSampleCount;
			return true;
		}
		++bufferSampleCount;
		return false;
	}

	inline bool isFrameNew(const bool& reset = true){ // should I really auto reset? means it can only be called once per cycle
		//const std::lock_guard<std::mutex> lock(mutex);
		if(bIsFrameNew){
			if(reset) bIsFrameNew = false;
			return true;
		}
		return false;
	}

	inline void copySortedBuffer(std::vector<ONI::Frame::Rhs2116MultiFrame>& to){
		if (to.size() != bufferSize) to.resize(bufferSize);
		std::memcpy(&to[0], &rawSpikeBuffer[currentBufferIndex], sizeof(ONI::Frame::Rhs2116MultiFrame) * bufferSize);
	}

	inline std::vector<ONI::Frame::Rhs2116MultiFrame>& getUnderlyingBuffer(){ // this actually returns the whole buffer which is 3 times larger than needed
		//const std::lock_guard<std::mutex> lock(mutex);
		return rawSpikeBuffer;
	}

	inline float* getAcuVFloatRaw(const size_t& probe, const int& from){
		return &acProbeVoltages[probe][from + bufferSize]; // allow negative values by wrapping 
	}

	inline float* getDcmVFloatRaw(const size_t& probe, const int& from){
		return &dcProbeVoltages[probe][from + bufferSize]; // allow negative values by wrapping 
	}

	inline float* getStimFloatRaw(const size_t& probe, const int& from){
		return &stimProbeData[probe][from + bufferSize]; // allow negative values by wrapping 
	}

	inline float* getSpikeFloatRaw(const size_t& probe, const int& from){
		return &spikeProbeData[probe][from + bufferSize]; // allow negative values by wrapping 
	}

	inline ONI::Frame::Rhs2116MultiFrame& getFrameAt(const int& idx){
		//assert(idx > 0 && idx < bufferSize);
		return rawSpikeBuffer[idx + bufferSize];
	}

	inline ONI::Frame::Rhs2116MultiFrame& getCentralFrame(){
		return getFrameAt(std::floor(bufferSize / 2.0));
	}

	inline ONI::Frame::Rhs2116MultiFrame getCentralFrames(){
		return getFrameAt(std::floor(bufferSize / 2.0));
	}

	inline ONI::Frame::Rhs2116MultiFrame& getLastMultiFrame() {
		return rawSpikeBuffer[getLastIndex()];
	}

	const inline size_t getLastIndex(){
	//const std::lock_guard<std::mutex> lock(mutex);
		return (currentBufferIndex - 1) % bufferSize;
	}

	const inline size_t& getCurrentIndex(){
		//const std::lock_guard<std::mutex> lock(mutex);
		return currentBufferIndex;
	}

	const inline size_t& getStep(){
	//const std::lock_guard<std::mutex> lock(mutex);
		return bufferSampleRateStep;
	}

	const inline size_t& getBufferCount(){
		//const std::lock_guard<std::mutex> lock(mutex);
		return bufferSampleCount;
	}

	const inline size_t& size(){
		//const std::lock_guard<std::mutex> lock(mutex);
		return bufferSize;//buffer.size();
	}

	const float& getMillisPerStep(){
		return millisPerStep;
	}

protected:

	std::vector< std::vector<float> > acProbeVoltages;
	std::vector< std::vector<float> > dcProbeVoltages;
	std::vector< std::vector<float> > stimProbeData;
	std::vector< std::vector<float> > spikeProbeData;

	std::vector<ONI::Frame::Rhs2116MultiFrame> rawSpikeBuffer;

	size_t currentBufferIndex = 0;
	size_t bufferSampleCount = 0;
	size_t bufferSampleRateStep = 0;

	size_t bufferSize = 0;
	size_t numProbes = 0;
	float millisPerStep = 0;

	bool bIsFrameNew = false;

};


} // namespace ONI

