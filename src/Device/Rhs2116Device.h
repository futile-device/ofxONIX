//
//  Rhs2116Device.h
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


#include "../Device/BaseDevice.h"

#pragma once

namespace ONI{

namespace Interface{
class Rhs2116Interface;
class Rhs2116MultiInterface;
} // namespace Interface

namespace Device{

class Rhs2116MultiDevice;


class Rhs2116Device : public ONI::Device::ProbeDevice{

public:

	friend class ONI::Interface::Rhs2116Interface;
	friend class ONI::Interface::Rhs2116MultiInterface;

	friend class ONI::Device::Rhs2116MultiDevice;

	Rhs2116Device(){
		numProbes = 16;							// default for base ONIProbeDevice
		sampleFrequencyHz = 30.1932367151e3;	// default for base ONIProbeDevice
	};
	
	~Rhs2116Device(){
		LOGDEBUG("RHS2116 Device DTOR");
	};

	void deviceSetup(){
		setStepSize(settings.stepSize);
		setDspCutOff(settings.dspCutoff);
		setAnalogLowCutoff(settings.lowCutoff);
		setAnalogLowCutoffRecovery(settings.lowCutoffRecovery);
		setAnalogHighCutoff(settings.highCutoff);
	}

	bool setEnabled(const bool& b){
		bool bOk = writeRegister(ONI::Register::Rhs2116::ENABLE, (unsigned int)b);
		if(bOk) getEnabled(true); // updates the cached value
		return bOk;
	}

	bool getEnabled(const bool& bCheckRegisters = true){
		if(bCheckRegisters) bEnabled = (bool)readRegister(ONI::Register::Rhs2116::ENABLE);
		return bEnabled;
	}

	bool setAnalogLowCutoff(ONI::Settings::Rhs2116AnalogLowCutoff lowcut){
		const unsigned int * regs = ONI::Settings::Rhs2116AnalogLowCutoffRegisters[lowcut];
		unsigned int reg = regs[2] << 13 | regs[1] << 7 | regs[0];
		bool bOk = writeRegister(ONI::Register::Rhs2116::BW2, reg);
		if(bOk) getAnalogLowCutoff(true);
		return bOk;
	}

	ONI::Settings::Rhs2116AnalogLowCutoff getAnalogLowCutoff(const bool& bCheckRegisters = true){
		if(bCheckRegisters){
			unsigned int bw2 = readRegister(ONI::Register::Rhs2116::BW2);
			settings.lowCutoff = getLowCutoffFromReg(bw2);
		}
		return settings.lowCutoff;
	}

	bool setAnalogLowCutoffRecovery(ONI::Settings::Rhs2116AnalogLowCutoff lowcut){
		const unsigned int * regs = ONI::Settings::Rhs2116AnalogLowCutoffRegisters[lowcut];
		unsigned int reg = regs[2] << 13 | regs[1] << 7 | regs[0];
		bool bOk = writeRegister(ONI::Register::Rhs2116::BW3, reg);
		if(bOk) getAnalogLowCutoffRecovery(true);
		return bOk;
	}

	ONI::Settings::Rhs2116AnalogLowCutoff getAnalogLowCutoffRecovery(const bool& bCheckRegisters = true){
		if(bCheckRegisters){
			unsigned int bw3 = readRegister(ONI::Register::Rhs2116::BW3);
			settings.lowCutoffRecovery = getLowCutoffFromReg(bw3);
		}
		return settings.lowCutoffRecovery;
	}

	bool setAnalogHighCutoff(ONI::Settings::Rhs2116AnalogHighCutoff hicut){
		const unsigned int * regs = ONI::Settings::Rhs2116AnalogHighCutoffRegisters[hicut];
		unsigned int reg0 = regs[1] << 6 | regs[0];
		unsigned int reg1 = regs[3] << 6 | regs[2];
		unsigned int reg2 = ONI::Settings::Rhs2116AnalogHighCutoffToFastSettleSamples[hicut];
		bool b0 = writeRegister(ONI::Register::Rhs2116::BW0, reg0);
		bool b1 = writeRegister(ONI::Register::Rhs2116::BW1, reg1);
		bool b2 = writeRegister(ONI::Register::Rhs2116::FASTSETTLESAMPLES, reg2);
		bool bOk = (b0 && b1 && b2);
		if(bOk) getAnalogHighCutoff(true);
		return bOk;
	}

	ONI::Settings::Rhs2116AnalogHighCutoff getAnalogHighCutoff(const bool& bCheckRegisters = true){
		if(bCheckRegisters){
			unsigned int bw0 = readRegister(ONI::Register::Rhs2116::BW0);
			unsigned int bw1 = readRegister(ONI::Register::Rhs2116::BW1);
			unsigned int fast = readRegister(ONI::Register::Rhs2116::FASTSETTLESAMPLES);
			settings.highCutoff = getHighCutoffFromReg(bw0, bw1, fast);
		}
		return settings.highCutoff;
	}

	bool setFormat(ONI::Settings::Rhs2116Format format){
		unsigned int uformat = *(reinterpret_cast<unsigned int*>(&format));
		bool bOk = writeRegister(ONI::Register::Rhs2116::FORMAT, uformat);
		if(bOk) getFormat(true); // updates the cached value
		return bOk;
	}

