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
#include "ONIDeviceConfig.h"
#include "ONIRegister.h"
#include "ONIUtility.h"

#pragma once

// later we can set which device config to use with specific #define for USE_IMGUI etc
//typedef _FmcDeviceConfig FmcDeviceConfig;

class Rhs2116Register : public ONIRegister{
public:
	Rhs2116Register(unsigned int val, std::string name) { this->addr = val; this->name = name; this->devn = "RHS2116Device"; };
};

struct ONIProbeStatistics{
	float sum;
	float mean;
	float ss;
	float variance;
	float deviation;
};

class Rhs2116Device : public ONIDevice{

public:

	// managed registers (what does this mean? copied from Bonsai C# code)
	static inline const Rhs2116Register ENABLE				= Rhs2116Register(0x8000, "ENABLE");			// Enable or disable the data output stream (32768)
	static inline const Rhs2116Register MAXDELTAS			= Rhs2116Register(0x8001, "MAXDELTAS");			// Maximum number of deltas in the delta table (32769)
	static inline const Rhs2116Register NUMDELTAS			= Rhs2116Register(0x8002, "NUMDELTAS");			// Number of deltas in the delta table (32770)
	static inline const Rhs2116Register DELTAIDXTIME		= Rhs2116Register(0x8003, "DELTAIDXTIME");		// The delta table index and corresponding application delta application time (32771)
	static inline const Rhs2116Register DELTAPOLEN			= Rhs2116Register(0x8004, "DELTAPOLEN");		// The polarity and enable vectors  (32772)
	static inline const Rhs2116Register SEQERROR			= Rhs2116Register(0x8005, "SEQERROR");			// Invalid sequence indicator (32773)
	static inline const Rhs2116Register TRIGGER				= Rhs2116Register(0x8006, "TRIGGER");			// Writing 1 to this register will trigger a stimulation sequence for this device (32774)
	static inline const Rhs2116Register FASTSETTLESAMPLES	= Rhs2116Register(0x8007, "FASTSETTLESAMPLES");	// Number of round-robbin samples to apply charge balance following the conclusion of a stimulus pulse (32775)
	static inline const Rhs2116Register RESPECTSTIMACTIVE	= Rhs2116Register(0x8008, "RESPECTSTIMACTIVE");	// Determines when artifact recovery sequence is applied to this chip (32776)
	
	// unmanaged registers
	static inline const Rhs2116Register BIAS		= Rhs2116Register(0x00, "BIAS");		// Supply Sensor and ADC Buffer Bias Current
	static inline const Rhs2116Register FORMAT		= Rhs2116Register(0x01, "FORMAT");		// ADC Output Format, DSP Offset Removal, and Auxiliary Digital Outputs
	static inline const Rhs2116Register ZCHECK		= Rhs2116Register(0x02, "ZCHECK");		// Impedance Check Control
	static inline const Rhs2116Register DAC			= Rhs2116Register(0x03, "DAC");			// Impedance Check DAC
	static inline const Rhs2116Register BW0			= Rhs2116Register(0x04, "BW0");			// On-Chip Amplifier Bandwidth Select
	static inline const Rhs2116Register BW1			= Rhs2116Register(0x05, "BW1");			// On-Chip Amplifier Bandwidth Select
	static inline const Rhs2116Register BW2			= Rhs2116Register(0x06, "BW2");			// On-Chip Amplifier Bandwidth Select
	static inline const Rhs2116Register BW3			= Rhs2116Register(0x07, "BW3");			// On-Chip Amplifier Bandwidth Select
	static inline const Rhs2116Register PWR			= Rhs2116Register(0x08, "PWR");			//Individual AC Amplifier Power

	static inline const Rhs2116Register SETTLE		= Rhs2116Register(0x0a, "SETTLE");		// Amplifier Fast Settle

	static inline const Rhs2116Register LOWAB		= Rhs2116Register(0x0c, "LOWAB");		// Amplifier Lower Cutoff Frequency Select

