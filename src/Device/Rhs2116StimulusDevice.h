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

    enum SettingType{
        STAGED,
        DEVICE
    };

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

        lastAppliedChannelMap = multi->getChannelMapIDX();

        ctx = multi->ctx;

        deviceType.idx = 1000;
        deviceType.id = RHS2116STIM;
        deviceTypeID = RHS2116STIM;

        std::ostringstream os; os << ONI::Device::toString(deviceTypeID) << " (" << deviceType.idx << ")";
        BaseProcessor::processorName = os.str();

        BaseDevice::reset(); // always call device specific setup/reset
        deviceSetup(); // always call device specific setup/reset

        deviceType.idx = 258; // TODO: this needs a lot more work to make it general for more rhs2116 devices [but this is the trig device for 256/257]
        //BaseDevice::writeRegister(RHS2116STIM_REG::ENABLE, 1); // do we need this?
        if(!ONI::Device::BaseDevice::writeRegister(ONI::Register::Rhs2116Stimulus::TRIGGERSOURCE, 0)){
            LOGERROR("Error writing to TRIGGERSOURCE!!");
        }

        deviceType.idx = 514; // TODO: this needs a lot more work to make it general for more rhs2116 devices [but this is the trig device for 256/257]
        //BaseDevice::writeRegister(RHS2116STIM_REG::ENABLE, 1); // do we need this?
        if(!ONI::Device::BaseDevice::writeRegister(ONI::Register::Rhs2116Stimulus::TRIGGERSOURCE, 0)){
            LOGERROR("Error writing to TRIGGERSOURCE!!");
        }

        deviceType.idx = 667;


    }

    void deviceSetup(){
        defaultStimuli.resize(numProbes);
        stagedSettings.stimuli = defaultStimuli;
        stagedSettings.stepSize = ONI::Settings::Step10nA;
        deviceSettings.stimuli = defaultStimuli;
        deviceSettings.stepSize = ONI::Settings::Step10nA;
    }

    inline void process(oni_frame_t* frame){};
    inline void process(ONI::Frame::BaseFrame& frame){};


    ONI::Rhs2116StimulusData& getStimulus(const unsigned int& probeIDX, const SettingType& type = SettingType::DEVICE){

        if(probeIDX > numProbes){
            LOGERROR("Probe index out of range: %i >= %i ", probeIDX, numProbes);
            return defaultStimulus;
        }

        std::vector<ONI::Rhs2116StimulusData>& deviceStimuli = getStimuli(type);
        return deviceStimuli[probeIDX];

    }

    std::vector<ONI::Rhs2116StimulusData>& getStimuli(const SettingType& type = SettingType::DEVICE){
        return (type == SettingType::DEVICE ? deviceSettings.stimuli : stagedSettings.stimuli);
    }

    ONI::Settings::Rhs2116StimulusStep& getStepSize(const SettingType& type = SettingType::DEVICE){
        return (type == SettingType::DEVICE ? deviceSettings.stepSize : stagedSettings.stepSize);
    }

    bool stageStimulus(const ONI::Rhs2116StimulusData& stimulus, const unsigned int& probeIDX){ // add a bAutoApply version ?

        if(probeIDX > numProbes){
            LOGERROR("Probe index out of range: %i >= %i ", probeIDX, numProbes);
            return false;
        }

        if(!isStimulusValid(stimulus)){
            LOGERROR("Stimulus is invalid");
            return false;
        }

        stagedSettings.stimuli[probeIDX] = stimulus;

    }

    bool unstageStimulus(const unsigned int& probeIDX){ // add a bAutoApply version ?

        if(probeIDX > numProbes){
            LOGERROR("Probe index out of range: %i >= %i ", probeIDX, numProbes);
            return false;
        }

        stagedSettings.stimuli[probeIDX] = defaultStimulus;

        return true;

    }

    bool stageStimuli(const std::vector<ONI::Rhs2116StimulusData>& stimuli){ // add a bAutoApply version ?

        if(stimuli.size() != numProbes){
            LOGERROR("Stimuli and probe sizes don't match: %i >= %i ", stimuli.size(), numProbes);
            return false;
        }

        for(size_t i = 0; i < stimuli.size(); ++i){
            if(!isStimulusValid(stimuli[i]) && stimuli[i] != defaultStimulus){
                LOGERROR("Stimulus is invalid: %i", i);
                return false;
            }
        }

        stagedSettings.stimuli = stimuli;

    }

    bool unstageStimuli(){ // add a bAutoApply version ?

        stagedSettings.stimuli = defaultStimuli;
        return true;

    }

    ONI::Settings::Rhs2116StimulusStep conformStepSize(const SettingType& type){
        std::vector<ONI::Rhs2116StimulusData>& stimuli = getStimuli(type);
        ONI::Settings::Rhs2116StimulusStep& stepSize = getStepSize(type);
        stepSize = getRequiredStepSize(stimuli);
        return stepSize;
    }

    bool isDeviceStimuliSynced(){

        if(deviceSettings.stepSize != stagedSettings.stepSize){
            //LOGDEBUG("Device Step Size != Stage Step Size");
            return false;
        }

        for(size_t i = 0; i < stagedSettings.stimuli.size(); ++i){
            size_t probeIDX = multi->getChannelMapIDX()[i];
            if(deviceSettings.stimuli[probeIDX] != stagedSettings.stimuli[i]){
                //LOGDEBUG("Device Stimulus != Staged Stimulus Size (%i != %i)", probeIDX, i);
                return false;
            }
        }

        return true;

    }

    bool isDeviceChannelMapSynced(){
        return (lastAppliedChannelMap == multi->getChannelMapIDX());
    }

    bool updateStimuliOnDevice(){
        ONI::Settings::Rhs2116StimulusSettings tempStagedSettings = stagedSettings; // make a back up, so we can restore
        const std::vector<size_t>& currentChannelMap = multi->getChannelMapIDX();
        const std::vector<size_t>& currentInverseChannelMap = multi->getInverseChannelMapIDX();
        for(size_t i = 0; i < deviceSettings.stimuli.size(); ++i){
            size_t a = currentChannelMap[i];
            size_t b = currentInverseChannelMap[i];
            if(a == lastAppliedChannelMap[i] || b == lastAppliedInverseChannelMap[i]) continue; // don't swap things that are unchanged
            size_t probeIDX = currentChannelMap[lastAppliedChannelMap[i]];

            stagedSettings.stimuli[probeIDX] = deviceSettings.stimuli[i]; // SO WE ONLY DO THE ACTUAL MAPPING WHEN SENDING THE STIMULI TO DEVICE (use a different function for plotting in the interface)
        }
        bool bOK = applyStagedStimuliToDevice();
        stagedSettings = tempStagedSettings;
        return bOK;
    }

    bool applyStagedStimuliToDevice(){



        // conform stepsize just in case
        ONI::Settings::Rhs2116StimulusStep ss = stagedSettings.stepSize;
        stagedSettings.stepSize = getRequiredStepSize(stagedSettings.stimuli);

        if(stagedSettings.stepSize != ss){
            LOGALERT("Step size not sync'd - better to check this before sending to device");
        }

        conformStepSize(SettingType::STAGED); 

        for(size_t i = 0; i < stagedSettings.stimuli.size(); ++i){
            size_t probeIDX = multi->getInverseChannelMapIDX()[i];
            deviceSettings.stimuli[probeIDX] = stagedSettings.stimuli[i]; // SO WE ONLY DO THE ACTUAL MAPPING WHEN SENDING THE STIMULI TO DEVICE (use a different function for plotting in the interface)
        }

        lastAppliedChannelMap = multi->getChannelMapIDX();
        lastAppliedInverseChannelMap = multi->getInverseChannelMapIDX();

        deviceSettings.stepSize = stagedSettings.stepSize; // conformStepSize(SettingType::DEVICE);
        
        for(size_t deviceCount = 0; deviceCount < multi->devices.size(); ++deviceCount){

            size_t deviceIDX = multi->expectDevceIDOrdered[deviceCount];
            Rhs2116Device* device = multi->getDevice(deviceIDX);

            if(!device->setStepSize(deviceSettings.stepSize)){
                LOGERROR("Error setting device STEPSZ!!");
                deviceSettings.stimuli = defaultStimuli; // hard reset device settings
                deviceSettings.stepSize = ONI::Settings::Step10nA;
                return false;
            }

            const unsigned int * regs = ONI::Settings::Rhs2116StimulusBiasRegisters[deviceSettings.stepSize];
            unsigned int rbias = regs[1] << 4 | regs[0];

            if(!device->writeRegister(ONI::Register::Rhs2116::STIMBIAS, rbias)){ // ask Jon about whether this is necessary
                LOGERROR("Error writing to STIMBIAS!!");
                deviceSettings.stimuli = defaultStimuli; // hard reset device settings
                deviceSettings.stepSize = ONI::Settings::Step10nA;
                return false;
            }

            size_t startIDX = deviceCount * 16;
            size_t endIDX = startIDX + 16;

            std::vector<ONI::Rhs2116StimulusData> tDeviceStimuli;

            for(size_t probeIDX = startIDX; probeIDX < endIDX; ++probeIDX){

                tDeviceStimuli.push_back(deviceSettings.stimuli[probeIDX]);

                LOGDEBUG("Set -ve probe stimulus: %i (%i)", probeIDX, multi->getInverseChannelMapIDX()[probeIDX]);
                if(!device->writeRegister(ONI::Register::Rhs2116::NEG[probeIDX % 16], deviceSettings.stimuli[probeIDX].anodicAmplitudeSteps)){
                    LOGERROR("Error writing to NEG!!");
                    deviceSettings.stimuli = defaultStimuli; // hard reset device settings
                    deviceSettings.stepSize = ONI::Settings::Step10nA;
                    return false;
                }

                LOGDEBUG("Set +ve probe stimulus: %i (%i)", probeIDX, multi->getInverseChannelMapIDX()[probeIDX]);
                if(!device->writeRegister(ONI::Register::Rhs2116::POS[probeIDX % 16], deviceSettings.stimuli[probeIDX].cathodicAmplitudeSteps)){
                    LOGERROR("Error writing to POS!!");
                    deviceSettings.stimuli = defaultStimuli; // hard reset device settings
                    deviceSettings.stepSize = ONI::Settings::Step10nA;
                    return false;
                }

            }

            auto result = getDeltaTable(tDeviceStimuli);

            if(!device->writeRegister(ONI::Register::Rhs2116::NUMDELTAS, result.size())){
                LOGERROR("Too many stimuli deltas!!");
                deviceSettings.stimuli = defaultStimuli; // hard reset device settings
                deviceSettings.stepSize = ONI::Settings::Step10nA;
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
                    deviceSettings.stimuli = defaultStimuli; // hard reset device settings
                    deviceSettings.stepSize = ONI::Settings::Step10nA;
                    return false;
                }

                if(!device->writeRegister(ONI::Register::Rhs2116::DELTAPOLEN, entry.second)){
                    LOGERROR("Error writing to DELTAPOLEN!!");
                    deviceSettings.stimuli = defaultStimuli; // hard reset device settings
                    deviceSettings.stepSize = ONI::Settings::Step10nA;
                    return false;
                }
                //LOGDEBUG("32771 %i", idxTime);
                //LOGDEBUG("32772 %i", entry.second);
                //++j;
            }

            //device->writeRegister(RHS2116_REG::RESPECTSTIMACTIVE, 1); // hmmm? not done in bonsai code

            if(device->readRegister(ONI::Register::Rhs2116::SEQERROR)){
                LOGERROR("Stimulus sequence error!!");
                deviceSettings.stimuli = defaultStimuli; // hard reset device settings
                deviceSettings.stepSize = ONI::Settings::Step10nA;
                return false;
            }

        }

    }


    inline void triggerStimulusSequence(){ // maybe proved a bAutoApply func?

        //if(deviceSettings != stagedSettings){ // ACTUALLY LETS CHECK WITH CHANNEL MAP!!
        //    LOGALERT("Staged settings are different to current device settings");
        //}

        deviceType.idx = 258; // TODO: this needs a lot more work to make it general for more rhs2116 devices [but this is the trig device for 256/257]
        ONI::Device::BaseDevice::writeRegister(ONI::Register::Rhs2116Stimulus::TRIGGER, 1);

        deviceType.idx = 514; // TODO: this needs a lot more work to make it general for more rhs2116 devices [but this is the trig device for 512/513]
        ONI::Device::BaseDevice::writeRegister(ONI::Register::Rhs2116Stimulus::TRIGGER, 1);

        deviceType.idx = 667;

    }

    unsigned int getTotalLengthSamples(const ONI::Rhs2116StimulusData& stimulus){
        unsigned int totalSamples = stimulus.anodicWidthSamples + stimulus.dwellSamples + stimulus.cathodicWidthSamples + stimulus.interStimulusIntervalSamples;
        totalSamples *= stimulus.numberOfStimuli;
        totalSamples += stimulus.delaySamples;
        return totalSamples;
    }

    unsigned int getMaxLengthSamples(const SettingType& type = SettingType::DEVICE){
        std::vector<ONI::Rhs2116StimulusData>& deviceStimuli = getStimuli(type);
        return getMaxLengthSamples(deviceStimuli);
    }

    std::vector<std::vector<float>> getAllStimulusAmplitudes(const SettingType& type = SettingType::DEVICE){
        std::vector<ONI::Rhs2116StimulusData>& deviceStimuli = getStimuli(type);
        return getAllStimulusAmplitudes(deviceStimuli);
    }

    ONI::Settings::Rhs2116StimulusStep getRequiredStepSize(std::vector<ONI::Rhs2116StimulusData>& stimuli){

        ONI::Settings::Rhs2116StimulusStep maxStep = ONI::Settings::Rhs2116StimulusStep::Step10nA;

        //std::vector<ONI::Settings::Rhs2116StimulusStep> stepSizes(stimuli.size());

        for(size_t i = 0; i < stimuli.size(); ++i){
            if(stimuli[i].requestedAnodicAmplitudeMicroAmps > 0 && stimuli[i].requestedCathodicAmplitudeMicroAmps > 0){
                ONI::Settings::Rhs2116StimulusStep stepSize = getRequiredStepSize(stimuli[i]);
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

    ONI::Settings::Rhs2116StimulusStep getRequiredStepSize(ONI::Rhs2116StimulusData& stimulus){

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

private:



    unsigned int getMaxLengthSamples(const std::vector<ONI::Rhs2116StimulusData>& deviceStimuli){
        unsigned int totalSamples = 0;
        for(auto& s : deviceStimuli){
            unsigned int t = getTotalLengthSamples(s);
            if(t >= totalSamples) totalSamples = t;
        }
        return totalSamples;
    }

    std::vector<std::vector<float>> getAllStimulusAmplitudes(const std::vector<ONI::Rhs2116StimulusData>& deviceStimuli){
        std::vector<std::vector<float>> allAmplitudes;
        allAmplitudes.resize(deviceStimuli.size());
        for(size_t i = 0; i < deviceStimuli.size(); ++i){
            allAmplitudes[i] = getStimulusAmplitudes(deviceStimuli[i]);
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

    std::map<uint32_t, uint32_t> getDeltaTable(const std::vector<ONI::Rhs2116StimulusData>& deviceStimuli) {

        std::map<uint32_t, std::bitset<32>> table;

        for (int i = 0; i < deviceStimuli.size(); ++i) {

            const ONI::Rhs2116StimulusData& s = deviceStimuli[i];
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

    Rhs2116MultiDevice* multi;

    ONI::Rhs2116StimulusData defaultStimulus;
    std::vector<ONI::Rhs2116StimulusData> defaultStimuli;

    ONI::Settings::Rhs2116StimulusSettings stagedSettings; // these are what we modify and check for conformance before commiting to the devices
    ONI::Settings::Rhs2116StimulusSettings deviceSettings; // these should always reflect what's programmed on the devices

    std::vector<size_t> lastAppliedChannelMap;
    std::vector<size_t> lastAppliedInverseChannelMap;

};

} // namespace Device
} // namespace ONI