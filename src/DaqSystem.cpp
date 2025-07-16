#include "DaqSystem.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <unistd.h>
#include <iomanip>
#include <map>
#include <algorithm>
#include <atomic>

// ROOT 헤더
#include "TSystem.h"
#include "TTimeStamp.h"
#include "TFile.h"
#include "TTree.h"

// 원시 데이터 패킷을 파싱하는 함수
bool ParsePacket(const char* buffer, EventData& event) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(buffer);
    event.data_length = p[0] | (p[4] << 8) | (p[8] << 16) | (p[12] << 24);

    if (event.data_length < 32 || event.data_length > 16384) return false;
    
    event.run_number         = p[16] | (p[20] << 8);
    event.trigger_type       = p[24] & 0x0F;
    event.trigger_number     = p[28] | (p[32] << 8) | (p[36] << 16) | (p[40] << 24);
    
    uint64_t fine_ttime      = p[44];
    uint64_t coarse_ttime    = p[48] | (p[52] << 8) | (p[56] << 16);
    event.ttime              = fine_ttime * 8 + coarse_ttime * 1000;

    event.mid                = p[60];
    event.local_tnum         = p[68] | (p[72] << 8) | (p[76] << 16) | (p[80] << 24);
    event.trigger_pattern    = p[84] | (p[88] << 8) | (p[92] << 16) | (p[96] << 24);

    uint64_t fine_ltime      = p[100];
    uint64_t coarse_ltime    = (uint64_t)p[104] | ((uint64_t)p[108] << 8)  | ((uint64_t)p[112] << 16) |
                               ((uint64_t)p[116] << 24) | ((uint64_t)p[120] << 32) | ((uint64_t)p[124] << 40);
    event.ltime              = fine_ltime * 8 + coarse_ltime * 1000;

    int num_samples = (event.data_length * 4 - 128) / 8;
    if (num_samples < 0) return false;

    event.clear();
    event.waveform1.reserve(num_samples);
    event.waveform2.reserve(num_samples);
    event.waveform3.reserve(num_samples);
    event.waveform4.reserve(num_samples);

    for (int i = 0; i < num_samples; ++i) {
        const uint8_t* sample_ptr = &p[128 + i * 8];
        event.waveform1.push_back(sample_ptr[0] | (sample_ptr[4] << 8));
        event.waveform2.push_back(sample_ptr[1] | (sample_ptr[5] << 8));
        event.waveform3.push_back(sample_ptr[2] | (sample_ptr[6] << 8));
        event.waveform4.push_back(sample_ptr[3] | (sample_ptr[7] << 8));
    }
    return true;
}

DaqSystem::DaqSystem() : m_usb(new usb3comroot()), m_fadc(new NKNKFADC500()), m_is_running(false) {}

DaqSystem::~DaqSystem() {}

bool DaqSystem::loadConfig(const std::string& config_path) {
    std::ifstream config_file(config_path);
    if (!config_file.is_open()) {
        std::cerr << "Error: Cannot open config file " << config_path << std::endl;
        return false;
    }
    std::string line;
    while (std::getline(config_file, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t delimiter_pos = line.find('=');
        if (delimiter_pos == std::string::npos) continue;
        std::string key = line.substr(0, delimiter_pos);
        std::string value = line.substr(delimiter_pos + 1);
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);
        
        if (key == "sid") m_settings.sid = std::stoi(value);
        if (key == "sampling_rate") m_settings.sr = std::stoul(value);
        if (key == "recording_length") m_settings.rl = std::stoul(value);
        if (key == "coincidence_width") m_settings.cw = std::stoul(value);
        if (key == "trigger_lookup_table") m_settings.tlt = std::stoul(value);
        if (key == "pedestal_trigger_interval") m_settings.ptrig_interval = std::stoul(value);
        if (key == "prescale") m_settings.pscale = std::stoul(value);
        if (key == "adc_offset") m_settings.offset = std::stoul(value);
        if (key == "waveform_delay") m_settings.dly = std::stoul(value);
        if (key == "threshold") m_settings.thr = std::stoul(value);
        if (key == "polarity") m_settings.pol = std::stoul(value);
        if (key == "peak_sum_width") m_settings.psw = std::stoul(value);
        if (key == "adc_mode") m_settings.amode = std::stoul(value);
        if (key == "pulse_count_threshold") m_settings.pct = std::stoul(value);
        if (key == "pulse_count_interval") m_settings.pci = std::stoul(value);
        if (key == "pulse_width_threshold") m_settings.pwt = std::stoul(value);
        if (key == "trigger_deadtime") m_settings.dt = std::stoul(value);
        if (key == "trigger_mode") m_settings.tmode = std::stoul(value);
        if (key == "trigger_enable") m_settings.trig_enable = std::stoul(value);
    }
    return true;
}