	static inline const Rhs2116Register STIMENA		= Rhs2116Register(0x20, "STIMENA");		// Stimulation Enable A
	static inline const Rhs2116Register STIMENB		= Rhs2116Register(0x21, "STIMENB");		// Stimulation Enable B
	static inline const Rhs2116Register STEPSZ		= Rhs2116Register(0x22, "STEPSZ");		// Stimulation Current Step Size
	static inline const Rhs2116Register STIMBIAS	= Rhs2116Register(0x23, "STIMBIAS");	// Stimulation Bias Voltages
	static inline const Rhs2116Register RECVOLT		= Rhs2116Register(0x24, "RECVOLT");		// Current-Limited Charge Recovery Target Voltage
	static inline const Rhs2116Register RECCUR		= Rhs2116Register(0x25, "RECCUR");		// Charge Recovery Current Limit
	static inline const Rhs2116Register DCPWR		= Rhs2116Register(0x26, "DCPWR");		// Individual DC Amplifier Power

	static inline const Rhs2116Register COMPMON		= Rhs2116Register(0x28, "COMPMON");		// Compliance Monitor
	static inline const Rhs2116Register STIMON		= Rhs2116Register(0x2a, "STIMON");		// Stimulator On
	static inline const Rhs2116Register STIMPOL		= Rhs2116Register(0x2c, "STIMPOL");		// Stimulator Polarity
	static inline const Rhs2116Register RECOV		= Rhs2116Register(0x2e, "RECOV");		// Charge Recovery Switch
	static inline const Rhs2116Register LIMREC		= Rhs2116Register(0x30, "LIMREC");		// Current-Limited Charge Recovery Enable
	static inline const Rhs2116Register FAULTDET	= Rhs2116Register(0x32, "FAULTDET");	// Fault Current Detector

	static inline const Rhs2116Register NEG00  = Rhs2116Register(0x40, "NEG00");
	static inline const Rhs2116Register NEG01  = Rhs2116Register(0x41, "NEG01");
	static inline const Rhs2116Register NEG02  = Rhs2116Register(0x42, "NEG02");
	static inline const Rhs2116Register NEG03  = Rhs2116Register(0x43, "NEG03");
	static inline const Rhs2116Register NEG04  = Rhs2116Register(0x44, "NEG04");
	static inline const Rhs2116Register NEG05  = Rhs2116Register(0x45, "NEG05");
	static inline const Rhs2116Register NEG06  = Rhs2116Register(0x46, "NEG06");
	static inline const Rhs2116Register NEG07  = Rhs2116Register(0x47, "NEG07");
	static inline const Rhs2116Register NEG08  = Rhs2116Register(0x48, "NEG08");
	static inline const Rhs2116Register NEG09  = Rhs2116Register(0x49, "NEG09");
	static inline const Rhs2116Register NEG010 = Rhs2116Register(0x4a, "NEG010");
	static inline const Rhs2116Register NEG011 = Rhs2116Register(0x4b, "NEG011");
	static inline const Rhs2116Register NEG012 = Rhs2116Register(0x4c, "NEG012");
	static inline const Rhs2116Register NEG013 = Rhs2116Register(0x4d, "NEG013");
	static inline const Rhs2116Register NEG014 = Rhs2116Register(0x4e, "NEG014");
	static inline const Rhs2116Register NEG015 = Rhs2116Register(0x4f, "NEG015");

	static inline const Rhs2116Register POS00  = Rhs2116Register(0x60, "POS00");
	static inline const Rhs2116Register POS01  = Rhs2116Register(0x61, "POS01");
	static inline const Rhs2116Register POS02  = Rhs2116Register(0x62, "POS02");
	static inline const Rhs2116Register POS03  = Rhs2116Register(0x63, "POS03");
	static inline const Rhs2116Register POS04  = Rhs2116Register(0x64, "POS04");
	static inline const Rhs2116Register POS05  = Rhs2116Register(0x65, "POS05");
	static inline const Rhs2116Register POS06  = Rhs2116Register(0x66, "POS06");
	static inline const Rhs2116Register POS07  = Rhs2116Register(0x67, "POS07");
	static inline const Rhs2116Register POS08  = Rhs2116Register(0x68, "POS08");
	static inline const Rhs2116Register POS09  = Rhs2116Register(0x69, "POS09");
	static inline const Rhs2116Register POS010 = Rhs2116Register(0x6a, "POS010");
	static inline const Rhs2116Register POS011 = Rhs2116Register(0x6b, "POS011");
	static inline const Rhs2116Register POS012 = Rhs2116Register(0x6c, "POS012");
	static inline const Rhs2116Register POS013 = Rhs2116Register(0x6d, "POS013");
	static inline const Rhs2116Register POS014 = Rhs2116Register(0x6e, "POS014");
	static inline const Rhs2116Register POS015 = Rhs2116Register(0x6f, "POS015");

