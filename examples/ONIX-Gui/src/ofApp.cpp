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
    fmc1->setPortVoltage(6.7);
    //fmc1->loadConfig("default");

    ONI::Device::FmcDevice* fmc2 = (ONI::Device::FmcDevice*)oni.getDevice(2);
    //fmc2->loadConfig("default");
    
    oni.update();
    oni.printDeviceTable();

    ONI::Device::HeartBeatDevice* heartBeatDevice = (ONI::Device::HeartBeatDevice*)oni.getDevice(0); // for some reason we need to do this before a context restart
    heartBeatDevice->setFrequencyHz(1);
    //heartBeatDevice->loadConfig("default");

    oni.update();

    ONI::Device::Rhs2116Device* rhs2116Device1 = (ONI::Device::Rhs2116Device*)oni.getDevice(256);
    ONI::Device::Rhs2116Device* rhs2116Device2 = (ONI::Device::Rhs2116Device*)oni.getDevice(257);

    //rhs2116Device1->loadConfig("default");
    //rhs2116Device2->loadConfig("default"); 

    //fu::debug << rhs2116Device1->getFormat(true) << fu::endl;

    //rhs2116Device1->setDspCutOff(Rhs2116DspCutoff::Dsp308Hz);
    //rhs2116Device2->setDspCutOff(Rhs2116DspCutoff::Dsp308Hz);

    //rhs2116Device1->setAnalogHighCutoff(Rhs2116AnalogHighCutoff::High10000Hz);
    //rhs2116Device2->setAnalogHighCutoff(Rhs2116AnalogHighCutoff::High10000Hz);

    rhs2116Device1->readRegister(ONI::Register::Rhs2116::MAXDELTAS);

    ONI::Device::Rhs2116MultiDevice* multi = oni.getMultiDevice();
    multi->addDevice(rhs2116Device1);
    multi->addDevice(rhs2116Device2);

    //std::vector<size_t> t = {31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,6,7,5,4,3,2,1,0};
    //multi->setChannelMap(t);

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
    s2.numberOfStimuli = 40;

    ONI::Device::Rhs2116StimulusDevice * rhs2116StimDevice = oni.getStimulusDevice(); // make sure to call this after multi device is initialized with devices!

    rhs2116StimDevice->conformStepSize(s1);
    rhs2116StimDevice->conformStepSize(s2);

    rhs2116StimDevice->setProbeStimulus(s1, 0);
    rhs2116StimDevice->setProbeStimulus(s2, 1);

    std::vector<ONI::Rhs2116StimulusData> stimuli = rhs2116StimDevice->getProbeStimuliMapped();
    std::vector<ONI::Rhs2116StimulusData> lastStimuli = stimuli;

    ONI::Settings::Rhs2116StimulusStep stepSize = rhs2116StimDevice->conformStepSize(stimuli);

    for(size_t i = 0; i < stimuli.size(); ++i){
        if(stimuli[i].requestedAnodicAmplitudeMicroAmps != stimuli[i].actualAnodicAmplitudeMicroAmps){
            LOGINFO("Probe %i anodic uA will change from %0.3f to %0.3f: ", i, stimuli[i].requestedAnodicAmplitudeMicroAmps, stimuli[i].actualAnodicAmplitudeMicroAmps);
            LOGINFO("Probe %i cathod uA will change from %0.3f to %0.3f: ", i, stimuli[i].requestedCathodicAmplitudeMicroAmps, stimuli[i].actualCathodicAmplitudeMicroAmps);
        }
    }

    //rhs2116StimDevice->setProbStimuliMapped(stimuli);


    rhs2116StimDevice->setStimulusSequence(stimuli, stepSize);



    //rhs2116Device1->setAnalogLowCutoff(Rhs2116AnalogLowCutoff::Low100mHz);
    //rhs2116Device1->setAnalogLowCutoffRecovery(Rhs2116AnalogLowCutoff::Low250Hz);
    //rhs2116Device1->setAnalogHighCutoff(Rhs2116AnalogHighCutoff::High10000Hz);

    //rhs2116Device2->setAnalogLowCutoff(Rhs2116AnalogLowCutoff::Low100mHz);
    //rhs2116Device2->setAnalogLowCutoffRecovery(Rhs2116AnalogLowCutoff::Low250Hz);
    //rhs2116Device2->setAnalogHighCutoff(Rhs2116AnalogHighCutoff::High10000Hz);

    //rhs2116Device1->setEnabled(true);
    //rhs2116Device2->setEnabled(true);

    //rhs2116Device1->saveConfig("default");
    //rhs2116Device2->saveConfig("default");

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

    if(key == 'r'){
        oni.startRecording();
    }

    if(key == 'p'){
        oni.startPlayng();
    }

    if(key == 't'){
        ONI::Device::Rhs2116StimulusDevice * rhs2116StimDevice = oni.getStimulusDevice();
        rhs2116StimDevice->triggerStimulusSequence();
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