#include "RawData.hh"

ClassImp(RawData)

RawData::RawData() : fEventID(0), fTriggerTime(0), fRunNumber(0) {
    fChannels = new TClonesArray("RawChannel", 4);
}

RawData::~RawData() {
    if(fChannels) {
        fChannels->Delete();
        delete fChannels;
    }
}

void RawData::Clear(Option_t* opt) {
    fEventID = 0;
    fTriggerTime = 0;
    fRunNumber = 0;
    fChannels->Clear("C");
}

RawChannel* RawData::AddChannel(int chId) {
    RawChannel* ch = (RawChannel*)fChannels->ConstructedAt(fChannels->GetEntriesFast());
    ch->SetChId(chId);
    return ch;
}

RawChannel* RawData::GetChannel(int index) const {
    return (RawChannel*)fChannels->At(index);
}

int RawData::GetNChannels() const {
    return fChannels->GetEntriesFast();
}