#include "Fadc500Device.hh"
#include "ELog.hh"

extern "C" {
    #include "usb3com.h"
    #include "NoticeNKFADC500.h"
}
#include <unistd.h>

Fadc500Device::Fadc500Device(int sid) : fSid(sid) {
    USB3Init(0);
    
    // 💡 [복구된 버그 픽스] 장비 오픈을 시도하고, 실패하면 무한루프에 빠지기 전에 즉시 프로그램 종료!
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

        ELog::Print(ELog::INFO, Form("  - Ch%d : THR=%d, DACOFF=%d, POL=%d, DLY(Apply)=%lu", 
                    ch, bdConfig->GetTHR(ch), bdConfig->GetDACOFF(ch), bdConfig->GetPOL(ch), apply_dly));
    }
    
    NKFADC500write_TLT(fSid, bdConfig->GetTLT());
    NKFADC500write_PSCALE(fSid, bdConfig->GetPRESCALE());
    NKFADC500write_TRIGENABLE(fSid, bdConfig->GetTRIGEN());

    // 💡 [유지됨] 500개 버퍼 제한 해제 로직
    NKFADC500reset(fSid);

    ELog::Print(ELog::INFO, "Hardware initialization complete.");
}

void Fadc500Device::StartDAQ() { 
    // 보드 버퍼를 싹 비우고, 새 마음 새 뜻으로 시작!
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
        
        // 💡 [유지됨] 보드에게 "내가 데이터 1뭉치 잘 가져갔어. 다음꺼 또 모아서 줘!" 라고 알려줌
        NKFADC500reset(fSid); 
        NKFADC500start(fSid); 
    }
}