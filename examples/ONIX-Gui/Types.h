#pragma once

namespace ONI{

enum DeviceTypeID{
	HEARTBEAT		= 12,
	FMC				= 23,
	RHS2116			= 31,
	//RHS2116STIM		= 31,
	RHS2116MULTI	= 666,
	RHS2116STIM		= 667,
	NONE			= 2048
};

} // namespace ONI



static std::string DeviceTypeIDStr(const ONI::DeviceTypeID& typeID){
	switch(typeID){
	case HEARTBEAT: {return "HeartBeat Device"; break;}
	case FMC: {return "FMC Device"; break;}
	case RHS2116: {return "RHS2116 Device"; break;}
	case RHS2116MULTI: {return "RHS2116MULTI Device"; break;}
	case RHS2116STIM: {return "RHS2116STIM Device"; break;}
	case NONE: {return "No Device"; break;}
	}
};