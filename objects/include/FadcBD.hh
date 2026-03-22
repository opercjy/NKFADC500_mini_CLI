#ifndef FADCBD_HH
#define FADCBD_HH

#include "TObject.h"
#include <iostream>

class FadcBD : public TObject {
public:
    FadcBD();
    virtual ~FadcBD();

    void SetMID(int mid) { fMID = mid; }
    int  GetMID() const { return fMID; }

    // FADC500 Mini는 보드당 4채널 고정
    int  NCHANNEL() const { return 4; }

    // 4채널 독립 제어 파라미터 Setters & Getters
    void SetTHR(int ch, int val)    { if(ch>=0 && ch<4) fTHR[ch] = val; }
    int  GetTHR(int ch) const       { return (ch>=0 && ch<4) ? fTHR[ch] : 0; }

    void SetDACOFF(int ch, int val) { if(ch>=0 && ch<4) fDACOFF[ch] = val; }
    int  GetDACOFF(int ch) const    { return (ch>=0 && ch<4) ? fDACOFF[ch] : 0; }

    void SetPOL(int ch, int val)    { if(ch>=0 && ch<4) fPOL[ch] = val; }
    int  GetPOL(int ch) const       { return (ch>=0 && ch<4) ? fPOL[ch] : 0; }

    void SetDLY(int ch, int val)    { if(ch>=0 && ch<4) fDLY[ch] = val; }
    int  GetDLY(int ch) const       { return (ch>=0 && ch<4) ? fDLY[ch] : 0; }

private:
    int fMID;          // Module ID
    int fTHR[4];       // Threshold
    int fDACOFF[4];    // DAC Offset (Baseline)
    int fPOL[4];       // Polarity (0: Negative, 1: Positive)
    int fDLY[4];       // Delay

    ClassDef(FadcBD, 1)
};

#endif
