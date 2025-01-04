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
#include "../Type/DataBuffer.h"
#include "../Type/RegisterTypes.h"
#include "../Type/SettingTypes.h"
#include "../Type/FrameTypes.h"
#include "../Type/DeviceTypes.h"
#include "../Processor/BaseProcessor.h"

#pragma once

namespace ONI{
namespace Processor{

class SpikeProcessor : public BaseProcessor{

public:

	~SpikeProcessor(){};

    void setup(ONI::Device::Rhs2116MultiDevice* multi){

        LOGDEBUG("Setting up SpikeProcessor");

        if(multi->devices.size() == 0){
            LOGERROR("Add devices to Rhs2116MultiDevice before setting up spike detection");
            assert(false);
            return;
        }

        this->multi = multi;
        this->multi->subscribeProcessor("SpikeProcessor", ONI::Processor::ProcessorType::POST_PROCESSOR, this);

    }

	inline void process(oni_frame_t* frame){};
	inline void process(ONI::Frame::BaseFrame& frame){
		

	}

protected:

    ONI::Device::Rhs2116MultiDevice* multi = nullptr;

};


} // namespace Processor
} // namespace ONI










