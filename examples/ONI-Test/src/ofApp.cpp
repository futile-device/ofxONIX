#include "ofApp.h"


#define ONI_LOG_FU 1
#include "ONIContext.h"

ONIContext oni;

fu::Timer frameTimer;

//std::vector<Rhs2116DataFrame> frames1;
//std::vector<Rhs2116DataFrame> frames2;





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

    FmcDevice* fmc = (FmcDevice*)oni.getDevice(1);

    //fmc->getLinkState();
    //fmc->setEnabled(true);
    //fmc->getPortVoltage();
    //fmc->setPortVoltage(6.6);

    fmc->loadConfig("default");
    

    //fu::debug << ofToDataPath(configPath + fmc->getName()) << fu::endl;

    oni.update();

    HeartBeatDevice* heartBeatDevice = (HeartBeatDevice*)oni.getDevice(0); // for some reason we need to do this before a context restart
    //heartBeatDevice->getFrequencyHz(true);
    //heartBeatDevice->setEnabled(true);
    //heartBeatDevice->setFrequencyHz(1);

    heartBeatDevice->loadConfig("default");
    //heartBeatDevice->saveConfig("default");

    oni.update(); // Rhs2116 needs two context rebuilds?
    //oni.setup();

    oni.printDeviceTable();

    //fmc = (FmcDevice*)oni.getDevice(1);
    //fmc->getLinkState();

    FmcDevice* fmc2 = (FmcDevice*)oni.getDevice(2);
    fmc2->loadConfig("default");

    oni.update();
    //fmc2->getLinkState();

    //fmc->saveConfig("default");
    //fmc2->saveConfig("default");

    Rhs2116Device* rhs2116Device1 = (Rhs2116Device*)oni.getDevice(256);
    Rhs2116Device* rhs2116Device2 = (Rhs2116Device*)oni.getDevice(257);

    rhs2116Device1->loadConfig("default");
    rhs2116Device2->loadConfig("default");

    //fu::debug << rhs2116Device1->getFormat(true) << fu::endl;

    //rhs2116Device1->setDspCutOff(Rhs2116DspCutoff::Dsp308Hz);
    //rhs2116Device2->setDspCutOff(Rhs2116DspCutoff::Dsp308Hz);

    //fu::debug << rhs2116Device1->getFormat(true) << fu::endl;

    ////// SEEMS LIKE I CAN'T CHANGE THE BIAS SETTINGS??!!!
    ////unsigned int bias1 = rhs2116Device1->readRegister(Rhs2116Device::BIAS);
    ////Rhs2116Device::Rhs2116Bias biasA;
    ////memcpy(&biasA, &bias1, sizeof(Rhs2116Device::Rhs2116Bias));

    ////fu::debug << "Dev1 ADC bias: " << biasA.adcBias << " mux: " << biasA.muxBias << " ==> " << bias1 << fu::endl;

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
    //oni.setContextOption(ONIContext::RESET, 1);

    //oni.startAcquisition();


    frameTimer.start<fu::millis>(20);
    
}

//getContextOption(BLOCKREADSIZE);
//getContextOption(BLOCKWRITESIZE);
//setContextOption(RESET, 1);
//std::vector< std::vector<float> > acProbeVoltages;
//std::vector< std::vector<float> > dcProbeVoltages;
//
//std::vector< std::vector<float> > probeTimeStamps;
//
//std::vector<ONIProbeStatistics> acProbeStats;
//std::vector<ONIProbeStatistics> dcProbeStats;

