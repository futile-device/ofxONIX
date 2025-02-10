#include "ofApp.h"


#define ONI_LOG_FU 1
#include "ONIContext.h"
#include "ContextInterface.h"

ONI::Context oni;


//--------------------------------------------------------------
void ofApp::setup(){

    ofSetBackgroundColor(0);
    ofSetVerticalSync(false);
    ofSetFrameRate(6000);
    ofSeedRandom();

    ofSetLogLevel(OF_LOG_VERBOSE);
    ofLogToConsole();

    fu::AddLogChannel((fu::LogChannel*)new fu::ConsoleLogChannel);
    fu::AddLogChannel((fu::LogChannel*)&fu::ImGuiConsole);

    fu::SetLogLevel(fu::LogLevel::FU_DEBUG);
    fu::SetLogWithInfo(true);
    //    fu::SetLogWithDate(true);
    fu::SetLogWithTime(true);

    fu::setupGui();
    plt.setup();

    oni.setup();
    oni.printDeviceTable();

    ONI::Device::FmcDevice* fmc1 = (ONI::Device::FmcDevice*)oni.getDevice(1);
    fmc1->setPortVoltage(6.9); // 4.33v // 49.2C

    //ONI::Device::FmcDevice* fmc2 = (ONI::Device::FmcDevice*)oni.getDevice(2);
    //fmc2->setPortVoltage(6.4); // 4.30v // 44.6C
    
    oni.update(); // requires an update to implement the port voltage settings
    //oni.printDeviceTable();

    ONI::Device::HeartBeatDevice* heartBeatDevice = (ONI::Device::HeartBeatDevice*)oni.getDevice(0); // for some reason we need to do this before a context restart
    heartBeatDevice->setFrequencyHz(1);

    //oni.update(); // requires an update to set the heart beat frequency, could wait 

    ONI::Device::Rhs2116Device* rhs2116Device1 = (ONI::Device::Rhs2116Device*)oni.getDevice(256);
    ONI::Device::Rhs2116Device* rhs2116Device2 = (ONI::Device::Rhs2116Device*)oni.getDevice(257);
    //ONI::Device::Rhs2116Device* rhs2116Device3 = (ONI::Device::Rhs2116Device*)oni.getDevice(512);
    //ONI::Device::Rhs2116Device* rhs2116Device4 = (ONI::Device::Rhs2116Device*)oni.getDevice(513);

    //rhs2116Device1->readRegister(ONI::Register::Rhs2116::MAXDELTAS);

    fu::debug << rhs2116Device1->getFormat() << fu::endl;

    ONI::Processor::Rhs2116MultiProcessor* multi = oni.createRhs2116MultiProcessor();

    multi->setup();

    multi->addDevice(rhs2116Device1);
    multi->addDevice(rhs2116Device2);
    //multi->addDevice(rhs2116Device3);
    //multi->addDevice(rhs2116Device4);


    multi->setDspCutOff(ONI::Settings::Rhs2116DspCutoff::Dsp308Hz);
    multi->setAnalogLowCutoff(ONI::Settings::Rhs2116AnalogLowCutoff::Low100mHz);
    multi->setAnalogLowCutoffRecovery(ONI::Settings::Rhs2116AnalogLowCutoff::Low250Hz);
    multi->setAnalogHighCutoff(ONI::Settings::Rhs2116AnalogHighCutoff::High10000Hz);

    fu::debug << rhs2116Device1->getFormat() << fu::endl;

    ONI::Processor::ChannelMapProcessor* channelProcessor = oni.createChannelMapProcessor();
    channelProcessor->setup(multi);

    std::vector<size_t> channelMap = {59, 34, 33, 32, 63, 16, 17, 11, 58, 35, 60, 61, 14, 13, 18, 10, 57, 36, 19, 62, 15, 12, 20, 9, 56, 38, 37, 39, 23, 21, 22, 8, 40, 54, 53, 55, 7, 5, 6, 24, 41, 52, 44, 47, 0, 28, 4, 25, 42, 51, 45, 46, 1, 2, 3, 26, 43, 50, 49, 48, 31, 30, 29, 27};
    //std::vector<size_t> inversechannelMap = {44, 52, 53, 54, 46, 37, 38, 36, 31, 23, 15, 7, 21, 13, 12, 20, 5, 6, 14, 18, 22, 29, 30, 28, 39, 47, 55, 63, 45, 62, 61, 60, 3, 2, 1, 9, 17, 26, 25, 27, 32, 40, 48, 56, 42, 50, 51, 43, 59, 58, 57, 49, 41, 34, 33, 35, 24, 16, 8, 0, 10, 11, 19, 4};

    //channelProcessor->setChannelMap(channelMap);

    const std::vector<size_t>& chi = oni.getChannelMapProcessor()->getInverseChannelMap();

    ONI::Rhs2116StimulusData s1;
    s1.requestedAnodicAmplitudeMicroAmps = 0.05;
    s1.requestedCathodicAmplitudeMicroAmps = 0.05;
    s1.actualAnodicAmplitudeMicroAmps = 0;
    s1.actualCathodicAmplitudeMicroAmps = 0;
    s1.anodicAmplitudeSteps = 0;
    s1.anodicFirst = true;
    s1.anodicWidthSamples = 300;
    s1.cathodicAmplitudeSteps = 0;
    s1.cathodicWidthSamples = 300;
    s1.delaySamples = 0;
    s1.dwellSamples = 1;
    s1.interStimulusIntervalSamples = 1;
    s1.numberOfStimuli = 10;

    ONI::Rhs2116StimulusData s2;
    s2.requestedAnodicAmplitudeMicroAmps = 18.1;
    s2.requestedCathodicAmplitudeMicroAmps = 99.3;
    s2.actualAnodicAmplitudeMicroAmps = 0;
    s2.actualCathodicAmplitudeMicroAmps = 0;
    s2.anodicAmplitudeSteps = 0;
    s2.anodicFirst = true;
    s2.anodicWidthSamples = 60;
    s2.cathodicAmplitudeSteps = 0;
    s2.cathodicWidthSamples = 60;
    s2.delaySamples = 694;
    s2.dwellSamples = 121;
    s2.interStimulusIntervalSamples = 362;
    s2.numberOfStimuli = 2;

    ONI::Processor::Rhs2116StimProcessor * rhs2116StimProcessor = oni.createRhs2116StimProcessor(); // make sure to call this after multi device is initialized with devices!
    rhs2116StimProcessor->setup();

    ONI::Device::Rhs2116StimDevice * rhs2116StimDevice1 = (ONI::Device::Rhs2116StimDevice*)oni.getDevice(258);
    //ONI::Device::Rhs2116StimDevice * rhs2116StimDevice2 = (ONI::Device::Rhs2116StimDevice*)oni.getDevice(514);

    rhs2116StimDevice1->getTriggerSource();
    rhs2116StimDevice1->setTriggerSource(true);
    //rhs2116StimDevice2->setTriggerSource(true);

    rhs2116StimProcessor->addStimDevice(rhs2116StimDevice1);
    //rhs2116StimProcessor->addStimDevice(rhs2116StimDevice2);

    rhs2116StimProcessor->addDevice(rhs2116Device1);
    rhs2116StimProcessor->addDevice(rhs2116Device2);
    //rhs2116StimProcessor->addDevice(rhs2116Device3);
    //rhs2116StimProcessor->addDevice(rhs2116Device4);

    rhs2116StimProcessor->getRequiredStepSize(s1);
    rhs2116StimProcessor->getRequiredStepSize(s2);

    rhs2116StimProcessor->stageStimulus(s1, 0);
    rhs2116StimProcessor->stageStimulus(s2, 1);

    std::vector<ONI::Rhs2116StimulusData> stagedStimuli = rhs2116StimProcessor->getStimuli(ONI::Processor::Rhs2116StimProcessor::SettingType::STAGED);
    std::vector<ONI::Rhs2116StimulusData> lastStimuli = stagedStimuli;

    ONI::Settings::Rhs2116StimulusStep stepSize = rhs2116StimProcessor->getRequiredStepSize(stagedStimuli);

    for(size_t i = 0; i < stagedStimuli.size(); ++i){
        if(stagedStimuli[i].requestedAnodicAmplitudeMicroAmps != stagedStimuli[i].actualAnodicAmplitudeMicroAmps){
            LOGINFO("Probe %i anodic uA will change from %0.3f to %0.3f: ", i, stagedStimuli[i].requestedAnodicAmplitudeMicroAmps, stagedStimuli[i].actualAnodicAmplitudeMicroAmps);
            LOGINFO("Probe %i cathod uA will change from %0.3f to %0.3f: ", i, stagedStimuli[i].requestedCathodicAmplitudeMicroAmps, stagedStimuli[i].actualCathodicAmplitudeMicroAmps);
        }
    }

    ONI::Processor::BufferProcessor* bufferProcessor = oni.createBufferProcessor();
    bufferProcessor->setup(rhs2116StimProcessor);

    ONI::Processor::RecordProcessor* recordProcessor = oni.createRecordProcessor();
    recordProcessor->setup();

    ONI::Processor::SpikeProcessor* spikeProcessor = oni.createSpikeProcessor();
    spikeProcessor->setup();
    

    rhs2116StimProcessor->applyStagedStimuliToDevice();


    oni.update();

    //oni.startAcquisition();
    
}


