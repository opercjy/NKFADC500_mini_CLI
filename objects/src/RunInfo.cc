#include "RunInfo.hh"
#include <iostream>

ClassImp(RunInfo)

RunInfo::RunInfo() : fRunNumber(0) {
    fBoards = new TClonesArray("FadcBD", 10); // 최대 10대의 보드 연동 고려
}

RunInfo::~RunInfo() {
    fBoards->Clear("C");
    delete fBoards;
}

FadcBD* RunInfo::AddFadcBD(int mid) {
    int n = fBoards->GetEntriesFast();
    FadcBD* bd = (FadcBD*)fBoards->ConstructedAt(n);
    bd->SetMID(mid);
    return bd;
}

FadcBD* RunInfo::GetFadcBD(int index) const {
    return (FadcBD*)fBoards->At(index);
}

FadcBD* RunInfo::FindFadcBD(int mid) const {
    for (int i = 0; i < GetNFadcBD(); i++) {
        FadcBD* bd = GetFadcBD(i);
        if (bd && bd->GetMID() == mid) return bd;
    }
    return nullptr;
}

int RunInfo::GetNFadcBD() const {
    return fBoards->GetEntriesFast();
}

void RunInfo::PrintInfo() const {
    std::cout << "========================================" << std::endl;
    std::cout << " RUN NUMBER : " << fRunNumber << std::endl;
    std::cout << " SAMPLING   : " << GetSamplingNs() << " ns/sample (500 MS/s)" << std::endl;
    std::cout << " BOARDS     : " << GetNFadcBD() << " connected" << std::endl;
    std::cout << "========================================" << std::endl;
}
