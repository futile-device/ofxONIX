//
//  ONIDevice.h
//
//  Created by Matt Gingold on 13.09.2024.
//

#include "oni.h"
#include "onix.h"

#include <cassert>
#include <string>
#include <vector>
#include <map>
#include <bitset>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <thread>
#include <mutex>
#include <syncstream>


#include "ONIDevice.h"
#include "ONIRegister.h"
#include "ONISettingTypes.h"
#include "ONIUtility.h"

#include "ONIDeviceRhs2116.h"

#pragma once

struct Rhs2116Stimulus {
    //bool valid = false;
    bool anodicFirst = true;

    double requestedAnodicAmplitudeMicroAmps = 0.0;
    double requestedCathodicAmplitudeMicroAmps = 0.0;
    
    double actualAnodicAmplitudeMicroAmps = 0.0;
    double actualCathodicAmplitudeMicroAmps = 0.0;

    unsigned int anodicAmplitudeSteps = 0;
    unsigned int cathodicAmplitudeSteps = 0;
    unsigned int anodicWidthSamples = 0;
    unsigned int cathodicWidthSamples = 0;
    unsigned int dwellSamples = 0;
    unsigned int delaySamples = 0;
    unsigned int interStimulusIntervalSamples = 0;
    unsigned int numberOfStimuli = 0;
};


class Rhs2116StimulusInterface;

class Rhs2116StimulusDevice : public ONIProbeDevice {

public:

	friend class Rhs2116StimulusInterface;

	Rhs2116StimulusDevice(){
        numProbes = 0;							// default for base ONIProbeDevice
        sampleFrequencyHz = 30.1932367151e3;	// default for base ONIProbeDevice
	};
	
	~Rhs2116StimulusDevice(){
		LOGDEBUG("RHS2116Stimulus Device DTOR");
		//const std::lock_guard<std::mutex> lock(mutex);
	};

    void setup(volatile oni_ctx* context, oni_device_t type, uint64_t acq_clock_khz) = delete;

    void setup(Rhs2116MultiDevice* multi){

        LOGDEBUG("Setting up device RHS2116 STIM");

        if(multi->devices.size() == 0){
            LOGERROR("Add edd devices to Rhs2116MultiDevice before setting up stimulus");
            assert(false);
            return;
        }

        this->multi = multi;
        numProbes = multi->numProbes;
        sampleFrequencyHz = multi->sampleFrequencyHz;
        acq_clock_khz = multi->acq_clock_khz;

        ctx = multi->ctx;

        deviceType.idx = 1000;
        deviceType.id = RHS2116STIM;
        deviceTypeID = RHS2116STIM;

        std::ostringstream os; os << ONIDeviceTypeIDStr(deviceTypeID) << " (" << deviceType.idx << ")";
        deviceName = os.str();
       
        deviceStimuli.clear();
        for(auto it : multi->devices){
            deviceStimuli[it.second->getDeviceTableID()] = std::vector<Rhs2116Stimulus>(it.second->getNumProbes());
        }

        defaultStimuli.resize(16); // brittle hard coded to 16

        reset(); // always call device specific setup/reset
        deviceSetup(); // always call device specific setup/reset

    }

    void deviceSetup(){

    }

    inline void process(oni_frame_t* frame){};
    inline void process(ONIFrame& frame){};

    void clearStimuli(){
        for(auto it : deviceStimuli){
            std::vector<Rhs2116Stimulus>& stimuli = it.second;
            stimuli.clear();
        }
    }

