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

#include "../Type/Log.h"

#pragma once

namespace ONI{
namespace ContextOption{

class BaseOption{

public:

	BaseOption() : optn(0), name("UNKNOWN") {};
	BaseOption(unsigned int optn, std::string name) { this->optn = optn; this->name = name; };

	const std::string& getName() const { return name; };
	const unsigned int& getOption() const { return optn; };
	operator unsigned int&() { return optn; }
	operator unsigned int() const { return optn; }

	friend inline bool operator< (const BaseOption& lhs, const BaseOption& rhs);
	friend inline bool operator==(const BaseOption& lhs, const BaseOption& rhs);
	friend inline std::ostream& operator<<(std::ostream& os, const BaseOption& obj);

	// copy assignment (copy-and-swap idiom)
	BaseOption& BaseOption::operator=(BaseOption other) noexcept {
		std::swap(optn, other.optn);
		std::swap(name, other.name);
		return *this;
	}

protected:

	unsigned int optn = -1;
	std::string name = "UNKNOWN";

};

inline std::ostream& operator<<(std::ostream& os, const BaseOption& obj) {
	os << obj.optn << " " << obj.name;
	return os;
}

inline bool operator< (const BaseOption& lhs, const BaseOption& rhs) { return lhs.optn < rhs.optn; }
inline bool operator> (const BaseOption& lhs, const BaseOption& rhs) { return rhs < lhs; }
inline bool operator<=(const BaseOption& lhs, const BaseOption& rhs) { return !(lhs > rhs); }
inline bool operator>=(const BaseOption& lhs, const BaseOption& rhs) { return !(lhs < rhs); }

inline bool operator==(const BaseOption& lhs, const BaseOption& rhs) { return (lhs.optn == rhs.optn && lhs.name == rhs.name); }
inline bool operator!=(const BaseOption& lhs, const BaseOption& rhs) { return !(lhs == rhs); }


static inline const BaseOption DEVICETABLE			= BaseOption(ONI_OPT_DEVICETABLE, "DEVICETABLE");
static inline const BaseOption NUMDEVICES			= BaseOption(ONI_OPT_NUMDEVICES, "NUMDEVICES");
static inline const BaseOption RUNNING				= BaseOption(ONI_OPT_RUNNING, "RUNNING");
static inline const BaseOption RESET				= BaseOption(ONI_OPT_RESET, "RESET");
static inline const BaseOption SYSCLKHZ				= BaseOption(ONI_OPT_SYSCLKHZ, "SYSCLKHZ");
static inline const BaseOption ACQCLKHZ				= BaseOption(ONI_OPT_ACQCLKHZ, "ACQCLKHZ");
static inline const BaseOption RESETACQCOUNTER		= BaseOption(ONI_OPT_RESETACQCOUNTER, "RESETACQCOUNTER");
static inline const BaseOption HWADDRESS			= BaseOption(ONI_OPT_HWADDRESS, "HWADDRESS");
static inline const BaseOption MAXREADFRAMESIZE		= BaseOption(ONI_OPT_MAXREADFRAMESIZE, "MAXREADFRAMESIZE");
static inline const BaseOption MAXWRITEFRAMESIZE	= BaseOption(ONI_OPT_MAXWRITEFRAMESIZE, "MAXWRITEFRAMESIZE");
static inline const BaseOption BLOCKREADSIZE		= BaseOption(ONI_OPT_BLOCKREADSIZE, "BLOCKREADSIZE");
static inline const BaseOption BLOCKWRITESIZE		= BaseOption(ONI_OPT_BLOCKWRITESIZE, "BLOCKWRITESIZE");
static inline const BaseOption CUSTOMBEGIN			= BaseOption(ONI_OPT_CUSTOMBEGIN, "CUSTOMBEGIN");



} // namespace ContextOption
} // namespace ONI

namespace ONI{
namespace Register{




class BaseRegister{

public:

	BaseRegister() : addr(0), name("UNKNOWN") {};
	//ONIRegister(unsigned int val, std::string name) : addr(val), name(name) {};
	const std::string& getName() const { return name; };
	const unsigned int& getAddress() const { return addr; };
	const std::string& getDeviceName() const { return devn; };
	operator unsigned int&() { return addr; }
	operator unsigned int() const { return addr; }

	friend inline bool operator< (const BaseRegister& lhs, const BaseRegister& rhs);
	friend inline bool operator==(const BaseRegister& lhs, const BaseRegister& rhs);
	friend inline std::ostream& operator<<(std::ostream& os, const BaseRegister& obj);

	// copy assignment (copy-and-swap idiom)
	BaseRegister& BaseRegister::operator=(BaseRegister other) noexcept {
		std::swap(addr, other.addr);
		std::swap(name, other.name);
		std::swap(devn, other.devn);
		return *this;
	}

protected:

