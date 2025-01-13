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
#include "../Type/GlobalTypes.h"

#include "../Processor/BaseProcessor.h"
#include "../Processor/Rhs2116MultiProcessor.h"

#pragma once

namespace ONI{
namespace Processor{

class SpikeProcessor : public BaseProcessor{

public:

	~SpikeProcessor(){};

    void setup(){ //ONI::Processor::Rhs2116MultiProcessor* multi

        LOGDEBUG("Setting up SpikeProcessor");

        //if(multi->devices.size() == 0){
        //    LOGERROR("Add devices to Rhs2116MultiProcessor before setting up spike detection");
        //    assert(false);
        //    return;
        //}

        //this->multi = multi;
        //this->multi->subscribeProcessor("SpikeProcessor", ONI::Processor::FrameProcessorType::POST_PROCESSOR, this);

    }

	inline void process(oni_frame_t* frame){};
	inline void process(ONI::Frame::BaseFrame& frame){
		

	}

protected:

    //ONI::Processor::Rhs2116MultiProcessor* multi = nullptr;

};


} // namespace Processor
} // namespace ONI










