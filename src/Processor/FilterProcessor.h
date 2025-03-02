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

		bandstopFilters.resize(numProbes);

		for(size_t probe = 0; probe < numProbes; ++probe){
			bandstopFilters[probe] = new Dsp::SmoothedFilterDesign<Dsp::Butterworth::Design::BandStop    // design type
																  <4>,                                   // order
																  1,                                     // number of channels (must be const)
																  Dsp::DirectFormII>(1);                 // realization
		}

		setBandStop(1300, 1000);

		lowpassFilters.resize(numProbes);

		for(size_t probe = 0; probe < numProbes; ++probe){
			lowpassFilters[probe] = new Dsp::SmoothedFilterDesign<Dsp::Butterworth::Design::LowPass     // design type
																 <4>,                                   // order
																 1,                                     // number of channels (must be const)
																 Dsp::DirectFormII>(1);                 // realization
		}

		setLowPass(25, 1.25);


		highpassFilters.resize(numProbes);

		for(size_t probe = 0; probe < numProbes; ++probe){
			highpassFilters[probe] = new Dsp::SmoothedFilterDesign<Dsp::Butterworth::Design::HighShelf    // design type
																  <4>,                                   // order
																  1,                                     // number of channels (must be const)
																  Dsp::DirectFormII>(1);                 // realization
		}

		setHighShelf(1000, -12, 0.1);


		bandpassFilters.resize(numProbes);

		for(size_t probe = 0; probe < numProbes; ++probe){
			bandpassFilters[probe] = new Dsp::SmoothedFilterDesign<Dsp::Butterworth::Design::BandPass    // design type
																  <4>,                                   // order
																  1,                                     // number of channels (must be const)
																  Dsp::DirectFormII>(1);                 // realization
		}

		setBandPass(100, 3000);

    }

	void reset(){};


	inline void process(oni_frame_t* frame){};
	inline void process(ONI::Frame::BaseFrame& frame){
		
		ONI::Frame::Rhs2116MultiFrame* f = reinterpret_cast<ONI::Frame::Rhs2116MultiFrame*>(&frame);


		if(settings.bUseBandStopFilter){
			for(size_t probe = 0; probe < numProbes; ++probe){
				float* ptr = &f->ac_uV[probe];
				bandstopFilters[probe]->process(1, &ptr);
			}
		}

		if(settings.bUseLowPass){
			for(size_t probe = 0; probe < numProbes; ++probe){
				float* ptr = &f->ac_uV[probe];
				lowpassFilters[probe]->process(1, &ptr);
			}
		}

		if(settings.bUseHighShelf){
			for(size_t probe = 0; probe < numProbes; ++probe){
				float* ptr = &f->ac_uV[probe];
				highpassFilters[probe]->process(1, &ptr);
			}
		}

		if(settings.bUseBandPassFilter){
			for(size_t probe = 0; probe < numProbes; ++probe){
				float* ptr = &f->ac_uV[probe];
				bandpassFilters[probe]->process(1, &ptr);
			}
		}

		for(auto& it : postProcessors){
			it.second->process(frame);
		}

	}

	void setBandStop(const int& frequency, const int& width){

		settings.bandStopFrequency = frequency;
		settings.bandStopWidth = width;

		Dsp::Params params;
		params[0] = RHS2116_SAMPLE_FREQUENCY_HZ;	// sample rate
		params[1] = 4;								// order
		params[2] = frequency;						// center frequency
		params[3] = width;							// band width

		for(size_t probe = 0; probe < numProbes; ++probe){
			bandstopFilters[probe]->setParams(params);
		}

	}

	void setLowPass(const int& frequency, const float& Q){

		settings.lowPassFrequency = frequency;
		settings.lowPassQ = Q;

		Dsp::Params params;
		params[0] = RHS2116_SAMPLE_FREQUENCY_HZ;	// sample rate
		params[1] = 4;								// order
		params[2] = frequency;						// center frequency
		params[3] = Q;							// band width

		for(size_t probe = 0; probe < numProbes; ++probe){
			lowpassFilters[probe]->setParams(params);
		}

	}

	void setHighShelf(const int& frequency, const float& gain, const float& ripple){

		settings.highShelfFrequency = frequency;
		settings.highShelfGain = gain;
		settings.highShelfRipple = ripple;

		Dsp::Params params;
		params[0] = RHS2116_SAMPLE_FREQUENCY_HZ;	// sample rate
		params[1] = 4;								// order
		params[2] = frequency;						// corner frequency
		params[3] = gain;							// shelf gain
		params[4] = ripple;							// passband ripple

		for(size_t probe = 0; probe < numProbes; ++probe){
			highpassFilters[probe]->setParams(params);
		}

	}

	void setBandPass(const int& lowCutFrequency, const int& highCutFrequency){

		settings.lowBandPassFrequency = lowCutFrequency;
		settings.highBandPassFrequency = highCutFrequency;

		Dsp::Params params;
		params[0] = RHS2116_SAMPLE_FREQUENCY_HZ;    // sample rate
		params[1] = 4;								// order
		params[2] = (highCutFrequency + lowCutFrequency) / 2;			// center frequency
		params[3] = highCutFrequency - lowCutFrequency;				// bandwidth

		for(size_t probe = 0; probe < numProbes; ++probe){
			bandpassFilters[probe]->setParams(params);
		}

	}


private:

protected:

	ONI::Settings::FilterSettings settings;
	ONI::Processor::BaseProcessor* source;

	std::vector<Dsp::Filter*> bandstopFilters;
	std::vector<Dsp::Filter*> lowpassFilters;
	std::vector<Dsp::Filter*> highpassFilters;
	std::vector<Dsp::Filter*> bandpassFilters;

};


} // namespace Processor
} // namespace ONI