	unsigned int addr = 0;
	std::string name = "UNKNOWN";
	std::string devn = "UNKNOWN";

};

inline std::ostream& operator<<(std::ostream& os, const BaseRegister& obj) {
	os << obj.addr << " " << obj.name;
	return os;
}

inline bool operator< (const BaseRegister& lhs, const BaseRegister& rhs) { return lhs.addr < rhs.addr; }
inline bool operator> (const BaseRegister& lhs, const BaseRegister& rhs) { return rhs < lhs; }
inline bool operator<=(const BaseRegister& lhs, const BaseRegister& rhs) { return !(lhs > rhs); }
inline bool operator>=(const BaseRegister& lhs, const BaseRegister& rhs) { return !(lhs < rhs); }

inline bool operator==(const BaseRegister& lhs, const BaseRegister& rhs) { return (lhs.addr == rhs.addr && lhs.name == rhs.name && lhs.devn == rhs.devn); }
inline bool operator!=(const BaseRegister& lhs, const BaseRegister& rhs) { return !(lhs == rhs); }



class FmcRegister : public ONI::Register::BaseRegister{
public:
	FmcRegister(unsigned int val, std::string name) { this->addr = val; this->name = name; this->devn = "FMCDevice"; };
};

namespace Fmc{
static inline const FmcRegister ENABLE		= FmcRegister(0, "ENABLE");			// The LSB is used to enable or disable the device data stream
static inline const FmcRegister GPOSTATE	= FmcRegister(1, "GPOSTATE");		// GPO output state (bits 31 downto 3: ignore. bits 2 downto 0: ‘1’ = high, ‘0’ = low)
static inline const FmcRegister DESPWR		= FmcRegister(2, "DESPWR");			// Set link deserializer PDB state, 0 = deserializer power off else on. Does not affect port voltage.
static inline const FmcRegister PORTVOLTAGE = FmcRegister(3, "PORTVOLTAGE");	// 10 * link voltage
static inline const FmcRegister SAVEVOLTAGE = FmcRegister(4, "SAVEVOLTAGE");	// Save link voltage to non-volatile EEPROM if greater than 0. This voltage will be applied after POR.
static inline const FmcRegister LINKSTATE	= FmcRegister(5, "LINKSTATE");		// bit 1 pass; bit 0 lock
} // namespace Fmc


class HeartBeatRegister : public ONI::Register::BaseRegister{
public:
	HeartBeatRegister(unsigned int val, std::string name) { this->addr = val; this->name = name; this->devn = "HeartBeatDevice"; };
};

namespace HeartBeat{
static inline const HeartBeatRegister ENABLE	= HeartBeatRegister(0, "ENABLE");	// Enable the heartbeat
static inline const HeartBeatRegister CLK_DIV	= HeartBeatRegister(1, "CLK_DIV");	// Heartbeat clock divider ratio. Default results in 10 Hz heartbeat. Values less than CLK_HZ / 10e6 Hz will result in 1kHz.
static inline const HeartBeatRegister CLK_HZ	= HeartBeatRegister(2, "CLK_HZ");	// The frequency parameter, CLK_HZ, used in the calculation of CLK_DIV
}; // namespace HeartBeat




class Rhs2116Register : public ONI::Register::BaseRegister{
public:
	Rhs2116Register(unsigned int val, std::string name) { this->addr = val; this->name = name; this->devn = "RHS2116Device"; };
};

namespace Rhs2116{

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
static inline const Rhs2116Register PWR			= Rhs2116Register(0x08, "PWR");			// Individual AC Amplifier Power

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

static inline const Rhs2116Register POS[16]	= {NEG00, NEG01, NEG02, NEG03, NEG04, NEG05, NEG06, NEG07, NEG08, NEG09, NEG010, NEG011, NEG012, NEG013, NEG014, NEG015};
static inline const Rhs2116Register NEG[16] = {POS00, POS01, POS02, POS03, POS04, POS05, POS06, POS07, POS08, POS09, POS010, POS011, POS012, POS013, POS014, POS015};

}; // namespace Rhs2116



class Rhs2116StimulusRegister : public ONI::Register::BaseRegister{
public:
	Rhs2116StimulusRegister(unsigned int val, std::string name) { this->addr = val; this->name = name; this->devn = "RHS2116StimnulusDevice"; };
};

namespace Rhs2116Stimulus{

// managed registers
static inline const Rhs2116StimulusRegister ENABLE = Rhs2116StimulusRegister(0, "ENABLE"); // Writes and reads to ENABLE are ignored without error
static inline const Rhs2116StimulusRegister TRIGGERSOURCE = Rhs2116StimulusRegister(1, "TRIGGERSOURCE"); // The LSB is used to determine the trigger source
static inline const Rhs2116StimulusRegister TRIGGER = Rhs2116StimulusRegister(2, "TRIGGER"); // Writing 0x1 to this register will trigger a stimulation sequence if the TRIGGERSOURCE is set to 0.
}; //namespace Rhs2116Stimulus{



} // namespace Register
} // namespace ONI









