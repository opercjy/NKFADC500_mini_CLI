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

    // 💡 [핵심 최적화] 하드코딩 방지: 500MS/s 장비의 샘플당 시간(2.0 ns) 반환
    double GetSamplingNs() const { return 2.0; }

    // 메모리 풀 기반 보드 객체 관리
    FadcBD* AddFadcBD(int mid);
    FadcBD* GetFadcBD(int index) const;
    FadcBD* FindFadcBD(int mid) const;
    int     GetNFadcBD() const;

    void PrintInfo() const;

private:
    int fRunNumber;
    TClonesArray* fBoards; //->

    ClassDef(RunInfo, 1)
};

#endif