fu::Timer processTimer;
//--------------------------------------------------------------
void ofApp::update(){

    oni.update();

    //if(frameTimer.finished()){

    //    frameTimer.restart();

    //    Rhs2116Device* rhs2116Device1 = (Rhs2116Device*)oni.getDevice(256);
    //    Rhs2116Device* rhs2116Device2 = (Rhs2116Device*)oni.getDevice(257);

    //    frames1 = rhs2116Device1->getDrawDataFrames();
    //    frames2 = rhs2116Device2->getDrawDataFrames();

    //    std::sort(frames1.begin(), frames1.end(), Rhs2116Device::acquisition_clock_compare());
    //    std::sort(frames2.begin(), frames2.end(), Rhs2116Device::acquisition_clock_compare());

    //    size_t frameCount = frames1.size(); // should we check frames1 vs frames2?

    //    if(acProbeVoltages.size() == 0){
    //        acProbeStats.resize(32);
    //        dcProbeStats.resize(32);
    //        acProbeVoltages.resize(32);
    //        dcProbeVoltages.resize(32);
    //        probeTimeStamps.resize(32);
    //    }

    //    if(acProbeVoltages[0].size() != frameCount){
    //        for(size_t probe = 0; probe < 32; ++probe){
    //            acProbeVoltages[probe].resize(frameCount);
    //            dcProbeVoltages[probe].resize(frameCount);
    //            probeTimeStamps[probe].resize(frameCount);
    //        }
    //    }

    //    for(size_t probe = 0; probe < 32; ++probe){

    //        acProbeStats[probe].sum = 0;
    //        dcProbeStats[probe].sum = 0;

    //        for(size_t frame = 0; frame < frameCount; ++frame){

    //            if(probe < 16){
    //                probeTimeStamps[probe][frame] =  uint64_t((frames1[frame].deltaTime - frames1[0].deltaTime) / 1000);
    //                acProbeVoltages[probe][frame] = frames1[frame].ac_uV[probe     ]; //0.195f * (frames1[frame].ac[probe     ] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
    //                dcProbeVoltages[probe][frame] = frames1[frame].dc_mV[probe     ]; //-19.23 * (frames1[frame].dc[probe     ] - 512) / 1000.0f;   // -19.23 mV × (ADC result – 512) divide by 1000 for V?
    //            }else{
    //                probeTimeStamps[probe][frame] =  uint64_t((frames2[frame].deltaTime - frames2[0].deltaTime) / 1000);
    //                acProbeVoltages[probe][frame] = frames2[frame].ac_uV[probe - 16]; //0.195f * (frames2[frame].ac[probe - 16] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
    //                dcProbeVoltages[probe][frame] = frames2[frame].dc_mV[probe - 16];; //-19.23 * (frames2[frame].dc[probe - 16] - 512) / 1000.0f;   // -19.23 mV × (ADC result – 512) divide by 1000 for V?
    //            }

    //            acProbeStats[probe].sum += acProbeVoltages[probe][frame];
    //            dcProbeStats[probe].sum += dcProbeVoltages[probe][frame];

    //        }

    //        acProbeStats[probe].mean = acProbeStats[probe].sum / frameCount;
    //        dcProbeStats[probe].mean = dcProbeStats[probe].sum / frameCount;

    //        acProbeStats[probe].ss = 0;
    //        dcProbeStats[probe].ss = 0;

    //        for(size_t frame = 0; frame < frameCount; ++frame){
    //            float acdiff = acProbeVoltages[probe][frame] - acProbeStats[probe].mean;
    //            float dcdiff = dcProbeVoltages[probe][frame] - dcProbeStats[probe].mean;
    //            acProbeStats[probe].ss += acdiff * acdiff; 
    //            dcProbeStats[probe].ss += dcdiff * dcdiff;
    //        }

    //        acProbeStats[probe].variance = acProbeStats[probe].ss / (frameCount - 1);  // use population (N) or sample (n-1) deviation?
    //        acProbeStats[probe].deviation = sqrt(acProbeStats[probe].variance);

    //        dcProbeStats[probe].variance = dcProbeStats[probe].ss / (frameCount - 1);  // use population (N) or sample (n-1) deviation?
    //        dcProbeStats[probe].deviation = sqrt(dcProbeStats[probe].variance);

    //    }
    //    
    //    processTimer.fcount();
    //}

    ofDisableArbTex();
    fu::gui.begin();
    { 
        
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());
        fu::ImGuiConsole.gui();
        //ImPlot::ShowDemoWindow();
        ImGui::ShowDemoWindow();
        oni.gui();
        //bufferPlot();
        //probePlot();
        

    }
    fu::gui.end();
    ofEnableArbTex();

}


