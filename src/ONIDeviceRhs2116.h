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

#pragma once

//struct ONIProbeStatistics{
//	float sum;
//	float mean;
//	float ss;
//	float variance;
//	float deviation;
//};

class Rhs2116Device : public ONIProbeDevice{

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

	Rhs2116Device(){

		numProbes = 16;							// default for base ONIProbeDevice
		sampleFrequencyHz = 30.1932367151e3;	// default for base ONIProbeDevice
	};
	
	~Rhs2116Device(){
		LOGDEBUG("RHS2116 Device DTOR");
	};

	void deviceSetup(){
		config.setup(this);
		getDspCutOff(true);
		getAnalogLowCutoff(true);
		getAnalogLowCutoffRecovery(true);
		getAnalogHighCutoff(true);
		config.syncSettings();
	}

	bool setEnabled(const bool& b){
		bool bOk = writeRegister(RHS2116_REG::ENABLE, (unsigned int)b);
		if(bOk) getEnabled(true); // updates the cached value
		return bOk;
	}

	bool getEnabled(const bool& bCheckRegisters = true){
		if(bCheckRegisters) bEnabled = (bool)readRegister(RHS2116_REG::ENABLE);
		return bEnabled;
	}

	bool setAnalogLowCutoff(Rhs2116AnalogLowCutoff lowcut){
		const unsigned int * regs = AnalogLowCutoffRegisters[lowcut];
		unsigned int reg = regs[2] << 13 | regs[1] << 7 | regs[0];
		bool bOk = writeRegister(RHS2116_REG::BW2, reg);
		if(bOk) getAnalogLowCutoff(true);
		return bOk;
	}

	Rhs2116AnalogLowCutoff getAnalogLowCutoff(const bool& bCheckRegisters = true){
		if(bCheckRegisters){
			unsigned int bw2 = readRegister(RHS2116_REG::BW2);
			config.getSettings().lowCutoff = getLowCutoffFromReg(bw2);
		}
		return config.getSettings().lowCutoff;
	}

	bool setAnalogLowCutoffRecovery(Rhs2116AnalogLowCutoff lowcut){
		const unsigned int * regs = AnalogLowCutoffRegisters[lowcut];
		unsigned int reg = regs[2] << 13 | regs[1] << 7 | regs[0];
		bool bOk = writeRegister(RHS2116_REG::BW3, reg);
		if(bOk) getAnalogLowCutoffRecovery(true);
		return bOk;
	}

	Rhs2116AnalogLowCutoff getAnalogLowCutoffRecovery(const bool& bCheckRegisters = true){
		if(bCheckRegisters){
			unsigned int bw3 = readRegister(RHS2116_REG::BW3);
			config.getSettings().lowCutoffRecovery = getLowCutoffFromReg(bw3);
		}
		return config.getSettings().lowCutoffRecovery;
	}

	bool setAnalogHighCutoff(Rhs2116AnalogHighCutoff hicut){
		const unsigned int * regs = AnalogHighCutoffRegisters[hicut];
		unsigned int reg0 = regs[1] << 6 | regs[0];
		unsigned int reg1 = regs[3] << 6 | regs[2];
		unsigned int reg2 = AnalogHighCutoffToFastSettleSamples[hicut];
		bool b0 = writeRegister(RHS2116_REG::BW0, reg0);
		bool b1 = writeRegister(RHS2116_REG::BW1, reg1);
		bool b2 = writeRegister(RHS2116_REG::FASTSETTLESAMPLES, reg2);
		bool bOk = (b0 && b1 && b2);
		if(bOk) getAnalogHighCutoff(true);
		return bOk;
	}

	Rhs2116AnalogHighCutoff getAnalogHighCutoff(const bool& bCheckRegisters = true){
		if(bCheckRegisters){
			unsigned int bw0 = readRegister(RHS2116_REG::BW0);
			unsigned int bw1 = readRegister(RHS2116_REG::BW1);
			unsigned int fast = readRegister(RHS2116_REG::FASTSETTLESAMPLES);
			config.getSettings().highCutoff = getHighCutoffFromReg(bw0, bw1, fast);
		}
		return config.getSettings().highCutoff;
	}

	bool setFormat(Rhs2116Format format){
		unsigned int uformat = *(reinterpret_cast<unsigned int*>(&format));
		bool bOk = writeRegister(RHS2116_REG::FORMAT, uformat);
		if(bOk) getFormat(true); // updates the cached value
		return bOk;
	}

