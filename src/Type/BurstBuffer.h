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
		const std::lock_guard<std::mutex> lock(mutex);

		bufferSize = size;
		this->numProbes = numProbes;

		
		currentBufferIndex = 0;
		bufferSampleCount = 0;

		rawBurstBuffer.clear();
		rawSPSABuffer.clear();

		rawBurstBuffer.resize(numProbes);
		rawSPSABuffer.assign(bufferSize * 3, 0);

		for(size_t probe = 0; probe < numProbes; ++probe){
			rawBurstBuffer[probe].assign(bufferSize * 3, 0);
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
		const std::lock_guard<std::mutex> lock(mutex);
		rawBurstBuffer.clear();
	}

	inline void push(const ONI::Spike& spike){
		const std::lock_guard<std::mutex> lock(mutex);

		const size_t& probe = spike.probe;
		rawBurstBuffer[probe][currentBufferIndex] = rawBurstBuffer[probe][currentBufferIndex + bufferSize] = rawBurstBuffer[probe][currentBufferIndex + bufferSize * 2] = rawBurstBuffer[probe][currentBufferIndex] + 1;

	}

	inline void updateClock(){
		const std::lock_guard<std::mutex> lock(mutex);
		if(burstIntervalTimer.finished()){
			burstIntervalTimer.restart();
			double totes = 0;
			for(int probe = 0; probe < numProbes; ++probe){
				totes += getCurrentBurstRatePSA2(probe, 1000);
			}
			rawSPSABuffer[currentBufferIndex] = rawSPSABuffer[currentBufferIndex + bufferSize] = rawSPSABuffer[currentBufferIndex + bufferSize * 2] = totes;
			++bufferSampleCount;
			currentBufferIndex = (currentBufferIndex + 1) % bufferSize;
		}
	}

	inline bool isFrameNew(const bool& reset = true){ // should I really auto reset? means it can only be called once per cycle
		const std::lock_guard<std::mutex> lock(mutex);
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

	inline double* getRawSPSA(const int& from){
		const std::lock_guard<std::mutex> lock(mutex);
		return &rawSPSABuffer[from + bufferSize]; // allow negative values by wrapping 
	}

	inline std::vector<std::vector<double>>& getUnderlyingBuffer(){ // this actually returns the whole buffer which is 3 times larger than needed
		const std::lock_guard<std::mutex> lock(mutex);
		return rawBurstBuffer;
	}

	inline double& getBurstCountAt(const size_t& probe, const int& idx){
		const std::lock_guard<std::mutex> lock(mutex);
		//assert(idx > 0 && idx < bufferSize);
		return rawBurstBuffer[probe][idx + bufferSize];
	}

	inline double getCurrentBurstRatePSA2(const size_t& probe, const size_t& windowIntervalMs){
		//const std::lock_guard<std::mutex> lock(mutex);
		assert(windowIntervalMs % burstIntervalTimeMs == 0);
		size_t steps = std::min(windowIntervalMs / burstIntervalTimeMs, bufferSampleCount); // make sure we actually have enough sample windows
		if(steps == 0) return 0;
		// 30000/100 =     100 * 100 = 10000;
		size_t sum = 0;
		for(size_t i = 0; i < steps; ++i){
			sum += rawBurstBuffer[probe][(currentBufferIndex - 1) % bufferSize - i + bufferSize]; // no need to worry about -ve index!
		}
		double totalTimeMs = steps * burstIntervalTimeMs;
		double rateMs = sum / totalTimeMs;
		double ratePS = rateMs * 1000;
		return ratePS;
	}

	inline double getCurrentBurstRatePSA(const size_t& probe, const size_t& windowIntervalMs){
		const std::lock_guard<std::mutex> lock(mutex);
		assert(windowIntervalMs % burstIntervalTimeMs == 0);
		size_t steps = std::min(windowIntervalMs / burstIntervalTimeMs, bufferSampleCount); // make sure we actually have enough sample windows
		if(steps == 0) return 0;
		// 30000/100 =     100 * 100 = 10000;
		size_t sum = 0;
		for(size_t i = 0; i < steps; ++i){
			sum += rawBurstBuffer[probe][(currentBufferIndex - 1) % bufferSize - i + bufferSize]; // no need to worry about -ve index!
		}
		double totalTimeMs = steps * burstIntervalTimeMs;
		double rateMs = sum / totalTimeMs;
		double ratePS = rateMs * 1000;
		return ratePS;
	}

	inline double& getLastBurstCount(const size_t& probe){
		const std::lock_guard<std::mutex> lock(mutex);
		return rawBurstBuffer[probe][(currentBufferIndex - 1) % bufferSize];
	}

	const inline size_t getLastIndex(){
		const std::lock_guard<std::mutex> lock(mutex);
		return (currentBufferIndex - 1) % bufferSize;
	}

	const inline size_t& getCurrentIndex(){
		const std::lock_guard<std::mutex> lock(mutex);
		return currentBufferIndex;
	}

	const inline size_t& getWindowSizeMs(){
		const std::lock_guard<std::mutex> lock(mutex);
		return burstIntervalTimeMs;
	}

	const inline size_t& getBufferCount(){
		const std::lock_guard<std::mutex> lock(mutex);
		return bufferSampleCount;
	}

	const inline size_t& interval(){
		const std::lock_guard<std::mutex> lock(mutex);
		return burstIntervalTimeMs;
	}

	const inline size_t& size(){
		const std::lock_guard<std::mutex> lock(mutex);
		return bufferSize;//buffer.size();
	}

protected:


	std::vector<std::vector<double>> rawBurstBuffer;
	std::vector<double> rawSPSABuffer;

	size_t currentBufferIndex = 0;
	size_t bufferSampleCount = 0;

	fu::Timer burstIntervalTimer;
	size_t burstIntervalTimeMs = 100;

	size_t bufferSize = 0;
	size_t numProbes = 0;

	bool bIsFrameNew = false;

	std::mutex mutex;

};


} // namespace ONI

