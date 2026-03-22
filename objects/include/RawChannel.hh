#ifndef RAWCHANNEL_HH
#define RAWCHANNEL_HH

#include "TObject.h"
#include <vector>

class RawChannel : public TObject {
public:
    RawChannel();
    virtual ~RawChannel();

    // TClonesArray에서 객체를 재사용할 때 호출되는 초기화 함수
    void Clear(Option_t* opt = ""); 

    void SetChId(int id) { fChId = id; }
    int  GetChId() const { return fChId; }

    // 12-bit (0~4095) 데이터 마스킹 후 고속 삽입
    inline void AddSample(unsigned short val) { 
        fSamples.push_back(val & 0x0FFF); 
    }

    const std::vector<unsigned short>& GetSamples() const { return fSamples; }
    int GetNPoints() const const { return fSamples.size(); }

private:
    int fChId;
    std::vector<unsigned short> fSamples; 

    ClassDef(RawChannel, 1) // ROOT Dictionary 매크로
};

#endif
