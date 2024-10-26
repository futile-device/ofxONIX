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


#include "ONIConfig.h"
#include "ONIDevice.h"
#include "ONIRegister.h"
#include "ONISettingTypes.h"
#include "ONIUtility.h"

#include "ONIDeviceRhs2116.h"

#pragma once


class Rhs2116MultiDevice : public Rhs2116Device{

public:

	//struct acquisition_clock_compare {
	//	inline bool operator() (const Rhs2116DataFrame& lhs, const Rhs2116DataFrame& rhs){
	//		return (lhs.acqTime < rhs.acqTime);
	//	}
	//};

	//struct hub_clock_compare {
	//	inline bool operator() (const Rhs2116DataFrame& lhs, const Rhs2116DataFrame& rhs){
	//		return (lhs.hubTime < rhs.hubTime);
	//	}
	//};

	Rhs2116MultiDevice(){
		numProbes = 0;							// default for base ONIProbeDevice
		sampleFrequencyHz = 30.1932367151e3;	// default for base ONIProbeDevice
	};
	
	~Rhs2116MultiDevice(){
		LOGDEBUG("RHS2116Multi Device DTOR");
		//const std::lock_guard<std::mutex> lock(mutex);
	};


	virtual void setup(volatile oni_ctx* context, uint64_t acq_clock_khz){
		LOGDEBUG("Setting up device RHS2116 MULTI");
		ctx = context;
		//deviceType = type;
		deviceType.idx = 999;
		deviceType.id = RHS2116MULTI;
		deviceTypeID = RHS2116MULTI;
		std::ostringstream os; os << ONIDeviceTypeIDStr(deviceTypeID) << " (" << deviceType.idx << ")";
		deviceName = os.str();
		this->acq_clock_khz = acq_clock_khz;
		reset(); // always call device specific setup/reset
		deviceSetup(); // always call device specific setup/reset
	}

	std::map<unsigned int, Rhs2116Device*> devices; 

	void addDevice(Rhs2116Device* device){
		auto it = devices.find(device->getDeviceTableID());
		if(it == devices.end()){
			LOGINFO("Adding device %s", device->getName().c_str());
			devices[device->getDeviceTableID()] = device;
		}else{
			LOGERROR("Device already added: %s", device->getName().c_str());
		}
		std::string processorName = deviceName + " MULTI PROC";
		device->subscribeProcessor(processorName, this);
		numProbes += device->getNumProbes();
		channelIDX.resize(numProbes);
		lastDeviceIDX = device->getDeviceTableID();
	}

	void deviceSetup(){
		config.setup(this);
		for(auto it : devices){
			it.second->deviceSetup();
		}
		config.syncSettings();
	}



	//const std::vector<Rhs2116DataFrame>& getRawDataFrames(){
	//	const std::lock_guard<std::mutex> lock(mutex);
	//	return rawDataFrames;
	//}

	//const std::vector<Rhs2116DataFrame>& getDrawDataFrames(){
	//	const std::lock_guard<std::mutex> lock(mutex);
	//	return drawDataFrames;
	//}

	//Rhs2116DataFrame getLastDataFrame(){
	//	const std::lock_guard<std::mutex> lock(mutex);
	//	return rawDataFrames[lastBufferIDX];
	//}

	//void setDrawBufferStride(const size_t& samples){
	//	mutex.lock();
	//	config.getSettings().drawStride = samples;
	//	config.getSettings().drawBufferSize = config.getSettings().bufferSize / config.getSettings().drawStride;
	//	currentDrawBufferIDX = 0;
	//	drawDataFrames.clear();
	//	drawDataFrames.resize(config.getSettings().drawBufferSize);
	//	mutex.unlock();
	//	LOGINFO("Draw Buffer size: %i (samples)", config.getSettings().drawBufferSize);
	//}

	//void setBufferSizeSamples(const size_t& samples){
	//	mutex.lock();
	//	config.getSettings().bufferSize = samples;
	//	currentBufferIDX = 0;
	//	rawDataFrames.clear();
	//	rawDataFrames.resize(config.getSettings().bufferSize);
	//	mutex.unlock();
	//	LOGINFO("Raw  Buffer size: %i (samples)", config.getSettings().bufferSize);
	//	//setDrawBufferStride(config.getSettings().drawStride);
	//}

