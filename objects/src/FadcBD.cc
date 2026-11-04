#include "FadcBD.hh"

ClassImp(FadcBD)

FadcBD::FadcBD()
    : fMID(-1), fSAMPLING(1), fRL(8), fPRESCALE(1), fTLT(0xFFFE), fTRIGEN(15), fPTRIG(0), fZEROSUP(0) {
    for (int i = 0; i < 4; i++) {
        fTHR[i] = 50;
        fDACOFF[i] = 3500;
        fPOL[i] = 0;
        fDLY[i] = 200;
        fCW[i] = 1000;
        fAMODE[i] = 0;
        fTMODE[i] = 1;
        fDT[i] = 0;
        fPSW[i] = 2;
        fPCT[i] = 1;
        fPCI[i] = 1000;
        fPWT[i] = 100;
        fPED[i] = 0;
    }
}

FadcBD::~FadcBD() {
}