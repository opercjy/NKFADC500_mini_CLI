#include "Fadc500Device.hh"
#include "ELog.hh"

extern "C" {
    #include "usb3com.h"
    #include "NoticeNKFADC500.h"
}
#include <unistd.h>

Fadc500Device::Fadc500Device(int sid) : fSid(sid) {
    USB3Init(0);
    NKFADC500open(fSid, 0); 
}

Fadc500Device::~Fadc500Device() {
    NKFADC500close(fSid);
    USB3Exit(0);
}

void Fadc500Device::Initialize(FadcBD* bdConfig) {
    ELog::Print(ELog::INFO, Form("Initializing FADC500 Mini (MID: %d) with Vendor-tested sequence...", fSid));
    
    unsigned long sr = 1;                 
    unsigned long ptrig_interval = 1;     // 💡 강제 1ms 트리거 유지
    unsigned long rl = 8;                 // 1us 레코드 길이 (이벤트 당 약 4KB)
    unsigned long cw = 1000;              
    unsigned long psw = 2;                
    unsigned long amode = 0;              
    unsigned long pct = 1;                
    unsigned long pci = 1000;             
    unsigned long pwt = 100;              
    unsigned long dt = 0;                 
    unsigned long tmode = 1;              
    unsigned long tlt = 0xFFFE;           
    unsigned long pscale = 1;             
    unsigned long trig_enable = 0x2;      // 💡 Pedestal 트리거만 허용 (0x2)

    NKFADC500write_DSR(fSid, sr);
    NKFADC500resetTIMER(fSid);  
    NKFADC500reset(fSid);
    NKFADC500_ADCALIGN_500(fSid);
    NKFADC500_ADCALIGN_DRAM(fSid);
    
    NKFADC500write_PTRIG(fSid, ptrig_interval);
    NKFADC500write_RL(fSid, rl); 
    NKFADC500write_DRAMON(fSid, 1);

    for (int ch = 0; ch < bdConfig->NCHANNEL(); ch++) {
        int cid = ch + 1; 
        NKFADC500write_CW(fSid, cid, cw);
        NKFADC500write_DACOFF(fSid, cid, bdConfig->GetDACOFF(ch));
    }

    for (int ch = 0; ch < bdConfig->NCHANNEL(); ch++) {
        NKFADC500measure_PED(fSid, ch + 1);
    }

    for (int ch = 0; ch < bdConfig->NCHANNEL(); ch++) {
        int cid = ch + 1;
        unsigned long apply_dly = cw + bdConfig->GetDLY(ch);
        
        NKFADC500write_DLY(fSid, cid, apply_dly);
        NKFADC500write_THR(fSid, cid, bdConfig->GetTHR(ch));
        NKFADC500write_POL(fSid, cid, bdConfig->GetPOL(ch));
        
        NKFADC500write_PSW(fSid, cid, psw);
        NKFADC500write_AMODE(fSid, cid, amode);
        NKFADC500write_PCT(fSid, cid, pct);
        NKFADC500write_PCI(fSid, cid, pci);
        NKFADC500write_PWT(fSid, cid, pwt);
        NKFADC500write_DT(fSid, cid, dt);
        NKFADC500write_TM(fSid, cid, tmode);

        ELog::Print(ELog::INFO, Form("  - Ch%d : THR=%d, DACOFF=%d, POL=%d, DLY=%d (Applied=%lu)", 
                    ch, bdConfig->GetTHR(ch), bdConfig->GetDACOFF(ch), bdConfig->GetPOL(ch), bdConfig->GetDLY(ch), apply_dly));
    }
    
    NKFADC500write_TLT(fSid, tlt);
    NKFADC500write_PSCALE(fSid, pscale);
    NKFADC500write_TRIGENABLE(fSid, trig_enable);

    ELog::Print(ELog::INFO, "Hardware initialization complete.");
}

void Fadc500Device::StartDAQ() { 
    NKFADC500reset(fSid); 
    NKFADC500start(fSid); 
}

void Fadc500Device::StopDAQ() { 
    NKFADC500stop(fSid); 
}

unsigned int Fadc500Device::ReadBCOUNT() {
    return NKFADC500read_BCOUNT(fSid);
}

void Fadc500Device::ReadDATA(unsigned int bcount_kb, unsigned char* dest) {
    if (bcount_kb > 0) {
        NKFADC500read_DATA(fSid, bcount_kb, (char*)dest);
    }
}