	//void setBufferSizeMillis(const int& millis){
	//	// TODO: I can't seem to change the ADC Bias and MUX settings to get different sample rates
	//	// SO I am just going to assume 30kS/s sample rate for the RHS2116 hard coded to sampleRate
	//	size_t samples = sampleFrequencyHz * millis / 1000;
	//	setBufferSizeSamples(samples);
	//}

	////fu::Timer frameTimer;

	//double getFrameTimerAvg(){
	//	/*
	//	const std::lock_guard<std::mutex> lock(mutex);
	//	frameTimer.stop();
	//	double t = frameTimer.avg<fu::micros>();
	//	frameTimer.start();
	//	return t;
	//	*/
	//	return 0;
	//}

	//std::vector<ONIProbeStatistics> probeStats;

	//uint64_t cnt = 0;
	
	inline void gui(){
		
		config.gui();
		if(config.applySettings()){
			LOGDEBUG("Rhs2116 Multi Device settings changed");
			//getDspCutOff(true);
			//getAnalogLowCutoff(true);
			//getAnalogLowCutoffRecovery(true);
			//getAnalogHighCutoff(true);
			//getDrawBufferStride(true);
			//getBufferSizeSamples(true);
			if(config.getSettings().dspCutoff != config.getChangedSettings().dspCutoff) setDspCutOff(config.getChangedSettings().dspCutoff);
			if(config.getSettings().lowCutoff != config.getChangedSettings().lowCutoff) setAnalogLowCutoff(config.getChangedSettings().lowCutoff);
			if(config.getSettings().lowCutoffRecovery != config.getChangedSettings().lowCutoffRecovery) setAnalogLowCutoffRecovery(config.getChangedSettings().lowCutoffRecovery);
			if(config.getSettings().highCutoff != config.getChangedSettings().highCutoff) setAnalogHighCutoff(config.getChangedSettings().highCutoff);
			//if(config.getSettings().bufferSize != config.getChangedSettings().bufferSize) setBufferSizeSamples(config.getChangedSettings().bufferSize);
			if(config.getSettings().channelMap != config.getChangedSettings().channelMap){
				config.getSettings().channelMap = config.getChangedSettings().channelMap;
				channelMapToIDX();
			}
			//setDrawBufferStride(config.getChangedSettings().drawStride);
			//setBufferSizeSamples(config.getChangedSettings().bufferSize);
			config.syncSettings();

			for(auto it : devices){
				it.second->deviceSetup();
			}

		}

	}

	void setChannelMap(const std::vector<size_t>& map){
		if(map.size() == numProbes){
			channelIDX = map;
			for(size_t y = 0; y < numProbes; ++y){
				for(size_t x = 0; x < numProbes; ++x){
					config.getSettings().channelMap[y][x] = false;
				}
				size_t xx = channelIDX[y];
				config.getSettings().channelMap[y][xx] = true;
			}
			config.syncSettings();
		}else{
			LOGERROR("Channel IDXs size doesn't equal number of probes");
		}
	}

	void channelMapToIDX(){
		if(config.getSettings().channelMap.size() == 0) return;
		for(size_t y = 0; y < numProbes; ++y){
			size_t idx = 0;
			for(size_t x = 0; x < numProbes; ++x){
				if(config.getSettings().channelMap[y][x]){
					idx = x;
					break;
				}
			}
			channelIDX[y] = idx;
		}

		ostringstream os;
		for(int i = 0; i < channelIDX.size(); i++){
			os << channelIDX[i] << (i == channelIDX.size() - 1 ? "" : ", ");
		}
		fu::debug << os.str() << fu::endl;
	}

	bool saveConfig(std::string presetName){
		return config.save(presetName);
	}

	bool loadConfig(std::string presetName){
		bool bOk = config.load(presetName);
		if(bOk){
			setDspCutOff(config.getSettings().dspCutoff);
			setAnalogLowCutoff(config.getSettings().lowCutoff);
			setAnalogLowCutoffRecovery(config.getSettings().lowCutoffRecovery);
			setAnalogHighCutoff(config.getSettings().highCutoff);
			//setDrawBufferStride(config.getSettings().drawStride);
			//setBufferSizeSamples(config.getSettings().bufferSize);
			channelMapToIDX();
			for(auto it : devices){
				it.second->deviceSetup();
			}
			config.syncSettings();
		}
		return bOk;
	}

