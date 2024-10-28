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



class Rhs2116Device : public ONIProbeDevice{

public:

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
	
	inline void gui(){
		
		config.gui();
		if(config.applySettings() || bForceGuiUpdate){
			LOGDEBUG("Rhs2116 Device settings changed");
			bForceGuiUpdate = false;
			if(config.getSettings().dspCutoff != config.getChangedSettings().dspCutoff) setDspCutOff(config.getChangedSettings().dspCutoff);
			if(config.getSettings().lowCutoff != config.getChangedSettings().lowCutoff) setAnalogLowCutoff(config.getChangedSettings().lowCutoff);
			if(config.getSettings().lowCutoffRecovery != config.getChangedSettings().lowCutoffRecovery) setAnalogLowCutoffRecovery(config.getChangedSettings().lowCutoffRecovery);
			if(config.getSettings().highCutoff != config.getChangedSettings().highCutoff) setAnalogHighCutoff(config.getChangedSettings().highCutoff);
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
				LOGINFO("AnalogHighCutoff %i", i);
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