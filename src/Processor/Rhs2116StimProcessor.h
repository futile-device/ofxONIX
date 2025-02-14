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


#include "../Processor/BaseProcessor.h"
#include "../Processor/Rhs2116MultiProcessor.h"

#pragma once

namespace ONI{

namespace Interface{
class Rhs2116StimulusInterface;
} // namespace Interface

namespace Processor{




class Rhs2116StimProcessor : public ONI::Processor::BaseProcessor {

public:

    enum SettingType{
        STAGED,
        DEVICE
    };

	friend class ONI::Interface::Rhs2116StimulusInterface;

	Rhs2116StimProcessor(){
        BaseProcessor::processorTypeID = ONI::Processor::TypeID::RHS2116_STIM_PROCESSOR;
        BaseProcessor::processorName = toString(processorTypeID);
    };
	
	~Rhs2116StimProcessor(){
		LOGDEBUG("RHS2116Stim Processor DTOR");
		//const std::lock_guard<std::mutex> lock(mutex);
	};

    void setup(){
        LOGDEBUG("Setting up device RHS2116STIM Processor");
        ONI::Global::model.getRhs2116MultiProcessor()->subscribeProcessor(processorName, ONI::Processor::SubscriptionType::POST_PROCESSOR, this);
        reset();
    }

    void addDevice(ONI::Device::Rhs2116Device* device){
        auto it = rhs2116Devices.find(device->getOnixDeviceTableIDX());
        if(it == rhs2116Devices.end()){
            LOGINFO("Adding device %s", device->getName().c_str());
            rhs2116Devices[device->getOnixDeviceTableIDX()] = device;
        }else{
            LOGERROR("Device already added: %s", device->getName().c_str());
        }
        numProbes += device->getNumProbes();
        reset();
    }

    void addStimDevice(ONI::Device::Rhs2116StimDevice* device){
        auto it = rhs2116StimDevices.find(device->getOnixDeviceTableIDX());
        if(it == rhs2116StimDevices.end()){
            LOGINFO("Adding stim device %s", device->getName().c_str());
            rhs2116StimDevices[device->getOnixDeviceTableIDX()] = device;
        }else{
            LOGERROR("Stim device already added: %s", device->getName().c_str());
        }
        //numProbes += device->getNumProbes();
        reset();
    }

    void reset(){
        defaultStimuli.resize(numProbes);
        stagedSettings.stimuli = defaultStimuli;
        stagedSettings.stepSize = ONI::Settings::Step10nA;
        deviceSettings.stimuli = defaultStimuli;
        deviceSettings.stepSize = ONI::Settings::Step10nA;
    }

    inline void process(oni_frame_t* frame){};
    inline void process(ONI::Frame::BaseFrame& frame){
        if(stimulusSampleCountRemaining > 0) {
            reinterpret_cast<ONI::Frame::Rhs2116MultiFrame*>(&frame)->stim = true;
            --stimulusSampleCountRemaining;
        }
        for(auto& it : postProcessors){
            it.second->process(frame);
        }
    };


    ONI::Rhs2116StimulusData& getStimulus(const unsigned int& probeIDX, const SettingType& type = SettingType::DEVICE){

        if(probeIDX > numProbes){
            LOGERROR("Probe index out of range: %i >= %i ", probeIDX, numProbes);
            return defaultStimulus;
        }

        std::vector<ONI::Rhs2116StimulusData>& deviceStimuli = getStimuli(type);
        return deviceStimuli[probeIDX];

    }

    bool isStimulusOnDevice(const size_t& probeIDX){
        return (deviceSettings.stimuli[getInverseChannelMap()[probeIDX]] != defaultStimulus);
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
            size_t probeIDX = getChannelMap()[i];
            if(deviceSettings.stimuli[probeIDX] != stagedSettings.stimuli[i]){
                //LOGDEBUG("Device Stimulus != Staged Stimulus Size (%i != %i)", probeIDX, i);
                return false;
            }
        }

