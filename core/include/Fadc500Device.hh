#ifndef FADC500DEVICE_HH
#define FADC500DEVICE_HH

#include "FadcBD.hh"

class Fadc500Device {
public:
    Fadc500Device(int sid);
    ~Fadc500Device();

    void Initialize(FadcBD* bdConfig);
    void StartDAQ();
    void StopDAQ();

    // 제조사 공식 API로 대체
    unsigned int ReadBCOUNT();
    void ReadDATA(unsigned int bcount_kb, unsigned char* dest);

private:
    int fSid;
};

#endif