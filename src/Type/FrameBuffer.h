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
		rawFrameBuffer.clear();
	}

	size_t resizeBySamples(const size_t& size, const size_t& step, const size_t& numProbes){
		//const std::lock_guard<std::mutex> lock(mutex);
		rawFrameBuffer.clear();
		bufferSize = size;
		this->numProbes = numProbes;
		rawFrameBuffer.resize(bufferSize * 3);
		probeData.resize(numProbes, bufferSize * 3, true);
		
		this->bufferSampleRateStep = step;
		currentBufferIndex = 0;
		bufferSampleCount = 0;
		bIsFrameNew = false;
		probeData.millisPerStep = step * framesPerMillis;
		return bufferSize;
	}

	constexpr long double framesPerMillis = 1.0 / RHS2116_SAMPLE_FREQUENCY_HZ * 1000.0;

	size_t resizeByMillis(const int& bufferSizeMillis, const int& bufferStepMillis,const size_t& numProbes){

		size_t bufferStepFrameSize = bufferStepMillis / framesPerMillis;
		if(bufferStepMillis == 0) bufferStepFrameSize = 1;
		size_t bufferFrameSizeRequired = bufferSizeMillis / framesPerMillis / bufferStepFrameSize;
		return resizeBySamples(bufferFrameSizeRequired, bufferStepFrameSize, numProbes);
	}

	void clear(){
		rawFrameBuffer.clear();
		probeData.clear();
	}

	inline void push(const ONI::Frame::Rhs2116MultiFrame& dataFrame){
		//const std::lock_guard<std::mutex> lock(mutex);

		if(bufferSampleCount % bufferSampleRateStep == 0){

			ONI::Frame::Rhs2116MultiFrame lastDataFrame = rawFrameBuffer[currentBufferIndex];

			rawFrameBuffer[currentBufferIndex] = rawFrameBuffer[currentBufferIndex + bufferSize] = rawFrameBuffer[currentBufferIndex + bufferSize * 2] = dataFrame;

			for (size_t probe = 0; probe < numProbes; ++probe) {
				
				//probeData.probeTimeStamps[probe][currentBufferIndex] = uint64_t((dataFrame.getDeltaTime() - dataFrame.getDeltaTime()) / 1000);
				probeData.acProbeVoltages[probe][currentBufferIndex] = dataFrame.ac_uV[probe]; //0.195f * (frames1[frame].ac[probe     ] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
				probeData.dcProbeVoltages[probe][currentBufferIndex] = dataFrame.dc_mV[probe]; //-19.23 * (frames1[frame].dc[probe     ] - 512) / 1000.0f;   // -19.23 mV × (ADC result – 512) divide by 1000 for V?
				probeData.stimProbeData[probe][currentBufferIndex] = (float)dataFrame.stim; //frameBuffer[frame].stim ? 8.0f : 0.0f;

				//probeData.probeTimeStamps[probe][currentBufferIndex + bufferSize] = probeData.probeTimeStamps[probe][currentBufferIndex + bufferSize * 2] = probeData.probeTimeStamps[probe][currentBufferIndex];
				probeData.acProbeVoltages[probe][currentBufferIndex + bufferSize] = probeData.acProbeVoltages[probe][currentBufferIndex + bufferSize * 2] = probeData.acProbeVoltages[probe][currentBufferIndex];
				probeData.dcProbeVoltages[probe][currentBufferIndex + bufferSize] = probeData.dcProbeVoltages[probe][currentBufferIndex + bufferSize * 2] = probeData.dcProbeVoltages[probe][currentBufferIndex];
				probeData.stimProbeData[probe][currentBufferIndex + bufferSize] = probeData.stimProbeData[probe][currentBufferIndex + bufferSize * 2] = probeData.stimProbeData[probe][currentBufferIndex];


				//probeData.acProbeStats[probe].sum += dataFrame.ac_uV[probe] - lastDataFrame.ac_uV[probe];
				//probeData.dcProbeStats[probe].sum += dataFrame.dc_mV[probe] - lastDataFrame.dc_mV[probe];

				//probeData.acProbeStats[probe].mean = probeData.acProbeStats[probe].sum / bufferSize;
				//probeData.dcProbeStats[probe].mean = probeData.dcProbeStats[probe].sum / bufferSize;

				//probeData.acProbeStats[probe].ss = 0;
				//probeData.dcProbeStats[probe].ss = 0;

				//for (size_t frame = 0; frame < frameCount; ++frame) {
				//	float acdiff = probeData.acProbeVoltages[probe][frame] - probeData.acProbeStats[probe].mean;
				//	float dcdiff = probeData.dcProbeVoltages[probe][frame] - probeData.dcProbeStats[probe].mean;
				//	probeData.acProbeStats[probe].ss += acdiff * acdiff;
				//	probeData.dcProbeStats[probe].ss += dcdiff * dcdiff;
				//}

				//probeData.acProbeStats[probe].variance = probeData.acProbeStats[probe].ss / (frameCount - 1);  // use population (N) or sample (n-1) deviation?
				//probeData.acProbeStats[probe].deviation = sqrt(probeData.acProbeStats[probe].variance);

				//probeData.dcProbeStats[probe].variance = probeData.dcProbeStats[probe].ss / (frameCount - 1);  // use population (N) or sample (n-1) deviation?
				//probeData.dcProbeStats[probe].deviation = sqrt(probeData.dcProbeStats[probe].variance);

			}

			currentBufferIndex = (currentBufferIndex + 1) % bufferSize;
			bIsFrameNew = true;
		}
		++bufferSampleCount;
	}

	inline bool isFrameNew(const bool& reset = true){ // should I really auto reset? means it can only be called once per cycle
		//const std::lock_guard<std::mutex> lock(mutex);
		if(bIsFrameNew){
			if(reset) bIsFrameNew = false;
			return true;
		}
		return false;
	}

	inline void copySortedProbeData(ONI::Frame::Rhs2116ProbeData& to){

		to.resize(numProbes, bufferSize, false);
		to.millisPerStep = probeData.millisPerStep;

		for (size_t probe = 0; probe < numProbes; ++probe) {
			std::memcpy(&to.acProbeVoltages[probe][0], &probeData.acProbeVoltages[probe][currentBufferIndex], sizeof(float) * bufferSize);
			std::memcpy(&to.dcProbeVoltages[probe][0], &probeData.dcProbeVoltages[probe][currentBufferIndex], sizeof(float) * bufferSize);
			//std::memcpy(&to.probeTimeStamps[probe][0], &probeData.probeTimeStamps[probe][currentBufferIndex], sizeof(float) * bufferSize);
			std::memcpy(&to.stimProbeData[probe][0], &probeData.stimProbeData[probe][currentBufferIndex], sizeof(float) * bufferSize);
			std::memcpy(&to.acProbeStats[probe], &probeData.acProbeStats[probe], sizeof(ONI::Frame::ProbeStatistics));
			std::memcpy(&to.dcProbeStats[probe], &probeData.dcProbeStats[probe], sizeof(ONI::Frame::ProbeStatistics));

			for(size_t frame = 0; frame < bufferSize; ++frame) {
				to.acProbeStats[probe].sum += to.acProbeVoltages[probe][frame];
				to.dcProbeStats[probe].sum += to.dcProbeVoltages[probe][frame];
			}

			to.acProbeStats[probe].mean = to.acProbeStats[probe].sum / bufferSize;
			to.dcProbeStats[probe].mean = to.dcProbeStats[probe].sum / bufferSize;

			to.acProbeStats[probe].ss = 0;
			to.dcProbeStats[probe].ss = 0;

			for (size_t frame = 0; frame < bufferSize; ++frame) {
				float acdiff = to.acProbeVoltages[probe][frame] - to.acProbeStats[probe].mean;
				float dcdiff = to.dcProbeVoltages[probe][frame] - to.dcProbeStats[probe].mean;
				to.acProbeStats[probe].ss += acdiff * acdiff;
				to.dcProbeStats[probe].ss += dcdiff * dcdiff;
			}

			to.acProbeStats[probe].variance = to.acProbeStats[probe].ss / (bufferSize - 1);  // use population (N) or sample (n-1) deviation?
			to.acProbeStats[probe].deviation = sqrt(to.acProbeStats[probe].variance);

			to.dcProbeStats[probe].variance = to.dcProbeStats[probe].ss / (bufferSize - 1);  // use population (N) or sample (n-1) deviation?
			to.dcProbeStats[probe].deviation = sqrt(to.dcProbeStats[probe].variance);

		}

	}

	inline void copySortedBuffer(std::vector<ONI::Frame::Rhs2116MultiFrame>& to){
		if (to.size() != bufferSize) to.resize(bufferSize);
		std::memcpy(&to[0], &rawFrameBuffer[currentBufferIndex], sizeof(ONI::Frame::Rhs2116MultiFrame) * bufferSize);
	}

	inline std::vector<ONI::Frame::Rhs2116MultiFrame>& getUnderlyingBuffer(){ // this actually returns the whole buffer which is 3 times larger than needed
		//const std::lock_guard<std::mutex> lock(mutex);
		return rawFrameBuffer;
	}

	inline float* getAcuVFloatRaw(const size_t& probe, const size_t& from){
		return &probeData.acProbeVoltages[probe][from + bufferSize]; // allow negative values by wrapping 
	}

	inline ONI::Frame::Rhs2116MultiFrame& getFrameAt(const size_t& idx){
		//assert(idx > 0 && idx < bufferSize);
		return rawFrameBuffer[idx];
	}

	inline ONI::Frame::Rhs2116MultiFrame& getCentralFrame(){
		return getFrameAt(std::floor(bufferSize / 2.0));
	}

	inline ONI::Frame::Rhs2116MultiFrame getCentralFrames(){
		return getFrameAt(std::floor(bufferSize / 2.0));
	}

	inline ONI::Frame::Rhs2116MultiFrame& getLastMultiFrame() {
		return rawFrameBuffer[getLastIndex()];
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

protected:

	//std::mutex mutex;

	//std::string name = "UNKNOWN";

	std::vector<ONI::Frame::Rhs2116MultiFrame> rawFrameBuffer;
	ONI::Frame::Rhs2116ProbeData probeData;
	//std::vector< std::vector<float> > probeTimeStamps;

	size_t currentBufferIndex = 0;
	size_t bufferSampleCount = 0;
	size_t bufferSampleRateStep = 0;

	size_t bufferSize = 0;
	size_t numProbes = 0;

	bool bIsFrameNew = false;

};


} // namespace ONI




//typedef ONIDataBuffer<Rhs2116DataFrame> Rhs2116FrameBuffer;