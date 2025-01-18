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

#pragma once

namespace ONI{

namespace Interface{
class ChannelMapInterface;
}

namespace Processor{

class ChannelMapProcessor : public BaseProcessor{

public:

	friend class ChannelMapInterface;

	ChannelMapProcessor(){
		BaseProcessor::processorTypeID = ONI::Processor::TypeID::CHANNEL_PROCESSOR;
		BaseProcessor::processorName = toString(processorTypeID);
	}

	~ChannelMapProcessor(){};

    void setup(ONI::Processor::BaseProcessor* source){
        LOGDEBUG("Setting up ChannelMap Processor");
		ONI::Processor::BaseProcessor::numProbes = source->getNumProbes();
		reset();
    }

	inline void process(oni_frame_t* frame){};
	inline void process(ONI::Frame::BaseFrame& frame){}

	void reset(){

		// reset to defualt identity matrix channel map

		channelMapBoolMatrix.clear();
		channelMapBoolMatrix.resize(numProbes);
		for(size_t y = 0; y < numProbes; ++y){
			channelMapBoolMatrix[y].resize(numProbes);
			for(size_t x = 0; x < numProbes; ++x){
				channelMapBoolMatrix[y][x] = 0;
				if(x == y) channelMapBoolMatrix[y][x] = 1;
			}
		}

		updateChannelMaps();

	} 

	void setChannelMap(const std::vector<size_t>& channelMap){

		assert(numProbes != 0, "Did you setup the channel mapper correctly?");
		if(channelMap.size() == numProbes){

			settings.channelMap = channelMap;

			for(size_t y = 0; y < numProbes; ++y){
				for(size_t x = 0; x < numProbes; ++x){
					channelMapBoolMatrix[y][x] = false;
				}
				size_t xx = settings.channelMap[y];
				channelMapBoolMatrix[y][xx] = true;

				inverseChannelMap.resize(numProbes); // make an inverse for when we need to map in the other direction eg., for stimulus plots
				for(size_t i = 0; i < settings.channelMap.size(); ++i){
					inverseChannelMap[settings.channelMap[i]] = i;
				}

			}
		}else{
			LOGERROR("Channel map size doesn't equal number of probes");
		}
	}

	const std::vector<size_t>& getChannelMap(){
		return settings.channelMap;
	}

	const std::vector<size_t>& getInverseChannelMap(){
		return inverseChannelMap;
	}

	void updateChannelMaps(){

		if(channelMapBoolMatrix.size() == 0) return;

		settings.channelMap.resize(numProbes);
		for(size_t y = 0; y < numProbes; ++y){
			size_t idx = 0;
			for(size_t x = 0; x < numProbes; ++x){
				if(channelMapBoolMatrix[y][x]){
					idx = x;
					break;
				}
			}
			settings.channelMap[y] = idx;
		}

		inverseChannelMap.resize(numProbes); // make an inverse for when we need to map in the other direction eg., for stimulus plots
		for(size_t i = 0; i < settings.channelMap.size(); ++i){
			inverseChannelMap[settings.channelMap[i]] = i;
		}

	}

	void setChannelMapFromBoolMatrix(const std::vector< std::vector<bool> >& matrix){
		channelMapBoolMatrix = matrix;
		updateChannelMaps();
	}

	std::vector< std::vector<bool> >& getChannelMapBoolMatrix(){
		return channelMapBoolMatrix;
	}



protected:

	std::vector< std::vector<bool> > channelMapBoolMatrix;
	std::vector<size_t> inverseChannelMap;

	ONI::Settings::ChannelMapProcessorSettings settings;

};


} // namespace Processor
} // namespace ONI










