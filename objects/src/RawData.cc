#include "RawData.hh"

ClassImp(RawData)

RawData::RawData() {
    // 4채널 보드 여러 개를 고려하여 넉넉하게 최대 32채널 공간의 메모리 풀을 생성합니다.
    fChannels = new TClonesArray("RawChannel", 32);
}

RawData::~RawData() {
    Clear("C");
    delete fChannels;
}

void RawData::Clear(Option_t* /*opt*/) {
    // "C" 옵션: fChannels 내부의 RawChannel 객체들을 delete 하지 않고 
    // 각 객체의 Clear() 함수만 연쇄적으로 호출하여 재활용 상태로 만듭니다.
    fChannels->Clear("C");
}

RawChannel* RawData::AddChannel(int chId) {
    int n = fChannels->GetEntriesFast();
    // Placement New 기법: 이미 할당된 메모리 공간(n번째)에 RawChannel 객체를 덮어씌움
    RawChannel* ch = (RawChannel*)fChannels->ConstructedAt(n);
    ch->SetChId(chId);
    return ch;
}

RawChannel* RawData::GetChannel(int index) const {
    return (RawChannel*)fChannels->At(index);
}

int RawData::GetNChannels() const {
    return fChannels->GetEntriesFast();
}