	std::vector<Rhs2116Frame> frames;
	unsigned int lastDeviceIDX = 0;
	inline void process(oni_frame_t* frame) override {


		
	}

	inline void process(ONIFrame& frame) override {

		mutex.lock();
		//LOGINFO("Incoming %i", frame.getDeviceTableID());
		//mutex.unlock();
		if(frames.size() < devices.size()){
			if(frames.size() == 0 && frame.getDeviceTableID() != devices.begin()->first){ // it's not starting with 256? TODO: seems like the FPGA iterates backwards?
				mutex.unlock();
				LOGERROR("First Rhs2116 multi frame is not: %i %i", frame.getDeviceTableID(), devices.begin()->first);
				return;
			}
			//LOGINFO("Push %i", frame.getDeviceTableID());
			frames.push_back(*reinterpret_cast<Rhs2116Frame*>(&frame));
		}
		if(frames.size() == devices.size()){
			//LOGINFO("Process %i", frame.getDeviceTableID());
			Rhs2116MultiFrame processedFrame(frames, channelIDX);
			frames.clear();
			
			for(auto it : processors){
				it.second->process(processedFrame);
			}
		}

		mutex.unlock();
	}

	//uint64_t lastAcquisitionTime = 0;
	//uint64_t lastSampleIDX = 0;
	//uint64_t sampleCounter = 0;
	//uint64_t frameDeltaSFX = 0;
	//float div = 0;  
	//inline void process(oni_frame_t* frame){

	//	const std::lock_guard<std::mutex> lock(mutex);
	//	
	//	return;

	//	for(auto it : processors){
	//		it.second->process(frame);
	//	}

	//	memcpy(&rawDataFrames[currentBufferIDX], frame->data, frame->data_sz);  // copy the data payload including hub clock
	//	rawDataFrames[currentBufferIDX].acqTime = frame->time;					// copy the acquisition clock
	//	rawDataFrames[currentBufferIDX].deltaTime = ONIDevice::getAcqDeltaTimeMicros(frame->time); //fu::time::now<fu::micros>() * 1000;
	//	//rawDataFrames[currentBufferIDX].sampleIDX = fu::time::now<fu::micros>();
	//	

	//	for(size_t probe = 0; probe < 16; ++probe){
	//		rawDataFrames[currentBufferIDX].ac_uV[probe] = 0.195f * (rawDataFrames[currentBufferIDX].ac[probe] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
	//		rawDataFrames[currentBufferIDX].dc_mV[probe] = -19.23 * (rawDataFrames[currentBufferIDX].dc[probe] - 512) / 1000.0f; // -19.23 mV × (ADC result – 512) divide by 1000 for V?
	//	}

	//	//if(currentBufferIDX % config.getSettings().drawStride == 0){
	//	//	//for(int i = 0; i < 16; ++i){
	//	//	//	acProbes[i][currentDrawBufferIDX] =  0.195f * (rawDataFrames[currentBufferIDX].ac[i] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
	//	//	//	dcProbes[i][currentDrawBufferIDX] = -19.23 * (rawDataFrames[currentBufferIDX].dc[i] - 512) / 1000.0f; // -19.23 mV × (ADC result – 512) divide by 1000 for V?
	//	//	//}
	//	//	//deltaTimes[currentDrawBufferIDX] = rawDataFrames[currentBufferIDX].deltaTime;
	//	//	drawDataFrames[currentDrawBufferIDX] = rawDataFrames[currentBufferIDX];
	//	//	currentDrawBufferIDX = (currentDrawBufferIDX + 1) % config.getSettings().drawBufferSize;
	//	//	//frameTimer.stop();
	//	//	//LOGDEBUG("RHS2116 Frame Timer %f", frameTimer.avg<fu::micros>());
	//	//	//frameTimer.start();
	//	//}

