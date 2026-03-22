#ifndef RAWDATA_HH
#define RAWDATA_HH

#include "TObject.h"
#include "TClonesArray.h"
#include "RawChannel.hh"

class RawData : public TObject {
public:
    RawData();
    virtual ~RawData();

    // 내부 채널 메모리 풀 초기화
    void Clear(Option_t* opt = ""); 

    // 새로운 채널 메모리를 할당하는 대신 풀(Pool)에서 꺼내오는 고속 함수
    RawChannel* AddChannel(int chId);
    
    RawChannel* GetChannel(int index) const;
    int GetNChannels() const;

private:
    TClonesArray* fChannels; //-> 메모리 파편화를 막아주는 ROOT의 핵심 객체 풀

    ClassDef(RawData, 1)
};

#endif
