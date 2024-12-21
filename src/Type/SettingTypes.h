//
//  BaseDevice.h
//
//  Created by Matt Gingold on 13.09.2024.
//

#include "oni.h"
#include "onix.h"

//#include "ofMain.h"
//#include "ofxFutilities.h"

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

#include "../Type/Log.h"


#pragma once

namespace ONI{



struct Rhs2116StimulusData{

	bool biphasic = true;
	bool anodicFirst = true;

	float requestedAnodicAmplitudeMicroAmps = 0.0;
	float requestedCathodicAmplitudeMicroAmps = 0.0;

	float actualAnodicAmplitudeMicroAmps = 0.0;
	float actualCathodicAmplitudeMicroAmps = 0.0;

	unsigned int anodicAmplitudeSteps = 0;
	unsigned int cathodicAmplitudeSteps = 0;

	unsigned int anodicWidthSamples = 1;
	unsigned int cathodicWidthSamples = 1;
	unsigned int dwellSamples = 1;
	unsigned int delaySamples = 0;
	unsigned int interStimulusIntervalSamples = 1;

	unsigned int numberOfStimuli = 0;

};


inline bool operator==(const Rhs2116StimulusData& lhs, const Rhs2116StimulusData& rhs){ return (lhs.anodicFirst == rhs.anodicFirst &&
																								lhs.requestedAnodicAmplitudeMicroAmps == rhs.requestedAnodicAmplitudeMicroAmps &&
																								lhs.requestedCathodicAmplitudeMicroAmps == rhs.requestedCathodicAmplitudeMicroAmps &&
																								lhs.actualAnodicAmplitudeMicroAmps == rhs.actualAnodicAmplitudeMicroAmps &&
																								lhs.actualCathodicAmplitudeMicroAmps == rhs.actualCathodicAmplitudeMicroAmps &&
																								lhs.anodicAmplitudeSteps == rhs.anodicAmplitudeSteps &&
																								lhs.cathodicAmplitudeSteps == rhs.cathodicAmplitudeSteps &&
																								lhs.anodicWidthSamples == rhs.anodicWidthSamples &&
																								lhs.cathodicWidthSamples == rhs.cathodicWidthSamples &&
																								lhs.dwellSamples == rhs.dwellSamples &&
																								lhs.delaySamples == rhs.delaySamples &&
																								lhs.interStimulusIntervalSamples == rhs.interStimulusIntervalSamples &&
																								lhs.numberOfStimuli == rhs.numberOfStimuli); }
inline bool operator!=(const Rhs2116StimulusData& lhs, const Rhs2116StimulusData& rhs) { return !(lhs == rhs); }



struct ProbeStatistics{
	float sum;
	float mean;
	float ss;
	float variance;
	float deviation;
};

struct ProbeData{
	std::vector< std::vector<float> > acProbeVoltages;
	std::vector< std::vector<float> > dcProbeVoltages;
	std::vector< std::vector<float> > probeTimeStamps;
	std::vector<ProbeStatistics> acProbeStats;
	std::vector<ProbeStatistics> dcProbeStats;
};


namespace Settings{

struct FmcDeviceSettings{
	float voltage = 4.9;
};

inline bool operator==(const FmcDeviceSettings& lhs, const FmcDeviceSettings& rhs){ return (lhs.voltage == rhs.voltage); }
inline bool operator!=(const FmcDeviceSettings& lhs, const FmcDeviceSettings& rhs) { return !(lhs == rhs); }



struct HeartBeatDeviceSettings{
	unsigned int frequencyHz = 1;
};

inline bool operator==(const HeartBeatDeviceSettings& lhs, const HeartBeatDeviceSettings& rhs){ return (lhs.frequencyHz == rhs.frequencyHz); }
inline bool operator!=(const HeartBeatDeviceSettings& lhs, const HeartBeatDeviceSettings& rhs) { return !(lhs == rhs); }



enum Rhs2116StimulusStep{
	Step10nA = 0,
	Step20nA,
	Step50nA,
	Step100nA,
	Step200nA,
	Step500nA,
	Step1000nA,
	Step2000nA,
	Step5000nA,
	Step10000nA,
	StepError
};

const double Rhs2116StimulusStepMicroAmps [10]{
	0.001 * 10,
	0.001 * 20,
	0.001 * 50,
	0.001 * 100,
	0.001 * 200,
	0.001 * 500,
	0.001 * 1000,
	0.001 * 2000,
	0.001 * 5000,
	0.001 * 10000
};

const unsigned int Rhs2116StimulusStepRegisters [10][3]{
	{ 64, 19, 3 },
	{ 40, 40, 1 },
	{ 64, 40, 0 },
	{ 30, 20, 0 },
	{ 25, 10, 0 },
	{ 101, 3, 0 },
	{ 98, 1, 0 },
	{ 94, 0, 0 },
	{ 38, 0, 0 },
	{ 15, 0, 0 }
};

const unsigned int Rhs2116StimulusBiasRegisters [10][2]{
	{ 6, 6 },
	{ 7, 7 },
	{ 7, 7 },
	{ 7, 7 },
	{ 8, 8 },
	{ 9, 9 },
	{ 10, 10 },
	{ 11, 11 },
	{ 14, 14 },
	{ 15, 15 }
};

#pragma pack(push, 1)
struct Rhs2116StimIndexTime{
	unsigned time:22;
	unsigned index:10;
};
#pragma pack(pop)


#pragma pack(push, 1)
struct Rhs2116StimReg{
	unsigned Step_Sel1:7;
	unsigned Step_Sel2:6;
	unsigned Step_Sel3:2;
	unsigned unused:1;
};
#pragma pack(pop)

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


const unsigned int Rhs2116AnalogLowCutoffRegisters [26][3]{
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

const unsigned int Rhs2116AnalogHighCutoffRegisters [17][4]{
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
const unsigned int Rhs2116AnalogHighCutoffToFastSettleSamples [17]{
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

inline bool operator==(const Rhs2116Format& lhs, const Rhs2116Format& rhs){ return (lhs.dspCutoff == rhs.dspCutoff &&
																					lhs.dspEnable == lhs.dspEnable &&
																					lhs.absmode == rhs.absmode &&
																					lhs.twoscomp == rhs.twoscomp &&
																					lhs.weakMISO == rhs.weakMISO &&
																					lhs.digout1HiZ == rhs.digout1HiZ &&
																					lhs.digout1 == rhs.digout1 &&
																					lhs.digout2HiZ == lhs.digout2HiZ &&
																					lhs.digout2 == rhs.digout2 &&
																					lhs.digoutOD == rhs.digoutOD); }
inline bool operator!=(const Rhs2116Format& lhs, const Rhs2116Format& rhs) { return !(lhs == rhs); }


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


#pragma pack(push, 1)
struct Rhs2116Bias{
	unsigned muxBias:6;
	unsigned adcBias:6;
	unsigned unused:4;
};
#pragma pack(pop)


struct Rhs2116DeviceSettings{

	Rhs2116Format format;

	Rhs2116StimulusStep stepSize = Step10nA;
	Rhs2116DspCutoff dspCutoff = Dsp308Hz;

	Rhs2116AnalogLowCutoff lowCutoff = Low100mHz;
	Rhs2116AnalogLowCutoff lowCutoffRecovery = Low250Hz;
	Rhs2116AnalogHighCutoff highCutoff = High10000Hz;

	std::vector< std::vector<bool> > channelMap;

	// copy assignment (copy-and-swap idiom)
	Rhs2116DeviceSettings& Rhs2116DeviceSettings::operator=(Rhs2116DeviceSettings other) noexcept{
		std::swap(format, other.format);
		std::swap(stepSize, other.stepSize);
		std::swap(dspCutoff, other.dspCutoff);
		std::swap(lowCutoff, other.lowCutoff);
		std::swap(lowCutoffRecovery, other.lowCutoffRecovery);
		std::swap(highCutoff, other.highCutoff);
		std::swap(channelMap, other.channelMap);
		return *this;
	}

};


inline bool operator==(const Rhs2116DeviceSettings& lhs, const Rhs2116DeviceSettings& rhs){ return (lhs.stepSize == rhs.stepSize &&
																									lhs.dspCutoff == rhs.dspCutoff &&
																									lhs.lowCutoff == rhs.lowCutoff &&
																									lhs.lowCutoffRecovery == rhs.lowCutoffRecovery &&
																									lhs.highCutoff == rhs.highCutoff &&
																									lhs.channelMap == rhs.channelMap); }
inline bool operator!=(const Rhs2116DeviceSettings& lhs, const Rhs2116DeviceSettings& rhs) { return !(lhs == rhs); }





struct Rhs2116StimulusSettings{

	ONI::Settings::Rhs2116StimulusStep stepSize = Step10nA;
	std::map<unsigned int, std::vector<ONI::Rhs2116StimulusData>> deviceStimuli;

	// copy assignment (copy-and-swap idiom)
	Rhs2116StimulusSettings& Rhs2116StimulusSettings::operator=(Rhs2116StimulusSettings other) noexcept{
		std::swap(stepSize, other.stepSize);
		std::swap(deviceStimuli, other.deviceStimuli);
		return *this;
	}

};


inline bool operator==(const Rhs2116StimulusSettings& lhs, const Rhs2116StimulusSettings& rhs){ return (lhs.stepSize == rhs.stepSize &&
																										lhs.deviceStimuli == rhs.deviceStimuli); }
inline bool operator!=(const Rhs2116StimulusSettings& lhs, const Rhs2116StimulusSettings& rhs) { return !(lhs == rhs); }





} // namespace Settings
} // namespace ONI