    Rhs2116StimulusStep conformMinimumStimulusStepSize(std::vector<Rhs2116Stimulus>& stimuli){

        Rhs2116StimulusStep maxStep = Rhs2116StimulusStep::Step10nA;

        //std::vector<Rhs2116StimulusStep> stepSizes(stimuli.size());

        for(size_t i = 0; i < stimuli.size(); ++i){
            if(stimuli[i].requestedAnodicAmplitudeMicroAmps > 0 && stimuli[i].requestedCathodicAmplitudeMicroAmps > 0){
                Rhs2116StimulusStep stepSize = conformMinimumStimulusStepSize(stimuli[i]);
                if(stepSize > maxStep) maxStep = stepSize;
            }
        }

        for(auto& stimulus : stimuli){

            double a1 = stimulus.requestedAnodicAmplitudeMicroAmps / Rhs2116StimulusStepMicroAmps[maxStep];
            stimulus.anodicAmplitudeSteps = std::clamp(std::round(a1), 1.0, 255.0);
            stimulus.actualAnodicAmplitudeMicroAmps = std::clamp(stimulus.anodicAmplitudeSteps * Rhs2116StimulusStepMicroAmps[maxStep], 0.0, 2550.0);

            double c1 = stimulus.requestedCathodicAmplitudeMicroAmps / Rhs2116StimulusStepMicroAmps[maxStep];
            stimulus.cathodicAmplitudeSteps = std::clamp(std::round(c1), 1.0, 255.0);
            stimulus.actualCathodicAmplitudeMicroAmps = std::clamp(stimulus.cathodicAmplitudeSteps * Rhs2116StimulusStepMicroAmps[maxStep], 0.0, 2550.0);

            LOGDEBUG("Requested Anodic uA (%0.2f) || Actual Anodic uA (%0.2f) || Min Step (M) (%f) || Steps %i", stimulus.requestedAnodicAmplitudeMicroAmps, stimulus.actualAnodicAmplitudeMicroAmps, Rhs2116StimulusStepMicroAmps[maxStep], stimulus.anodicAmplitudeSteps);
            LOGDEBUG("Requested Cathod uA (%0.2f) || Actual Cathod uA (%0.2f) || Min Step (M) (%f) || Steps %i", stimulus.requestedCathodicAmplitudeMicroAmps, stimulus.actualCathodicAmplitudeMicroAmps, Rhs2116StimulusStepMicroAmps[maxStep], stimulus.cathodicAmplitudeSteps);

        }

        return maxStep;

    }

    Rhs2116StimulusStep conformMinimumStimulusStepSize(Rhs2116Stimulus& stimulus){

        Rhs2116StimulusStep anodicMinStep = Rhs2116StimulusStep::StepError; // 10
        
        unsigned int anodicSteps = 0;
        for(size_t i = 0; i < 10; ++i){
            double stepMicroAmps = Rhs2116StimulusStepMicroAmps[i];
            anodicSteps = std::floor(stimulus.requestedAnodicAmplitudeMicroAmps / stepMicroAmps);
            if(anodicSteps <= 255){
                anodicMinStep = Rhs2116StimulusStep(i);
                break;
           }
        }

        if(anodicMinStep == Rhs2116StimulusStep::StepError){
            LOGERROR("Requested anodic current is too high");
            anodicMinStep = Rhs2116StimulusStep::Step10000nA;
            anodicSteps = 255;
        }

        Rhs2116StimulusStep cathodicMinStep = Rhs2116StimulusStep::StepError; // 11

        unsigned int cathodicSteps = 0;
        for(size_t i = 0; i < 10; ++i){
            double stepMicroAmps = Rhs2116StimulusStepMicroAmps[i];
            cathodicSteps = std::floor(stimulus.requestedCathodicAmplitudeMicroAmps / stepMicroAmps);
            if(cathodicSteps <= 255){
                cathodicMinStep = Rhs2116StimulusStep(i);
                break;
            }
        }

        Rhs2116StimulusStep minStep = std::max(anodicMinStep, cathodicMinStep);

        double a1 = stimulus.requestedAnodicAmplitudeMicroAmps / Rhs2116StimulusStepMicroAmps[minStep];
        stimulus.anodicAmplitudeSteps = std::clamp(std::round(a1), 1.0, 255.0);
        stimulus.actualAnodicAmplitudeMicroAmps = std::clamp(stimulus.anodicAmplitudeSteps * Rhs2116StimulusStepMicroAmps[minStep], 0.0, 2550.0);

        double c1 = stimulus.requestedCathodicAmplitudeMicroAmps / Rhs2116StimulusStepMicroAmps[minStep];
        stimulus.cathodicAmplitudeSteps = std::clamp(std::round(c1), 1.0, 255.0);
        stimulus.actualCathodicAmplitudeMicroAmps = std::clamp(stimulus.cathodicAmplitudeSteps * Rhs2116StimulusStepMicroAmps[minStep], 0.0, 2550.0);

        LOGDEBUG("Requested Anodic uA (%0.2f) || Actual Anodic uA (%0.2f) || Min Step (A, M) (%f %f) || Steps %i", stimulus.requestedAnodicAmplitudeMicroAmps, stimulus.actualAnodicAmplitudeMicroAmps, Rhs2116StimulusStepMicroAmps[anodicMinStep], Rhs2116StimulusStepMicroAmps[minStep], stimulus.anodicAmplitudeSteps);
        LOGDEBUG("Requested Cathod uA (%0.2f) || Actual Cathod uA (%0.2f) || Min Step (A, M) (%f %f) || Steps %i", stimulus.requestedCathodicAmplitudeMicroAmps, stimulus.actualCathodicAmplitudeMicroAmps, Rhs2116StimulusStepMicroAmps[cathodicMinStep], Rhs2116StimulusStepMicroAmps[minStep], stimulus.cathodicAmplitudeSteps);
        
        return minStep;

    }