	enum Rhs2116DspCutoff{
		Differential = 0,
		Dsp3309Hz,
		Dsp1374Hz,
		Dsp638Hz,
		Dsp308Hz,
		Dsp152Hz,
		Dsp75Hz,
		Dsp37Hz,
		Dsp19Hz,
		Dsp9336mHz,
		Dsp4665mHz,
		Dsp2332mHz,
		Dsp1166mHz,
		Dsp583mHz,
		Dsp291mHz,
		Dsp146mHz,
		Off
	};

	enum Rhs2116AnalogLowCutoff{
		Low1000Hz = 0,
		Low500Hz,
		Low300Hz,
		Low250Hz,
		Low200Hz,
		Low150Hz,
		Low100Hz,
		Low75Hz,
		Low50Hz,
		Low30Hz,
		Low25Hz,
		Low20Hz,
		Low15Hz,
		Low10Hz,
		Low7500mHz,
		Low5000mHz,
		Low3090mHz,
		Low2500mHz,
		Low2000mHz,
		Low1500mHz,
		Low1000mHz,
		Low750mHz,
		Low500mHz,
		Low300mHz,
		Low250mHz,
		Low100mHz
	};

	enum Rhs2116AnalogHighCutoff{
		High20000Hz = 0,
		High15000Hz,
		High10000Hz,
		High7500Hz,
		High5000Hz,
		High3000Hz,
		High2500Hz,
		High2000Hz,
		High1500Hz,
		High1000Hz,
		High750Hz,
		High500Hz,
		High300Hz,
		High250Hz,
		High200Hz,
		High150Hz,
		High100Hz,
	};


	const unsigned int AnalogLowCutoffRegisters [26][3]{
		{ 10, 0, 0 },
		{ 13, 0, 0 },
		{ 15, 0, 0 },
		{ 17, 0, 0 },
		{ 18, 0, 0 },
		{ 21, 0, 0 },
		{ 25, 0, 0 },
		{ 28, 0, 0 },
		{ 34, 0, 0 },
		{ 44, 0, 0 },
		{ 48, 0, 0 },
		{ 54, 0, 0 },
		{ 62, 0, 0 },
		{ 5, 1, 0 },
		{ 18, 1, 0 },
		{ 40, 1, 0 },
		{ 20, 2, 0 },
		{ 42, 2, 0 },
		{ 8, 3, 0 },
		{ 9, 4, 0 },
		{ 44, 6, 0 },
		{ 49, 9, 0 },
		{ 35, 17, 0 },
		{ 1, 40, 0 },
		{ 56, 54, 0 },
		{ 16, 60, 1 }
	};

	const unsigned int AnalogHighCutoffRegisters [17][4]{
		{ 8, 0, 4, 0 },
		{ 11, 0, 8, 0 },
		{ 17, 0, 16, 0 },
		{ 22, 0, 23, 0 },
		{ 33, 0, 37, 0 },
		{ 3, 1, 13, 1 },
		{ 13, 1, 25, 1 },
		{ 27, 1, 44, 1 },
		{ 1, 2, 23, 2 },
		{ 46, 2, 30, 3 },
		{ 41, 3, 36, 4 },
		{ 30, 5, 43, 6 },
		{ 6, 9, 2, 11 },
		{ 42, 10, 5, 13 },
		{ 24, 13, 7, 16 },
		{ 44, 17, 8, 21 },
		{ 38, 26, 5, 31 }
	};
	const unsigned int AnalogHighCutoffToFastSettleSamples [17]{
		4, 5, 8, 10, 15, 25, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30
	};

#pragma pack(push, 1)
	struct Rhs2116LowCutReg{
		unsigned RL_Asel1:7;
		unsigned RL_Asel2:6;
		unsigned RL_Asel3:1;
		unsigned unused:2;
	};
#pragma pack(pop)

#pragma pack(push, 1)
	struct Rhs2116HighCutReg{
		unsigned RH1_sel1:6;
		unsigned RH1_sel2:5;
		unsigned unused:5;
	};
#pragma pack(pop)

#pragma pack(push, 1)
	struct Rhs2116Format{
		unsigned dspCutoff:4;
		unsigned dspEnable:1;
		unsigned absmode:1;
		unsigned twoscomp:1;
		unsigned weakMISO:1;
		unsigned digout1HiZ:1;
		unsigned digout1:1;
		unsigned digout2HiZ:1;
		unsigned digout2:1;
		unsigned digoutOD:1;
		unsigned unused:3;
	};
#pragma pack(pop)

#pragma pack(push, 1)
	struct Rhs2116Bias{
		unsigned muxBias:6;
		unsigned adcBias:6;
		unsigned unused:4;
	};
#pragma pack(pop)

#pragma pack(push, 1)
	struct Rhs2116DataFrame{
		uint64_t hubTime;
		uint16_t ac[16];
		uint16_t dc[16];
		uint16_t unused;
		uint64_t acqTime;
		float acf[16];
		float dcf[16];
		long double deltaTime;
		uint64_t sampleIDX;
	};
#pragma pack(pop)

