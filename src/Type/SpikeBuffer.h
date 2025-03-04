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


class SpikeBuffer{

public:

	virtual ~SpikeBuffer(){
		rawSpikeBuffer.clear();
	}

	size_t resizeBySamples(const size_t& size, const size_t& step, const size_t& numProbes){

		bufferSize = size;
		this->numProbes = numProbes;

		rawSpikeBuffer.clear();
		currentBufferIDXs.clear();
		bufferSampleCounts.clear();

		rawSpikeBuffer.resize(numProbes);
		currentBufferIDXs.assign(numProbes, 0);
		bufferSampleCounts.assign(numProbes, 0);
		
		for(size_t probe = 0; probe < numProbes; ++probe){
			rawSpikeBuffer[probe].resize(bufferSize * 3);
		}

		this->bufferSampleRateStep = step;
		bIsFrameNew = false;

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
		currentBufferIDXs.clear();
		bufferSampleCounts.clear();
	}

	inline bool push(const ONI::Spike& spike){
		//const std::lock_guard<std::mutex> lock(mutex);

		const size_t& probe = spike.probe;
		const size_t& currentBufferIndex = currentBufferIDXs[probe];

		if(bufferSampleCounts[probe] % bufferSampleRateStep == 0){

			rawSpikeBuffer[probe][currentBufferIndex] = rawSpikeBuffer[probe][currentBufferIndex + bufferSize] = rawSpikeBuffer[probe][currentBufferIndex + bufferSize * 2] = spike;

			currentBufferIDXs[probe] = (currentBufferIndex + 1) % bufferSize;
			bIsFrameNew = true;
			++bufferSampleCounts[probe];
			return true;
		}
		++bufferSampleCounts[probe];
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
		//if (to.size() != bufferSize) to.resize(bufferSize);
		//std::memcpy(&to[0], &rawSpikeBuffer[currentBufferIndex], sizeof(ONI::Frame::Rhs2116MultiFrame) * bufferSize);
	}

	inline std::vector<std::vector<ONI::Spike>>& getUnderlyingBuffer(){ // this actually returns the whole buffer which is 3 times larger than needed
		//const std::lock_guard<std::mutex> lock(mutex);
		return rawSpikeBuffer;
	}

	inline ONI::Spike& getSpikeAt(const size_t& probe, const size_t& idx){
		//assert(idx > 0 && idx < bufferSize);
		return rawSpikeBuffer[probe][idx + bufferSize];
	}

	inline std::vector<float>& getSpikeWaveAt(const size_t& probe, const size_t& idx){
		return rawSpikeBuffer[probe][idx + bufferSize].rawWaveform;
	}

	inline float* getRawSpikeWaveAt(const size_t& probe, const size_t& idx){
		return rawSpikeBuffer[probe][idx + bufferSize].rawWaveform.data(); // allow negative values by wrapping 
	}

	inline ONI::Spike& getCentralSpike(const size_t& probe){
		return getSpikeAt(probe, std::floor(bufferSize / 2.0));
	}

	inline ONI::Spike& getLastSpike(const size_t& probe) {
		return rawSpikeBuffer[probe][getLastIndex()];
	}

	const inline size_t getLastIndex(const size_t& probe){
	//const std::lock_guard<std::mutex> lock(mutex);
		return (currentBufferIDXs[probe] - 1) % bufferSize;
	}

	const inline size_t& getCurrentIndex(const size_t& probe){
		//const std::lock_guard<std::mutex> lock(mutex);
		return currentBufferIDXs[probe];
	}

	const inline size_t& getStep(){
	//const std::lock_guard<std::mutex> lock(mutex);
		return bufferSampleRateStep;
	}

	const inline size_t& getBufferCount(const size_t& probe){
		//const std::lock_guard<std::mutex> lock(mutex);
		return bufferSampleCounts[probe];
	}

	const inline size_t& size(){
		//const std::lock_guard<std::mutex> lock(mutex);
		return bufferSize;//buffer.size();
	}

protected:


	std::vector<std::vector<ONI::Spike>> rawSpikeBuffer;

	std::vector<size_t> currentBufferIDXs;
	std::vector<size_t> bufferSampleCounts;

	size_t bufferSampleRateStep = 0;

	size_t bufferSize = 0;
	size_t numProbes = 0;

	bool bIsFrameNew = false;

};


} // namespace ONI

