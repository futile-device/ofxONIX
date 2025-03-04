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

#include "ofxFutilities.h"

#pragma once

namespace ONI{


class BurstBuffer{

public:

	virtual ~BurstBuffer(){
		rawBurstBuffer.clear();
	}

	size_t resizeBySamples(const size_t& size, const int& windowSizeMillis, const size_t& numProbes){

		bufferSize = size;
		this->numProbes = numProbes;

		rawBurstBuffer.clear();
		currentBufferIndex = 0;
		bufferSampleCount = 0;

		rawBurstBuffer.resize(numProbes);

		for(size_t probe = 0; probe < numProbes; ++probe){
			rawBurstBuffer[probe].resize(bufferSize * 3);
		}

		bIsFrameNew = false;

		burstIntervalTimer.start<fu::millis>(burstIntervalTimeMs);

		return bufferSize;

	}

	size_t resizeByMillis(const int& bufferSizeMillis, const int& windowSizeMillis, const size_t& numProbes){
		size_t bufferStepFrameSize = windowSizeMillis / framesPerMillis;
		assert(windowSizeMillis != 0);
		size_t bufferFrameSizeRequired = bufferSizeMillis / framesPerMillis / bufferStepFrameSize;
		return resizeBySamples(bufferFrameSizeRequired, windowSizeMillis, numProbes);
	}

	void clear(){
		rawBurstBuffer.clear();
	}

	inline void push(const ONI::Spike& spike){
		//const std::lock_guard<std::mutex> lock(mutex);

		const size_t& probe = spike.probe;
		rawBurstBuffer[probe][currentBufferIndex];
		if(burstIntervalTimer.finished()){
			burstIntervalTimer.restart();
			currentBufferIndex = (currentBufferIndex + 1) % bufferSize;
		}

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

	inline std::vector<std::vector<size_t>>& getUnderlyingBuffer(){ // this actually returns the whole buffer which is 3 times larger than needed
		//const std::lock_guard<std::mutex> lock(mutex);
		return rawBurstBuffer;
	}

	inline size_t& getBurstCountAt(const size_t& probe, const size_t& idx){
		//assert(idx > 0 && idx < bufferSize);
		return rawBurstBuffer[probe][idx + bufferSize];
	}

	inline double getLastBurstRate(const size_t& probe, const size_t& perMillis){
		assert(perMillis % burstIntervalTimeMs == 0);
		size_t steps = perMillis / burstIntervalTimeMs;
		double rate = 0;
		for(size_t i = 0; i < steps; ++i){
			rate += getBurstCountAt(probe, getLastIndex() - i); // no need to worry about -ve index!
		}
		rate /= steps;
		return rate;
	}

	inline size_t& getLastBurstCount(const size_t& probe) {
		return rawBurstBuffer[probe][getLastIndex()];
	}

	const inline size_t getLastIndex(){
	//const std::lock_guard<std::mutex> lock(mutex);
		return (currentBufferIndex - 1) % bufferSize;
	}

	const inline size_t& getCurrentIndex(){
		//const std::lock_guard<std::mutex> lock(mutex);
		return currentBufferIndex;
	}

	const inline size_t& getWindowSizeMs(){
	//const std::lock_guard<std::mutex> lock(mutex);
		return burstIntervalTimeMs;
	}

	const inline size_t& getBufferCount(){
		//const std::lock_guard<std::mutex> lock(mutex);
		return bufferSampleCount;
	}

	const inline size_t& interval(){
		return burstIntervalTimeMs;
	}

	const inline size_t& size(){
		//const std::lock_guard<std::mutex> lock(mutex);
		return bufferSize;//buffer.size();
	}

protected:


	std::vector<std::vector<uint64_t>> rawBurstBuffer;

	size_t currentBufferIndex = 0;
	size_t bufferSampleCount = 0;

	fu::Timer burstIntervalTimer;
	size_t burstIntervalTimeMs = 100;

	size_t bufferSize = 0;
	size_t numProbes = 0;

	bool bIsFrameNew = false;

};


} // namespace ONI