    bool setProbeStimulus(const Rhs2116Stimulus& stimulus, const unsigned int& channelMappedProbeIDX){

        if(channelMappedProbeIDX > multi->channelIDX.size()){
            LOGERROR("Channel mapped probe index out of range for stimulus: %i", channelMappedProbeIDX);
            return false;
        }

        if(!isStimulusValid(stimulus)){
            LOGERROR("Stimulus is invalid");
            return false;
        }

        size_t idx = multi->channelIDX[channelMappedProbeIDX];
        size_t numProbesPerDevice = 16; // (numProbes / multi->devices.size())
        size_t deviceNum = floor(idx / numProbesPerDevice);
        size_t probeIDX = idx - deviceNum * numProbesPerDevice;

        size_t i = 0;
        for(auto it : deviceStimuli){
            if(deviceNum == i){
                std::vector<Rhs2116Stimulus>& stimuli = it.second;
                stimuli[probeIDX] = stimulus;
                return true;
            }
        }
        return false;
    }

    bool setProbeStimulus(const Rhs2116Stimulus& stimulus, const unsigned int& probeIDX, const unsigned int& deviceTableIDX){

        auto it = deviceStimuli.find(deviceTableIDX);

        if(it == deviceStimuli.end()){
            LOGERROR("Device IDX not available for stimulus: %i", deviceTableIDX);
            return false;
        }

        if(!isStimulusValid(stimulus)){
            LOGERROR("Stimulus is invalid");
            return false;
        }

        std::vector<Rhs2116Stimulus>& stimuli = it->second;
        stimuli[probeIDX] = stimulus;

        return true;

    }

    Rhs2116Stimulus& getProbeStimulus(const unsigned int& channelMappedProbeIDX){

        if(channelMappedProbeIDX > multi->channelIDX.size()){
            LOGERROR("Channel mapped probe index out of range for stimulus: %i", channelMappedProbeIDX);
            return defaultStimulus;
        }

        size_t idx = multi->channelIDX[channelMappedProbeIDX];
        size_t numProbesPerDevice = 16; // (numProbes / multi->devices.size())
        size_t deviceNum = floor(idx / numProbesPerDevice);
        size_t probeIDX = idx - deviceNum * numProbesPerDevice;

        size_t i = 0;
        for(auto it : deviceStimuli){
            if(deviceNum == i){
                std::vector<Rhs2116Stimulus>& stimuli = it.second;
                return stimuli[probeIDX];
            }
        }

    }

    Rhs2116Stimulus& getProbeStimulus(const unsigned int& probeIDX, const unsigned int& deviceTableIDX){

        auto it = deviceStimuli.find(deviceTableIDX);

        if(it == deviceStimuli.end()){
            LOGERROR("Device IDX not available for stimulus: %i", deviceTableIDX);
            return defaultStimulus;
        }

        std::vector<Rhs2116Stimulus>& stimuli = it->second;
        return stimuli[probeIDX];

    }

    bool setProbeStimuli(const std::vector<Rhs2116Stimulus>& stimuli, const unsigned int& deviceTableIDX){

        auto it = deviceStimuli.find(deviceTableIDX);

        if(it == deviceStimuli.end()){
            LOGERROR("Device IDX not available for stimulus: %i", deviceTableIDX);
            return false;
        }

        it->second = stimuli;

        return true;

    }

    std::vector<Rhs2116Stimulus>& getProbeStimuli(const unsigned int& deviceTableIDX){

        auto it = deviceStimuli.find(deviceTableIDX);

        if(it == deviceStimuli.end()){
            LOGERROR("Device IDX not available for stimulus: %i", deviceTableIDX);
            return defaultStimuli;
        }

        return it->second;

    }

#pragma pack(push, 1)
    struct Rhs2116StimIndex{
        unsigned time:22;
        unsigned index:10;
    };
#pragma pack(pop)

