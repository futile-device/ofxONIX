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

//#include "../Device/BaseDevice.h"
#include "../Processor/BaseProcessor.h"

#pragma once

namespace ONI{
namespace Interface{

class BaseInterface : public ONI::Processor::BaseProcessor{

public:

	//ONIInterface(){};
	virtual ~BaseInterface(){};

	// inherited from BufferProcessor
	//virtual inline void process(ONI::Frame::BaseFrame& frame) = 0;
	//virtual inline void process(oni_frame_t* frame) = 0;
	//void subscribeProcessor(const std::string& processorName, const FrameProcessorType& type, BufferProcessor * processor){...}
	//void unsubscribeProcessor(const std::string& processorName, const FrameProcessorType& type, BufferProcessor * processor){...}
	//std::mutex& getMutex(){...}

	virtual inline void gui(ONI::Processor::BaseProcessor& processor) = 0;

	virtual bool save(std::string presetName) = 0;
	virtual bool load(std::string presetName) = 0;

};

} // namespace Interface
} // namespace ONI