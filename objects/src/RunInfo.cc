#include "RunInfo.hh"
#include <iostream>

ClassImp(RunInfo)

RunInfo::RunInfo() : fRunNumber(0), fBoards(new TClonesArray("FadcBD", 10)) {
    fBoards->SetOwner(kTRUE);
}

RunInfo::~RunInfo() {
    if (fBoards) {
        fBoards->Clear("C");
        delete fBoards;
        fBoards = nullptr;
    }
}

double RunInfo::GetSamplingNs(int boardIdx) const {
    const FadcBD* bd = GetFadcBD(boardIdx);
    const int dsr = (bd && bd->GetSAMPLING() > 0) ? bd->GetSAMPLING() : 1;
    return 2.0 * static_cast<double>(dsr);
}

FadcBD* RunInfo::AddFadcBD(int mid) {
    int n = fBoards->GetEntriesFast();
    FadcBD* bd = static_cast<FadcBD*>(fBoards->ConstructedAt(n));
    bd->SetMID(mid);
    return bd;
}

FadcBD* RunInfo::GetFadcBD(int index) const {
    if (!fBoards || index < 0 || index >= fBoards->GetEntriesFast()) return nullptr;
    return static_cast<FadcBD*>(fBoards->At(index));
}

FadcBD* RunInfo::FindFadcBD(int mid) const {
    for (int i = 0; i < GetNFadcBD(); i++) {
        FadcBD* bd = GetFadcBD(i);
        if (bd && bd->GetMID() == mid) return bd;
    }
    return nullptr;
}

int RunInfo::GetNFadcBD() const {
    return fBoards ? fBoards->GetEntriesFast() : 0;
}

void RunInfo::PrintInfo() const {
    const double dt_ns = GetSamplingNs(0);
    const double msps = (dt_ns > 0.0) ? (1000.0 / dt_ns) : 500.0;
    std::cout << "========================================" << std::endl;
    std::cout << " RUN NUMBER : " << fRunNumber << std::endl;
    std::cout << " SAMPLING   : " << dt_ns << " ns/sample (" << msps << " MS/s)" << std::endl;
    std::cout << " BOARDS     : " << GetNFadcBD() << " connected" << std::endl;
    std::cout << "========================================" << std::endl;
}
