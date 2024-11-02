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

#include "ONISettingTypes.h"
#include "ONIUtility.h"

#pragma once

template<typename DataType>
class ONIDataBuffer{

public:

	virtual ~ONIDataBuffer(){
		buffer.clear();
	}

	void setup(const std::string& name, size_t size){
		this->name = name;
		resize(size);
	}

	inline const std::string& getName(){
		return name;
	}

	size_t resize(const size_t& size, const size_t& step){
		const std::lock_guard<std::mutex> lock(mutex);
		buffer.clear();
		buffer.resize(size);
		this->bufferSampleRateStep = step;
		currentBufferIndex = 0;
		bufferSampleCount = 0;
		return buffer.size();
	}

	size_t resize(const int& bufferSizeMillis, const int& bufferStepMillis, const size_t& sampleFrequencyHz){
		long double framesPerMillis = 1.0 / sampleFrequencyHz * 1000.0;
		size_t bufferStepFrameSize = bufferStepMillis / framesPerMillis;
		if(bufferStepMillis == 0) bufferStepFrameSize = 1;
		size_t bufferFrameSizeRequired = bufferSizeMillis / framesPerMillis / bufferStepFrameSize;
		return resize(bufferFrameSizeRequired, bufferStepFrameSize);
	}

	void clear(){
		resize(0, 0);
	}

	inline void push(const DataType& data){
		const std::lock_guard<std::mutex> lock(mutex);
		if(bufferSampleCount % bufferSampleRateStep == 0){
			buffer[currentBufferIndex] = data;
			currentBufferIndex = (currentBufferIndex + 1) % buffer.size();
			bIsFrameNew = true;
		}
		++bufferSampleCount;
	}

	inline bool isFrameNew(){ // should I really auto reset? means it can only be called once per cycle
		const std::lock_guard<std::mutex> lock(mutex);
		bool b = bIsFrameNew;
		bIsFrameNew = false;
		return b;
	}

	inline DataType& at(const size_t& index){
		const std::lock_guard<std::mutex> lock(mutex);
		return buffer[index];
	}

	inline DataType& getCurrentBuffer(){
		const std::lock_guard<std::mutex> lock(mutex);
		return buffer[currentBufferIndex];
	}

	inline DataType& getLastBuffer(){
		const std::lock_guard<std::mutex> lock(mutex);
		return buffer[getLastIndex()];
	}

	inline DataType& getNextBuffer(){
		const std::lock_guard<std::mutex> lock(mutex);
		return buffer[nextIDX()];
	}

	inline size_t getLastIndex(){
		return getOffsetIndex(-1);
	}

	inline size_t getNextIndex(){
		return getOffsetIndex(+1);
	}

	inline size_t getOffsetIndex(const size_t& offset){
		const std::lock_guard<std::mutex> lock(mutex);
		return (currentBufferIndex + offset) % buffer.size();
	}

	inline std::vector<DataType>& getBuffer(){
		const std::lock_guard<std::mutex> lock(mutex);
		return buffer;
	}

	inline size_t getCurrentIndex(){
		const std::lock_guard<std::mutex> lock(mutex);
		return currentBufferIndex;
	}

	inline size_t getBufferCount(){
		const std::lock_guard<std::mutex> lock(mutex);
		return bufferSampleCount;
	}

	inline size_t size(){
		const std::lock_guard<std::mutex> lock(mutex);
		return buffer.size();
	}

protected:

	std::mutex mutex;

	std::string name = "UNKNOWN";

	std::vector<DataType> buffer;

	size_t currentBufferIndex = 0;
	size_t bufferSampleCount = 0;
	size_t bufferSampleRateStep = 0;

	bool bIsFrameNew = false;

};


//typedef ONIDataBuffer<Rhs2116DataFrame> Rhs2116FrameBuffer;