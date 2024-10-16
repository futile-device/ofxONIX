//
//  ONIDevice.h
//
//  Created by Matt Gingold on 13.09.2024.
//

#include "oni.h"
#include "onix.h"

#include "ofMain.h"
#include "ofxFutilities.h"

#pragma once

static inline uint64_t uint16_to_bin16(uint16_t u) {
	uint64_t sum = 0;
	uint64_t power = 1;
	while (u) {
		if (u & 1) {
			sum += power;
		}
		power *= 16;
		u /= 2;
	}
	return sum;
}

class ONIRegister{

public:

	ONIRegister() : addr(0), name("UNKNOWN") {};
	//ONIRegister(unsigned int val, std::string name) : addr(val), name(name) {};
	const std::string& getName() const { return name; };
	const unsigned int& getAddress() const { return addr; };
	const std::string& getDeviceName() const { return devn; };
	operator unsigned int&() { return addr; }
	operator unsigned int() const { return addr; }

	friend inline bool operator< (const ONIRegister& lhs, const ONIRegister& rhs);
	friend inline bool operator==(const ONIRegister& lhs, const ONIRegister& rhs);
	friend inline std::ostream& operator<<(std::ostream& os, const ONIRegister& obj);

	// copy assignment (copy-and-swap idiom)
	ONIRegister& ONIRegister::operator=(ONIRegister other) noexcept {
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

inline std::ostream& operator<<(std::ostream& os, const ONIRegister& obj) {
	os << obj.addr << " " << obj.name;
	return os;
}

inline bool operator< (const ONIRegister& lhs, const ONIRegister& rhs) { return lhs.addr < rhs.addr; }
inline bool operator> (const ONIRegister& lhs, const ONIRegister& rhs) { return rhs < lhs; }
inline bool operator<=(const ONIRegister& lhs, const ONIRegister& rhs) { return !(lhs > rhs); }
inline bool operator>=(const ONIRegister& lhs, const ONIRegister& rhs) { return !(lhs < rhs); }

inline bool operator==(const ONIRegister& lhs, const ONIRegister& rhs) { return (lhs.addr == rhs.addr && lhs.name == rhs.name && lhs.devn == rhs.devn); }
inline bool operator!=(const ONIRegister& lhs, const ONIRegister& rhs) { return !(lhs == rhs); }

class FmcRegister : public ONIRegister{
public:
	FmcRegister(unsigned int val, std::string name) { this->addr = val; this->name = name; this->devn = "FMCDevice"; };
};

class HeartBeatRegister : public ONIRegister{
public:
	HeartBeatRegister(unsigned int val, std::string name) { this->addr = val; this->name = name; this->devn = "HeartBeatDevice"; };
};

class Rhs2116Register : public ONIRegister{
public:
	Rhs2116Register(unsigned int val, std::string name) { this->addr = val; this->name = name; this->devn = "RHS2116Device"; };
};

inline std::ostream& operator<<(std::ostream& os, const oni_device_t& type) {

	char typeBuffer[256];

	sprintf(typeBuffer, "%05u: 0x%02x.0x%02x\t|%02d\t|%02d\t|%02u\t|%02u\t|%s",
		   type.idx,
		   (uint8_t)(type.idx >> 8),
		   (uint8_t)type.idx,
			type.id,
			type.version,
			type.read_size,
			type.write_size,
			onix_device_str(type.id));

	os << typeBuffer;
	return os;
}

//class ONIContext; // pre-declare for friend class?

class ONIDevice{

public:


	enum TypeID{
		HEARTBEAT	= 12,
		FMC			= 23,
		RHS2116		= 31,
		NONE		= 2048
	};

	//ONIDevice(){};
	virtual ~ONIDevice(){};

	virtual void setup(volatile oni_ctx* context, oni_device_t type, uint64_t acq_clock_khz){
		LOGDEBUG("Setting up device: %s", onix_device_str(type.id));
		ctx = context;
		deviceType = type;
		deviceTypeID = (TypeID)type.id;
		deviceName = std::string(onix_device_str(type.id));
		this->acq_clock_khz = acq_clock_khz;
	}

	inline const std::string& getName(){
		return deviceName;
	}

	inline const TypeID& getDeviceTypeID(){
		return deviceTypeID;
	}

	void reset(){
		firstFrameTime = -1;
		bDeviceRequiresContextRestart = false;
	}

	virtual inline void process(oni_frame_t* frame) = 0;

	inline bool getDeviceRequiresContextRestart(){
		return bDeviceRequiresContextRestart;
	}

	//inline bool isDeviceType(unsigned int deviceID){ return (deviceID == deviceType.id); };

protected:

	unsigned int readRegister(const ONIRegister& reg){

		assert(ctx != NULL && deviceTypeID != -1, "ONIContext and ONIDevice must be setup");
		
		int rc = ONI_ESUCCESS;

		unsigned int value = 0;
		rc = oni_read_reg(*ctx, deviceType.idx, reg.getAddress(), &value);
		if(rc){
			LOGERROR("Could not read read register: %i %s", reg, oni_error_str(rc));
			//assert(false); //???
			return 0;
		}

		LOGDEBUG("%s  read register: %s == dec(%05i) bin(%016llX) hex(%#06x)", reg.getDeviceName().c_str(), reg.getName().c_str(), value, uint16_to_bin16(value), value);

		return value;

	}

	bool writeRegister(const ONIRegister& reg, const unsigned int& value){

		assert(ctx != NULL && deviceTypeID != -1, "ONIContext and ONIDevice must be setup");
		LOGDEBUG("%s write register: %s == dec(%05i) bin(%016llX) hex(%#06x)", reg.getDeviceName().c_str(), reg.getName().c_str(), value, uint16_to_bin16(value), value);

		int rc = ONI_ESUCCESS;

		rc = oni_write_reg(*ctx, deviceType.idx, reg.getAddress(), value);
		
		if(rc){
			LOGERROR("Could not write to register: %s", oni_error_str(rc));
			return false;
		}

		// double check it set correctly
		unsigned int t_value = 0;
		rc = oni_read_reg(*ctx, deviceType.idx, reg.getAddress(), &t_value);
		if(rc || value != t_value){
			LOGERROR("Write does not match read values");
			return false;
		}

		return true;
	}

	inline long double getAcqDeltaTimeMicros(const uint64_t& t){
		if(firstFrameTime == -1) firstFrameTime = t;
		return (t - firstFrameTime) / (long double)acq_clock_khz * 1000000; // 250000000
	}

	bool bDeviceRequiresContextRestart = false;

	volatile oni_ctx* ctx = NULL;
	oni_device_t deviceType;

	TypeID deviceTypeID = TypeID::NONE;

	std::string deviceName = "UNDEFINED";

	long double firstFrameTime = -1;
	uint64_t acq_clock_khz = -1; // 250000000

};



class FmcDevice : public ONIDevice{

public:

	static inline const FmcRegister ENABLE		= FmcRegister(0, "ENABLE");			// The LSB is used to enable or disable the device data stream
	static inline const FmcRegister GPOSTATE	= FmcRegister(1, "GPOSTATE");		// GPO output state (bits 31 downto 3: ignore. bits 2 downto 0: ‘1’ = high, ‘0’ = low)
	static inline const FmcRegister DESPWR		= FmcRegister(2, "DESPWR");			// Set link deserializer PDB state, 0 = deserializer power off else on. Does not affect port voltage.
	static inline const FmcRegister PORTVOLTAGE = FmcRegister(3, "PORTVOLTAGE");	// 10 * link voltage
	static inline const FmcRegister SAVEVOLTAGE = FmcRegister(4, "SAVEVOLTAGE");	// Save link voltage to non-volatile EEPROM if greater than 0. This voltage will be applied after POR.
	static inline const FmcRegister LINKSTATE	= FmcRegister(5, "LINKSTATE");		// bit 1 pass; bit 0 lock

	//FmcDevice(){};

	~FmcDevice(){
		LOGDEBUG("FMC Device DTOR");
		//readRegister(ENABLE);
		//writeRegister(ENABLE, 0);
	};

	unsigned int readRegister(const FmcRegister& reg){
		return ONIDevice::readRegister(reg);
	}

	bool writeRegister(const FmcRegister& reg, const unsigned int& value){
		//if(reg == PORTVOLTAGE && value > 69){
		//	LOGALERT("Voltage too high!!? %i", value);
		//	assert(false);
		//	return false;
		//}
		return ONIDevice::writeRegister(reg, value);
	}

	inline void process(oni_frame_t* frame){
		// nothing
	}

	inline bool getLinkState(const bool& bCheckRegisters = true){
		if(bCheckRegisters) bLinkState = (bool)readRegister(FmcDevice::LINKSTATE);
		return bLinkState;
	}

	inline bool setEnabled(const bool& b){
		bool bOk = writeRegister(FmcDevice::ENABLE, (unsigned int)b);
		if(bOk) getEnabled(true);
		return bOk;
	}

	inline bool getEnabled(const bool& bCheckRegisters = true){
		if(bCheckRegisters) bEnabled = (bool)readRegister(FmcDevice::ENABLE);
		return bEnabled;
	}

	inline bool setPortVoltage(const float& v){
		writeRegister(FmcDevice::PORTVOLTAGE, 0); // have to set to 0 and back to voltage or else won't init properly
		ofSleepMillis(500); // needs to pause
		bool bOk = writeRegister(FmcDevice::PORTVOLTAGE, v * 10);
		ofSleepMillis(500); // needs to pause
		if(bOk) getPortVoltage(true); bDeviceRequiresContextRestart = true;
		return bOk;
	}

	inline float getPortVoltage(const bool& bCheckRegisters = true){
		if(bCheckRegisters) voltage = readRegister(FmcDevice::PORTVOLTAGE) / 10.0f;
		return voltage;
	}

protected:

	float voltage = 0;
	bool bLinkState = false;
	bool bEnabled = false;

};


class HeartBeatDevice : public ONIDevice{

public:

	static inline const HeartBeatRegister ENABLE	= HeartBeatRegister(0, "ENABLE");	// Enable the heartbeat
	static inline const HeartBeatRegister CLK_DIV	= HeartBeatRegister(1, "CLK_DIV");	// Heartbeat clock divider ratio. Default results in 10 Hz heartbeat. Values less than CLK_HZ / 10e6 Hz will result in 1kHz.
	static inline const HeartBeatRegister CLK_HZ	= HeartBeatRegister(2, "CLK_HZ");	// The frequency parameter, CLK_HZ, used in the calculation of CLK_DIV

	//HeartBeatDevice(){};

	~HeartBeatDevice(){
		LOGDEBUG("HeartBeat Device DTOR");
		//readRegister(ENABLE); // FOR SOME REASON IF WE DISABLE WE HAVE A PROBLEM
		//writeRegister(ENABLE, 0);
	};

	unsigned int readRegister(const HeartBeatRegister& reg){
		return ONIDevice::readRegister(reg);
	}

	bool writeRegister(const HeartBeatRegister& reg, const unsigned int& value){
		return ONIDevice::writeRegister(reg, value);
	}

	inline void process(oni_frame_t* frame){
		
		LOGDEBUG("HEARTBEAT: %i %i %i %s", frame->dev_idx, (uint64_t)ONIDevice::getAcqDeltaTimeMicros(frame->time), frame->data_sz, deviceName.c_str());
		//fu::debug << (uint64_t)ONIDevice::getAcqDeltaTimeMicros(frame->time) << fu::endl;
	}

	inline bool setEnabled(const bool& b){
		bool bOk = writeRegister(HeartBeatDevice::ENABLE, (unsigned int)b);
		if(bOk) getEnabled(true);
		return bOk;
	}

	inline bool getEnabled(const bool& bCheckRegisters = true){
		if(bCheckRegisters) bEnabled = (bool)readRegister(HeartBeatDevice::ENABLE);
		return bEnabled;
	}

	inline bool setFrequencyHz(const unsigned int& beatsPerSecond){
		unsigned int clkHz = readRegister(HeartBeatDevice::CLK_HZ);
		unsigned int clkDv = clkHz / beatsPerSecond;
		bool bOk = writeRegister(HeartBeatDevice::CLK_DIV, clkDv);
		if(bOk) getFrequencyHz(true); bDeviceRequiresContextRestart = true;
		return bOk;
	}

	inline float getFrequencyHz(const bool& bCheckRegisters = true){
		if(bCheckRegisters){
			unsigned int clkHz  = readRegister(HeartBeatDevice::CLK_HZ);
			unsigned int clkDv = readRegister(HeartBeatDevice::CLK_DIV);
			frequencyHz = clkHz / clkDv;
			LOGDEBUG("Heartbeat FrequencyHz: %i", frequencyHz);
		}
		return frequencyHz;
	}

protected:

	unsigned int frequencyHz = 0;
	bool bEnabled = false;

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

#pragma pack(push, 1)
	struct Rhs2116Format{
		unsigned dspCutOff:4;
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
		long double deltaTime;
	};
#pragma pack(pop)

//#pragma pack(push, 1)
//	struct Rhs2116ProcessedDataFrame{
//		uint64_t hubTime;
//		float ac[16];
//		float dc[16];
//		float acAvg[16];
//		float dcAvg[16];
//		float acStdDev[16];
//		float dcStdDev[16];
//		uint64_t acqTime;
//		long double deltaTime;
//	};
//#pragma pack(pop)

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
		//sampleRateTimer.start<fu::millis>(1000);
		//cntTimer.start<fu::millis>(1000);
		
	};
	
	~Rhs2116Device(){
		LOGDEBUG("RHS2116 Device DTOR");
		const std::lock_guard<std::mutex> lock(mutex);
		rawDataFrames.clear();
		//readRegister(ENABLE);
		//writeRegister(ENABLE, 0);
	};

	unsigned int readRegister(const Rhs2116Register& reg){
		return ONIDevice::readRegister(reg);
	}

	bool writeRegister(const Rhs2116Register& reg, const unsigned int& value){
		return ONIDevice::writeRegister(reg, value);
	}

	const std::vector<Rhs2116DataFrame>& getRawDataFrames(){
		const std::lock_guard<std::mutex> lock(mutex);
		return rawDataFrames;
	}

	const std::vector<Rhs2116DataFrame>& getDrawDataFrames(){
		const std::lock_guard<std::mutex> lock(mutex);
		return drawDataFrames;
	}

	inline void getSortedDataFrames(std::vector<Rhs2116DataFrame>& to, const size_t& stride = 1){
		//const std::lock_guard<std::mutex> lock(mutex);

		//size_t n_frames = floor(bufferSize / stride);
		//std::vector<Rhs2116DataFrame> to(n_frames);
		//to.resize(n_frames);
		mutex.lock();
		to = rawDataFrames;
		//size_t sorted_idx = currentBufferIDX;
		//for(size_t i = 0; i < n_frames; ++i){
		//	to[i] = rawDataFrames[sorted_idx];
		//	sorted_idx = (sorted_idx + stride) % bufferSize;
		//}
		mutex.unlock();
		std::sort(to.begin(), to.end(), acquisition_clock_compare());
		//if(to[0].acqTime != rawDataFrames[currentBufferIDX].acqTime) fu::debug << "not sorted?" << fu::endl;
		//return to;
	}

	Rhs2116DataFrame getLastDataFrame(){
		const std::lock_guard<std::mutex> lock(mutex);
		return rawDataFrames[lastBufferIDX];
	}

	fu::Timer frameTimer;

	double getFrameTimerAvg(){
		const std::lock_guard<std::mutex> lock(mutex);
		frameTimer.stop();
		double t = frameTimer.avg<fu::micros>();
		frameTimer.start();
		return t;
	}
	struct ProbeStatistics{
		float sum;
		float mean;
		float ss;
		float variance;
		float deviation;
	};
	std::vector<ProbeStatistics> probeStats;

	//uint64_t cnt = 0;
	inline void process(oni_frame_t* frame){
		const std::lock_guard<std::mutex> lock(mutex);
		//++cnt;
		//if(cntTimer.finished()){
		//	fu::debug << "count: " << cnt << fu::endl;
		//	cntTimer.restart();
		//	cnt = 0;
		//}
		memcpy(&rawDataFrames[currentBufferIDX], frame->data, frame->data_sz);  // copy the data payload including hub clock
		rawDataFrames[currentBufferIDX].acqTime = frame->time;					// copy the acquisition clock
		rawDataFrames[currentBufferIDX].deltaTime = ONIDevice::getAcqDeltaTimeMicros(frame->time); //fu::time::now<fu::micros>() * 1000;
		//fu::debug << "DEV: " << frame->dev_idx << " " << frame->time << " " << rawDataFrames[currentBufferIDX].hubTime << fu::endl;

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
		frameTimer.fcount();

		lastBufferIDX = currentBufferIDX;
		currentBufferIDX = (currentBufferIDX + 1) % bufferSize;
		//ofSleepMillis(1);
		//sortedDataFrames = rawDataFrames;
		//std::sort(sortedDataFrames.begin(), sortedDataFrames.end(), acquisition_clock_compare());
		//++sampleCount;

	}

	//std::vector< std::vector<float> > acProbes;
	//std::vector< std::vector<float> > dcProbes;
	//std::vector<uint64_t> deltaTimes;

	std::vector<Rhs2116DataFrame> drawDataFrames;
	size_t drawStride = 100;
	size_t currentDrawBufferIDX = 0;
	size_t drawBufferSize = 0;
	void setBufferSizeSamples(const size_t& samples){
		const std::lock_guard<std::mutex> lock(mutex);
		bufferSize = samples;
		currentBufferIDX = 0;
		rawDataFrames.resize(bufferSize);
		drawBufferSize = bufferSize / drawStride;
		currentDrawBufferIDX = 0;
		drawDataFrames.resize(drawBufferSize);
		LOGINFO("Raw  Buffer size: %i (samples)", bufferSize);
		LOGINFO("Draw Buffer size: %i (samples)", drawBufferSize);
		probeStats.resize(16);
		//acProbes.resize(16);
		//dcProbes.resize(16);
		//for(int i = 0; i < 16; ++i){
		//	acProbes[i].resize(drawBufferSize);
		//	dcProbes[i].resize(drawBufferSize);
		//}
		//deltaTimes.resize(drawBufferSize);
	}

	void setBufferSizeMillis(const int& millis){
		// TODO: I can't seem to change the ADC Bias and MUX settings to get different sample rates
		// SO I am just going to assume 30kS/s sample rate for the RHS2116 hard coded to sampleRate
		size_t samples = sampleRatekSs * millis / 1000;
		setBufferSizeSamples(samples);
	}

	inline void printSampleCount(){
		const std::lock_guard<std::mutex> lock(mutex);
		fu::debug << sampleCount << fu::endl;
		sampleCount = 0;
	}

private:

	uint64_t hubClockFirst = -1;

	std::vector<Rhs2116DataFrame> rawDataFrames;

	size_t bufferSize = 100;
	size_t currentBufferIDX = 0;
	size_t lastBufferIDX = 0;

	const int sampleRatekSs = 30000; // see above, cannot seem to change this using BIAS registers!!

	uint64_t sampleCount = 0;
	//fu::Timer sampleRateTimer;

	std::mutex mutex;

};

inline std::ostream& operator<<(std::ostream& os, const Rhs2116Device::Rhs2116Format& format) {

	os << "[unsigned: " << *(unsigned int*)&format << "] ";
	os << "[dspCutOff: " << format.dspCutOff << "] ";
	os << "[dsPen: " << format.dspEnable << "] ";
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