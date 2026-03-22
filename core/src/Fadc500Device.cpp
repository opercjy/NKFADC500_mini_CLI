#include "Fadc500Device.hh"
#include "ELog.hh"

extern "C" {
    #include "usb3com.h"
    #include "NoticeNKFADC500.h"
}
#include <unistd.h>

Fadc500Device::Fadc500Device(int sid) : fSid(sid) {
    USB3Init(0);
    int status = NKFADC500open(fSid, 0); 
    if (status < 0) {
        ELog::Print(ELog::FATAL, Form("Cannot open FADC500 Mini (MID: %d). Please check power and USB connection!", fSid));
    }
}

Fadc500Device::~Fadc500Device() {
    NKFADC500close(fSid);
    USB3Exit(0);
}

void Fadc500Device::Initialize(FadcBD* bdConfig) {
    ELog::Print(ELog::INFO, Form("Initializing FADC500 Mini (MID: %d) with Custom Settings...", fSid));
    
    // 💡 [핵심 패치] 벤더 함수의 무한루프에 갇히기 전에, 파이프라인의 쓰레기를 먼저 완전 소각합니다!
    ClearAndFlushUSB();
    
    NKFADC500write_DSR(fSid, bdConfig->GetSAMPLING());
    NKFADC500resetTIMER(fSid);  
    NKFADC500reset(fSid);
    NKFADC500_ADCALIGN_500(fSid);
    NKFADC500_ADCALIGN_DRAM(fSid);
    
    NKFADC500write_PTRIG(fSid, bdConfig->GetPTRIG());
    NKFADC500write_RL(fSid, bdConfig->GetRL()); 
    NKFADC500write_DRAMON(fSid, 1);

    for (int ch = 0; ch < bdConfig->NCHANNEL(); ch++) {
        int cid = ch + 1; 
        NKFADC500write_CW(fSid, cid, bdConfig->GetCW(ch));
        NKFADC500write_DACOFF(fSid, cid, bdConfig->GetDACOFF(ch));
    }

    for (int ch = 0; ch < bdConfig->NCHANNEL(); ch++) {
        NKFADC500measure_PED(fSid, ch + 1);
    }

    for (int ch = 0; ch < bdConfig->NCHANNEL(); ch++) {
        int cid = ch + 1;
        unsigned long apply_dly = bdConfig->GetCW(ch) + bdConfig->GetDLY(ch);
        
        NKFADC500write_DLY(fSid, cid, apply_dly);
        NKFADC500write_THR(fSid, cid, bdConfig->GetTHR(ch));
        NKFADC500write_POL(fSid, cid, bdConfig->GetPOL(ch));
        NKFADC500write_PSW(fSid, cid, bdConfig->GetPSW(ch));
        NKFADC500write_AMODE(fSid, cid, bdConfig->GetAMODE(ch));
        NKFADC500write_PCT(fSid, cid, bdConfig->GetPCT(ch));
        NKFADC500write_PCI(fSid, cid, bdConfig->GetPCI(ch));
        NKFADC500write_PWT(fSid, cid, bdConfig->GetPWT(ch));
        NKFADC500write_DT(fSid, cid, bdConfig->GetDT(ch));
        NKFADC500write_TM(fSid, cid, bdConfig->GetTMODE(ch));
    }
    
    NKFADC500write_TLT(fSid, bdConfig->GetTLT());
    NKFADC500write_PSCALE(fSid, bdConfig->GetPRESCALE());
    NKFADC500write_TRIGENABLE(fSid, bdConfig->GetTRIGEN());

    NKFADC500reset(fSid);
    ELog::Print(ELog::INFO, "Hardware initialization complete.");
}

void Fadc500Device::ClearAndFlushUSB() {
    NKFADC500stop(fSid);
    
    unsigned int raw_bcount = NKFADC500read_BCOUNT(fSid);
    
    if (raw_bcount != 0xFFFFFFFF) {
        unsigned int bcount_kb = raw_bcount & 0x0000FFFF;
        
        if (bcount_kb > 0) {
            ELog::Print(ELog::WARNING, Form("[HW INIT] Stale data found: %u KB. Safe draining...", bcount_kb));
            
            const unsigned int chunk_kb = 1024;
            unsigned char* safe_dummy = new unsigned char[chunk_kb * 1024];
            
            while (bcount_kb > 0) {
                unsigned int read_kb = (bcount_kb > chunk_kb) ? chunk_kb : bcount_kb;
                NKFADC500read_DATA(fSid, read_kb, (char*)safe_dummy);
                
                raw_bcount = NKFADC500read_BCOUNT(fSid);
                if (raw_bcount == 0xFFFFFFFF) break;
                
                unsigned int next_bcount = raw_bcount & 0x0000FFFF;
                if (next_bcount >= bcount_kb) break; // 무한루프 방어
                bcount_kb = next_bcount;
            }
            delete[] safe_dummy;
        }
    }
    
    NKFADC500resetTIMER(fSid);
    NKFADC500reset(fSid);
}

void Fadc500Device::StartDAQ() { 
    ClearAndFlushUSB(); 
    NKFADC500start(fSid); 
}

void Fadc500Device::StopDAQ() { 
    NKFADC500stop(fSid); 
    ClearAndFlushUSB(); 
}

unsigned int Fadc500Device::ReadBCOUNT() {
    return NKFADC500read_BCOUNT(fSid);
}

void Fadc500Device::ReadDATA(unsigned int bcount_kb, unsigned char* dest) {
    if (bcount_kb > 0) {
        NKFADC500read_DATA(fSid, bcount_kb, (char*)dest);
    }
}