	//	// nice way to sort
	//	//size_t sorted_idx = currentBufferIDX;
	//	//for(size_t i = 0; i < n_frames; ++i){
	//	//	to[i] = rawDataFrames[sorted_idx];
	//	//	sorted_idx = (sorted_idx + stride) % bufferSize;
	//	//}

	//	//for(size_t probe = 0; probe < 16; ++probe){
	//	//	probeStats[probe].sum = 0;
	//	//	for(size_t frame = 0; frame < bufferSize; ++frame){
	//	//		probeStats[probe].sum += rawDataFrames[frame].ac[probe];
	//	//	}
	//	//	
	//	//	probeStats[probe].mean = probeStats[probe].sum / bufferSize;
	//	//	probeStats[probe].mean = 0.195f * (probeStats[probe].mean - 32768) / 1000.0f;
	//	//	for(size_t frame = 0; frame < bufferSize; ++frame){
	//	//		float acdiff = 0.195f * (rawDataFrames[frame].ac[probe] - 32768) / 1000.0f - probeStats[probe].mean;
	//	//		probeStats[probe].ss += acdiff * acdiff; 
	//	//	}
	//	//	probeStats[probe].variance = probeStats[probe].ss / (bufferSize - 1);  // use population (N) or sample (n-1) deviation?
	//	//	probeStats[probe].deviation = sqrt(probeStats[probe].variance);
	//	//}
	//	
	//	//long double deltaFrameAcqTime = (rawDataFrames[currentBufferIDX].acqTime - rawDataFrames[lastBufferIDX].acqTime) / (long double)acq_clock_khz * 1000000;
	//	//double deltaFrameTimer = frameTimer.elapsed<fu::micros>();
	//	//fu::debug << rawDataFrames[currentBufferIDX].acqTime - rawDataFrames[lastBufferIDX].acqTime << " " << deltaFrameTimer << " " << deltaFrameAcqTime << fu::endl;
	//	//if(deltaFrameTimer > 1000000.0 / sampleRatekSs) LOGDEBUG("RHS2116 Frame Buffer Underrun %f", deltaFrameTimer);
	//	

	//	//lastBufferIDX = currentBufferIDX;
	//	/*
	//	frameDeltaSFX = frameTimer.elapsed<fu::micros>(); //(rawDataFrames[currentBufferIDX].sampleIDX - lastSampleIDX);
	//	if(frameDeltaSFX > 5) {
	//		div = (float)frameDeltaSFX / sampleCounter;
	//		//LOGDEBUG("Framt: %i %i %f", frameDeltaSFX, sampleCounter, (float)frameDeltaSFX / sampleCounter );
	//		sampleCounter = 0;
	//	}
	//	sampleCounter++;
	//	frameTimer.start();
	//	float frameDeltaAQX = (rawDataFrames[currentBufferIDX].acqTime - lastAcquisitionTime) / (float)acq_clock_khz * 1000000;
	//	if(frameDeltaAQX > 33.333333333333333f) LOGDEBUG("Underflow: %f", frameDeltaAQX);
	//	*/

	//	//if(currentBufferIDX % 30000 == 0) LOGDEBUG("Delta %s: %i %f %i %i %i", deviceName.c_str(), frameDeltaSFX, frameDeltaAQX, sampleIDX, lastSampleIDX, rawDataFrames[currentBufferIDX].sampleIDX);

	//	lastAcquisitionTime = rawDataFrames[currentBufferIDX].acqTime;
	//	lastSampleIDX = rawDataFrames[currentBufferIDX].sampleIDX;
	//	
	//	currentBufferIDX = (currentBufferIDX + 1) % config.getSettings().bufferSize;

	//	//frameTimer.fcount();

	//	//ofSleepMillis(1);
	//	//sortedDataFrames = rawDataFrames;
	//	//std::sort(sortedDataFrames.begin(), sortedDataFrames.end(), acquisition_clock_compare());
	//	//++sampleCount;

	//}

