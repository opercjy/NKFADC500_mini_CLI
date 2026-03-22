#ifndef FADCBD_HH
#define FADCBD_HH

#include "TObject.h"

class FadcBD : public TObject {
public:
    FadcBD();
    virtual ~FadcBD();

    // Board Global
    void SetMID(int mid) { fMID = mid; }
    int  GetMID() const  { return fMID; }

    void SetSAMPLING(int val) { fSAMPLING = val; }
    int  GetSAMPLING() const  { return fSAMPLING; }

    void SetRL(int val)       { fRL = val; }
    int  GetRL() const        { return fRL; }

    void SetPRESCALE(int val) { fPRESCALE = val; }
    int  GetPRESCALE() const  { return fPRESCALE; }

    void SetTLT(int val)      { fTLT = val; }
    int  GetTLT() const       { return fTLT; }

    void SetTRIGEN(int val)   { fTRIGEN = val; }
    int  GetTRIGEN() const    { return fTRIGEN; }

    void SetPTRIG(int val)    { fPTRIG = val; }
    int  GetPTRIG() const     { return fPTRIG; }

    int  NCHANNEL() const     { return 4; }

    // Channel Specific
    void SetTHR(int ch, int val)    { if(ch>=0 && ch<4) fTHR[ch] = val; }
    int  GetTHR(int ch) const       { return (ch>=0 && ch<4) ? fTHR[ch] : 0; }

    void SetDACOFF(int ch, int val) { if(ch>=0 && ch<4) fDACOFF[ch] = val; }
    int  GetDACOFF(int ch) const    { return (ch>=0 && ch<4) ? fDACOFF[ch] : 0; }

    void SetPOL(int ch, int val)    { if(ch>=0 && ch<4) fPOL[ch] = val; }
    int  GetPOL(int ch) const       { return (ch>=0 && ch<4) ? fPOL[ch] : 0; }

    void SetDLY(int ch, int val)    { if(ch>=0 && ch<4) fDLY[ch] = val; }
    int  GetDLY(int ch) const       { return (ch>=0 && ch<4) ? fDLY[ch] : 0; }

    void SetCW(int ch, int val)     { if(ch>=0 && ch<4) fCW[ch] = val; }
    int  GetCW(int ch) const        { return (ch>=0 && ch<4) ? fCW[ch] : 0; }

    void SetAMODE(int ch, int val)  { if(ch>=0 && ch<4) fAMODE[ch] = val; }
    int  GetAMODE(int ch) const     { return (ch>=0 && ch<4) ? fAMODE[ch] : 0; }

    void SetTMODE(int ch, int val)  { if(ch>=0 && ch<4) fTMODE[ch] = val; }
    int  GetTMODE(int ch) const     { return (ch>=0 && ch<4) ? fTMODE[ch] : 0; }

    void SetDT(int ch, int val)     { if(ch>=0 && ch<4) fDT[ch] = val; }
    int  GetDT(int ch) const        { return (ch>=0 && ch<4) ? fDT[ch] : 0; }

    void SetPSW(int ch, int val)    { if(ch>=0 && ch<4) fPSW[ch] = val; }
    int  GetPSW(int ch) const       { return (ch>=0 && ch<4) ? fPSW[ch] : 0; }

    void SetPCT(int ch, int val)    { if(ch>=0 && ch<4) fPCT[ch] = val; }
    int  GetPCT(int ch) const       { return (ch>=0 && ch<4) ? fPCT[ch] : 0; }

    void SetPCI(int ch, int val)    { if(ch>=0 && ch<4) fPCI[ch] = val; }
    int  GetPCI(int ch) const       { return (ch>=0 && ch<4) ? fPCI[ch] : 0; }

    void SetPWT(int ch, int val)    { if(ch>=0 && ch<4) fPWT[ch] = val; }
    int  GetPWT(int ch) const       { return (ch>=0 && ch<4) ? fPWT[ch] : 0; }

private:
    int fMID;          
    int fSAMPLING;
    int fRL;
    int fPRESCALE;
    int fTLT;
    int fTRIGEN;
    int fPTRIG;

    int fTHR[4];       
    int fDACOFF[4];    
    int fPOL[4];       
    int fDLY[4];       
    int fCW[4];
    int fAMODE[4];
    int fTMODE[4];
    int fDT[4];
    int fPSW[4];
    int fPCT[4];
    int fPCI[4];
    int fPWT[4];

    ClassDef(FadcBD, 2) // 버전 2로 업데이트
};

#endif