	ONI::Settings::Rhs2116Format getFormat(const bool& bCheckRegisters = true){
		if(bCheckRegisters){
			unsigned int uformat = readRegister(ONI::Register::Rhs2116::FORMAT);
			settings.format = *(reinterpret_cast<ONI::Settings::Rhs2116Format*>(&uformat));
			if(settings.format.dspEnable == 0){
				settings.dspCutoff = ONI::Settings::Rhs2116DspCutoff::Off; // is this a bug? that seems to be default status
			}else{
				settings.dspCutoff = (ONI::Settings::Rhs2116DspCutoff)settings.format.dspCutoff;
			}
		}
		return settings.format;
	}

	bool setDspCutOff(ONI::Settings::Rhs2116DspCutoff cutoff){
		ONI::Settings::Rhs2116Format format = getFormat(true);
		if(cutoff == ONI::Settings::Rhs2116DspCutoff::Off){
			format.dspEnable = 0;
			format.dspCutoff = ONI::Settings::Rhs2116DspCutoff::Dsp146mHz; // is this a bug? that seems to be default status
		}else{
			format.dspEnable = 1;
			format.dspCutoff = cutoff;
		}
		return setFormat(format);

	}

	const ONI::Settings::Rhs2116DspCutoff& getDspCutOff(const bool& bCheckRegisters = true){
		getFormat(bCheckRegisters);
		return settings.dspCutoff;
	}
	
	bool setStepSize(ONI::Settings::Rhs2116StimulusStep stepSize){
		const unsigned int * regs = ONI::Settings::Rhs2116StimulusStepRegisters[stepSize];
		unsigned int reg = regs[2] << 13 | regs[1] << 7 | regs[0];
		bool bOk = writeRegister(ONI::Register::Rhs2116::STEPSZ, reg);
		if(bOk) getStepSize(true);
		return bOk;
	}

	ONI::Settings::Rhs2116StimulusStep getStepSize(const bool& bCheckRegisters = true){
		if(bCheckRegisters){
			unsigned int reg = readRegister(ONI::Register::Rhs2116::STEPSZ);
			settings.stepSize = getStepSizeFromReg(reg);
		}
		return settings.stepSize;
	}

	inline void process(oni_frame_t* frame){
		//const std::lock_guard<std::mutex> lock(mutex);
		if(postProcessors.size() > 0){
			ONI::Frame::Rhs2116Frame processedFrame(frame, (uint64_t)BaseDevice::getAcqDeltaTimeMicros(frame->time));
			process(processedFrame);
		}
		for(auto it : preProcessors){
			it.second->process(frame);
		}

	}

	inline void process(ONI::Frame::BaseFrame& frame){
		//const std::lock_guard<std::mutex> lock(mutex);
		for(auto it : postProcessors){
			it.second->process(frame);
		}
	}

	virtual unsigned int readRegister(const ONI::Register::Rhs2116Register& reg){
		return ONI::Device::BaseDevice::readRegister(reg);
	}

	virtual bool writeRegister(const ONI::Register::Rhs2116Register& reg, const unsigned int& value){
		return ONI::Device::BaseDevice::writeRegister(reg, value);
	}

	inline void forceGuiUpdate(){
		bForceGuiUpdate = true;
	}

protected:

	inline ONI::Settings::Rhs2116StimulusStep getStepSizeFromReg(unsigned int reg){
		ONI::Settings::Rhs2116StimReg r = *reinterpret_cast<ONI::Settings::Rhs2116StimReg*>(&reg);
		for(size_t i = 0; i < 10; ++i){
			const unsigned int * regs = ONI::Settings::Rhs2116StimulusStepRegisters[i];
			if(regs[0] == r.Step_Sel1 &&
			   regs[1] == r.Step_Sel2 &&
			   regs[2] == r.Step_Sel3){
				return (ONI::Settings::Rhs2116StimulusStep)(i);
			}
		}
	}

	inline ONI::Settings::Rhs2116AnalogLowCutoff getLowCutoffFromReg(unsigned int bw){
		ONI::Settings::Rhs2116LowCutReg r = *reinterpret_cast<ONI::Settings::Rhs2116LowCutReg*>(&bw);
		for(size_t i = 0; i < 26; ++i){
			const unsigned int * regs = ONI::Settings::Rhs2116AnalogLowCutoffRegisters[i];
			if(regs[0] == r.RL_Asel1 &&
			   regs[1] == r.RL_Asel2 &&
			   regs[2] == r.RL_Asel3){
				return (ONI::Settings::Rhs2116AnalogLowCutoff)(i);
			}
		}
	}

	inline ONI::Settings::Rhs2116AnalogHighCutoff getHighCutoffFromReg(unsigned int bw0, unsigned int bw1, unsigned int fast){
		ONI::Settings::Rhs2116HighCutReg r0 = *reinterpret_cast<ONI::Settings::Rhs2116HighCutReg*>(&bw0);
		ONI::Settings::Rhs2116HighCutReg r1 = *reinterpret_cast<ONI::Settings::Rhs2116HighCutReg*>(&bw1);
		for(size_t i = 0; i < 17; ++i){
			const unsigned int * regs = ONI::Settings::Rhs2116AnalogHighCutoffRegisters[i];
			const unsigned int regf = ONI::Settings::Rhs2116AnalogHighCutoffToFastSettleSamples[i];
			if(regs[0] == r0.RH1_sel1 && 
			   regs[1] == r0.RH1_sel2 && 
			   regs[2] == r1.RH1_sel1 && 
			   regs[3] == r1.RH1_sel2 && regf == fast){
				LOGINFO("AnalogHighCutoff %i", i);
				return (ONI::Settings::Rhs2116AnalogHighCutoff)(i);
			}
		}
	}



protected:

	bool bForceGuiUpdate = false;

	bool bEnabled = false;


	ONI::Settings::Rhs2116DeviceSettings settings;

};




} // namespace Device
} // namespace ONI