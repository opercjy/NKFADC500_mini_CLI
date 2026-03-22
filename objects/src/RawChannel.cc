#include "RawChannel.hh"

ClassImp(RawChannel)

RawChannel::RawChannel() : fChId(-1) {
    // [최적화] CPU 캐시 미스 방지 및 std::vector 재할당 오버헤드 제거를 위해 
    // FADC500의 최대 레코드 길이(예: 4096)만큼 메모리 공간을 미리 확보해 둡니다.
    fSamples.reserve(4096); 
}

RawChannel::~RawChannel() {
}

void RawChannel::Clear(Option_t* /*opt*/) {
    fChId = -1;
    // clear()는 내부 메모리 용량(Capacity)은 유지한 채 크기(Size)만 0으로 만드므로 매우 빠릅니다.
    fSamples.clear(); 
}
