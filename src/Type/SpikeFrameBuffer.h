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


class SpikeFrameBuffer{

public:

	virtual ~SpikeFrameBuffer(){};

	size_t resizeBySamples(const size_t& size, const size_t& step, const size_t& numProbes){

		bufferSize = size;
		this->numProbes = numProbes;

		spikeProbeData.resize(numProbes);

		for(size_t probe = 0; probe < numProbes; ++probe) {
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

		spikeProbeData.clear();
	}

	inline bool push(const ONI::Frame::Rhs2116MultiFrame& dataFrame){

		//const std::lock_guard<std::mutex> lock(mutex);
		bool bForceSpike = false;
		int t = (currentBufferIndex - 1) % bufferSize;
		if(t < 0){
			LOGERROR("Negative t time spike buffer");
			t = 0;
		}

		for(size_t probe = 0; probe < numProbes; ++probe) {
			if(dataFrame.spikes[probe]){
				bForceSpike = true;
				spikeProbeData[probe][t] = dataFrame.spikes[probe] ? (float)(64 - probe) * 10.0 : -10.0f;
				spikeProbeData[probe][t + bufferSize] = spikeProbeData[probe][t + bufferSize * 2] = spikeProbeData[probe][t];
			}
		}
		if(bForceSpike) return true;

		if(bufferSampleCount % bufferSampleRateStep == 0){


			for (size_t probe = 0; probe < numProbes; ++probe) {


				spikeProbeData[probe][currentBufferIndex] = dataFrame.spikes[probe] ? (float)(64 - probe) * 10.0 : -10.0f;
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

	inline void setSpike(const size_t& idx, const size_t& probe, const bool& b){
		spikeProbeData[probe][idx] = b ? (float)(64 - probe) * 10.0 : -10.0f;
		spikeProbeData[probe][idx + bufferSize] = spikeProbeData[probe][idx + bufferSize * 2] = spikeProbeData[probe][idx];
	}

	inline bool isFrameNew(const bool& reset = true){ // should I really auto reset? means it can only be called once per cycle
		//const std::lock_guard<std::mutex> lock(mutex);
		if(bIsFrameNew){
			if(reset) bIsFrameNew = false;
			return true;
		}
		return false;
	}


	inline float* getSpikeFloatRaw(const size_t& probe, const int& from){
		return &spikeProbeData[probe][from + bufferSize]; // allow negative values by wrapping 
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


	std::vector< std::vector<float> > spikeProbeData;

	size_t currentBufferIndex = 0;
	size_t bufferSampleCount = 0;
	size_t bufferSampleRateStep = 0;

	size_t bufferSize = 0;
	size_t numProbes = 0;
	float millisPerStep = 0;

	bool bIsFrameNew = false;

};


} // namespace ONI