//--------------------------------------------------------------
void ofApp::bufferPlot(){

    //if(acProbeVoltages.size() == 0) return;

    //ImGui::Begin("Buffered");
    //ImGui::PushID("BufferPlot");

    //ImPlot::BeginPlot("Data Frames", ImVec2(-1,-1));
    //ImPlot::SetupAxes("mS","mV");
    //
    //size_t sampleCount = frames1.size();

    //for (int probe = 0; probe < 32; probe++) {

    //    //float * data = new float[sampleCount];
    //    //float * time = new float[sampleCount];

    //    //for(int i = 0; i < sampleCount; ++i){

    //    //    if(probe < 32){
    //    //        //time[i] = uint64_t(frames1[i].deltaTime / 1000);
    //    //        time[i] = uint64_t((frames1[i].deltaTime - frames1[0].deltaTime) / 1000);
    //    //        data[i] = 0.195f * (frames1[i].ac[probe] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
    //    //        //data[i] = -19.23 * (frames1[i].dc[probe] - 512) / 1000.0f; // -19.23 mV × (ADC result – 512) divide by 1000 for V?
    //    //        //		//dcdD = dcdD / 2 * 2; // clear LSB?
    //    //        //		//dcdL &= ~(1U << 1); // x &= ~(1U << n) clear LS n bits ??
    //    //    }else{
    //    //        //time[i] = uint64_t(frames2[i].deltaTime / 1000);
    //    //        time[i] = uint64_t((frames2[i].deltaTime - frames2[0].deltaTime) / 1000);
    //    //        data[i] = 0.195f * (frames2[i].ac[probe - 16] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
    //    //        //data[i] = -19.23 * (frames1[i].dc[probe] - 512) / 1000.0f; // -19.23 mV × (ADC result – 512) divide by 1000 for V?
    //    //        //		//dcdD = dcdD / 2 * 2; // clear LSB?
    //    //        //		//dcdL &= ~(1U << 1); // x &= ~(1U << n) clear LS n bits ??
    //    //    }
    //    //    
    //    //}

    //    ImGui::PushID(probe);
    //    ImPlot::SetNextLineStyle(ImPlot::GetColormapColor(probe));
    //   
    //    int offset = 0;

    //    //ImPlot::SetupAxesLimits(time[0], time[sampleCount - 1] - 1, -5.0f, 5.0f, ImGuiCond_Always);
    //    ImPlot::SetupAxesLimits(0, probeTimeStamps[probe][sampleCount - 1] - 1, -5.0f, 5.0f, ImGuiCond_Always);
    //    ImPlot::PlotLine("##probe", &probeTimeStamps[probe][0], &acProbeVoltages[probe][0], sampleCount, ImPlotLineFlags_None, offset);
    //   
    //    //ImPlot::SetupAxesLimits(0, sampleCount - 1, -6.0f, 6.0f, ImGuiCond_Always);
    //    //ImPlot::PlotLine("###probe", data, sampleCount, 1, 0, ImPlotLineFlags_None, offset);
    //    
    //    
    //    ImGui::PopID();

    //    //delete [] data;
    //    //delete [] time;
    //}
    //
    //ImPlot::EndPlot();
    //ImGui::PopID();
    //ImGui::End();

}

//--------------------------------------------------------------
static void Sparkline(const char* id, const float* values, int count, float min_v, float max_v, int offset, const ImVec4& col, const ImVec2& size) {
    ImPlot::PushStyleVar(ImPlotStyleVar_PlotPadding, ImVec2(0,0));
    if (ImPlot::BeginPlot(id, size, ImPlotFlags_CanvasOnly)){
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoDecorations, ImPlotAxisFlags_None);
        ImPlot::SetupAxesLimits(0, count - 1, min_v, max_v, ImGuiCond_Always);
        ImPlot::SetNextLineStyle(col);
        //ImPlot::SetNextFillStyle(col, 0.25);
        ImPlot::PlotLine(id, values, count, 1, 0, ImPlotLineFlags_None, offset); //ImPlotLineFlags_Shaded
        ImPlot::EndPlot();
    }
    ImPlot::PopStyleVar();
}

