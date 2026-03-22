#ifndef FADC500DEVICE_HH
#define FADC500DEVICE_HH

#include "FadcBD.hh"

class Fadc500Device {
private:
    int fSid;

public:
    Fadc500Device(int sid);
    ~Fadc500Device();

    void Initialize(FadcBD* bdConfig);
    void StartDAQ();
    void StopDAQ();
    
    // 💡 [신규 추가] 좀비 상태 해제 및 하드웨어 완전 세척
    void ClearAndFlushUSB(); 

    unsigned int ReadBCOUNT();
    void ReadDATA(unsigned int bcount_kb, unsigned char* dest);
};

#endif