	unsigned int readRegister(const Rhs2116Register& reg) override {
		std::vector<unsigned int> regs; 
		regs.resize(devices.size());
		size_t n = 0;
		for(auto it : devices){
			regs[n] = it.second->readRegister(reg);
			++n;
		}
		for(size_t i = 0; i < regs.size(); ++i){
			for(size_t j = 0; j < regs.size(); ++j){
				if(regs[i] != regs[j]){
					LOGERROR("REGISTER MISMATCH!!!!!!!!!!!!!!!!!!!!!!!");
				}
			}
		}
		return regs[0]; // TODO: we should do something if they mismatch like force setting the register?
	}

	bool writeRegister(const Rhs2116Register& reg, const unsigned int& value) override {
		bool bOk = true;
		for(auto it : devices){
			bOk = it.second->writeRegister(reg, value);
		}
		return bOk;
	}

protected:


private:

	//uint64_t hubClockFirst = -1;

	

	/*std::vector<Rhs2116DataFrame> rawDataFrames;
	std::vector<Rhs2116DataFrame> drawDataFrames;

	size_t currentBufferIDX = 0;
	size_t currentDrawBufferIDX = 0;

	size_t lastBufferIDX = 0;*/

	std::vector<size_t> channelIDX;

	bool bEnabled = false;

	Rhs2116MultiDeviceConfig config;

	//size_t bufferSize = 100;
	//size_t drawBufferSize = 0;
	//size_t drawStride = 100;
	
	Rhs2116Format format;

	//Rhs2116AnalogLowCutoff lowCutoff;
	//Rhs2116AnalogLowCutoff lowCutoffRecovery;
	//Rhs2116AnalogHighCutoff highCutoff;

};




//inline bool operator< (const RHS2116_REG::Rhs2116DataFrame& lhs, const RHS2116_REG::Rhs2116DataFrame& rhs) { return lhs.hubTime < rhs.hubTime; } // only using hub time for sorting
//inline bool operator> (const RHS2116_REG::Rhs2116DataFrame& lhs, const RHS2116_REG::Rhs2116DataFrame& rhs) { return rhs < lhs; }
//inline bool operator<=(const RHS2116_REG::Rhs2116DataFrame& lhs, const RHS2116_REG::Rhs2116DataFrame& rhs) { return !(lhs > rhs); }
//inline bool operator>=(const RHS2116_REG::Rhs2116DataFrame& lhs, const RHS2116_REG::Rhs2116DataFrame& rhs) { return !(lhs < rhs); }
//
//inline bool operator==(const RHS2116_REG::Rhs2116DataFrame& lhs, const RHS2116_REG::Rhs2116DataFrame& rhs) { return (lhs.hubTime == rhs.hubTime); }
//inline bool operator!=(const RHS2116_REG::Rhs2116DataFrame& lhs, const RHS2116_REG::Rhs2116DataFrame& rhs) { return !(lhs == rhs); }