bool DaqSystem::initialize() {
    gSystem->Load("libusb3comroot.so");
    gSystem->Load("libNoticeNKFADC500ROOT.so");
    
    m_usb->USB3Init(0);
    m_fadc->NKFADC500open(m_settings.sid, 0);

    m_fadc->NKFADC500write_DSR(m_settings.sid, m_settings.sr);
    m_fadc->NKFADC500resetTIMER(m_settings.sid);
    m_fadc->NKFADC500reset(m_settings.sid);
    m_fadc->NKFADC500_ADCALIGN_500(m_settings.sid);
    m_fadc->NKFADC500_ADCALIGN_DRAM(m_settings.sid);
    m_fadc->NKFADC500write_PTRIG(m_settings.sid, m_settings.ptrig_interval);
    m_fadc->NKFADC500write_RL(m_settings.sid, m_settings.rl);
    m_fadc->NKFADC500write_DRAMON(m_settings.sid, 1);
    m_fadc->NKFADC500write_TLT(m_settings.sid, m_settings.tlt);
    m_fadc->NKFADC500write_PSCALE(m_settings.sid, m_settings.pscale);
    m_fadc->NKFADC500write_TRIGENABLE(m_settings.sid, m_settings.trig_enable);

    for (int ch = 1; ch <= 4; ++ch) {
        m_fadc->NKFADC500write_CW(m_settings.sid, ch, m_settings.cw);
        m_fadc->NKFADC500write_DACOFF(m_settings.sid, ch, m_settings.offset);
        m_fadc->NKFADC500measure_PED(m_settings.sid, ch);
        m_fadc->NKFADC500write_DLY(m_settings.sid, ch, m_settings.cw + m_settings.dly);
        m_fadc->NKFADC500write_THR(m_settings.sid, ch, m_settings.thr);
        m_fadc->NKFADC500write_POL(m_settings.sid, ch, m_settings.pol);
        m_fadc->NKFADC500write_PSW(m_settings.sid, ch, m_settings.psw);
        m_fadc->NKFADC500write_AMODE(m_settings.sid, ch, m_settings.amode);
        m_fadc->NKFADC500write_PCT(m_settings.sid, ch, m_settings.pct);
        m_fadc->NKFADC500write_PCI(m_settings.sid, ch, m_settings.pci);
        m_fadc->NKFADC500write_PWT(m_settings.sid, ch, m_settings.pwt);
        m_fadc->NKFADC500write_DT(m_settings.sid, ch, m_settings.dt);
        m_fadc->NKFADC500write_TM(m_settings.sid, ch, m_settings.tmode);
    }
    
    printSettingsSummary();
    return true;
}

