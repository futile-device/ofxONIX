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

#include "../DSPFilters/Dsp.h"

#pragma once

namespace ONI{

namespace Interface{
class FilterInterface;
};

class Context;

namespace Processor{

class FilterProcessor : public BaseProcessor{

public:

	friend class ONI::Interface::FilterInterface;
	friend class ONI::Context;


	FilterProcessor(){
		BaseProcessor::processorTypeID = ONI::Processor::TypeID::FILTER_PROCESSOR;
		BaseProcessor::processorName = toString(processorTypeID);
	}

	~FilterProcessor(){};

    void setup(ONI::Processor::BaseProcessor* source){

        LOGDEBUG("Setting up FilterProcessor");

		this->source = source;
		this->source->subscribeProcessor("FilterProcessor", ONI::Processor::SubscriptionType::POST_PROCESSOR, this);

		BaseProcessor::numProbes = source->getNumProbes();

		filters.resize(numProbes);

		for(size_t probe = 0; probe < numProbes; ++probe){
			filters[probe] = new Dsp::SmoothedFilterDesign<Dsp::Butterworth::Design::BandPass    // design type
														  <2>,                                   // order
														  1,                                     // number of channels (must be const)
														  Dsp::DirectFormII>(1);               // realization
		}


		setBandPass(100, 3000);

    }

	void reset(){
	
	};

	void setBandPass(const int& lowCut, const int& highCut){

		settings.lowCut = lowCut;
		settings.highCut = highCut;

		Dsp::Params params;
		params[0] = RHS2116_SAMPLE_FREQUENCY_HZ;    // sample rate
		params[1] = 2;								// order
		params[2] = (highCut + lowCut) / 2;			// center frequency
		params[3] = highCut - lowCut;				// bandwidth

		for(size_t probe = 0; probe < numProbes; ++probe){
			filters[probe]->setParams(params);
		}
		

	}

	inline void process(oni_frame_t* frame){};
	inline void process(ONI::Frame::BaseFrame& frame){
		
		ONI::Frame::Rhs2116MultiFrame* f = reinterpret_cast<ONI::Frame::Rhs2116MultiFrame*>(&frame);

		for(size_t probe = 0; probe < numProbes; ++probe){
			float* ptr = &f->ac_uV[probe];
			filters[probe]->process(1, &ptr);
		}

		for(auto& it : postProcessors){
			it.second->process(frame);
		}

	}


private:

protected:

	ONI::Settings::FilterSettings settings;
	ONI::Processor::BaseProcessor* source;

	std::vector<Dsp::Filter*> filters;

};


} // namespace Processor
} // namespace ONI










