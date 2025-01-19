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

#include "../Type/SettingTypes.h"
#include "../Type/Log.h"

#pragma once

namespace ONI{



template<typename DataType>
class DataBuffer{

public:

	virtual ~DataBuffer(){
		buffer.clear();
	}

	//void setup(const std::string& name, size_t size){
	//	this->name = name;
	//	resize(size);
	//}

	//const inline std::string& getName(){
	//	return name;
	//}

	size_t resize(const size_t& size, const size_t& step){
		//const std::lock_guard<std::mutex> lock(mutex);
		buffer.clear();
		bufferSize = size;
		buffer.resize(bufferSize * 3);
		this->bufferSampleRateStep = step;
		currentBufferIndex = 0;
		bufferSampleCount = 0;
		bIsFrameNew = false;
		return bufferSize;
	}

	size_t resize(const int& bufferSizeMillis, const int& bufferStepMillis, const size_t& sampleFrequencyHz){
		long double framesPerMillis = 1.0 / sampleFrequencyHz * 1000.0;
		size_t bufferStepFrameSize = std::floor(bufferStepMillis / framesPerMillis);
		if(bufferStepMillis == 0) bufferStepFrameSize = 1;
		size_t bufferFrameSizeRequired = std::floor(bufferSizeMillis / framesPerMillis / bufferStepFrameSize);
		return resize(bufferFrameSizeRequired, bufferStepFrameSize);
	}

	void clear(){
		resize(0, 0);
	}

	inline void push(const DataType& data){
		//const std::lock_guard<std::mutex> lock(mutex);
		if(bufferSampleCount % bufferSampleRateStep == 0){
			buffer[currentBufferIndex] = data;
			buffer[currentBufferIndex + bufferSize] = data;
			buffer[currentBufferIndex + bufferSize * 2] = data;
			currentBufferIndex = (currentBufferIndex + 1) % bufferSize;
			bIsFrameNew = true;
		}
		++bufferSampleCount;
	}

	inline bool isFrameNew(){ // should I really auto reset? means it can only be called once per cycle
		//const std::lock_guard<std::mutex> lock(mutex);
		if(bIsFrameNew){
			bIsFrameNew = false;
			return true;
		}
		return false;
	}

	/*inline DataType& at(const size_t& index){
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
	}*/

	inline void copySortedBuffer(std::vector<DataType>& to){
		//to.resize(bufferSize); // is this better than if(to.size() != bufferSize) ?
		size_t fromIDX = currentBufferIndex; //((currentBufferIndex - 1) % bufferSize) + bufferSize;
		size_t toIDX = fromIDX + bufferSize;
		to.assign(buffer.begin() + fromIDX, buffer.begin() + toIDX);
		//std::reverse(to.begin(), to.end());
		//size_t size = sizeof(DataType) * bufferSize;
		//std::memcpy(&to[0], &buffer[index], size);
	}

	inline std::vector<DataType>& getUnderlyingBuffer(){ // this actually returns the whole buffer which is 3 times larger than needed
		//const std::lock_guard<std::mutex> lock(mutex);
		return buffer;
	}

	const inline size_t& getCurrentIndex(){
		//const std::lock_guard<std::mutex> lock(mutex);
		return currentBufferIndex;
	}

	const inline size_t& getBufferCount(){
		//const std::lock_guard<std::mutex> lock(mutex);
		return bufferSampleCount;
	}

	const inline size_t& size(){
		//const std::lock_guard<std::mutex> lock(mutex);
		return bufferSize;//buffer.size();
	}

protected:

	//std::mutex mutex;

	//std::string name = "UNKNOWN";

	std::vector<DataType> buffer;

	size_t currentBufferIndex = 0;
	size_t bufferSampleCount = 0;
	size_t bufferSampleRateStep = 0;

	size_t bufferSize = 0;

	bool bIsFrameNew = false;

};


} // namespace ONI




//typedef ONIDataBuffer<Rhs2116DataFrame> Rhs2116FrameBuffer;