	struct acquisition_clock_compare {
		inline bool operator() (const Rhs2116DataFrame& lhs, const Rhs2116DataFrame& rhs){
			return (lhs.acqTime < rhs.acqTime);
		}
	};

	struct hub_clock_compare {
		inline bool operator() (const Rhs2116DataFrame& lhs, const Rhs2116DataFrame& rhs){
			return (lhs.hubTime < rhs.hubTime);
		}
	};

	Rhs2116Device(){
		//deviceID = Rhs2116DeviceID;
		//setBufferSizeSamples(15000);
		setBufferSizeMillis(5000);
		setDrawBufferStride(300);
		//sampleRateTimer.start<fu::millis>(1000);
		//cntTimer.start<fu::millis>(1000);
	};
	
	~Rhs2116Device(){
		LOGDEBUG("RHS2116 Device DTOR");
		const std::lock_guard<std::mutex> lock(mutex);
		rawDataFrames.clear();
		drawDataFrames.clear();
		//readRegister(ENABLE);
		//writeRegister(ENABLE, 0);
	};

	void deviceSetup(){
		//config.setup(deviceName);
		getFormat(true);
		getAnalogLowCutoff(true);
		getAnalogLowCutoffRecovery(true);
		getAnalogHighCutoff(true);

		setBufferSizeSamples(bufferSize);
	}

	bool setEnabled(const bool& b){
		bool bOk = writeRegister(Rhs2116Device::ENABLE, (unsigned int)b);
		if(bOk) getEnabled(true); // updates the cached value
		return bOk;
	}

	bool getEnabled(const bool& bCheckRegisters = true){
		if(bCheckRegisters) bEnabled = (bool)readRegister(Rhs2116Device::ENABLE);
		return bEnabled;
	}

	bool setAnalogLowCutoff(Rhs2116AnalogLowCutoff lowcut){
		const unsigned int * regs = AnalogLowCutoffRegisters[lowcut];
		unsigned int reg = regs[2] << 13 | regs[1] << 7 | regs[0];
		bool bOk = writeRegister(Rhs2116Device::BW2, reg);
		if(bOk) getAnalogLowCutoff(true);

		//std::vector<std::string> lowCutoffStrs = ofSplitString(std::string(lowCutoffOptions), "\0");
		//LOGDEBUG("Analog Low Cutoff: %s", lowCutoffStrs[lowcut].c_str());
		//unsigned int chr = readRegister(Rhs2116Device::BW2);
		//Rhs2116LowCutReg r = *reinterpret_cast<Rhs2116LowCutReg*>(&chr);
		//LOGDEBUG("Low reg: %i %i %i == %i ===> %i %i %i == %i", regs[0], regs[1], regs[2], reg, r.RL_Asel1, r.RL_Asel2, r.RL_Asel3, chr);
		//LOGDEBUG("Struct val %i == %i", lowcut, getLowCutoffFromReg(reg));
		return bOk;
	}

	Rhs2116AnalogLowCutoff getAnalogLowCutoff(const bool& bCheckRegisters = true){
		if(bCheckRegisters){
			unsigned int bw2 = readRegister(Rhs2116Device::BW2);
			lowCutoff = getLowCutoffFromReg(bw2);
		}
		return lowCutoff;
	}

