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


#include "../Device/BaseDevice.h"
#include "../Device/Rhs2116MultiDevice.h"

#pragma once

namespace ONI{

namespace Interface{
class Rhs2116StimulusInterface;
} // namespace Interface

namespace Device{




class Rhs2116StimulusDevice : public ONI::Device::ProbeDevice {

public:

	friend class ONI::Interface::Rhs2116StimulusInterface;

	Rhs2116StimulusDevice(){
        ONI::Device::ProbeDevice::numProbes = 0;							// default for base ONIProbeDevice
        ONI::Device::ProbeDevice::sampleFrequencyHz = 30.1932367151e3;	// default for base ONIProbeDevice
	};
	
	~Rhs2116StimulusDevice(){
		LOGDEBUG("RHS2116Stimulus Device DTOR");
		//const std::lock_guard<std::mutex> lock(mutex);
	};

    void setup(volatile oni_ctx* context, oni_device_t type, uint64_t acq_clock_khz) = delete;

    void setup(ONI::Device::Rhs2116MultiDevice* multi){

        LOGDEBUG("Setting up device RHS2116 STIM");

        if(multi->devices.size() == 0){
            LOGERROR("Add devices to Rhs2116MultiDevice before setting up stimulus");
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

        std::ostringstream os; os << ONI::Device::toString(deviceTypeID) << " (" << deviceType.idx << ")";
        BaseProcessor::processorName = os.str();
       
        settings.deviceStimuli.clear();

        for(auto& it : multi->devices){
            settings.deviceStimuli[it.second->getDeviceTableID()] = std::vector<ONI::Rhs2116StimulusData>(it.second->getNumProbes());
        }

        defaultStimuli.resize(numProbes / multi->devices.size()); // brittle? hard coded to 16?

        reset(); // always call device specific setup/reset
        deviceSetup(); // always call device specific setup/reset

    }

    void deviceSetup(){

    }

    inline void process(oni_frame_t* frame){};
    inline void process(ONI::Frame::BaseFrame& frame){};

    void clearStimuli(){
        for(auto& it : settings.deviceStimuli){
            std::vector<ONI::Rhs2116StimulusData>& stimuli = it.second;
            stimuli.clear();
            if(multi != nullptr) stimuli.resize(numProbes / multi->devices.size()); // brittle? hard coded to 16?
        }
    }

    unsigned int getTotalLengthSamples(const ONI::Rhs2116StimulusData& stimulus){
        unsigned int totalSamples = stimulus.anodicWidthSamples + stimulus.dwellSamples + stimulus.cathodicWidthSamples + stimulus.interStimulusIntervalSamples;
        totalSamples *= stimulus.numberOfStimuli;
        totalSamples += stimulus.delaySamples;
        return totalSamples;
    }

    unsigned int getMaxLengthSamples(std::vector<ONI::Rhs2116StimulusData>& stimuli){
        unsigned int totalSamples = 0;
        for(auto& s : stimuli){
            unsigned int t = getTotalLengthSamples(s);
            if(t >= totalSamples) totalSamples = t;
        }
        return totalSamples;
    }

    std::vector<std::vector<float>> getAllStimulusAmplitudes(const std::vector<ONI::Rhs2116StimulusData>& stimuli){
        std::vector<std::vector<float>> allAmplitudes;
        allAmplitudes.resize(stimuli.size());
        for(size_t i = 0; i < stimuli.size(); ++i){
            allAmplitudes[i] = getStimulusAmplitudes(stimuli[i]);
        }
        return allAmplitudes;
    }

    std::vector<float> getStimulusAmplitudes(const ONI::Rhs2116StimulusData& stimulus){

        unsigned int totalLength = getTotalLengthSamples(stimulus);

        std::vector<float> amplitudes;
        amplitudes.resize(totalLength);

        size_t s0 = stimulus.delaySamples;
        size_t s1 = stimulus.anodicFirst ? stimulus.anodicWidthSamples : stimulus.cathodicWidthSamples;
        size_t s2 = stimulus.dwellSamples;
        size_t s3 = stimulus.anodicFirst ? stimulus.cathodicWidthSamples : stimulus.anodicWidthSamples;
        size_t s4 = stimulus.interStimulusIntervalSamples;

        for(size_t i = 0; i < s0; ++i){
            amplitudes[i] = 0;
        }

        size_t singleStimulusLength = s1 + s2 + s3 + s4;

        std::vector<float> singleStimulus;
        singleStimulus.resize(singleStimulusLength);

        for(size_t i = 0; i < singleStimulus.size(); ++i){
            if(i >= 0 && i < s1) singleStimulus[i] = stimulus.anodicFirst ? -stimulus.actualAnodicAmplitudeMicroAmps : stimulus.actualCathodicAmplitudeMicroAmps;
            if(i >= s1 && i < s1 + s2) singleStimulus[i] = 0; // ??
            if(i >= s1 + s2 && i < s1 + s2 + s3) singleStimulus[i] = stimulus.anodicFirst ? stimulus.actualCathodicAmplitudeMicroAmps : -stimulus.actualAnodicAmplitudeMicroAmps;
            if(i >= s1 + s2 + s3 && i < s1 + s2 + s3 + s4) singleStimulus[i] = 0; // ??
        }

        for(size_t i = 0; i < stimulus.numberOfStimuli; ++i){
            for(size_t j = 0; j < singleStimulus.size(); ++j){
                size_t idx = s0 + i * singleStimulus.size() + j;
                amplitudes[idx] = singleStimulus[j];
            }
        }
    
        return amplitudes;

    }

    ONI::Settings::Rhs2116StimulusStep conformStepSize(std::vector<ONI::Rhs2116StimulusData>& stimuli){

        ONI::Settings::Rhs2116StimulusStep maxStep = ONI::Settings::Rhs2116StimulusStep::Step10nA;

        //std::vector<ONI::Settings::Rhs2116StimulusStep> stepSizes(stimuli.size());

        for(size_t i = 0; i < stimuli.size(); ++i){
            if(stimuli[i].requestedAnodicAmplitudeMicroAmps > 0 && stimuli[i].requestedCathodicAmplitudeMicroAmps > 0){
                ONI::Settings::Rhs2116StimulusStep stepSize = conformStepSize(stimuli[i]);
                if(stepSize > maxStep) maxStep = stepSize;
            }
        }

        for(auto& stimulus : stimuli){

            stimulus.actualAnodicAmplitudeMicroAmps = 0;
            stimulus.actualCathodicAmplitudeMicroAmps = 0;

            if(stimulus.requestedCathodicAmplitudeMicroAmps != 0){
                double a1 = stimulus.requestedAnodicAmplitudeMicroAmps / ONI::Settings::Rhs2116StimulusStepMicroAmps[maxStep];
                stimulus.anodicAmplitudeSteps = std::clamp(std::round(a1), 1.0, 255.0);
                stimulus.actualAnodicAmplitudeMicroAmps = std::clamp(stimulus.anodicAmplitudeSteps * ONI::Settings::Rhs2116StimulusStepMicroAmps[maxStep], 0.0, 2550.0);
            }

            if(stimulus.requestedCathodicAmplitudeMicroAmps != 0){
                double c1 = stimulus.requestedCathodicAmplitudeMicroAmps / ONI::Settings::Rhs2116StimulusStepMicroAmps[maxStep];
                stimulus.cathodicAmplitudeSteps = std::clamp(std::round(c1), 1.0, 255.0);
                stimulus.actualCathodicAmplitudeMicroAmps = std::clamp(stimulus.cathodicAmplitudeSteps * ONI::Settings::Rhs2116StimulusStepMicroAmps[maxStep], 0.0, 2550.0);
            }

            LOGDEBUG("Requested Anodic uA (%0.2f) || Actual Anodic uA (%0.2f) || Min Step (M) (%f) || Steps %i", stimulus.requestedAnodicAmplitudeMicroAmps, stimulus.actualAnodicAmplitudeMicroAmps, ONI::Settings::Rhs2116StimulusStepMicroAmps[maxStep], stimulus.anodicAmplitudeSteps);
            LOGDEBUG("Requested Cathod uA (%0.2f) || Actual Cathod uA (%0.2f) || Min Step (M) (%f) || Steps %i", stimulus.requestedCathodicAmplitudeMicroAmps, stimulus.actualCathodicAmplitudeMicroAmps, ONI::Settings::Rhs2116StimulusStepMicroAmps[maxStep], stimulus.cathodicAmplitudeSteps);

        }

        return maxStep;

    }

    ONI::Settings::Rhs2116StimulusStep conformStepSize(ONI::Rhs2116StimulusData& stimulus){

        ONI::Settings::Rhs2116StimulusStep anodicMinStep = ONI::Settings::Rhs2116StimulusStep::StepError; // 10
        
        unsigned int anodicSteps = 0;
        for(size_t i = 0; i < 10; ++i){
            double stepMicroAmps = ONI::Settings::Rhs2116StimulusStepMicroAmps[i];
            anodicSteps = std::floor(stimulus.requestedAnodicAmplitudeMicroAmps / stepMicroAmps);
            if(anodicSteps <= 255){
                anodicMinStep = ONI::Settings::Rhs2116StimulusStep(i);
                break;
           }
        }

        if(anodicMinStep == ONI::Settings::Rhs2116StimulusStep::StepError){
            LOGERROR("Requested anodic current is too high");
            anodicMinStep = ONI::Settings::Rhs2116StimulusStep::Step10000nA;
            anodicSteps = 255;
        }

        ONI::Settings::Rhs2116StimulusStep cathodicMinStep = ONI::Settings::Rhs2116StimulusStep::StepError; // 11

        unsigned int cathodicSteps = 0;
        for(size_t i = 0; i < 10; ++i){
            double stepMicroAmps = ONI::Settings::Rhs2116StimulusStepMicroAmps[i];
            cathodicSteps = std::floor(stimulus.requestedCathodicAmplitudeMicroAmps / stepMicroAmps);
            if(cathodicSteps <= 255){
                cathodicMinStep = ONI::Settings::Rhs2116StimulusStep(i);
                break;
            }
        }

        ONI::Settings::Rhs2116StimulusStep minStep = std::max(anodicMinStep, cathodicMinStep);

        if(stimulus.requestedCathodicAmplitudeMicroAmps != 0){
            double a1 = stimulus.requestedAnodicAmplitudeMicroAmps / ONI::Settings::Rhs2116StimulusStepMicroAmps[minStep];
            stimulus.anodicAmplitudeSteps = std::clamp(std::round(a1), 1.0, 255.0);
            stimulus.actualAnodicAmplitudeMicroAmps = std::clamp(stimulus.anodicAmplitudeSteps * ONI::Settings::Rhs2116StimulusStepMicroAmps[minStep], 0.0, 2550.0);
        }

        if(stimulus.requestedCathodicAmplitudeMicroAmps != 0){
            double c1 = stimulus.requestedCathodicAmplitudeMicroAmps / ONI::Settings::Rhs2116StimulusStepMicroAmps[minStep];
            stimulus.cathodicAmplitudeSteps = std::clamp(std::round(c1), 1.0, 255.0);
            stimulus.actualCathodicAmplitudeMicroAmps = std::clamp(stimulus.cathodicAmplitudeSteps * ONI::Settings::Rhs2116StimulusStepMicroAmps[minStep], 0.0, 2550.0);
        }

        //LOGDEBUG("Requested Anodic uA (%0.2f) || Actual Anodic uA (%0.2f) || Min Step (A, M) (%f %f) || Steps %i", stimulus.requestedAnodicAmplitudeMicroAmps, stimulus.actualAnodicAmplitudeMicroAmps, Rhs2116StimulusStepMicroAmps[anodicMinStep], Rhs2116StimulusStepMicroAmps[minStep], stimulus.anodicAmplitudeSteps);
        //LOGDEBUG("Requested Cathod uA (%0.2f) || Actual Cathod uA (%0.2f) || Min Step (A, M) (%f %f) || Steps %i", stimulus.requestedCathodicAmplitudeMicroAmps, stimulus.actualCathodicAmplitudeMicroAmps, Rhs2116StimulusStepMicroAmps[cathodicMinStep], Rhs2116StimulusStepMicroAmps[minStep], stimulus.cathodicAmplitudeSteps);
        
        return minStep;

    }

    bool deleteProbeStimulus(const unsigned int& channelMappedProbeIDX){

        if(channelMappedProbeIDX > multi->channelIDX.size()){
            LOGERROR("Channel mapped probe index out of range for stimulus: %i", channelMappedProbeIDX);
            return false;
        }

        size_t idx = multi->channelIDX[channelMappedProbeIDX];
        size_t numProbesPerDevice = 16; // (numProbes / multi->devices.size())
        size_t deviceNum = floor(idx / numProbesPerDevice);
        size_t probeIDX = idx - deviceNum * numProbesPerDevice;

        size_t i = 0;
        for(auto& it : settings.deviceStimuli){
            if(deviceNum == i){
                std::vector<ONI::Rhs2116StimulusData>& stimuli = it.second;
                stimuli[probeIDX] = defaultStimulus;
                return true;
            }
            ++i;
        }


        return false;

    }

    bool setProbeStimulus(const ONI::Rhs2116StimulusData& stimulus, const unsigned int& channelMappedProbeIDX){

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
        for(auto& it : settings.deviceStimuli){
            if(deviceNum == i){
                std::vector<ONI::Rhs2116StimulusData>& stimuli = it.second;
                stimuli[probeIDX] = stimulus;
                return true;
            }
            ++i;
        }


        return false;
    }

    bool setProbeStimulus(const ONI::Rhs2116StimulusData& stimulus, const unsigned int& probeIDX, const unsigned int& deviceTableIDX){

        auto& it = settings.deviceStimuli.find(deviceTableIDX);

        if(it == settings.deviceStimuli.end()){
            LOGERROR("Device IDX not available for stimulus: %i", deviceTableIDX);
            return false;
        }

        if(!isStimulusValid(stimulus)){
            LOGERROR("Stimulus is invalid");
            return false;
        }

        std::vector<ONI::Rhs2116StimulusData>& stimuli = it->second;
        stimuli[probeIDX] = stimulus;

        return true;

    }

    ONI::Rhs2116StimulusData& getProbeStimulus(const unsigned int& channelMappedProbeIDX){

        if(channelMappedProbeIDX > multi->channelIDX.size()){
            LOGERROR("Channel mapped probe index out of range for stimulus: %i", channelMappedProbeIDX);
            return defaultStimulus;
        }

        size_t idx = multi->channelIDX[channelMappedProbeIDX];
        size_t numProbesPerDevice = 16; // (numProbes / multi->devices.size())
        size_t deviceNum = floor(idx / numProbesPerDevice);
        size_t probeIDX = idx - deviceNum * numProbesPerDevice;

        size_t i = 0;
        for(auto& it : settings.deviceStimuli){
            if(deviceNum == i){
                std::vector<ONI::Rhs2116StimulusData>& stimuli = it.second;
                return stimuli[probeIDX];
            }
        }

    }

    ONI::Rhs2116StimulusData& getProbeStimulus(const unsigned int& probeIDX, const unsigned int& deviceTableIDX){

        auto& it = settings.deviceStimuli.find(deviceTableIDX);

        if(it == settings.deviceStimuli.end()){
            LOGERROR("Device IDX not available for stimulus: %i", deviceTableIDX);
            return defaultStimulus;
        }

        std::vector<ONI::Rhs2116StimulusData>& stimuli = it->second;
        return stimuli[probeIDX];

    }

    bool setProbeStimuli(const std::vector<ONI::Rhs2116StimulusData>& stimuli, const unsigned int& deviceTableIDX){

        auto& it = settings.deviceStimuli.find(deviceTableIDX);

        if(it == settings.deviceStimuli.end()){
            LOGERROR("Device IDX not available for stimulus: %i", deviceTableIDX);
            return false;
        }

        it->second = stimuli;

        return true;

    }

    std::vector<ONI::Rhs2116StimulusData>& getProbeStimuli(const unsigned int& deviceTableIDX){

        auto& it = settings.deviceStimuli.find(deviceTableIDX);

        if(it == settings.deviceStimuli.end()){
            LOGERROR("Device IDX not available for stimulus: %i", deviceTableIDX);
            return defaultStimuli;
        }

        return it->second;

    }

    bool setProbStimuliMapped(const std::vector<ONI::Rhs2116StimulusData>& stimuli){

        if(stimuli.size() != numProbes){
            LOGERROR("Stimuli and number of probes don't match!");
            return false;
        }

        size_t numProbesPerDevice = 16; // (numProbes / multi->devices.size())

        size_t deviceCount = 0;
        for(auto& it : settings.deviceStimuli){

            std::vector<ONI::Rhs2116StimulusData>& ds = it.second;

            size_t startIDX = deviceCount * numProbesPerDevice;
            size_t endIDX = startIDX + numProbesPerDevice;

            size_t j = 0;
            for(size_t i = startIDX; i < endIDX; ++i){
                ds[j] = stimuli[i];
                ++j;
            }

            ++deviceCount;

        }

        return true;

    }

    std::vector<ONI::Rhs2116StimulusData> getProbeStimuliMapped(){

        std::vector<ONI::Rhs2116StimulusData> stimuli;

        for(auto& it : settings.deviceStimuli){
            std::vector<ONI::Rhs2116StimulusData>& ds = it.second;
            for(const auto& s : ds){
                stimuli.push_back(s);
            }
        }

        if(stimuli.size() != numProbes){
            LOGERROR("Stimuli and number of probes don't match!");
            return defaultStimuli;
        }

        return stimuli;

    }

    inline void triggerStimulusSequence(){
        deviceType.idx = 258; // TODO: this needs a lot more work to make it general for more rhs2116 devices [but this is the trig device for 256/257]
        ONI::Device::BaseDevice::writeRegister(ONI::Register::Rhs2116Stimulus::TRIGGER, 1);
        deviceType.idx = 667;
    }

    bool setStimulusSequence(const std::vector<ONI::Rhs2116StimulusData>& stimuli, const ONI::Settings::Rhs2116StimulusStep& stepSize){

        setProbStimuliMapped(stimuli);

        deviceType.idx = 258; // TODO: this needs a lot more work to make it general for more rhs2116 devices [but this is the trig device for 256/257]
        //BaseDevice::writeRegister(RHS2116STIM_REG::ENABLE, 1); // do we need this?
        if(!ONI::Device::BaseDevice::writeRegister(ONI::Register::Rhs2116Stimulus::TRIGGERSOURCE, 0)){
            LOGERROR("Error writing to TRIGGERSOURCE!!");
            return false;
        }

        deviceType.idx = 667;

        for(const auto& it : settings.deviceStimuli){

            Rhs2116Device* device = multi->getDevice(it.first);

            if(!device->setStepSize(stepSize)){
                LOGERROR("Error setting STEPSZ!!");
                return false;
            }

            const unsigned int * regs = ONI::Settings::Rhs2116StimulusBiasRegisters[stepSize];
            unsigned int rbias = regs[1] << 4 | regs[0];

            if(!device->writeRegister(ONI::Register::Rhs2116::STIMBIAS, rbias)){ // ask Jon about whether this is necessary
                LOGERROR("Error writing to STIMBIAS!!");
                return false;
            }

            const std::vector<ONI::Rhs2116StimulusData>& thisStimuli = it.second;

            for(size_t i = 0; i < thisStimuli.size(); ++i){
                if(!device->writeRegister(ONI::Register::Rhs2116::NEG[i], thisStimuli[i].anodicAmplitudeSteps)){
                    LOGERROR("Error writing to NEG!!");
                    return false;
                }
            }

            for(size_t i = 0; i < thisStimuli.size(); ++i){
                if(!device->writeRegister(ONI::Register::Rhs2116::POS[i], thisStimuli[i].cathodicAmplitudeSteps)){
                    LOGERROR("Error writing to POS!!");
                    return false;
                }
            }

            auto result = getDeltaTable(thisStimuli);

            if(!device->writeRegister(ONI::Register::Rhs2116::NUMDELTAS, result.size())){
                LOGERROR("Too many stimuli deltas!!");
                return false;
            }

            size_t j = 0;
            for (const auto& entry : result) {
                //std::cout << "Key: " << entry.first << " Value: " << entry.second << std::endl;
                //Rhs2116StimIndexTime p;
                //p.index = j;
                //p.time = entry.first;
                //LOGDEBUG("3277X %i", *(unsigned int*)&p);
                unsigned int idxTime = j++ << 22 | (entry.first & 0x003FFFFF);
                if(!device->writeRegister(ONI::Register::Rhs2116::DELTAIDXTIME, idxTime)){
                    LOGERROR("Error writing to DELTAIDXTIME!!");
                    return false;
                }
                
                if(!device->writeRegister(ONI::Register::Rhs2116::DELTAPOLEN, entry.second)){
                    LOGERROR("Error writing to DELTAPOLEN!!");
                    return false;
                }
                //LOGDEBUG("32771 %i", idxTime);
                //LOGDEBUG("32772 %i", entry.second);
                //++j;
            }

            //device->writeRegister(RHS2116_REG::RESPECTSTIMACTIVE, 1); // hmmm? not done in bonsai code

            if(device->readRegister(ONI::Register::Rhs2116::SEQERROR)){
                LOGERROR("Stimulus sequence error!!");
                return false;
            }

        }

        return true;

    }

    bool isStimulusValid(const ONI::Rhs2116StimulusData& s){
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

    std::map<uint32_t, uint32_t> getDeltaTable(const std::vector<ONI::Rhs2116StimulusData>& stimuli) {

        std::map<uint32_t, std::bitset<32>> table;

        for (int i = 0; i < stimuli.size(); ++i) {

            const ONI::Rhs2116StimulusData& s = stimuli[i];
            if(!isStimulusValid(s)) continue;

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

    //unsigned int readRegister(const Rhs2116StimulusRegister& reg){
    //    return BaseDevice::readRegister(reg);
    //}

    //bool writeRegister(const Rhs2116StimulusRegister& reg, const unsigned int& value){
    //    return BaseDevice::writeRegister(reg, value);
    //}

protected:

    ONI::Rhs2116StimulusData defaultStimulus;
    std::vector<ONI::Rhs2116StimulusData> defaultStimuli;

    Rhs2116MultiDevice* multi;

    ONI::Settings::Rhs2116StimulusSettings settings;

};

} // namespace Device
} // namespace ONI