	Rhs2116Format getFormat(const bool& bCheckRegisters = true){
		if(bCheckRegisters){
			unsigned int uformat = readRegister(RHS2116_REG::FORMAT);
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
	//	setDrawBufferStride(config.getSettings().drawStride);
	//}

	//void setBufferSizeMillis(const int& millis){
	//	// TODO: I can't seem to change the ADC Bias and MUX settings to get different sample rates
	//	// SO I am just going to assume 30kS/s sample rate for the RHS2116 hard coded to sampleRate
	//	size_t samples = sampleFrequencyHz * millis / 1000;
	//	setBufferSizeSamples(samples);
	//}

	//fu::Timer frameTimer;

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
		if(config.applySettings() || bForceGuiUpdate){
			LOGDEBUG("Rhs2116 Device settings changed");
			bForceGuiUpdate = false;
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
			//setDrawBufferStride(config.getChangedSettings().drawStride);
			//setBufferSizeSamples(config.getChangedSettings().bufferSize);
			config.syncSettings();
		}



		
		
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
			config.syncSettings();
		}
		return bOk;
	}

	inline void process(oni_frame_t* frame){
		mutex.lock();
		Rhs2116Frame processedFrame(frame, (uint64_t)ONIDevice::getAcqDeltaTimeMicros(frame->time));
		mutex.unlock();
		process(processedFrame);
	}

	inline void process(ONIFrame& frame){
		for(auto it : processors){
			it.second->process(frame);
		}
	}

	//uint64_t lastAcquisitionTime = 0;
	//uint64_t lastSampleIDX = 0;
	//uint64_t sampleCounter = 0;
	//uint64_t frameDeltaSFX = 0;
	//float div = 0;
	//inline void process(oni_frame_t* frame){

	//	const std::lock_guard<std::mutex> lock(mutex);
		//for(auto it : processors){
		//	it.second->process(frame);
		//}

	//	memcpy(&rawDataFrames[currentBufferIDX], frame->data, frame->data_sz);  // copy the data payload including hub clock
	//	rawDataFrames[currentBufferIDX].acqTime = frame->time;					// copy the acquisition clock
	//	rawDataFrames[currentBufferIDX].deltaTime = ONIDevice::getAcqDeltaTimeMicros(frame->time); //fu::time::now<fu::micros>() * 1000;
	//	//rawDataFrames[currentBufferIDX].sampleIDX = fu::time::now<fu::micros>();
	//	

	//	for(size_t probe = 0; probe < 16; ++probe){
	//		rawDataFrames[currentBufferIDX].ac_uV[probe] = 0.195f * (rawDataFrames[currentBufferIDX].ac[probe] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
	//		rawDataFrames[currentBufferIDX].dc_mV[probe] = -19.23 * (rawDataFrames[currentBufferIDX].dc[probe] - 512) / 1000.0f; // -19.23 mV × (ADC result – 512) divide by 1000 for V?
	//	}

	//	if(currentBufferIDX % config.getSettings().drawStride == 0){
	//		//for(int i = 0; i < 16; ++i){
	//		//	acProbes[i][currentDrawBufferIDX] =  0.195f * (rawDataFrames[currentBufferIDX].ac[i] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
	//		//	dcProbes[i][currentDrawBufferIDX] = -19.23 * (rawDataFrames[currentBufferIDX].dc[i] - 512) / 1000.0f; // -19.23 mV × (ADC result – 512) divide by 1000 for V?
	//		//}
	//		//deltaTimes[currentDrawBufferIDX] = rawDataFrames[currentBufferIDX].deltaTime;
	//		drawDataFrames[currentDrawBufferIDX] = rawDataFrames[currentBufferIDX];
	//		currentDrawBufferIDX = (currentDrawBufferIDX + 1) % config.getSettings().drawBufferSize;
	//		//frameTimer.stop();
	//		//LOGDEBUG("RHS2116 Frame Timer %f", frameTimer.avg<fu::micros>());
	//		//frameTimer.start();
	//	}

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

	virtual unsigned int readRegister(const Rhs2116Register& reg){
		return ONIDevice::readRegister(reg);
	}

	virtual bool writeRegister(const Rhs2116Register& reg, const unsigned int& value){
		return ONIDevice::writeRegister(reg, value);
	}

	inline void forceGuiUpdate(){
		bForceGuiUpdate = true;
	}

protected:

	inline Rhs2116AnalogLowCutoff getLowCutoffFromReg(unsigned int bw){
		Rhs2116LowCutReg r = *reinterpret_cast<Rhs2116LowCutReg*>(&bw);
		for(size_t i = 0; i < 26; ++i){
			const unsigned int * regs = AnalogLowCutoffRegisters[i];
			if(regs[0] == r.RL_Asel1 &&
			   regs[1] == r.RL_Asel2 &&
			   regs[2] == r.RL_Asel3){
				return (Rhs2116AnalogLowCutoff)(i);
			}
		}
	}

	inline Rhs2116AnalogHighCutoff getHighCutoffFromReg(unsigned int bw0, unsigned int bw1, unsigned int fast){
		Rhs2116HighCutReg r0 = *reinterpret_cast<Rhs2116HighCutReg*>(&bw0);
		Rhs2116HighCutReg r1 = *reinterpret_cast<Rhs2116HighCutReg*>(&bw1);
		for(size_t i = 0; i < 17; ++i){
			const unsigned int * regs = AnalogHighCutoffRegisters[i];
			const unsigned int regf = AnalogHighCutoffToFastSettleSamples[i];
			if(regs[0] == r0.RH1_sel1 && 
			   regs[1] == r0.RH1_sel2 && 
			   regs[2] == r1.RH1_sel1 && 
			   regs[3] == r1.RH1_sel2 && regf == fast){
				return (Rhs2116AnalogHighCutoff)(i);
			}
		}
	}



private:

	//uint64_t hubClockFirst = -1;

	bool bForceGuiUpdate = false;

	//std::vector<Rhs2116DataFrame> rawDataFrames;
	//std::vector<Rhs2116DataFrame> drawDataFrames;

	//size_t currentBufferIDX = 0;
	//size_t currentDrawBufferIDX = 0;

	//size_t lastBufferIDX = 0;

	

	bool bEnabled = false;

	Rhs2116DeviceConfig config;

	//size_t bufferSize = 100;
	//size_t drawBufferSize = 0;
	//size_t drawStride = 100;
	
	Rhs2116Format format;

	//Rhs2116AnalogLowCutoff lowCutoff;
	//Rhs2116AnalogLowCutoff lowCutoffRecovery;
	//Rhs2116AnalogHighCutoff highCutoff;

};

inline std::ostream& operator<<(std::ostream& os, const Rhs2116Format& format) {

	os << "[unsigned: " << *(unsigned int*)&format << "] ";
	os << "[dspCutOff: " << format.dspCutoff << "] ";
	os << "[dsPenable: " << format.dspEnable << "] ";
	os << "[absmode: " << format.absmode << "] ";
	os << "[twoscomp: " << format.twoscomp << "] ";
	os << "[weakMISO: " << format.weakMISO << "] ";
	os << "[digout1HiZ: " << format.digout1HiZ << "] ";
	os << "[digout1: " << format.digout1 << "] ";
	os << "[digout2HiZ: " << format.digout2HiZ << "] ";
	os << "[digout2: " << format.digout2 << "] ";
	os << "[digoutOD: " << format.digoutOD << "]";
	
	return os;
};



//inline bool operator< (const Rhs2116Device::Rhs2116DataFrame& lhs, const Rhs2116Device::Rhs2116DataFrame& rhs) { return lhs.hubTime < rhs.hubTime; } // only using hub time for sorting
//inline bool operator> (const Rhs2116Device::Rhs2116DataFrame& lhs, const Rhs2116Device::Rhs2116DataFrame& rhs) { return rhs < lhs; }
//inline bool operator<=(const Rhs2116Device::Rhs2116DataFrame& lhs, const Rhs2116Device::Rhs2116DataFrame& rhs) { return !(lhs > rhs); }
//inline bool operator>=(const Rhs2116Device::Rhs2116DataFrame& lhs, const Rhs2116Device::Rhs2116DataFrame& rhs) { return !(lhs < rhs); }
//
//inline bool operator==(const Rhs2116Device::Rhs2116DataFrame& lhs, const Rhs2116Device::Rhs2116DataFrame& rhs) { return (lhs.hubTime == rhs.hubTime); }
//inline bool operator!=(const Rhs2116Device::Rhs2116DataFrame& lhs, const Rhs2116Device::Rhs2116DataFrame& rhs) { return !(lhs == rhs); }