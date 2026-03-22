#include "Fadc500Device.hh"
#include "ELog.hh"

// 제조사 라이브러리 연동
extern "C" {
    #include "nkusb.h"
    #include "usb3com.h"
    #include "NoticeNKFADC500.h"
}
#include <unistd.h>
#include <iostream>

Fadc500Device::Fadc500Device(int sid) : fSid(sid), fUsbContext(nullptr), fDevHandle(nullptr) {
    USB3Init(&fUsbContext);
    NKFADC500open(fSid, fUsbContext);
    
    // [우회 기술] libusb 핸들을 가로채어 직접 제어할 준비를 합니다.
    fDevHandle = nkusb_get_device_handle(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, fSid);
    if (!fDevHandle) {
        ELog::Print(ELog::FATAL, "Failed to capture libusb device handle!");
    }
}

Fadc500Device::~Fadc500Device() {
    NKFADC500close(fSid);
    if (fUsbContext) USB3Exit(fUsbContext);
}

void Fadc500Device::FastWriteDACOFF(int ch, unsigned long data) {
    // 🚨 벤더의 sleep(1)을 제거하고 50ms 대기로 최적화
    unsigned long addr = 0x20000004 + (((ch - 1) & 0xFF) << 16);
    USB3Write(NKFADC500_VENDOR_ID, NKFADC500_PRODUCT_ID, fSid, addr, data);
    usleep(50000); 
}

void Fadc500Device::Initialize(FadcBD* bdConfig) {
    ELog::Print(ELog::INFO, Form("Initializing FADC500 Mini (MID: %d)...", fSid));
    NKFADC500reset(fSid);
    
    // 💡 초기화 시퀀스 (매뉴얼 필수 권장 사항)
    NKFADC500_ADCALIGN_500(fSid);
    NKFADC500_ADCALIGN_DRAM(fSid);

    for (int ch = 0; ch < bdConfig->NCHANNEL(); ch++) {
        int cid = ch + 1; // 1-based index for hardware
        NKFADC500write_THR(fSid, cid, bdConfig->GetTHR(ch));
        NKFADC500write_POL(fSid, cid, bdConfig->GetPOL(ch));
        NKFADC500write_DLY(fSid, cid, bdConfig->GetDLY(ch));
        
        // 커스텀 Fast 함수로 지연시간 최소화
        FastWriteDACOFF(cid, bdConfig->GetDACOFF(ch)); 
    }
    ELog::Print(ELog::INFO, "Hardware initialization complete.");
}

void Fadc500Device::StartDAQ() {
    NKFADC500start(fSid);
}

void Fadc500Device::StopDAQ() {
    NKFADC500stop(fSid);
}

unsigned int Fadc500Device::FastReadBCOUNT() {
    if (!fDevHandle) return 0;
    unsigned char cmd[8] = { 1, 0, 0, 0, 0x10, 0, 0, 0x20 | 0x80 }; // 레지스터 0x20000010
    unsigned char buf[4] = {0};
    int transferred;
    
    // 동적 할당 없이 직행 (Zero-Malloc)
    libusb_bulk_transfer(fDevHandle, USB3_SF_WRITE, cmd, 8, &transferred, 1000);
    libusb_bulk_transfer(fDevHandle, USB3_SF_READ, buf, 4, &transferred, 1000);
    
    return buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
}

void Fadc500Device::FastReadData(unsigned int bcount, unsigned char* dest) {
    if (!fDevHandle || bcount == 0) return;
    
    uint32_t count = bcount * 256; 
    unsigned char cmd[8] = { 
        (unsigned char)(count & 0xFF), (unsigned char)((count >> 8) & 0xFF), 
        (unsigned char)((count >> 16) & 0xFF), (unsigned char)((count >> 24) & 0xFF), 
        0, 0, 0, 0x40 | 0x80 // Data Buffer 레지스터
    }; 
                             
    int transferred;
    libusb_bulk_transfer(fDevHandle, USB3_SF_WRITE, cmd, 8, &transferred, 1000);
    
    // 16KB 청크 단위로 나누어 직접 수신 (OS 오버헤드 최소화)
    uint32_t total_bytes = count * 4;
    uint32_t read_bytes = 0;
    while (read_bytes < total_bytes) {
        uint32_t to_read = total_bytes - read_bytes;
        if (to_read > 16384) to_read = 16384; 
        libusb_bulk_transfer(fDevHandle, USB3_SF_READ, dest + read_bytes, to_read, &transferred, 2000);
        read_bytes += transferred;
    }
}
