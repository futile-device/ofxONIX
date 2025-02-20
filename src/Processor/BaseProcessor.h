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
#include "../Type/RegisterTypes.h"
#include "../Type/SettingTypes.h"
#include "../Type/FrameTypes.h"
#include "../Type/GlobalTypes.h"

#pragma once

namespace ONI{
namespace Processor{

class BaseProcessor{

public:

	virtual ~BaseProcessor(){};

	virtual void reset() = 0; // processor specific for setup/reset

	virtual inline void process(oni_frame_t* frame) = 0;
	virtual inline void process(ONI::Frame::BaseFrame& frame) = 0;

	inline void subscribeProcessor(const std::string& processorName, const SubscriptionType& type, BaseProcessor * processor){
		const std::lock_guard<std::mutex> lock(mutex);
		std::map<std::string, BaseProcessor*>& processors = (type == PRE_PROCESSOR ? preProcessors : postProcessors);
		auto it = processors.find(processorName);
		if(it == processors.end()){
			LOGINFO("Adding processor %s", processorName.c_str());
			processors[processorName] = processor;
		}else{
			//LOGALERT("Processor %s already exists", processorName.c_str());
		}
	}

	inline void unsubscribeProcessor(const std::string& processorName, const SubscriptionType& type, BaseProcessor * processor){
		const std::lock_guard<std::mutex> lock(mutex);
		std::map<std::string, BaseProcessor*>& processors = (type == PRE_PROCESSOR ? preProcessors : postProcessors);
		auto it = processors.find(processorName);
		if(it == processors.end()){
			LOGALERT("No processor %s", processorName.c_str());
		}else{
			LOGINFO("Deleting processor %s", processorName.c_str());
			processors.erase(it);
		}
	}

	inline const ONI::Processor::TypeID& getProcessorTypeID(){
		return processorTypeID;
	}

	inline const std::string& getName(){
		return processorName;
	}

	inline const uint32_t& getNumProbes(){
		return numProbes;
	}

protected:

	std::map<std::string, BaseProcessor*> preProcessors;
	std::map<std::string, BaseProcessor*> postProcessors;

	ONI::Processor::TypeID processorTypeID = ONI::Processor::TypeID::UNDEFINED;
	std::string processorName = "UNDEFINED";

	uint32_t numProbes = 0;

private:

	std::mutex mutex;

};


} // namespace Processor
} // namespace ONI










