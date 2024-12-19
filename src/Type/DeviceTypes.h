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

enum FrameProcessorType{
	PRE_FRAME_PROCESSOR = 0,
	POST_FRAME_PROCESSOR
};

static std::string toString(const ONI::Device::FrameProcessorType& typeID){
	switch(typeID){
	case PRE_FRAME_PROCESSOR: {return "PRE_FRAME_PROCESSOR"; break;}
	case POST_FRAME_PROCESSOR: {return "POST_FRAME_PROCESSOR"; break;}
	}
};



} // namespace Device





} // namespace ONI{