        return true;

    }

    bool isDeviceChannelMapSynced(){
        return (lastAppliedChannelMap == getChannelMap());
    }

    const std::vector<size_t>& getChannelMap(){
        return ONI::Global::model.getChannelMapProcessor()->getChannelMap();
    }

    const std::vector<size_t>& getInverseChannelMap(){
        return ONI::Global::model.getChannelMapProcessor()->getInverseChannelMap();
    }

    bool updateStimuliOnDevice(){
        ONI::Settings::Rhs2116StimulusSettings tempStagedSettings = stagedSettings; // make a back up, so we can restore
        const std::vector<size_t>& currentChannelMap = getChannelMap();
        const std::vector<size_t>& currentInverseChannelMap = getInverseChannelMap();
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

    bool applyStagedStimuliToDevice(const bool& bSetWithoutCheck = true){

        // conform stepsize just in case
        ONI::Settings::Rhs2116StimulusStep ss = stagedSettings.stepSize;
        stagedSettings.stepSize = getRequiredStepSize(stagedSettings.stimuli);

        if(stagedSettings.stepSize != ss){
            LOGALERT("Step size not sync'd - better to check this before sending to device");
        }

        conformStepSize(SettingType::STAGED); 

        for(size_t i = 0; i < stagedSettings.stimuli.size(); ++i){
            size_t probeIDX = getInverseChannelMap()[i];
            deviceSettings.stimuli[probeIDX] = stagedSettings.stimuli[i]; // SO WE ONLY DO THE ACTUAL MAPPING WHEN SENDING THE STIMULI TO DEVICE (use a different function for plotting in the interface)
        }

        lastAppliedChannelMap = getChannelMap();
        lastAppliedInverseChannelMap = getInverseChannelMap();

        deviceSettings.stepSize = stagedSettings.stepSize; // conformStepSize(SettingType::DEVICE);
        
        for(size_t deviceIDX = 0; deviceIDX < rhs2116Devices.size(); ++deviceIDX){

            size_t onixDeviceTableIDX = ONI::Global::model.getRhs2116DeviceOrderIDX()[deviceIDX];
            ONI::Device::Rhs2116Device * device = getRhs2116Device(onixDeviceTableIDX);

            if(!device->setStepSize(deviceSettings.stepSize, bSetWithoutCheck)){
                LOGERROR("Error setting device STEPSZ!!");
                deviceSettings.stimuli = defaultStimuli; // hard reset device settings
                deviceSettings.stepSize = ONI::Settings::Step10nA;
                return false;
            }

            const unsigned int * regs = ONI::Settings::Rhs2116StimulusBiasRegisters[deviceSettings.stepSize];
            unsigned int rbias = regs[1] << 4 | regs[0];

            if(!device->writeRegister(ONI::Register::Rhs2116::STIMBIAS, rbias, bSetWithoutCheck)){ // ask Jon about whether this is necessary
                LOGERROR("Error writing to STIMBIAS!!");
                deviceSettings.stimuli = defaultStimuli; // hard reset device settings
                deviceSettings.stepSize = ONI::Settings::Step10nA;
                return false;
            }

            size_t startIDX = deviceIDX * 16;
            size_t endIDX = startIDX + 16;

            std::vector<ONI::Rhs2116StimulusData> tDeviceStimuli;

            for(size_t probeIDX = startIDX; probeIDX < endIDX; ++probeIDX){

                tDeviceStimuli.push_back(deviceSettings.stimuli[probeIDX]);

                if(!bSetWithoutCheck) LOGDEBUG("Set -ve probe stimulus: %i (%i)", probeIDX, getInverseChannelMap()[probeIDX]);
                if(!device->writeRegister(ONI::Register::Rhs2116::NEG[probeIDX % 16], deviceSettings.stimuli[probeIDX].anodicAmplitudeSteps, bSetWithoutCheck)){
                    LOGERROR("Error writing to NEG!!");
                    deviceSettings.stimuli = defaultStimuli; // hard reset device settings
                    deviceSettings.stepSize = ONI::Settings::Step10nA;
                    return false;
                }

                if(!bSetWithoutCheck) LOGDEBUG("Set +ve probe stimulus: %i (%i)", probeIDX, getInverseChannelMap()[probeIDX]);
                if(!device->writeRegister(ONI::Register::Rhs2116::POS[probeIDX % 16], deviceSettings.stimuli[probeIDX].cathodicAmplitudeSteps, bSetWithoutCheck)){
                    LOGERROR("Error writing to POS!!");
                    deviceSettings.stimuli = defaultStimuli; // hard reset device settings
                    deviceSettings.stepSize = ONI::Settings::Step10nA;
                    return false;
                }

            }

            auto result = getDeltaTable(tDeviceStimuli);

            if(!device->writeRegister(ONI::Register::Rhs2116::NUMDELTAS, result.size(), bSetWithoutCheck)){
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
                if(!device->writeRegister(ONI::Register::Rhs2116::DELTAIDXTIME, idxTime, bSetWithoutCheck)){
                    LOGERROR("Error writing to DELTAIDXTIME!!");
                    deviceSettings.stimuli = defaultStimuli; // hard reset device settings
                    deviceSettings.stepSize = ONI::Settings::Step10nA;
                    return false;
                }

                if(!device->writeRegister(ONI::Register::Rhs2116::DELTAPOLEN, entry.second, bSetWithoutCheck)){
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

            if(!bSetWithoutCheck && device->readRegister(ONI::Register::Rhs2116::SEQERROR)){
                LOGERROR("Stimulus sequence error!!");
                deviceSettings.stimuli = defaultStimuli; // hard reset device settings
                deviceSettings.stepSize = ONI::Settings::Step10nA;
                return false;
            }

            device->getContexteNeedsReset(); // forces context to not restart

        }

        return true;

    }

    inline bool isStimulousPlaying(){
        return stimulusSampleCountRemaining > 0;
    }

    inline void triggerStimulusSequence(){ // maybe proved a bAutoApply func?

        

        for(auto& it : rhs2116StimDevices){
            ONI::Device::Rhs2116StimDevice* device = it.second;
            device->triggerStimulus();
            stimulusSampleCountRemaining = stimulusSampleCountTotal = getMaxLengthSamples();
            device->getContexteNeedsReset(); // force no context resetting (cos this sets it to false)
        }

        

        //if(deviceSettings != stagedSettings){ // ACTUALLY LETS CHECK WITH CHANNEL MAP!!
        //    LOGALERT("Staged settings are different to current device settings");
        //}

        //onixDeviceType.idx = 258; // TODO: this needs a lot more work to make it general for more rhs2116 devices [but this is the trig device for 256/257]
        //ONI::Device::BaseDevice::writeRegister(ONI::Register::Rhs2116Stimulus::TRIGGER, 1);

        //onixDeviceType.idx = 514; // TODO: this needs a lot more work to make it general for more rhs2116 devices [but this is the trig device for 512/513]
        //ONI::Device::BaseDevice::writeRegister(ONI::Register::Rhs2116Stimulus::TRIGGER, 1);

        //onixDeviceType.idx = 667;

    }

    size_t getTotalLengthSamples(const ONI::Rhs2116StimulusData& stimulus){
        size_t totalSamples = stimulus.anodicWidthSamples + stimulus.dwellSamples + stimulus.cathodicWidthSamples + stimulus.interStimulusIntervalSamples;
        totalSamples *= stimulus.numberOfStimuli;
        totalSamples += stimulus.delaySamples;
        return totalSamples;
    }

    size_t getMaxLengthSamples(const SettingType& type = SettingType::DEVICE){
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

            //LOGDEBUG("Requested Anodic uA (%0.2f) || Actual Anodic uA (%0.2f) || Min Step (M) (%f) || Steps %i", stimulus.requestedAnodicAmplitudeMicroAmps, stimulus.actualAnodicAmplitudeMicroAmps, ONI::Settings::Rhs2116StimulusStepMicroAmps[maxStep], stimulus.anodicAmplitudeSteps);
            //LOGDEBUG("Requested Cathod uA (%0.2f) || Actual Cathod uA (%0.2f) || Min Step (M) (%f) || Steps %i", stimulus.requestedCathodicAmplitudeMicroAmps, stimulus.actualCathodicAmplitudeMicroAmps, ONI::Settings::Rhs2116StimulusStepMicroAmps[maxStep], stimulus.cathodicAmplitudeSteps);

        }

        return maxStep;

    }

    ONI::Settings::Rhs2116StimulusStep getRequiredStepSize(ONI::Rhs2116StimulusData& stimulus){

        ONI::Settings::Rhs2116StimulusStep anodicMinStep = ONI::Settings::Rhs2116StimulusStep::StepNotSet; // 10

        unsigned int anodicSteps = 0;
        for(size_t i = 0; i < 10; ++i){
            double stepMicroAmps = ONI::Settings::Rhs2116StimulusStepMicroAmps[i];
            anodicSteps = std::floor(stimulus.requestedAnodicAmplitudeMicroAmps / stepMicroAmps);
            if(anodicSteps <= 255){
                anodicMinStep = ONI::Settings::Rhs2116StimulusStep(i);
                break;
            }
        }

        if(anodicMinStep == ONI::Settings::Rhs2116StimulusStep::StepNotSet){
            LOGERROR("Requested anodic current is too high");
            anodicMinStep = ONI::Settings::Rhs2116StimulusStep::Step10000nA;
            anodicSteps = 255;
        }

        ONI::Settings::Rhs2116StimulusStep cathodicMinStep = ONI::Settings::Rhs2116StimulusStep::StepNotSet; // 11

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



    size_t getMaxLengthSamples(const std::vector<ONI::Rhs2116StimulusData>& deviceStimuli){
        size_t totalSamples = 0;
        for(auto& s : deviceStimuli){
            size_t t = getTotalLengthSamples(s);
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

    ONI::Device::Rhs2116Device* getRhs2116Device(const uint32_t& deviceTableIDX){
        auto it = rhs2116Devices.find(deviceTableIDX);
        if(it == rhs2116Devices.end()){
            LOGERROR("No device found for IDX: %i", deviceTableIDX);
            return nullptr;
        }else{
            return it->second;
        }
    }

    //unsigned int readRegister(const Rhs2116StimulusRegister& reg){
    //    return BaseDevice::readRegister(reg);
    //}

    //bool writeRegister(const Rhs2116StimulusRegister& reg, const unsigned int& value){
    //    return BaseDevice::writeRegister(reg, value);
    //}

protected:

    //Rhs2116MultiProcessor* multi;

    volatile bool bIsStimulusActive = false;
    volatile size_t stimulusSampleCountRemaining = 0;
    volatile size_t stimulusSampleCountTotal = 0;

    std::map<uint32_t, ONI::Device::Rhs2116Device*> rhs2116Devices;
    std::map<uint32_t, ONI::Device::Rhs2116StimDevice*> rhs2116StimDevices;

    ONI::Rhs2116StimulusData defaultStimulus;
    std::vector<ONI::Rhs2116StimulusData> defaultStimuli;

    ONI::Settings::Rhs2116StimulusSettings stagedSettings; // these are what we modify and check for conformance before commiting to the devices
    ONI::Settings::Rhs2116StimulusSettings deviceSettings; // these should always reflect what's programmed on the devices

    std::vector<size_t> lastAppliedChannelMap;
    std::vector<size_t> lastAppliedInverseChannelMap;

};

} // namespace Device
} // namespace ONI