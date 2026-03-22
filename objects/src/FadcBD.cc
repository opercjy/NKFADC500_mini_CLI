#include "FadcBD.hh"

ClassImp(FadcBD)

FadcBD::FadcBD() : fMID(-1) {
    for (int i = 0; i < 4; i++) {
        fTHR[i] = 0;
        fDACOFF[i] = 0;
        fPOL[i] = 0;
        fDLY[i] = 0;
    }
}

FadcBD::~FadcBD() {
}