//--------------------------------------------------------------
void ofApp::probePlot(){
    //if(acProbeVoltages.size() == 0) return;
    //ImGui::Begin("Probes");
    //ImGui::PushID("Probe Plot");
    //static ImGuiTableFlags flags = ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV |
    //    ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable;
   
    //static bool anim = true;
    //static int offset = 0;

    //ImGui::BeginTable("##table", 3, flags, ImVec2(-1,0));
    //ImGui::TableSetupColumn("Electrode", ImGuiTableColumnFlags_WidthFixed, 75.0f);
    //ImGui::TableSetupColumn("Voltage", ImGuiTableColumnFlags_WidthFixed, 75.0f);
    //ImGui::TableSetupColumn("AC Signal");

    //ImGui::TableHeadersRow();
    //ImPlot::PushColormap(ImPlotColormap_Cool);

    //size_t sampleCount = frames1.size();

    //for (int probe = 0; probe < 32; probe++){

    //    ImGui::TableNextRow();

    //    //float *  data = new float[sampleCount];
    //    //
    //    //if(probe < 16){
    //    //    for(int i = 0; i < sampleCount; ++i){
    //    //        data[i] = 0.195f * (frames1[i].ac[probe] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
    //    //        //data[i] = -19.23 * (frames1[i].dc[probe] - 512) / 1000.0f; // -19.23 mV × (ADC result – 512) divide by 1000 for V?
    //    //        //		//dcdD = dcdD / 2 * 2; // clear LSB?
    //    //        //		//dcdL &= ~(1U << 1); // x &= ~(1U << n) clear LS n bits ??
    //    //    }
    //    //}else{
    //    //    for(int i = 0; i < sampleCount; ++i){
    //    //        data[i] = 0.195f * (frames2[i].ac[probe - 16] - 32768) / 1000.0f; // 0.195 uV × (ADC result – 32768) divide by 1000 for mV?
    //    //        //data[i] = -19.23 * (frames1[i].dc[probe] - 512) / 1000.0f; // -19.23 mV × (ADC result – 512) divide by 1000 for V?
    //    //        //		//dcdD = dcdD / 2 * 2; // clear LSB?
    //    //        //		//dcdL &= ~(1U << 1); // x &= ~(1U << n) clear LS n bits ??
    //    //    }
    //    //}
  
    //    ImGui::TableSetColumnIndex(0);
    //    ImGui::Text("Probe %d", probe);
    //    ImGui::TableSetColumnIndex(1);
    //    ImGui::Text("%.3f mV \n%.3f avg \n%.3f dev \n%i N", acProbeVoltages[probe][offset], acProbeStats[probe].mean, acProbeStats[probe].deviation, sampleCount);
    //    ImGui::TableSetColumnIndex(2);

    //    ImGui::PushID(probe);

    //    Sparkline("##spark", &acProbeVoltages[probe][0], sampleCount, -5.0f, 5.0f, offset, ImPlot::GetColormapColor(probe), ImVec2(-1, 120));
    //    
    //    ImGui::PopID();

    //    //delete [] data;
    //}

    //ImPlot::PopColormap();
    //ImGui::EndTable();
    //ImGui::PopID();
    //ImGui::End();

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
    if(key == 't'){
        Rhs2116Device* rhs2116Device1 = (Rhs2116Device*)oni.getDevice(256);
        //fu::debug << "RHD2116FrameTimer: " << rhs2116Device1->getFrameTimerAvg() << fu::endl;
        //oni.frameCtxTimer.stop();
        //fu::debug << "ContextFrameTimer: " << oni.frameCtxTimer.avg<fu::micros>() << fu::endl;
        //oni.frameCtxTimer.start();
        //processTimer.stop();
        //fu::debug << "Process Timer: " << processTimer.avg<fu::micros>() << " RHD2116FrameTimer: " << rhs2116Device1->getFrameTimerAvg() << fu::endl;
        //processTimer.start();
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