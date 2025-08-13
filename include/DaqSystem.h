#ifndef DAQSYSTEM_H
#define DAQSYSTEM_H

#include <string>
#include <memory>
#include <vector>
#include <cstdint>
#include <atomic>
#include "NoticeNKFADC500ROOT.h"
#include "usb3comroot.h"

// TTree에 저장할 데이터 구조체
struct EventData {
    uint32_t data_length;
    uint16_t run_number;
    uint8_t  trigger_type;
    uint32_t trigger_number;
    uint64_t ttime;
    uint8_t  mid;
    uint32_t local_tnum;
    uint32_t trigger_pattern;
    uint64_t ltime;
    std::vector<uint16_t> waveform1;
    std::vector<uint16_t> waveform2;
    std::vector<uint16_t> waveform3;
    std::vector<uint16_t> waveform4;

    void clear() {
        waveform1.clear();
        waveform2.clear();
        waveform3.clear();
        waveform4.clear();
    }
};

// FADC500 DAQ의 모든 기능을 캡슐화하는 클래스
class DaqSystem {
public:
    DaqSystem();
    ~DaqSystem();

    bool loadConfig(const std::string& config_path);
    bool initialize();
    void run(int n_events, int duration_sec, const std::string& outfile_base);
    void shutdown();
    void stop();

private:
    // DAQ 파라미터들을 저장할 구조체
    struct DaqSettings {
        int sid = 1;
        unsigned long rl = 8;
        unsigned long tlt = 0xFFFE;
        unsigned long ptrig_interval = 0;
        unsigned long pscale = 1;
        unsigned long sr = 1;
        unsigned long cw = 1000;
        unsigned long offset = 3500;
        unsigned long dly = 200;
        unsigned long thr = 50;
        unsigned long pol = 0;
        unsigned long psw = 2;
        unsigned long amode = 1;
        unsigned long pct = 1;
        unsigned long pci = 1000;
        unsigned long pwt = 100;
        unsigned long dt = 0;
        unsigned long tmode = 1;
        unsigned long trig_enable = 1;
    };

    DaqSettings m_settings;
    std::unique_ptr<usb3comroot> m_usb;
    std::unique_ptr<NKNKFADC500> m_fadc;
    
    std::atomic<bool> m_is_running;

    void printSettingsSummary();
};

#endif // DAQSYSTEM_H
