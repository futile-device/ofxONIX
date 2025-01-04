//
//  DeviceTypes.h
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

#pragma once

namespace ONI{
namespace Device{

enum TypeID{
	HEARTBEAT		= 12,
	FMC				= 23,
	RHS2116			= 31,
	//RHS2116STIM		= 31,
	RHS2116MULTI	= 666,
	RHS2116STIM		= 667,
	NONE			= 2048
};

static std::string toString(const ONI::Device::TypeID& typeID){
	switch(typeID){
		case HEARTBEAT: {return "HeartBeat Device"; break;}
		case FMC: {return "FMC Device"; break;}
		case RHS2116: {return "RHS2116 Device"; break;}
		case RHS2116MULTI: {return "RHS2116MULTI Device"; break;}
		case RHS2116STIM: {return "RHS2116STIM Device"; break;}
		case NONE: {return "No Device"; break;}
	}
};


} // namespace Device

namespace Processor{

enum ProcessorType{
	PRE_PROCESSOR = 0,
	POST_PROCESSOR
};

static std::string toString(const ONI::Processor::ProcessorType& typeID){
	switch(typeID){
	case PRE_PROCESSOR: {return "PRE_PROCESSOR"; break;}
	case POST_PROCESSOR: {return "POST_PROCESSOR"; break;}
	}
};

} // namespace Processor{



} // namespace ONI{