    void test(){

        Rhs2116Stimulus s1;
        s1.requestedAnodicAmplitudeMicroAmps = 1.90;
        s1.requestedCathodicAmplitudeMicroAmps = 1.20;
        s1.anodicAmplitudeSteps = 2;
        s1.anodicFirst = true;
        s1.anodicWidthSamples = 30;
        s1.cathodicAmplitudeSteps = 2;
        s1.cathodicWidthSamples = 30;
        s1.delaySamples = 0;
        s1.dwellSamples = 30;
        s1.interStimulusIntervalSamples = 30;
        s1.numberOfStimuli = 1;
        //s1.valid = isStimulusValid(s1);

        Rhs2116Stimulus s2;
        s2.requestedAnodicAmplitudeMicroAmps = 18.1;
        s2.requestedCathodicAmplitudeMicroAmps = 99.3;
        s2.anodicAmplitudeSteps = 24;
        s2.anodicFirst = true;
        s2.anodicWidthSamples = 60;
        s2.cathodicAmplitudeSteps = 24;
        s2.cathodicWidthSamples = 60;
        s2.delaySamples = 694;
        s2.dwellSamples = 121;
        s2.interStimulusIntervalSamples = 362;
        s2.numberOfStimuli = 40;
        //s2.valid = isStimulusValid(s2);

        //conformMinimumStimulusStepSize(s1);
        //conformMinimumStimulusStepSize(s2);

        std::vector<Rhs2116Stimulus> stimuli;
        stimuli.push_back(s1);
        stimuli.push_back(s2);

        Rhs2116StimulusStep stepSize = conformMinimumStimulusStepSize(stimuli);

        auto result = getDeltaTable(stimuli);

        size_t j = 0;
        for (const auto& entry : result) {
            //std::cout << "Key: " << entry.first << " Value: " << entry.second << std::endl;
            Rhs2116StimIndex p;
            p.index = j;
            p.time = entry.first;
            LOGDEBUG("3277X %i", *(unsigned int*)&p);
            unsigned int idxTime = j++ << 22 | (entry.first & 0x003FFFFF);
            LOGDEBUG("32771 %i", idxTime);
            LOGDEBUG("32772 %i", entry.second);
            
        }

    }

    bool isStimulusValid(const Rhs2116Stimulus& s){
        return s.numberOfStimuli == 0 ? s.cathodicWidthSamples == 0 && s.anodicWidthSamples == 0 && 
               s.interStimulusIntervalSamples == 0 && s.anodicAmplitudeSteps == 0 && s.cathodicAmplitudeSteps == 0 && 
               s.delaySamples == 0 && s.dwellSamples == 0 : !(s.anodicWidthSamples == 0 && s.anodicAmplitudeSteps > 0) &&
               !(s.cathodicWidthSamples == 0 && s.cathodicAmplitudeSteps > 0) &&
               ((s.numberOfStimuli == 1 && s.interStimulusIntervalSamples >= 0) || (s.numberOfStimuli > 1 && s.interStimulusIntervalSamples > 0));
    }

    void addOrInsert(std::map<uint32_t, std::bitset<32>>& table, int channel, uint32_t key, bool polarity, bool enable) {
        // Check if key exists in the map
        if (table.find(key) != table.end()) {
            table[key][channel] = enable;
            table[key][channel + 16] = polarity;
        } else {
            std::bitset<32> bitset;  // Initialize a bitset of 32 bits, all set to false
            table[key] = bitset;
            table[key][channel] = enable;
            table[key][channel + 16] = polarity;
        }
    }

    std::map<uint32_t, uint32_t> getDeltaTable(std::vector<Rhs2116Stimulus>& stimuli) {

        std::map<uint32_t, std::bitset<32>> table;

        for (int i = 0; i < stimuli.size(); ++i) {

            Rhs2116Stimulus& s = stimuli[i];

            bool e0 = s.anodicFirst ? s.anodicAmplitudeSteps > 0 : s.cathodicAmplitudeSteps > 0;
            bool e1 = s.anodicFirst ? s.cathodicAmplitudeSteps > 0 : s.anodicAmplitudeSteps > 0;
            int d0 = s.anodicFirst ? s.anodicWidthSamples : s.cathodicWidthSamples;
            int d1 = d0 + s.dwellSamples;
            int d2 = d1 + (s.anodicFirst ? s.cathodicWidthSamples : s.anodicWidthSamples);

            int t0 = s.delaySamples;

            for (int j = 0; j < s.numberOfStimuli; ++j) {

                addOrInsert(table, i, t0, s.anodicFirst, e0);
                addOrInsert(table, i, t0 + d0, s.anodicFirst, false);
                addOrInsert(table, i, t0 + d1, !s.anodicFirst, e1);
                addOrInsert(table, i, t0 + d2, !s.anodicFirst, false);

                t0 += d2 + s.interStimulusIntervalSamples;

            }
        }

        // Convert the bitsets in the table to uint32_t values
        std::map<uint32_t, uint32_t> result;

        for (const auto& d : table) {
            result[d.first] = static_cast<uint32_t>(d.second.to_ulong());  // Convert bitset to uint32_t
        }
        
        return result;
    }

 protected:

     Rhs2116Stimulus defaultStimulus;
     std::vector<Rhs2116Stimulus> defaultStimuli;

     std::map<unsigned int, std::vector<Rhs2116Stimulus>> deviceStimuli;

     Rhs2116MultiDevice* multi;

};
