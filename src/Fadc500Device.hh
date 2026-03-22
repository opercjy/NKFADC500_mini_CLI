#ifndef FADC500DEVICE_HH
#define FADC500DEVICE_HH

#include "FadcBD.hh"
#include <libusb-1.0/libusb.h>

class Fadc500Device {
public:
    Fadc500Device(int sid);
    ~Fadc500Device();

    void Initialize(FadcBD* bdConfig);
    void StartDAQ();
    void StopDAQ();

    // [핵심 최적화] Malloc 없는 초고속 BCOUNT 폴링 & 데이터 리드
    unsigned int FastReadBCOUNT();
    void FastReadData(unsigned int bcount, unsigned char* dest);

private:
    // 벤더의 프리징(sleep) 결함을 우회하는 Fast Write 내부 함수
    void FastWriteDACOFF(int ch, unsigned long data);

    int fSid;
    libusb_context* fUsbContext;
    libusb_device_handle* fDevHandle; // libusb 다이렉트 접근 핸들
};

#endif
