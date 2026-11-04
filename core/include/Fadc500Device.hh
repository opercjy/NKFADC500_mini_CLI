#ifndef FADC500DEVICE_HH
#define FADC500DEVICE_HH

#include "FadcBD.hh"

class Fadc500Device {
private:
    int fSid;
    bool fIsOpen;

    bool EnableDramWithTimeout(bool enable);
    void SafeAlignADC500();
    bool SafeAlignDRAM();
    void VerifyRegistersAndLogPedestals(FadcBD* bdConfig);

public:
    explicit Fadc500Device(int sid);
    ~Fadc500Device();

    Fadc500Device(const Fadc500Device&) = delete;
    Fadc500Device& operator=(const Fadc500Device&) = delete;

    void Initialize(FadcBD* bdConfig);
    void StartDAQ();
    void StopAcquisitionOnly();
    void StopDAQ();

    // Drain residual DRAM/FIFO bytes and reset timers before/after DAQ runs
    void ClearAndFlushUSB();

    unsigned int ReadBCOUNT();
    void ReadDATA(unsigned int bcount_kb, unsigned char* dest);
};

#endif
