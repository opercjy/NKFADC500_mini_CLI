#ifndef RUNINFO_HH
#define RUNINFO_HH

#include "TObject.h"
#include "TClonesArray.h"
#include "FadcBD.hh"

class RunInfo : public TObject {
public:
    RunInfo();
    virtual ~RunInfo();

    void SetRunNumber(int run) { fRunNumber = run; }
    int  GetRunNumber() const  { return fRunNumber; }

    // Base 500 MS/s clock = 2.0 ns * decimation factor (SAMPLING_RATE: 1=500MS/s, 2=250MS/s, ...)
    double GetSamplingNs(int boardIdx = 0) const;

    // Board configuration pool
    FadcBD* AddFadcBD(int mid);
    FadcBD* GetFadcBD(int index) const;
    FadcBD* FindFadcBD(int mid) const;
    int     GetNFadcBD() const;

    void PrintInfo() const;

private:
    int fRunNumber;
    TClonesArray* fBoards; //->

    ClassDefOverride(RunInfo, 2)
};

#endif
