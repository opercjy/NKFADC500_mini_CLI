#ifndef RAWDATA_HH
#define RAWDATA_HH

#include "TObject.h"
#include "TClonesArray.h"
#include "RawChannel.hh"

class RawData : public TObject {
public:
    RawData();
    virtual ~RawData();

    void Clear(Option_t* opt = ""); 

    RawChannel* AddChannel(int chId);
    RawChannel* GetChannel(int index) const;
    int GetNChannels() const;

    // 이벤트 메타데이터 
    void SetEventID(unsigned int id) { fEventID = id; }
    unsigned int GetEventID() const { return fEventID; }

    void SetTriggerTime(unsigned long t) { fTriggerTime = t; }
    unsigned long GetTriggerTime() const { return fTriggerTime; }

    void SetRunNumber(int r) { fRunNumber = r; }
    int GetRunNumber() const { return fRunNumber; }

private:
    unsigned int fEventID;
    unsigned long fTriggerTime;
    int fRunNumber;
    TClonesArray* fChannels; //->

    ClassDef(RawData, 2) // 버전 2로 업데이트
};

#endif