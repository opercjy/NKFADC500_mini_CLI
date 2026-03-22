#include "RawChannel.hh"

ClassImp(RawChannel)

RawChannel::RawChannel() : fChId(-1), fCharge(0.0), fPeak(0) {
    fSamples.reserve(2048); // 메모리 파편화 방지용 사전 할당
}

RawChannel::~RawChannel() {}

void RawChannel::Clear(Option_t* opt) {
    fChId = -1;
    fSamples.clear();
    fCharge = 0.0;
    fPeak = 0;
}

void RawChannel::ComputeMetrics() {
    fCharge = 0.0;
    fPeak = 0;
    // 파형 데이터를 순회하며 전하량(단순 면적 적분)과 피크값 탐색
    for(auto val : fSamples) {
        fCharge += val;
        if(val > fPeak) fPeak = val;
    }
}