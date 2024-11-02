#include "ofApp.h"


#define ONI_LOG_FU 1
#include "ONIContext.h"
#include "ONIInterface.h"

ONIContext oni;


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

    FmcDevice* fmc1 = (FmcDevice*)oni.getDevice(1);
    fmc1->setPortVoltage(6.6);
    //fmc1->loadConfig("default");

    FmcDevice* fmc2 = (FmcDevice*)oni.getDevice(2);
    //fmc2->loadConfig("default");
    
    oni.update();

    HeartBeatDevice* heartBeatDevice = (HeartBeatDevice*)oni.getDevice(0); // for some reason we need to do this before a context restart
    heartBeatDevice->setFrequencyHz(1);
    //heartBeatDevice->loadConfig("default");

    oni.update();

    Rhs2116Device* rhs2116Device1 = (Rhs2116Device*)oni.getDevice(256);
    Rhs2116Device* rhs2116Device2 = (Rhs2116Device*)oni.getDevice(257);

    //rhs2116Device1->loadConfig("default");
    //rhs2116Device2->loadConfig("default"); 

    //fu::debug << rhs2116Device1->getFormat(true) << fu::endl;

    rhs2116Device1->setDspCutOff(Rhs2116DspCutoff::Dsp308Hz);
    rhs2116Device2->setDspCutOff(Rhs2116DspCutoff::Dsp308Hz);

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
        oni.getContextOption(ONIContext::BLOCKREADSIZE);
        oni.setContextOption(ONIContext::RESET, 1);
        //oni.getContextOption(ONIContext::BLOCKREADSIZE);
        oni.setContextOption(ONIContext::BLOCKREADSIZE, 2048);
        oni.setContextOption(ONIContext::BLOCKWRITESIZE, 2048);
        //oni.setup();
        ofSleepMillis(500);
        oni.startAcquisition();
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