/*
bool setEnabled(const bool& b){
	bool bOk = true;
	for(auto it : devices){
		bOk = it.second->setEnabled(b);
	}
	return bOk;
}

bool getEnabled(const bool& bCheckRegisters = true){
	if(bCheckRegisters){
		for(auto it : devices){
			bEnabled = (bool)it.second->readRegister(RHS2116_REG::ENABLE);  //TODO: really this should be somke kind of check to align values!!
		}
	}
	return bEnabled;
}

bool setAnalogLowCutoff(Rhs2116AnalogLowCutoff lowcut){
	const unsigned int * regs = AnalogLowCutoffRegisters[lowcut];
	unsigned int reg = regs[2] << 13 | regs[1] << 7 | regs[0];
	bool bOk = true;
	for(auto it : devices){
		bOk = it.second->writeRegister(RHS2116_REG::BW2, reg);
	}
	if(bOk) getAnalogLowCutoff(true);
	return bOk;
}

Rhs2116AnalogLowCutoff getAnalogLowCutoff(const bool& bCheckRegisters = true){
	if(bCheckRegisters){
		unsigned int bw2 = 0;
		for(auto it : devices){
			bw2 = it.second->readRegister(RHS2116_REG::BW2); //TODO: really this should be somke kind of check to align values!!
		}
		config.getSettings().lowCutoff = getLowCutoffFromReg(bw2);
	}
	return config.getSettings().lowCutoff;
}

bool setAnalogLowCutoffRecovery(Rhs2116AnalogLowCutoff lowcut){
	const unsigned int * regs = AnalogLowCutoffRegisters[lowcut];
	unsigned int reg = regs[2] << 13 | regs[1] << 7 | regs[0];
	bool bOk = true;
	for(auto it : devices){
		bOk = it.second->writeRegister(RHS2116_REG::BW3, reg);
	}
	if(bOk) getAnalogLowCutoffRecovery(true);
	return bOk;
}

Rhs2116AnalogLowCutoff getAnalogLowCutoffRecovery(const bool& bCheckRegisters = true){
	if(bCheckRegisters){
		unsigned int bw3 = 0;
		for(auto it : devices){
			bw3 = it.second->readRegister(RHS2116_REG::BW3); //TODO: really this should be somke kind of check to align values!!
		}
		config.getSettings().lowCutoffRecovery = getLowCutoffFromReg(bw3);
	}
	return config.getSettings().lowCutoffRecovery;
}

bool setAnalogHighCutoff(Rhs2116AnalogHighCutoff hicut){
	const unsigned int * regs = AnalogHighCutoffRegisters[hicut];
	unsigned int reg0 = regs[1] << 6 | regs[0];
	unsigned int reg1 = regs[3] << 6 | regs[2];
	unsigned int reg2 = AnalogHighCutoffToFastSettleSamples[hicut];
	bool bOk = true;
	for(auto it : devices){
		bool b0 = it.second->writeRegister(RHS2116_REG::BW0, reg0);
		bool b1 = it.second->writeRegister(RHS2116_REG::BW1, reg1);
		bool b2 = it.second->writeRegister(RHS2116_REG::FASTSETTLESAMPLES, reg2);
		bOk = (b0 && b1 && b2);
	}
	if(bOk) getAnalogHighCutoff(true);
	return bOk;
}

Rhs2116AnalogHighCutoff getAnalogHighCutoff(const bool& bCheckRegisters = true){
	if(bCheckRegisters){
		unsigned int bw0 = 0;
		unsigned int bw1 = 0;
		unsigned int fast = 0;
		for(auto it : devices){
			bw0 = it.second->readRegister(RHS2116_REG::BW0); //TODO: really this should be somke kind of check to align values!!
			bw1 = it.second->readRegister(RHS2116_REG::BW1);
			fast = it.second->readRegister(RHS2116_REG::FASTSETTLESAMPLES);
		}
		config.getSettings().highCutoff = getHighCutoffFromReg(bw0, bw1, fast);
	}
	return config.getSettings().highCutoff;
}

bool setFormat(Rhs2116Format format){
	unsigned int uformat = *(reinterpret_cast<unsigned int*>(&format));
	bool bOk = true;
	for(auto it : devices){
		bOk = it.second->writeRegister(RHS2116_REG::FORMAT, uformat);
	}
	if(bOk) getFormat(true); // updates the cached value
	return bOk;
}

Rhs2116Format getFormat(const bool& bCheckRegisters = true){
	if(bCheckRegisters){
		unsigned int uformat = 0;
		for(auto it : devices){
			uformat = it.second->readRegister(RHS2116_REG::FORMAT); //TODO: really this should be somke kind of check to align values!!
		}
		format = *(reinterpret_cast<Rhs2116Format*>(&uformat));
		if(format.dspEnable == 0){
			config.getSettings().dspCutoff = Rhs2116DspCutoff::Off; // is this a bug? that seems to be default status
		}else{
			config.getSettings().dspCutoff = (Rhs2116DspCutoff)format.dspCutoff;
		}
	}
	return format;
}

bool setDspCutOff(Rhs2116DspCutoff cutoff){
	Rhs2116Format format = getFormat(true);
	if(cutoff == Rhs2116DspCutoff::Off){
		format.dspEnable = 0;
		format.dspCutoff = Rhs2116DspCutoff::Dsp146mHz; // is this a bug? that seems to be default status
	}else{
		format.dspEnable = 1;
		format.dspCutoff = cutoff;
	}
	return setFormat(format);

}

const Rhs2116DspCutoff& getDspCutOff(const bool& bCheckRegisters = true){
	getFormat(bCheckRegisters);
	return config.getSettings().dspCutoff;
}
*/