	bool setAnalogLowCutoffRecovery(Rhs2116AnalogLowCutoff lowcut){
		const unsigned int * regs = AnalogLowCutoffRegisters[lowcut];
		unsigned int reg = regs[2] << 13 | regs[1] << 7 | regs[0];
		bool bOk = writeRegister(Rhs2116Device::BW3, reg);
		if(bOk) getAnalogLowCutoffRecovery(true);

		//std::vector<std::string> lowCutoffStrs = ofSplitString(std::string(lowCutoffOptions), "\\0");
		//LOGDEBUG("Analog Low Cutoff: %s", lowCutoffStrs[lowcut].c_str());
		//unsigned int chr = readRegister(Rhs2116Device::BW3);
		//Rhs2116LowCutReg r = *reinterpret_cast<Rhs2116LowCutReg*>(&chr);
		//LOGDEBUG("Low reg recover: %i %i %i == %i ===> %i %i %i == %i", regs[0], regs[1], regs[2], reg, r.RL_Asel1, r.RL_Asel2, r.RL_Asel3, chr);
		//LOGDEBUG("Struct val %i == %i", lowcut, getLowCutoffFromReg(reg));
		return bOk;
	}

	Rhs2116AnalogLowCutoff getAnalogLowCutoffRecovery(const bool& bCheckRegisters = true){
		if(bCheckRegisters){
			unsigned int bw3 = readRegister(Rhs2116Device::BW3);
			lowCutoffRecovery = getLowCutoffFromReg(bw3);
		}
		return lowCutoffRecovery;
	}

	bool setAnalogHighCutoff(Rhs2116AnalogHighCutoff hicut){
		const unsigned int * regs = AnalogHighCutoffRegisters[hicut];
		unsigned int reg0 = regs[1] << 6 | regs[0];
		unsigned int reg1 = regs[3] << 6 | regs[2];
		unsigned int reg2 = AnalogHighCutoffToFastSettleSamples[hicut];
		bool b0 = writeRegister(Rhs2116Device::BW0, reg0);
		bool b1 = writeRegister(Rhs2116Device::BW1, reg1);
		bool b2 = writeRegister(Rhs2116Device::FASTSETTLESAMPLES, reg2);
		bool bOk = (b0 && b1 && b2);
		if(bOk) getAnalogHighCutoff(true);



		//std::vector<std::string> highCutoffStrs = ofSplitString(std::string(highCutoffOptions), "\\0");
		//LOGDEBUG("Analog High Cutoff to: %s", highCutoffStrs[hicut].c_str());

		//unsigned int chr0 = readRegister(Rhs2116Device::BW0);
		//unsigned int chr1 = readRegister(Rhs2116Device::BW1);
		//Rhs2116HighCutReg r0 = *reinterpret_cast<Rhs2116HighCutReg*>(&chr0);
		//Rhs2116HighCutReg r1 = *reinterpret_cast<Rhs2116HighCutReg*>(&chr1);
		//LOGDEBUG("High reg: %i %i %i %i === %i %i ====> %i %i %i %i === %i %i", regs[0], regs[1], regs[2], regs[3], reg0, reg1, r0.RH1_sel1, r0.RH1_sel2, r1.RH1_sel1, r1.RH1_sel2, chr0, chr1);
		//LOGDEBUG("Struct val %i == %i", hicut, getHighCutoffFromReg(reg0, reg1, reg2));
		return bOk;
	}

	Rhs2116AnalogHighCutoff getAnalogHighCutoff(const bool& bCheckRegisters = true){
		if(bCheckRegisters){
			unsigned int bw0 = readRegister(Rhs2116Device::BW0);
			unsigned int bw1 = readRegister(Rhs2116Device::BW1);
			unsigned int fast = readRegister(Rhs2116Device::FASTSETTLESAMPLES);
			highCutoff = getHighCutoffFromReg(bw0, bw1, fast);
		}
		return highCutoff;
	}

	bool setFormat(Rhs2116Format format){
		unsigned int uformat = *(reinterpret_cast<unsigned int*>(&format));
		bool bOk = writeRegister(Rhs2116Device::FORMAT, uformat);
		if(bOk) getFormat(true); // updates the cached value
		return bOk;
	}

