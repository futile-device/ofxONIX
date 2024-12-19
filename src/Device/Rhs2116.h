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


#include "ONIDevice.h"
#include "ONIRegister.h"
#include "ONISettingTypes.h"
#include "ONIUtility.h"

#pragma once

class Rhs2116Interface;
class Rhs2116MultiInterface;
class Rhs2116MultiDevice;

class Rhs2116Device : public ONIProbeDevice{

public:

	friend class Rhs2116Interface;
	friend class Rhs2116MultiInterface;
	friend class Rhs2116MultiDevice;

	Rhs2116Device(){
		numProbes = 16;							// default for base ONIProbeDevice
		sampleFrequencyHz = 30.1932367151e3;	// default for base ONIProbeDevice
	};
	
	~Rhs2116Device(){
		LOGDEBUG("RHS2116 Device DTOR");
	};

	void deviceSetup(){
		//config.setup(this);
		//getStepSize(true); 
		//getDspCutOff(true);
		//getAnalogLowCutoff(true);
		//getAnalogLowCutoffRecovery(true);
		//getAnalogHighCutoff(true);
		setStepSize(settings.stepSize);
		setDspCutOff(settings.dspCutoff);
		setAnalogLowCutoff(settings.lowCutoff);
		setAnalogLowCutoffRecovery(settings.lowCutoffRecovery);
		setAnalogHighCutoff(settings.highCutoff);


		/*Rhs2116StimulusStep stepSize = Step10nA;
		Rhs2116DspCutoff dspCutoff = Dsp308Hz;

		Rhs2116AnalogLowCutoff lowCutoff = Low100mHz;
		Rhs2116AnalogLowCutoff lowCutoffRecovery = Low250Hz;
		Rhs2116AnalogHighCutoff highCutoff = High10000Hz;*/
		//config.syncSettings();
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
			settings.lowCutoff = getLowCutoffFromReg(bw2);
		}
		return settings.lowCutoff;
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
			settings.lowCutoffRecovery = getLowCutoffFromReg(bw3);
		}
		return settings.lowCutoffRecovery;
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
			settings.highCutoff = getHighCutoffFromReg(bw0, bw1, fast);
		}
		return settings.highCutoff;
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
			settings.format = *(reinterpret_cast<Rhs2116Format*>(&uformat));
			if(settings.format.dspEnable == 0){
				settings.dspCutoff = Rhs2116DspCutoff::Off; // is this a bug? that seems to be default status
			}else{
				settings.dspCutoff = (Rhs2116DspCutoff)settings.format.dspCutoff;
			}
		}
		return settings.format;
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
		return settings.dspCutoff;
	}
	
	bool setStepSize(Rhs2116StimulusStep stepSize){
		const unsigned int * regs = Rhs2116StimulusStepRegisters[stepSize];
		unsigned int reg = regs[2] << 13 | regs[1] << 7 | regs[0];
		bool bOk = writeRegister(RHS2116_REG::STEPSZ, reg);
		if(bOk) getStepSize(true);
		return bOk;
	}

	Rhs2116StimulusStep getStepSize(const bool& bCheckRegisters = true){
		if(bCheckRegisters){
			unsigned int reg = readRegister(RHS2116_REG::STEPSZ);
			settings.stepSize = getStepSizeFromReg(reg);
		}
		return settings.stepSize;
	}

	//inline void gui(){
	//	//config.gui();
	//	//if(config.applySettings() || bForceGuiUpdate){
	//	//	LOGDEBUG("Rhs2116 Device settings changed");
	//	//	bForceGuiUpdate = false;
	//		//if(settings.dspCutoff != config.getChangedSettings().dspCutoff) setDspCutOff(config.getChangedSettings().dspCutoff);
	//		//if(settings.lowCutoff != config.getChangedSettings().lowCutoff) setAnalogLowCutoff(config.getChangedSettings().lowCutoff);
	//		//if(settings.lowCutoffRecovery != config.getChangedSettings().lowCutoffRecovery) setAnalogLowCutoffRecovery(config.getChangedSettings().lowCutoffRecovery);
	//		//if(settings.highCutoff != config.getChangedSettings().highCutoff) setAnalogHighCutoff(config.getChangedSettings().highCutoff);
	//	//	config.syncSettings();
	//	//}
	//}

	//bool saveConfig(std::string presetName){
	//	//return config.save(presetName);
	//	return false;
	//}

	//bool loadConfig(std::string presetName){
	//	//bool bOk = config.load(presetName);
	//	//if(bOk){
	//	//	setDspCutOff(settings.dspCutoff);
	//	//	setAnalogLowCutoff(settings.lowCutoff);
	//	//	setAnalogLowCutoffRecovery(settings.lowCutoffRecovery);
	//	//	setAnalogHighCutoff(settings.highCutoff);
	//	//	config.syncSettings();
	//	//}
	//	//return bOk;
	//	return false;
	//}

	inline void process(oni_frame_t* frame){
		//const std::lock_guard<std::mutex> lock(mutex);
		if(postFrameProcessors.size() > 0){
			Rhs2116Frame processedFrame(frame, (uint64_t)ONIDevice::getAcqDeltaTimeMicros(frame->time));
			process(processedFrame);
		}
		for(auto it : preFrameProcessors){
			it.second->process(frame);
		}

	}

	inline void process(ONIFrame& frame){
		//const std::lock_guard<std::mutex> lock(mutex);
		for(auto it : postFrameProcessors){
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

	inline Rhs2116StimulusStep getStepSizeFromReg(unsigned int reg){
		Rhs2116StimReg r = *reinterpret_cast<Rhs2116StimReg*>(&reg);
		for(size_t i = 0; i < 10; ++i){
			const unsigned int * regs = Rhs2116StimulusStepRegisters[i];
			if(regs[0] == r.Step_Sel1 &&
			   regs[1] == r.Step_Sel2 &&
			   regs[2] == r.Step_Sel3){
				return (Rhs2116StimulusStep)(i);
			}
		}
	}

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



protected:

	bool bForceGuiUpdate = false;

	bool bEnabled = false;

	//Rhs2116DeviceConfig config;
	
	

	Rhs2116DeviceSettings settings;

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