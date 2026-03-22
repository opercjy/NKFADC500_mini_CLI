#ifndef RAWCHANNEL_HH
#define RAWCHANNEL_HH

#include "TObject.h"
#include <vector>

class RawChannel : public TObject {
public:
    RawChannel();
    virtual ~RawChannel();

    void Clear(Option_t* opt = ""); 

    void SetChId(int id) { fChId = id; }
    int  GetChId() const { return fChId; }

    // 12-bit 마스킹 후 파형 삽입
    inline void AddSample(unsigned short val) { 
        fSamples.push_back(val & 0x0FFF); 
    }

    const std::vector<unsigned short>& GetSamples() const { return fSamples; }
    int GetNPoints() const { return fSamples.size(); }

    // 물리량 분석용 Getter
    double GetCharge() const { return fCharge; }
    unsigned short GetPeak() const { return fPeak; }

    // 파형이 입력된 후 전하량(면적)과 피크를 자동 계산하는 함수
    void ComputeMetrics(); 

private:
    int fChId;
    std::vector<unsigned short> fSamples; 
    double fCharge;
    unsigned short fPeak;

    ClassDef(RawChannel, 2) // 버전 2로 업데이트 (중요)
};

#endif