void DaqSystem::run(int n_events, const std::string& outfile_base) {
    m_is_running = true;

    std::string root_filename = outfile_base + ".root";
    std::unique_ptr<TFile> outfile(TFile::Open(root_filename.c_str(), "RECREATE"));
    if (!outfile || outfile->IsZombie()) {
        std::cerr << "Error: Cannot create output file '" << root_filename << "'" << std::endl;
        return;
    }
    
    // --- 1. 실행 정보 저장을 위한 TTree 생성 ---
    TTree* run_info_tree = new TTree("run_info", "Run Information");
    run_info_tree->Branch("sid", &m_settings.sid, "sid/I");
    run_info_tree->Branch("sampling_rate", &m_settings.sr, "sampling_rate/l");
    run_info_tree->Branch("recording_length", &m_settings.rl, "recording_length/l");
    run_info_tree->Branch("coincidence_width", &m_settings.cw, "coincidence_width/l");
    run_info_tree->Branch("threshold", &m_settings.thr, "threshold/l");
    run_info_tree->Branch("polarity", &m_settings.pol, "polarity/l");
    run_info_tree->Branch("trigger_mode", &m_settings.tmode, "trigger_mode/l");
    run_info_tree->Branch("trigger_enable", &m_settings.trig_enable, "trigger_enable/l");
    run_info_tree->Branch("trigger_lookup_table", &m_settings.tlt, "trigger_lookup_table/l");
    run_info_tree->Fill(); // 설정값으로 한 번만 채움

    // --- 2. 이벤트 데이터 저장을 위한 TTree 생성 ---
    TTree* event_tree = new TTree("fadc_tree", "FADC500 Waveform Data");
    EventData event;
    event_tree->Branch("event", &event.local_tnum, "event/i");
    event_tree->Branch("trigger_number", &event.trigger_number, "trigger_number/i");
    event_tree->Branch("trigger_type", &event.trigger_type, "trigger_type/b");
    event_tree->Branch("trigger_pattern", &event.trigger_pattern, "trigger_pattern/i");
    event_tree->Branch("ttime", &event.ttime, "ttime/l");
    event_tree->Branch("ltime", &event.ltime, "ltime/l");
    event_tree->Branch("waveform1", &event.waveform1);
    event_tree->Branch("waveform2", &event.waveform2);
    event_tree->Branch("waveform3", &event.waveform3);
    event_tree->Branch("waveform4", &event.waveform4);

    m_fadc->NKFADC500reset(m_settings.sid);
    m_fadc->NKFADC500start(m_settings.sid);

    std::cout << "Starting DAQ for " << n_events << " events. Data will be saved to '" << root_filename << "'" << std::endl;
    TTimeStamp start_time;
    std::cout << "DAQ started at: " << start_time.AsString("s") << std::endl;
    
    long evtn = 0;
    long last_printed_evtn = -1;
    std::vector<char> data_buffer(10 * 1024 * 1024);
    std::map<uint32_t, long> trigger_pattern_counts;
    std::map<uint8_t, long> trigger_type_counts;

    while (m_is_running && evtn < n_events) {
        unsigned long bcount = m_fadc->NKFADC500read_BCOUNT(m_settings.sid);
        if(bcount > 0) {
            long bytes_to_read = bcount * 1024;
            if(data_buffer.size() < bytes_to_read) data_buffer.resize(bytes_to_read);
            m_fadc->NKFADC500read_DATA(m_settings.sid, bcount, data_buffer.data());
            
            long processed_bytes = 0;
            while(processed_bytes < bytes_to_read && m_is_running && evtn < n_events) {
                const char* packet_ptr = data_buffer.data() + processed_bytes;
                if(ParsePacket(packet_ptr, event)) {
                    event_tree->Fill();
                    trigger_pattern_counts[event.trigger_pattern]++;
                    trigger_type_counts[event.trigger_type]++;
                    evtn++;
                    processed_bytes += event.data_length * 4;

                    if (evtn / 1000 > last_printed_evtn / 1000) {
                        TTimeStamp current_time;
                        std::cout << "Processing event: " << evtn << "... (" << current_time.AsString("s") << ")" << std::endl;
                        last_printed_evtn = evtn;
                    }
                } else {
                    break;
                }
            }
        }
        usleep(10000);
    }
    
    TTimeStamp end_time;
    double elapsed_time = end_time.GetSec() - start_time.GetSec() + (end_time.GetNanoSec() - start_time.GetNanoSec()) * 1e-9;
    double rate = (evtn > 0 && elapsed_time > 0) ? evtn / elapsed_time : 0;
    std::cout << "\n---- DAQ Summary ----" << std::endl;
    std::cout << "DAQ finished at: " << end_time.AsString("s") << std::endl;
    std::cout << "Total elapsed time: " << std::fixed << std::setprecision(2) << elapsed_time << " seconds" << std::endl;
    std::cout << "Total events collected: " << evtn << std::endl;
    std::cout << "Average trigger rate: " << std::fixed << std::setprecision(2) << rate << " Hz" << std::endl;

    // --- Trigger Type Statistics ---
    std::cout << "\n--- Trigger Type Statistics ---" << std::endl;
    for(const auto& pair : trigger_type_counts) {
        std::string type_name;
        switch(pair.first) {
            case 0: type_name = "TCB trigger"; break;
            case 1: type_name = "Pedestal trigger"; break;
            case 2: type_name = "Software trigger"; break;
            case 3: type_name = "External trigger"; break;
            default: type_name = "Unknown"; break;
        }
        std::cout << "  - " << std::left << std::setw(18) << type_name << ": "
                  << std::right << std::setw(8) << pair.second << " times ("
                  << std::fixed << std::setprecision(2) << (100.0 * pair.second / evtn) << "%)" << std::endl;
    }
    std::cout << "-------------------------------" << std::endl;


    // --- Trigger Pattern Statistics ---
    std::cout << "\n--- Trigger Pattern Statistics ---" << std::endl;
    std::vector<std::pair<uint32_t, long>> sorted_patterns;
    for (const auto& pair : trigger_pattern_counts) {
        sorted_patterns.push_back(pair);
    }
    std::sort(sorted_patterns.begin(), sorted_patterns.end(), [](const auto& a, const auto& b) {
        return a.second > b.second;
    });

    int patterns_to_show = 10;
    std::cout << "Top " << patterns_to_show << " most frequent trigger patterns:" << std::endl;
    for (int i = 0; i < patterns_to_show && i < sorted_patterns.size(); ++i) {
        const auto& pattern = sorted_patterns[i];
        std::cout << "  - Pattern " << std::setw(5) << pattern.first 
                  << " (0x" << std::hex << std::setw(4) << std::setfill('0') << pattern.first << std::dec << "): " 
                  << pattern.second << " times (" 
                  << std::fixed << std::setprecision(2) << (100.0 * pattern.second / evtn) << "%)" << std::endl;
    }
    if (trigger_pattern_counts.size() > patterns_to_show) {
        std::cout << "... and " << trigger_pattern_counts.size() - patterns_to_show << " other patterns." << std::endl;
    }
    std::cout << "--------------------------------" << std::endl;


    // --- 3. 파일에 TTree 저장 ---
    outfile->cd();
    run_info_tree->Write(); // 실행 정보 TTree 저장
    event_tree->Write();    // 이벤트 데이터 TTree 저장
    outfile->Close();

    std::cout << "Data successfully saved to " << root_filename << std::endl;
}

void DaqSystem::stop() {
    std::cout << "\nStop signal received. Finalizing DAQ..." << std::endl;
    m_is_running = false;
}

void DaqSystem::shutdown() {
    m_fadc->NKFADC500stop(m_settings.sid);
    m_fadc->NKFADC500close(m_settings.sid);
    m_usb->USB3Exit(0);
    std::cout << "DAQ System shut down properly." << std::endl;
}

void DaqSystem::printSettingsSummary() {
    std::cout << "---- DAQ Settings Summary ----" << std::endl;
    std::cout << " * SID: " << m_settings.sid << std::endl;
    std::cout << " * Recording Length (rl=" << m_settings.rl << "): " << 128 * m_settings.rl << " ns" << std::endl;
    std::cout << " * Coincidence Width (cw): " << m_settings.cw << " ns" << std::endl;
    std::cout << " * Threshold (thr): " << m_settings.thr << " (ADC counts)" << std::endl;
    std::cout << " * Polarity (pol): " << (m_settings.pol == 0 ? "Negative" : "Positive") << std::endl;
    std::cout << "-----------------------------" << std::endl << std::endl;
}
