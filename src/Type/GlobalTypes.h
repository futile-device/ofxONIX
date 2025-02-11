//
//  GlobalTypes.h
//
//  Created by Matt Gingold on 19.12.2024.
//


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

//#include "../Processor/BaseProcessor.h"
//#include "../Device/BaseDevice.h"

#pragma once

#define RHS2116_SAMPLE_FREQUENCY_HZ 30.1932367151e3
#define RHS2116_SAMPLE_FREQUENCY_MS RHS2116_SAMPLE_FREQUENCY_HZ / 1000.0
#define RHS2116_NUM_DEVICE_PROBES 16

namespace ONI{

class Context; // predeclare for friend access

namespace Processor{
class BaseProcessor; // predeclare for pointer storage in model
class BufferProcessor;
class ChannelMapProcessor;
class RecordProcessor;
class SpikeProcessor;
class Rhs2116MultiProcessor;
class Rhs2116StimProcessor;


enum TypeID{
	UNDEFINED				= 9999,
	HEARTBEAT_DEVICE		= 12,
	FMC_DEVICE				= 23,
	RHS2116_DEVICE			= 31,
	RHS2116STIM_DEVICE		= 32,
	BUFFER_PROCESSOR		= 600,
	CHANNEL_PROCESSOR		= 601,
	RECORD_PROCESSOR		= 602,
	SPIKE_PROCESSOR			= 603,
	RHS2116_MULTI_PROCESSOR	= 666,
	RHS2116_STIM_PROCESSOR	= 667,
};

static std::string toString(const ONI::Processor::TypeID& typeID){
	switch(typeID){
	case UNDEFINED: {return "UNDEFINED Processor"; break;}
	case HEARTBEAT_DEVICE: {return "HEARTBEAT Device"; break;}
	case FMC_DEVICE: {return "FMC Device"; break;}
	case RHS2116_DEVICE: {return "RHS2116 Device"; break;}
	case RHS2116STIM_DEVICE: {return "RHS2116STIM Device"; break;}
	case BUFFER_PROCESSOR: {return "BUFFER Processor"; break;}
	case CHANNEL_PROCESSOR: {return "CHANNEL Processor"; break;}
	case RECORD_PROCESSOR: {return "RECORD Processor"; break;}
	case SPIKE_PROCESSOR: {return "SPIKE Processor"; break;}
	case RHS2116_MULTI_PROCESSOR: {return "RHS2116MULTI Processor"; break;}
	case RHS2116_STIM_PROCESSOR: {return "RHS2116STIM Processor"; break;}
	default: {assert(false, "UNKNOWN TYPE"); return "UNKNOWN Processor"; break; }
	}
};

enum SubscriptionType{
	PRE_PROCESSOR = 0,
	POST_PROCESSOR
};

static std::string toString(const ONI::Processor::SubscriptionType& typeID){
	switch(typeID){
	case PRE_PROCESSOR: {return "PRE_PROCESSOR"; break;}
	case POST_PROCESSOR: {return "POST_PROCESSOR"; break;}
	}
};


}

namespace Device{
class BaseDevice; // predeclare for pointer storage in model
}

namespace Global{

template <class T>
class Singleton{
public:
	static T& Instance() {
		if(!m_pInstance) m_pInstance = new T;
		assert(m_pInstance !=nullptr);
		return *m_pInstance;
	};
protected:
	Singleton();
	~Singleton();
private:
	Singleton(Singleton const&);
	Singleton& operator=(Singleton const&);
	static T* m_pInstance;
};
template <class T> T* Singleton<T>::m_pInstance=nullptr;

class Model{

	friend class ONI::Context;

public:

	Model(){};
	~Model(){};

	volatile oni_ctx* getOnixContext(){
		return &ctx;
	}

	const uint32_t& getMaxReadSize(){
		assert(maxReadSize > 0);
		return maxReadSize;
	}

	const uint32_t& getMaxWriteSize(){
		assert(maxWriteSize > 0);
		return maxWriteSize;
	}

	const uint32_t& getAcquireClockKHZ(){
		assert(acq_clock_khz > 0);
		return acq_clock_khz;
	}

	const uint32_t& getSysClockKHZ(){
		assert(sys_clock_khz > 0);
		return sys_clock_khz;
	}

	std::map<uint32_t, ONI::Device::BaseDevice*>& getDevices(){
		return devices;
	}

	std::map<ONI::Processor::TypeID, ONI::Processor::BaseProcessor*>& getProcessors(){
		return processors;
	}

	inline void resetAcquireTimeStart(){
		firstAcquireTimeMicros = -1;
	}

	inline long double getAcquireDeltaTimeMicros(const uint64_t& t){
		if(firstAcquireTimeMicros == -1) firstAcquireTimeMicros = t;
		return (t - firstAcquireTimeMicros) / (long double)acq_clock_khz * 1000000; 
	}

	const std::vector<uint32_t>& getRhs2116DeviceOrderIDX(){
		return rhs2116DeviceOrderIDX;
	}

	const std::vector<uint32_t>& getRhs2116StimeDeviceOrderIDX(){
		return rhs2116StimDeviceOrderIDX;
	}

	ONI::Processor::BufferProcessor* getBufferProcessor(){
		return bufferProcessor;
	}

	ONI::Processor::ChannelMapProcessor* getChannelMapProcessor(){
		return channelMapProcessor;
	}

	ONI::Processor::RecordProcessor* getRecordProcessor(){
		return recordProcessor;
	}

	ONI::Processor::SpikeProcessor* getSpikeProcessor(){
		return spikeProcessor;
	}

	ONI::Processor::Rhs2116MultiProcessor* getRhs2116MultiProcessor(){
		return rhs2116MultiProcessor;
	}

	ONI::Processor::Rhs2116StimProcessor* getRhs2116StimProcessor(){
		return rhs2116StimProcessor;
	}

private:

	volatile oni_ctx ctx = nullptr;

	uint32_t maxReadSize = -1;
	uint32_t maxWriteSize = -1;
	uint32_t acq_clock_khz = -1; // defulat is 250000000 ns
	uint32_t sys_clock_khz = -1; // defulat is 250000000 ns

	std::map<uint32_t, ONI::Device::BaseDevice*> devices;
	std::map<ONI::Processor::TypeID, ONI::Processor::BaseProcessor*> processors; // for now just one instance of each?!
	//std::map<uint32_t, ONI::Device::BaseInterface*> interfaces; //?

	long double firstAcquireTimeMicros = -1;

	const std::vector<uint32_t> rhs2116DeviceOrderIDX = {256, 257, 512, 513};
	const std::vector<uint32_t> rhs2116StimDeviceOrderIDX = {258, 514};

	ONI::Processor::BufferProcessor * bufferProcessor = nullptr;
	ONI::Processor::ChannelMapProcessor * channelMapProcessor = nullptr;
	ONI::Processor::RecordProcessor * recordProcessor = nullptr;
	ONI::Processor::SpikeProcessor * spikeProcessor = nullptr;
	ONI::Processor::Rhs2116MultiProcessor * rhs2116MultiProcessor = nullptr;
	ONI::Processor::Rhs2116StimProcessor * rhs2116StimProcessor = nullptr;

};

static Model model;

} // namespace Global


} // namespace ONI{