	Rhs2116Format getFormat(const bool& bCheckRegisters = true){
		if(bCheckRegisters){
			unsigned int uformat = readRegister(Rhs2116Device::FORMAT);
			format = *(reinterpret_cast<Rhs2116Format*>(&uformat));
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
		
		//std::vector<std::string> dspCutoffStrs = ofSplitString(std::string(dspCutoffOptions), "\\0");
		//LOGDEBUG("Setting DSP Cutoff to: %s", dspCutoffStrs[format.dspCutoff].c_str());

		return setFormat(format);
	}

	const std::vector<Rhs2116DataFrame>& getRawDataFrames(){
		const std::lock_guard<std::mutex> lock(mutex);
		return rawDataFrames;
	}

	const std::vector<Rhs2116DataFrame>& getDrawDataFrames(){
		const std::lock_guard<std::mutex> lock(mutex);
		return drawDataFrames;
	}

	Rhs2116DataFrame getLastDataFrame(){
		const std::lock_guard<std::mutex> lock(mutex);
		return rawDataFrames[lastBufferIDX];
	}

	void setDrawBufferStride(const size_t& samples){
		mutex.lock();
		drawStride = samples;
		drawBufferSize = bufferSize / drawStride;
		currentDrawBufferIDX = 0;
		drawDataFrames.clear();
		drawDataFrames.resize(drawBufferSize);
		mutex.unlock();
		LOGINFO("Draw Buffer size: %i (samples)", drawBufferSize);
	}

	void setBufferSizeSamples(const size_t& samples){
		mutex.lock();
		bufferSize = samples;
		currentBufferIDX = 0;
		rawDataFrames.clear();
		rawDataFrames.resize(bufferSize);
		mutex.unlock();
		LOGINFO("Raw  Buffer size: %i (samples)", bufferSize);
		setDrawBufferStride(drawStride);
	}

	void setBufferSizeMillis(const int& millis){
		// TODO: I can't seem to change the ADC Bias and MUX settings to get different sample rates
		// SO I am just going to assume 30kS/s sample rate for the RHS2116 hard coded to sampleRate
		size_t samples = sampleRatekSs * millis / 1000;
		setBufferSizeSamples(samples);
	}

	//fu::Timer frameTimer;

	double getFrameTimerAvg(){
		/*
		const std::lock_guard<std::mutex> lock(mutex);
		frameTimer.stop();
		double t = frameTimer.avg<fu::micros>();
		frameTimer.start();
		return t;
		*/
		return 0;
	}

	//std::vector<ONIProbeStatistics> probeStats;

	//uint64_t cnt = 0;
	
	inline void gui(){
		/*
		//ImGui::Begin(deviceName.c_str());
		ImGui::PushID(deviceName.c_str());

		static char * dspCutoffOptions = "Differential\0Dsp3309Hz\0Dsp1374Hz\0Dsp638Hz\0Dsp308Hz\0Dsp152Hz\0Dsp75Hz\0Dsp37Hz\0Dsp19Hz\0Dsp9336mHz\0Dsp4665mHz\0Dsp2332mHz\0Dsp1166mHz\0Dsp583mHz\0Dsp291mHz\0Dsp146mHz\0Off";
		static char * lowCutoffOptions = "Low1000Hz\0Low500Hz\0Low300Hz\0Low250Hz\0Low200Hz\0Low150Hz\0Low100Hz\0Low75Hz\0Low50Hz\0Low30Hz\0Low25Hz\0Low20Hz\0Low15Hz\0Low10Hz\0Low7500mHz\0Low5000mHz\0Low3090mHz\0Low2500mHz\0Low2000mHz\0Low1500mHz\0Low1000mHz\0Low750mHz\0Low500mHz\0Low300mHz\0Low250mHz\0Low100mHz";
		static char * highCutoffOptions = "High20000Hz\0High15000Hz\0High10000Hz\0High7500Hz\0High5000Hz\0High3000Hz\0High2500Hz\0High2000Hz\0High1500Hz\0High1000Hz\0High750Hz\0High500Hz\0High300Hz\0High250Hz\0High200Hz\0High150Hz\0High100Hz";

		ImGui::SetNextItemWidth(200);
		int dspFormatItem = format.dspEnable == 1 ? format.dspCutoff : Rhs2116DspCutoff::Off;
		ImGui::Combo("DSP Cutoff", &dspFormatItem, dspCutoffOptions, 5);
		if(dspFormatItem != Rhs2116DspCutoff::Off && dspFormatItem != format.dspCutoff) setDspCutOff((Rhs2116DspCutoff)dspFormatItem);
		if(dspFormatItem == Rhs2116DspCutoff::Off && format.dspEnable == 1) setDspCutOff((Rhs2116DspCutoff)dspFormatItem);

		ImGui::SetNextItemWidth(200);
		int lowCutoffItem = getAnalogLowCutoff(false);
		ImGui::Combo("Analog Low Cutoff", &lowCutoffItem, lowCutoffOptions, 5);
		if(lowCutoffItem != lowCutoff) setAnalogLowCutoff((Rhs2116AnalogLowCutoff)lowCutoffItem);

		ImGui::SetNextItemWidth(200);
		int lowCutoffRecoveryItem = getAnalogLowCutoffRecovery(false);
		ImGui::Combo("Analog Low Cutoff Recovery", &lowCutoffRecoveryItem, lowCutoffOptions, 5);
		if(lowCutoffRecoveryItem != lowCutoffRecovery) setAnalogLowCutoffRecovery((Rhs2116AnalogLowCutoff)lowCutoffRecoveryItem);

		ImGui::SetNextItemWidth(200);
		int highCutoffItem = getAnalogHighCutoff(false);
		ImGui::Combo("Analog High Cutoff", &highCutoffItem, highCutoffOptions, 5);
		if(highCutoffItem != highCutoff) setAnalogHighCutoff((Rhs2116AnalogHighCutoff)highCutoffItem);
		mutex.lock();
		ImGui::Text("Frame delta: %i", frameDeltaSFX);
		ImGui::Text("Sample cont: %i", sampleCounter);
		ImGui::Text("Div delta  : %f", div);
		mutex.unlock();
		ImGui::PopID();
		//ImGui::End();
		*/
	}

	bool saveConfig(std::string filePath){
		return true;//config.save(filePath);
	}

	bool loadConfig(std::string filePath){
		return true;//config.load(filePath);
	}

	uint64_t lastAcquisitionTime = 0;
	uint64_t lastSampleIDX = 0;
	uint64_t sampleCounter = 0;
	uint64_t frameDeltaSFX = 0;
	float div = 0;
	inline void process(oni_frame_t* frame, const uint64_t & sampleIDX){

		const std::lock_guard<std::mutex> lock(mutex);

		memcpy(&rawDataFrames[currentBufferIDX], frame->data, frame->data_sz);  // copy the data payload including hub clock
		rawDataFrames[currentBufferIDX].acqTime = frame->time;					// copy the acquisition clock
		rawDataFrames[currentBufferIDX].deltaTime = ONIDevice::getAcqDeltaTimeMicros(frame->time); //fu::time::now<fu::micros>() * 1000;
		//rawDataFrames[currentBufferIDX].sampleIDX = fu::time::now<fu::micros>();
		

		for(size_t probe = 0; probe < 16; ++probe){
			rawDataFrames[currentBufferIDX].acf[probe] = 0.195f * (rawDataFrames[currentBufferIDX].ac[probe] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
			rawDataFrames[currentBufferIDX].dcf[probe] = -19.23 * (rawDataFrames[currentBufferIDX].dc[probe] - 512) / 1000.0f; // -19.23 mV × (ADC result – 512) divide by 1000 for V?
		}

		if(currentBufferIDX % drawStride == 0){
			//for(int i = 0; i < 16; ++i){
			//	acProbes[i][currentDrawBufferIDX] =  0.195f * (rawDataFrames[currentBufferIDX].ac[i] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
			//	dcProbes[i][currentDrawBufferIDX] = -19.23 * (rawDataFrames[currentBufferIDX].dc[i] - 512) / 1000.0f; // -19.23 mV × (ADC result – 512) divide by 1000 for V?
			//}
			//deltaTimes[currentDrawBufferIDX] = rawDataFrames[currentBufferIDX].deltaTime;
			drawDataFrames[currentDrawBufferIDX] = rawDataFrames[currentBufferIDX];
			currentDrawBufferIDX = (currentDrawBufferIDX + 1) % drawBufferSize;
			//frameTimer.stop();
			//LOGDEBUG("RHS2116 Frame Timer %f", frameTimer.avg<fu::micros>());
			//frameTimer.start();
		}

		// nice way to sort
		//size_t sorted_idx = currentBufferIDX;
		//for(size_t i = 0; i < n_frames; ++i){
		//	to[i] = rawDataFrames[sorted_idx];
		//	sorted_idx = (sorted_idx + stride) % bufferSize;
		//}

		//for(size_t probe = 0; probe < 16; ++probe){
		//	probeStats[probe].sum = 0;
		//	for(size_t frame = 0; frame < bufferSize; ++frame){
		//		probeStats[probe].sum += rawDataFrames[frame].ac[probe];
		//	}
		//	
		//	probeStats[probe].mean = probeStats[probe].sum / bufferSize;
		//	probeStats[probe].mean = 0.195f * (probeStats[probe].mean - 32768) / 1000.0f;
		//	for(size_t frame = 0; frame < bufferSize; ++frame){
		//		float acdiff = 0.195f * (rawDataFrames[frame].ac[probe] - 32768) / 1000.0f - probeStats[probe].mean;
		//		probeStats[probe].ss += acdiff * acdiff; 
		//	}
		//	probeStats[probe].variance = probeStats[probe].ss / (bufferSize - 1);  // use population (N) or sample (n-1) deviation?
		//	probeStats[probe].deviation = sqrt(probeStats[probe].variance);
		//}
		
		//long double deltaFrameAcqTime = (rawDataFrames[currentBufferIDX].acqTime - rawDataFrames[lastBufferIDX].acqTime) / (long double)acq_clock_khz * 1000000;
		//double deltaFrameTimer = frameTimer.elapsed<fu::micros>();
		//fu::debug << rawDataFrames[currentBufferIDX].acqTime - rawDataFrames[lastBufferIDX].acqTime << " " << deltaFrameTimer << " " << deltaFrameAcqTime << fu::endl;
		//if(deltaFrameTimer > 1000000.0 / sampleRatekSs) LOGDEBUG("RHS2116 Frame Buffer Underrun %f", deltaFrameTimer);
		

		//lastBufferIDX = currentBufferIDX;
		/*
		frameDeltaSFX = frameTimer.elapsed<fu::micros>(); //(rawDataFrames[currentBufferIDX].sampleIDX - lastSampleIDX);
		if(frameDeltaSFX > 5) {
			div = (float)frameDeltaSFX / sampleCounter;
			//LOGDEBUG("Framt: %i %i %f", frameDeltaSFX, sampleCounter, (float)frameDeltaSFX / sampleCounter );
			sampleCounter = 0;
		}
		sampleCounter++;
		frameTimer.start();
		float frameDeltaAQX = (rawDataFrames[currentBufferIDX].acqTime - lastAcquisitionTime) / (float)acq_clock_khz * 1000000;
		if(frameDeltaAQX > 33.333333333333333f) LOGDEBUG("Underflow: %f", frameDeltaAQX);
		*/

		//if(currentBufferIDX % 30000 == 0) LOGDEBUG("Delta %s: %i %f %i %i %i", deviceName.c_str(), frameDeltaSFX, frameDeltaAQX, sampleIDX, lastSampleIDX, rawDataFrames[currentBufferIDX].sampleIDX);

		lastAcquisitionTime = rawDataFrames[currentBufferIDX].acqTime;
		lastSampleIDX = rawDataFrames[currentBufferIDX].sampleIDX;
		
		currentBufferIDX = (currentBufferIDX + 1) % bufferSize;

		//frameTimer.fcount();

		//ofSleepMillis(1);
		//sortedDataFrames = rawDataFrames;
		//std::sort(sortedDataFrames.begin(), sortedDataFrames.end(), acquisition_clock_compare());
		//++sampleCount;

	}

	unsigned int readRegister(const Rhs2116Register& reg){
		return ONIDevice::readRegister(reg);
	}

	bool writeRegister(const Rhs2116Register& reg, const unsigned int& value){
		return ONIDevice::writeRegister(reg, value);
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

	uint64_t hubClockFirst = -1;

	std::vector<Rhs2116DataFrame> rawDataFrames;

	size_t bufferSize = 100;
	size_t currentBufferIDX = 0;
	size_t lastBufferIDX = 0;

	std::vector<Rhs2116DataFrame> drawDataFrames;

	size_t drawStride = 100;
	size_t currentDrawBufferIDX = 0;
	size_t drawBufferSize = 0;

	const int sampleRatekSs = 30000; // see above, cannot seem to change this using BIAS registers!!

	std::mutex mutex;

	bool bEnabled = false;
	Rhs2116Format format;

	Rhs2116AnalogLowCutoff lowCutoff;
	Rhs2116AnalogLowCutoff lowCutoffRecovery;
	Rhs2116AnalogHighCutoff highCutoff;

};

inline std::ostream& operator<<(std::ostream& os, const Rhs2116Device::Rhs2116Format& format) {

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