fu::Timer processTimer;
//--------------------------------------------------------------
void ofApp::update(){

    oni.update();

    ofDisableArbTex();
    fu::gui.begin();
    { 
        
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        fu::ImGuiConsole.gui();

        //ImPlot::ShowDemoWindow();
        //ImGui::ShowDemoWindow();

        ONIGui.gui(oni);
        

    }
    fu::gui.end();
    ofEnableArbTex();

}


//--------------------------------------------------------------
void ofApp::draw(){
    fu::gui.draw();
}

//--------------------------------------------------------------
void ofApp::exit(){

    //oni.close();
    plt.exit();
    fu::gui.exit();

}

//--------------------------------------------------------------
void ofApp::keyPressed(int key){
    if(key == 'c'){
        
        oni.stopAcquisition();
        oni.getContextOption(ONI::ContextOption::BLOCKREADSIZE);
        oni.setContextOption(ONI::ContextOption::RESET, 1);
        //oni.getContextOption(ONI::Context::BLOCKREADSIZE);
        oni.setContextOption(ONI::ContextOption::BLOCKREADSIZE, 2048);
        oni.setContextOption(ONI::ContextOption::BLOCKWRITESIZE, 2048);
        //oni.setup();
        ofSleepMillis(500);
        oni.startAcquisition();
    }

    ONI::Processor::Rhs2116StimProcessor * rhs2116StimProcessor = oni.getRhs2116StimProcessor();
    ONI::Processor::RecordProcessor* recordProcessor = ONI::Global::model.getRecordProcessor();

    if(key == 'r'){
        //recordProcessor->record();
        oni.record();
    }

    if(key == 'p'){
        //recordProcessor->play();
        oni.play();
    }

    if(key == 't'){
        rhs2116StimProcessor->triggerStimulusSequence();
    }
}

//--------------------------------------------------------------
void ofApp::keyReleased(int key){

}

//--------------------------------------------------------------
void ofApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void ofApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseReleased(int x, int y, int button){

}

//--------------------------------------------------------------
void ofApp::mouseEntered(int x, int y){

}

//--------------------------------------------------------------
void ofApp::mouseExited(int x, int y){

}

//--------------------------------------------------------------
void ofApp::windowResized(int w, int h){

}

//--------------------------------------------------------------
void ofApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void ofApp::dragEvent(ofDragInfo dragInfo){ 

}


/*
      for(size_t probe = 0; probe < 32; ++probe){

            acProbeStats[probe].sum = 0;
            dcProbeStats[probe].sum = 0;

            for(size_t frame = 0; frame < drawFrameCount; ++frame){

                if(probe < 16){
                    probeTimeStamps[probe][frame] =  uint64_t((drawFrames1[frame].deltaTime - drawFrames1[0].deltaTime) / 1000);
                    acProbeVoltages[probe][frame] = 0.195f * (drawFrames1[frame].ac[probe     ] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
                    dcProbeVoltages[probe][frame] = -19.23 * (drawFrames1[frame].dc[probe     ] - 512) / 1000.0f;   // -19.23 mV × (ADC result – 512) divide by 1000 for V?
                }else{
                    probeTimeStamps[probe][frame] =  uint64_t((drawFrames2[frame].deltaTime - drawFrames2[0].deltaTime) / 1000);
                    acProbeVoltages[probe][frame] = 0.195f * (drawFrames1[frame].ac[probe - 16] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
                    dcProbeVoltages[probe][frame] = -19.23 * (drawFrames1[frame].dc[probe - 16] - 512) / 1000.0f;   // -19.23 mV × (ADC result – 512) divide by 1000 for V?
                }

                acProbeStats[probe].sum += acProbeVoltages[probe][frame];
                dcProbeStats[probe].sum += dcProbeVoltages[probe][frame];

            }

            for(size_t frame = 0; frame < rawFrameCount; ++frame){

                if(probe < 16){
                    acProbeStats[probe].sum += 0.195f * (rawFrames1[frame].ac[probe     ] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
                    dcProbeStats[probe].sum += -19.23 * (rawFrames1[frame].dc[probe     ] - 512) / 1000.0f;   // -19.23 mV × (ADC result – 512) divide by 1000 for V?
                }else{
                    acProbeStats[probe].sum += 0.195f * (rawFrames2[frame].ac[probe - 16] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
                    dcProbeStats[probe].sum += -19.23 * (rawFrames2[frame].dc[probe - 16] - 512) / 1000.0f;   // -19.23 mV × (ADC result – 512) divide by 1000 for V?
                }

            }

            acProbeStats[probe].mean = acProbeStats[probe].sum / rawFrameCount;
            dcProbeStats[probe].mean = dcProbeStats[probe].sum / rawFrameCount;

            acProbeStats[probe].ss = 0;
            dcProbeStats[probe].ss = 0;

            for(size_t frame = 0; frame < rawFrameCount; ++frame){

                if(probe < 16){
                    float acdiff = 0.195f * (rawFrames1[frame].ac[probe     ] - 32768) / 1000.0f - acProbeStats[probe].mean;
                    float dcdiff = -19.23 * (rawFrames1[frame].dc[probe     ] - 512) / 1000.0f - dcProbeStats[probe].mean;
                    acProbeStats[probe].ss += acdiff * acdiff; 
                    dcProbeStats[probe].ss += dcdiff * dcdiff;
                }else{
                    float acdiff = 0.195f * (rawFrames2[frame].ac[probe - 16] - 32768) / 1000.0f - acProbeStats[probe].mean;
                    float dcdiff = -19.23 * (rawFrames2[frame].dc[probe - 16] - 512) / 1000.0f - dcProbeStats[probe].mean;
                    acProbeStats[probe].ss += acdiff * acdiff; 
                    dcProbeStats[probe].ss += dcdiff * dcdiff;
                }
                
            }

            acProbeStats[probe].variance = acProbeStats[probe].ss / (rawFrameCount - 1);  // use population (N) or sample (n-1) deviation?
            acProbeStats[probe].deviation = sqrt(acProbeStats[probe].variance);

            dcProbeStats[probe].variance = dcProbeStats[probe].ss / (rawFrameCount - 1);  // use population (N) or sample (n-1) deviation?
            dcProbeStats[probe].deviation = sqrt(dcProbeStats[probe].variance);

        }
*/