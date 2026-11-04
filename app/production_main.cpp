#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <sys/select.h>
#include <unistd.h>
#include <vector>

#include "ConfigParser.hh"
#include "ELog.hh"
#include "NoticeFadc500Spec.hh"
#include "RunInfo.hh"
#include "TApplication.h"
#include "TCanvas.h"
#include "TFile.h"
#include "TGraph.h"
#include "TH1I.h"
#include "TLine.h"
#include "TNamed.h"
#include "TString.h"
#include "TSystem.h"
#include "TTree.h"

namespace {

// =========================================================================
// Browser History Cache Manager
// =========================================================================
class HistoryCache {
private:
    std::string cacheFile = ".fadc_browser.cache";
public:
    std::string GetLastFile() const {
        std::ifstream in(cacheFile);
        std::string file;
        if (in.is_open()) {
            std::getline(in, file);
        }
        return file;
    }
    void SaveFile(const std::string& file) const {
        std::ofstream out(cacheFile);
        if (out.is_open()) {
            out << file;
        }
    }
};

int InferRunNumberFromFilename(const std::string& filepath, int fallbackRun) {
    const std::string stem = std::filesystem::path(filepath).stem().string();
    // Match <prefix>_<run>, <prefix>_<run>_<subrun>, or <prefix>_<run>_thr<N>
    static const std::regex re_run(R"(^.+?_(\d+)(?:_(?:thr\d+|\d{3}))?$)", std::regex::icase);
    std::smatch m;
    if (std::regex_match(stem, m, re_run)) {
        try { return std::stoi(m[1].str()); } catch (...) {}
    }
    // Fallback: last integer group in filename stem
    static const std::regex re_digits(R"((\d+))");
    auto it = std::sregex_iterator(stem.begin(), stem.end(), re_digits);
    auto end = std::sregex_iterator();
    int last_num = fallbackRun;
    for (; it != end; ++it) {
        try { last_num = std::stoi((*it)[1].str()); } catch (...) {}
    }
    return last_num;
}

// Attempt bounded mid-stream resynchronization (up to 1 MB) when a corrupted block is encountered
bool TryResyncToNextHeader(
    FILE* fp,
    size_t totalFileBytes,
    uint32_t expectedDataLen,
    unsigned char (&outHeader)[128],
    size_t& ioCurrentBytes)
{
    const long corruptStart = std::ftell(fp) - 128L;
    if (corruptStart < 0) return false;

    constexpr size_t kMaxScanBytes = 1048576ULL;
    const size_t avail = (static_cast<size_t>(corruptStart) < totalFileBytes)
        ? std::min(kMaxScanBytes, totalFileBytes - static_cast<size_t>(corruptStart))
        : 0ULL;
    if (avail <= 129ULL) return false;

    std::vector<unsigned char> scanBuf(avail);
    std::fseek(fp, corruptStart, SEEK_SET);
    const size_t nRead = std::fread(scanBuf.data(), 1, avail, fp);
    if (nRead <= 129ULL) return false;

    for (size_t off = 1; off + 128ULL <= nRead; ++off) {
        const auto cand = NoticeFadc500Spec::DecodeHeader128(scanBuf.data() + off);
        if (!cand.valid_geometry) continue;
        // Require strict hardware channel tags {1, 2, 3, 4} to eliminate false positives
        if (!(cand.channel_ids[0] == 1 && cand.channel_ids[1] == 2 &&
              cand.channel_ids[2] == 3 && cand.channel_ids[3] == 4)) {
            continue;
        }
        if (expectedDataLen > 0U && cand.data_length != expectedDataLen) continue;

        const size_t pktBytes = static_cast<size_t>(cand.data_length) * 4ULL;
        const size_t nextPacketFilePos = static_cast<size_t>(corruptStart) + off + pktBytes;
        if (nextPacketFilePos > totalFileBytes) continue;

        if (nextPacketFilePos < totalFileBytes) {
            if (off + pktBytes + 128ULL <= nRead) {
                const auto nextCand = NoticeFadc500Spec::DecodeHeader128(scanBuf.data() + off + pktBytes);
                if (!nextCand.valid_geometry || !nextCand.valid_channel_tags) continue;
            }
        }

        std::memcpy(outHeader, scanBuf.data() + off, 128);
        const long resumedAfterHdr = corruptStart + static_cast<long>(off) + 128L;
        std::fseek(fp, resumedAfterHdr, SEEK_SET);
        ioCurrentBytes = static_cast<size_t>(resumedAfterHdr);
        ELog::Print(ELog::WARNING,
            Form("Resynchronized binary stream after skipping %zu corrupted bytes at offset %ld.",
                 off, corruptStart));
        return true;
    }
    return false;
}

bool kbhit() {
    struct timeval tv = {0L, 0L};
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) > 0;
}

void PrintUsage(const char* progName) {
    std::cout << "\n\033[1;36m======================================================================\033[0m\n";
    std::cout << "\033[1;32m      NKFADC500 Mini - Offline Production & Event Display Tool\033[0m\n";
    std::cout << "\033[1;36m======================================================================\033[0m\n";
    std::cout << "\033[1;33mUsage:\033[0m " << progName << " <raw_data_file.dat> [options]\n\n";
    std::cout << "\033[1;37m[Options]\033[0m\n";
    std::cout << "  -w               : Save full calibrated waveforms (wTime_Ch*, wDrop_Ch*)\n";
    std::cout << "  -d               : Interactive Event Display Mode (ROOT 4-pad debugger)\n";
    std::cout << "  -f <config.cfg>  : Explicit settings.cfg (auto-detects <raw_file>.cfg first)\n";
    std::cout << "  -o <out.root>    : Custom output ROOT path (default: <raw_file>_prod.root)\n";
    std::cout << "  -r <run_number>  : Explicit RunNumber override\n";
    std::cout << "  --dir <data_dir> : Data directory when combined with -p <prefix_run>\n";
    std::cout << "  -p <prefix_run>  : Resolve <data_dir>/<prefix_run>.dat\n";
    std::cout << "  -h, --help       : Print this help message\n";
    std::cout << "\033[1;36m======================================================================\033[0m\n\n";
}

} // namespace

int main(int argc, char** argv) {
    std::string inputFile;
    std::string outputFile;
    std::string explicitConfig;
    std::string dataDir = "data";
    std::string prefixName;
    int explicitRunNumber = -1;
    bool saveWaveform = false;
    bool interactiveMode = false;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-w" || arg == "--waveform") {
            saveWaveform = true;
        } else if (arg == "-d" || arg == "--display") {
            interactiveMode = true;
        } else if ((arg == "-f" || arg == "--config") && i + 1 < argc) {
            explicitConfig = argv[++i];
        } else if ((arg == "-o" || arg == "--output") && i + 1 < argc) {
            outputFile = argv[++i];
        } else if ((arg == "-r" || arg == "--run") && i + 1 < argc) {
            explicitRunNumber = std::atoi(argv[++i]);
        } else if (arg == "--dir" && i + 1 < argc) {
            dataDir = argv[++i];
        } else if (arg == "-p" && i + 1 < argc) {
            prefixName = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            PrintUsage(argv[0]);
            return 0;
        } else if (!arg.empty() && arg[0] != '-') {
            inputFile = arg;
        }
    }

    if (inputFile.empty() && !prefixName.empty()) {
        std::filesystem::path candidate = std::filesystem::path(dataDir) / prefixName;
        if (candidate.extension() != ".dat") candidate += ".dat";
        inputFile = candidate.string();
    }

    HistoryCache cache;
    if (inputFile.empty()) {
        inputFile = cache.GetLastFile();
        if (inputFile.empty()) {
            PrintUsage(argv[0]);
            return 1;
        }
        std::cout << "\033[1;32m[INFO] Auto-loading last used file from cache:\033[0m " << inputFile << "\n";
    } else {
        cache.SaveFile(inputFile);
    }

    if (interactiveMode && gSystem->Getenv("DISPLAY") == nullptr) {
        std::cout << "\n\033[1;31m[FATAL ERROR]\033[0m No X11 DISPLAY found!\n";
        std::cout << "Cannot open Interactive Canvas in headless mode.\n";
        std::cout << "Reconnect via '\033[1;33mssh -X\033[0m' or '\033[1;33mssh -Y\033[0m'.\n\n";
        return 1;
    }

    FILE* fp = std::fopen(inputFile.c_str(), "rb");
    if (!fp) {
        ELog::Print(ELog::FATAL, Form("Cannot open file: %s", inputFile.c_str()));
        return 1;
    }

    // 16 MB read buffer to maximize sequential binary throughput
    std::setvbuf(fp, nullptr, _IOFBF, 16 * 1024 * 1024);

    std::fseek(fp, 0, SEEK_END);
    const long rawPos = std::ftell(fp);
    const size_t totalBytes = (rawPos > 0) ? static_cast<size_t>(rawPos) : 0ULL;
    std::rewind(fp);
    const double totalMB = static_cast<double>(totalBytes) / 1048576.0;

    // Priority 1: Deserialize Frontend-captured RunInfo/FadcBD object (<run>_cfg.root) if available and no explicit -f override
    // Priority 2: Parse companion <run>.cfg or explicit -f <config.cfg> into RunInfo/FadcBD object
    RunInfo prodRunInfo;
    NoticeFadc500Spec::StreamDecodeConfig prodCfg;
    bool loadedFromSerializedObj = false;

    std::filesystem::path companionObjPath(inputFile);
    companionObjPath.replace_extension("");
    companionObjPath += "_cfg.root";

    if (explicitConfig.empty() && std::filesystem::exists(companionObjPath)) {
        TFile* cfgObjFile = TFile::Open(companionObjPath.string().c_str(), "READ");
        if (cfgObjFile && !cfgObjFile->IsZombie()) {
            if (RunInfo* savedRi = dynamic_cast<RunInfo*>(cfgObjFile->Get("RunInfo"))) {
                prodRunInfo.SetRunNumber(savedRi->GetRunNumber());
                for (int b = 0; b < savedRi->GetNFadcBD(); ++b) {
                    if (const FadcBD* srcBd = savedRi->GetFadcBD(b)) {
                        FadcBD* dstBd = prodRunInfo.AddFadcBD(srcBd->GetMID());
                        *dstBd = *srcBd;
                    }
                }
                prodCfg.loaded_path = companionObjPath.string();
                prodCfg.config_run_number = prodRunInfo.GetRunNumber();
                prodCfg.dt_ns = prodRunInfo.GetSamplingNs(0);
                if (const FadcBD* bd0 = prodRunInfo.GetFadcBD(0)) {
                    for (int ch = 0; ch < 4; ++ch) {
                        prodCfg.delay_ns[ch] = static_cast<double>(bd0->GetDLY(ch));
                        prodCfg.polarity[ch] = bd0->GetPOL(ch);
                    }
                }
                loadedFromSerializedObj = true;
            }
            cfgObjFile->Close();
            delete cfgObjFile;
        }
    }

    if (!loadedFromSerializedObj) {
        const std::string resolvedCfgPath =
            NoticeFadc500Spec::ResolveCompanionConfigPath(explicitConfig, inputFile, argv[0]);
        prodCfg = NoticeFadc500Spec::LoadStreamDecodeConfig(resolvedCfgPath);
        if (!prodCfg.loaded_path.empty()) {
            ConfigParser::Parse(prodCfg.loaded_path, &prodRunInfo);
        } else {
            prodRunInfo.SetRunNumber(0);
            prodRunInfo.AddFadcBD(1);
            if (!resolvedCfgPath.empty()) {
                ELog::Print(ELog::WARNING,
                    "Configuration file not found (" + resolvedCfgPath + "). Using defaults (DLY=400 ns, POL=0, 500 MS/s).");
            }
        }
    }

    const double dt_ns = prodCfg.dt_ns;
    const int resolvedRunNumber = (explicitRunNumber >= 0)
        ? explicitRunNumber
        : InferRunNumberFromFilename(inputFile, prodCfg.config_run_number);
    prodRunInfo.SetRunNumber(resolvedRunNumber);

    if (outputFile.empty()) {
        outputFile = inputFile;
        size_t dotPos = outputFile.find_last_of('.');
        if (dotPos != std::string::npos) outputFile = outputFile.substr(0, dotPos);
        outputFile += "_prod.root";
    }

    std::string modeStr = "\033[1;33mFast Physics + PSD Mode (Zero-Alloc Loop)\033[0m";
    if (saveWaveform) modeStr = "\033[1;35mFull Waveform + PSD Mode (-w)\033[0m";
    if (interactiveMode) modeStr = "\033[1;36mInteractive Event Display (-d)\033[0m";

    std::cout << "\n\033[1;36m========================================================\033[0m\n";
    std::cout << "\033[1;32m       NKFADC500 Mini - Offline Production\033[0m\n";
    std::cout << "       [Input File]   " << inputFile << " (" << std::fixed << std::setprecision(2) << totalMB << " MB)\n";
    if (!interactiveMode) {
        std::cout << "       [Output File]  " << outputFile << "\n";
    }
    std::cout << "       [Config Used]  " << (prodCfg.loaded_path.empty() ? "Built-in Defaults" : prodCfg.loaded_path) << "\n";
    std::cout << "       [Run / Clock]  Run " << resolvedRunNumber << " | dt = " << std::setprecision(1) << dt_ns << " ns/sample\n";
    std::cout << "       [Process Mode] " << modeStr << "\n";
    std::cout << "       [Trig. Delay]  Ch0=" << prodCfg.delay_ns[0] << " ns (POL=" << prodCfg.polarity[0] << ")\n";
    std::cout << "\033[1;36m========================================================\033[0m\n\n";

    // =========================================================================
    // Batch Production Mode
    // =========================================================================
    if (!interactiveMode) {
        TFile* rootFile = TFile::Open(outputFile.c_str(), "RECREATE");
        if (!rootFile || rootFile->IsZombie()) {
            std::fclose(fp);
            ELog::Print(ELog::FATAL, Form("Cannot create output ROOT file: %s", outputFile.c_str()));
            return 1;
        }

        TTree* tree = new TTree("PROD", "FADC500 Mini Processed Data Tree");

        unsigned int eventID = 0;
        ULong64_t triggerTime = 0;
        ULong64_t localTriggerTime = 0;
        unsigned int localTriggerNumber = 0;
        unsigned int triggerPattern = 0;
        unsigned int triggerType = 0;
        unsigned int triggerDestination = 0;
        unsigned int moduleID = 0;
        int runNumber = resolvedRunNumber;
        int recordLength = 0;
        double samplingNs = dt_ns;

        double baseline[4] = {};
        double baselineRMS[4] = {};
        double amplitude[4] = {};
        double charge[4] = {};
        double peakTime[4] = {};
        double qTotal[4] = {};
        double qTail[4] = {};
        double psdRatio[4] = {};
        int hwPedestal[4] = {};
        std::vector<double> wTime[4], wDrop[4];

        tree->Branch("EventID", &eventID, "EventID/i");
        tree->Branch("TriggerTime", &triggerTime, "TriggerTime/l");
        tree->Branch("LocalTriggerTime", &localTriggerTime, "LocalTriggerTime/l");
        tree->Branch("TriggerNumber", &localTriggerNumber, "TriggerNumber/i");
        tree->Branch("TriggerPattern", &triggerPattern, "TriggerPattern/i");
        tree->Branch("TriggerType", &triggerType, "TriggerType/i");
        tree->Branch("TriggerDestination", &triggerDestination, "TriggerDestination/i");
        tree->Branch("ModuleID", &moduleID, "ModuleID/i");
        tree->Branch("RunNumber", &runNumber, "RunNumber/I");
        tree->Branch("RecordLength", &recordLength, "RecordLength/I");
        tree->Branch("SamplingNs", &samplingNs, "SamplingNs/D");

        for (int i = 0; i < 4; i++) {
            tree->Branch(Form("Baseline_Ch%d", i), &baseline[i], Form("Baseline_Ch%d/D", i));
            tree->Branch(Form("BaselineRMS_Ch%d", i), &baselineRMS[i], Form("BaselineRMS_Ch%d/D", i));
            tree->Branch(Form("Amplitude_Ch%d", i), &amplitude[i], Form("Amplitude_Ch%d/D", i));
            tree->Branch(Form("Charge_Ch%d", i), &charge[i], Form("Charge_Ch%d/D", i));
            tree->Branch(Form("PeakTime_Ch%d", i), &peakTime[i], Form("PeakTime_Ch%d/D", i));
            tree->Branch(Form("Qtotal_Ch%d", i), &qTotal[i], Form("Qtotal_Ch%d/D", i));
            tree->Branch(Form("Qtail_Ch%d", i), &qTail[i], Form("Qtail_Ch%d/D", i));
            tree->Branch(Form("PsdRatio_Ch%d", i), &psdRatio[i], Form("PsdRatio_Ch%d/D", i));
            tree->Branch(Form("HwPedestal_Ch%d", i), &hwPedestal[i], Form("HwPedestal_Ch%d/I", i));

            if (saveWaveform) {
                tree->Branch(Form("wTime_Ch%d", i), &wTime[i]);
                tree->Branch(Form("wDrop_Ch%d", i), &wDrop[i]);
            }
        }

        // Pre-allocate reusable buffers outside event loop (eliminates 5 * N_events heap allocations!)
        unsigned char header[128];
        std::vector<unsigned char> payload;
        payload.reserve(16384);
        std::array<std::vector<uint16_t>, 4> rawWave;
        for (int ch = 0; ch < 4; ++ch) rawWave[ch].reserve(2048);

        // PSD integration offsets around peak index ([-10 ns, +120 ns] total; [+16 ns, +120 ns] tail)
        const int psdPrePts       = std::max(1, static_cast<int>(std::llround(10.0 / dt_ns)));
        const int psdTailStartPts = std::max(1, static_cast<int>(std::llround(16.0 / dt_ns)));
        const int psdEndPts       = std::max(psdTailStartPts + 1, static_cast<int>(std::llround(120.0 / dt_ns)));

        size_t currentBytes = 0;
        uint32_t lockedDataLength = 0;
        auto start_time = std::chrono::steady_clock::now();
        auto ui_timer = start_time;

        std::cout << "\033[1;36m[  Production Real-time Monitor  ]\033[0m\n";

        while (std::fread(header, 1, 128, fp) == 128) {
            currentBytes += 128;
            NoticeFadc500Spec::DecodedPacketHeader hdr = NoticeFadc500Spec::DecodeHeader128(header);

            if (!hdr.valid_geometry || !hdr.valid_channel_tags) {
                if (TryResyncToNextHeader(fp, totalBytes, lockedDataLength, header, currentBytes)) {
                    hdr = NoticeFadc500Spec::DecodeHeader128(header);
                } else {
                    ELog::Print(ELog::WARNING,
                        Form("Corrupted header detected at byte %zu (Length: %u) with no valid resync header. Stopping safely.",
                             currentBytes - 128, hdr.data_length));
                    break;
                }
            }
            if (lockedDataLength == 0U) {
                lockedDataLength = hdr.data_length;
            }

            triggerTime = static_cast<ULong64_t>(hdr.best_trig_time_ns);
            localTriggerTime = static_cast<ULong64_t>(hdr.local_trig_time_ns);
            localTriggerNumber = (hdr.local_trig_num > 0U) ? hdr.local_trig_num : hdr.tcb_trig_num;
            triggerPattern = hdr.trigger_pattern;
            triggerType = hdr.trigger_type;
            triggerDestination = hdr.trigger_destination;
            moduleID = static_cast<unsigned int>(hdr.mid);
            for (int ch = 0; ch < 4; ++ch) hwPedestal[ch] = hdr.hw_pedestal[ch];

            recordLength = static_cast<int>((hdr.data_length - 32U) / 2U);
            const size_t payload_bytes = static_cast<size_t>(recordLength) * 8ULL;

            if (recordLength == 0) {
                // Zero-suppressed header-only packet (NKFADC500write_ZEROSUP enabled, data_length == 32)
                for (int ch = 0; ch < 4; ++ch) {
                    baseline[ch] = static_cast<double>(hdr.hw_pedestal[ch]);
                    baselineRMS[ch] = 0.0;
                    amplitude[ch] = 0.0;
                    charge[ch] = 0.0;
                    peakTime[ch] = 0.0;
                    qTotal[ch] = 0.0;
                    qTail[ch] = 0.0;
                    psdRatio[ch] = 0.0;
                    if (saveWaveform) {
                        wTime[ch].clear();
                        wDrop[ch].clear();
                    }
                }
                tree->Fill();
                eventID++;
                continue;
            }

            payload.resize(payload_bytes);
            if (std::fread(payload.data(), 1, payload_bytes, fp) != payload_bytes) break;
            currentBytes += payload_bytes;

            for (int ch = 0; ch < 4; ++ch) {
                rawWave[ch].resize(recordLength);
                if (saveWaveform) {
                    wTime[ch].resize(recordLength);
                    wDrop[ch].resize(recordLength);
                }
            }

            const unsigned char* p = payload.data();
            for (int j = 0; j < recordLength; ++j) {
                const size_t offset = static_cast<size_t>(j) * 8ULL;
                rawWave[0][j] = static_cast<uint16_t>((p[offset + 0] | (static_cast<uint16_t>(p[offset + 4]) << 8)) & 0x0FFFU);
                rawWave[1][j] = static_cast<uint16_t>((p[offset + 1] | (static_cast<uint16_t>(p[offset + 5]) << 8)) & 0x0FFFU);
                rawWave[2][j] = static_cast<uint16_t>((p[offset + 2] | (static_cast<uint16_t>(p[offset + 6]) << 8)) & 0x0FFFU);
                rawWave[3][j] = static_cast<uint16_t>((p[offset + 3] | (static_cast<uint16_t>(p[offset + 7]) << 8)) & 0x0FFFU);
            }

            for (int ch = 0; ch < 4; ++ch) {
                const int nPed = NoticeFadc500Spec::ComputeBaselineSamples(
                    prodCfg.delay_ns[ch], dt_ns, recordLength);
                const int pol = prodCfg.polarity[ch];
                const uint16_t* wf = rawWave[ch].data();

                double pedSum = 0.0;
                double pedSqSum = 0.0;
                for (int pt = 0; pt < nPed; ++pt) {
                    const double v = static_cast<double>(wf[pt]);
                    pedSum += v;
                    pedSqSum += v * v;
                }
                const double bsl = (nPed > 0) ? (pedSum / static_cast<double>(nPed)) : 0.0;
                const double var = (nPed > 1)
                    ? std::max(0.0, (pedSqSum / static_cast<double>(nPed)) - (bsl * bsl))
                    : 0.0;

                baseline[ch] = bsl;
                baselineRMS[ch] = std::sqrt(var);

                double maxAmp = -9999.0;
                double sumCharge = 0.0;
                int maxIdx = 0;

                for (int pt = 0; pt < recordLength; ++pt) {
                    const double drop = (pol == 0)
                        ? (bsl - static_cast<double>(wf[pt]))
                        : (static_cast<double>(wf[pt]) - bsl);
                    if (drop > 0.0) sumCharge += drop;
                    if (drop > maxAmp) {
                        maxAmp = drop;
                        maxIdx = pt;
                    }
                    if (saveWaveform) {
                        wTime[ch][pt] = static_cast<double>(pt) * dt_ns;
                        wDrop[ch][pt] = drop;
                    }
                }

                amplitude[ch] = maxAmp;
                charge[ch] = sumCharge;
                peakTime[ch] = static_cast<double>(maxIdx) * dt_ns;

                // Charge-comparison PSD around peakTime: Qtotal in [-10, +120] ns, Qtail in [+16, +120] ns
                const int iStart = std::max(0, maxIdx - psdPrePts);
                const int iTail  = std::min(recordLength, maxIdx + psdTailStartPts);
                const int iEnd   = std::min(recordLength, maxIdx + psdEndPts + 1);
                double qt = 0.0;
                double qtl = 0.0;
                for (int pt = iStart; pt < iEnd; ++pt) {
                    const double drop = (pol == 0)
                        ? (bsl - static_cast<double>(wf[pt]))
                        : (static_cast<double>(wf[pt]) - bsl);
                    if (drop > 0.0) {
                        qt += drop;
                        if (pt >= iTail) qtl += drop;
                    }
                }
                qTotal[ch] = qt;
                qTail[ch] = qtl;
                psdRatio[ch] = (qt > 0.0) ? (qtl / qt) : 0.0;
            }

            tree->Fill();
            eventID++;

            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration<double>(now - ui_timer).count() >= 0.5) {
                double total_elapsed = std::chrono::duration<double>(now - start_time).count();
                double progress = (totalBytes > 0) ? (static_cast<double>(currentBytes) / static_cast<double>(totalBytes)) * 100.0 : 100.0;
                double speed_mbps = (total_elapsed > 0.0) ? ((currentBytes / 1048576.0) / total_elapsed) : 0.0;
                double eta_sec = (speed_mbps > 0.0) ? std::max(0.0, (totalMB - (currentBytes / 1048576.0)) / speed_mbps) : 0.0;

                std::cout << "\r\033[F\033[K" << "\033[1;36m[  Production Real-time Monitor  ]\033[0m"
                          << "  ( Elapsed: \033[1;32m" << std::fixed << std::setprecision(1) << total_elapsed << " s\033[0m"
                          << " | ETA: \033[1;33m" << std::fixed << std::setprecision(1) << eta_sec << " s\033[0m )\n"
                          << "\r\033[K"
                          << "   \033[1;33mProgress:\033[0m " << std::setw(5) << std::fixed << std::setprecision(1) << progress << " % | "
                          << "\033[1;34mEvents:\033[0m " << std::setw(7) << eventID << " | "
                          << "\033[1;35mSpeed:\033[0m " << std::setw(6) << std::fixed << std::setprecision(2) << speed_mbps << " MB/s"
                          << std::flush;
                ui_timer = now;
            }
        }

        auto end_time = std::chrono::steady_clock::now();
        const double final_elapsed = std::chrono::duration<double>(end_time - start_time).count();
        const double final_speed_mbps = (final_elapsed > 0.0) ? ((currentBytes / 1048576.0) / final_elapsed) : 0.0;

        std::cout << "\n\n\033[1;36m========================================================\033[0m\n";
        std::cout << "\033[1;32m   [ Production Summary ]\033[0m\n";
        std::cout << "   Total Events  : " << eventID << "\n";
        std::cout << "   Time Taken    : " << std::fixed << std::setprecision(2) << final_elapsed << " sec\n";
        std::cout << "   Avg Speed     : " << std::fixed << std::setprecision(2) << final_speed_mbps << " MB/s\n";
        std::cout << "\033[1;36m========================================================\033[0m\n";

        // Serialize full RunInfo & FadcBD hardware configuration objects + provenance metadata inside ROOT file
        prodRunInfo.Write("RunInfo", TObject::kSingleKey);
        if (FadcBD* bd0 = prodRunInfo.GetFadcBD(0)) {
            bd0->Write("FadcBD", TObject::kSingleKey);
        }
        TNamed cfgMeta("ProductionConfigPath", prodCfg.loaded_path.c_str());
        cfgMeta.Write();
        rootFile->Write();
        rootFile->Close();
        delete rootFile;
        std::fclose(fp);
        return 0;
    }

    // =========================================================================
    // Interactive Event Display Mode (-d)
    // =========================================================================
    TApplication app("app", &argc, argv);
    TCanvas* c1 = new TCanvas("c1", "NKFADC500 Mini - Interactive Event Display", 1400, 900);
    c1->Divide(2, 2);

    TH1I* hWave[4];
    TGraph* gFill[4];
    TLine* lPed[4] = {nullptr, nullptr, nullptr, nullptr};

    for (int i = 0; i < 4; i++) {
        hWave[i] = new TH1I(Form("hWave_Ch%d", i), Form("Channel %d Signal;Time (ns);Signal Amplitude (ADC)", i), 1000, 0, 2000);
        hWave[i]->SetStats(0);
        hWave[i]->SetLineColor(kAzure + 2);
        hWave[i]->SetLineWidth(2);
        hWave[i]->SetFillStyle(0);

        gFill[i] = new TGraph();
        gFill[i]->SetFillColor(kAzure - 9);
        gFill[i]->SetFillStyle(3004);
    }

    unsigned char header[128];
    std::vector<unsigned char> payload;
    std::array<std::vector<uint16_t>, 4> rawWave;
    unsigned int eventID = 0;
    unsigned int targetEventID = 0;

    std::cout << "\n\033[1;35m========================================================\033[0m\n";
    std::cout << "\033[1;32m   [ Interactive Display Mode Activated ]\033[0m\n";
    std::cout << "   -> \033[1;33m[ENTER/n]\033[0m : Next Event\n";
    std::cout << "   -> \033[1;36m[p]\033[0m       : Previous Event\n";
    std::cout << "   -> \033[1;36m[j]\033[0m       : Jump to Event\n";
    std::cout << "   -> \033[1;31m[q]\033[0m       : Quit\n";
    std::cout << "\033[1;35m========================================================\033[0m\n\n";

    while (std::fread(header, 1, 128, fp) == 128) {
        const auto hdr = NoticeFadc500Spec::DecodeHeader128(header);
        if (!hdr.valid_geometry || !hdr.valid_channel_tags) {
            ELog::Print(ELog::WARNING, "Corrupted header detected. Aborting.");
            break;
        }

        const int recordLength = static_cast<int>((hdr.data_length - 32U) / 2U);
        const long payload_bytes = static_cast<long>(recordLength) * 8L;

        if (eventID < targetEventID) {
            if (payload_bytes > 0 && std::fseek(fp, payload_bytes, SEEK_CUR) != 0) break;
            eventID++;
            continue;
        }

        if (recordLength == 0) {
            std::cout << "\n\033[1;36m=== Event " << eventID << " (Zero-Suppressed Header Only, TrigTime: "
                      << hdr.best_trig_time_ns << " ns) ===\033[0m\n";
            eventID++;
            targetEventID = eventID;
            continue;
        }

        payload.resize(static_cast<size_t>(payload_bytes));
        if (std::fread(payload.data(), 1, static_cast<size_t>(payload_bytes), fp) != static_cast<size_t>(payload_bytes)) break;

        for (int ch = 0; ch < 4; ++ch) rawWave[ch].resize(recordLength);
        for (int j = 0; j < recordLength; j++) {
            const size_t offset = static_cast<size_t>(j) * 8ULL;
            rawWave[0][j] = static_cast<uint16_t>((payload[offset + 0] | (payload[offset + 4] << 8)) & 0x0FFFU);
            rawWave[1][j] = static_cast<uint16_t>((payload[offset + 1] | (payload[offset + 5] << 8)) & 0x0FFFU);
            rawWave[2][j] = static_cast<uint16_t>((payload[offset + 2] | (payload[offset + 6] << 8)) & 0x0FFFU);
            rawWave[3][j] = static_cast<uint16_t>((payload[offset + 3] | (payload[offset + 7] << 8)) & 0x0FFFU);
        }

        std::cout << "\n\033[1;36m=== Event " << eventID << " (TrigTime: " << hdr.best_trig_time_ns << " ns) ===\033[0m\n";

        for (int i = 0; i < 4; i++) {
            const int nPed = NoticeFadc500Spec::ComputeBaselineSamples(
                prodCfg.delay_ns[i], dt_ns, recordLength);
            const int pol = prodCfg.polarity[i];

            hWave[i]->Reset();
            hWave[i]->SetBins(recordLength, 0, recordLength * dt_ns);
            c1->cd(i + 1);
            gPad->SetGrid();

            double pedSum = 0.0;
            for (int pt = 0; pt < nPed; pt++) pedSum += rawWave[i][pt];
            const double baseline = (nPed > 0) ? (pedSum / static_cast<double>(nPed)) : 0.0;

            gFill[i]->Set(0);
            gFill[i]->SetPoint(0, 0.0, 0.0);

            double maxAmp = -9999.0;
            double qSum = 0.0;
            int maxIdx = 0;

            for (int j = 0; j < recordLength; j++) {
                const double sig = (pol == 0)
                    ? (baseline - static_cast<double>(rawWave[i][j]))
                    : (static_cast<double>(rawWave[i][j]) - baseline);
                hWave[i]->SetBinContent(j + 1, sig);
                gFill[i]->SetPoint(j + 1, j * dt_ns, sig);

                if (sig > 0.0) qSum += sig;
                if (sig > maxAmp) {
                    maxAmp = sig;
                    maxIdx = j;
                }
            }
            gFill[i]->SetPoint(recordLength + 1, (recordLength - 1) * dt_ns, 0.0);

            const double peakTime = maxIdx * dt_ns;

            hWave[i]->SetTitle(Form("Event %u - Channel %d (Base: %.1f);Time (ns);Signal Amplitude (ADC)", eventID, i, baseline));
            hWave[i]->GetYaxis()->SetRangeUser(-100, 4200);

            hWave[i]->Draw("HIST");
            gFill[i]->Draw("F SAME");

            delete lPed[i];
            lPed[i] = new TLine(0.0, 0.0, (recordLength - 1) * dt_ns, 0.0);
            lPed[i]->SetLineColor(kRed);
            lPed[i]->SetLineStyle(2);
            lPed[i]->Draw();

            std::printf(" \033[1;33m[Ch %d]\033[0m Bsl: %6.1f | Amp: %6.1f | \033[1;32mTime: %6.1f ns\033[0m | Charge: %6.1f \n",
                        i, baseline, maxAmp, peakTime, qSum);
        }
        c1->Modified();
        c1->Update();

        bool requires_rewind = false;
        while (true) {
            std::cout << "\033[1;36mEvent " << eventID << " Loaded.\033[0m [ENTER]: Next | [p]: Prev | [q]: Quit | [j]: Jump -> " << std::flush;

            while (!kbhit()) {
                gSystem->ProcessEvents();
                usleep(10000);
            }

            std::string input;
            if (!std::getline(std::cin, input)) {
                std::fclose(fp);
                return 0;
            }

            if (input.empty() || input == "n" || input == "N") {
                targetEventID = eventID + 1;
                break;
            } else if (input == "p" || input == "P") {
                if (eventID > 0) {
                    std::rewind(fp);
                    targetEventID = eventID - 1;
                    eventID = 0;
                    requires_rewind = true;
                    break;
                }
                std::cout << "\033[1;31mAlready at the first event.\033[0m\n";
            } else if (input == "q" || input == "Q") {
                std::cout << "\n\033[1;33mUser requested exit.\033[0m\n";
                std::fclose(fp);
                return 0;
            } else if (input == "j" || input == "J") {
                std::cout << "Enter target event number: " << std::flush;
                std::string destStr;
                if (!std::getline(std::cin, destStr)) break;
                try {
                    int jump_idx = std::stoi(destStr);
                    if (jump_idx < 0) {
                        std::cout << "\033[1;31mEvent number cannot be negative.\033[0m\n";
                    } else if (jump_idx <= static_cast<int>(eventID)) {
                        std::rewind(fp);
                        eventID = 0;
                        targetEventID = static_cast<unsigned int>(jump_idx);
                        requires_rewind = true;
                        break;
                    } else {
                        targetEventID = static_cast<unsigned int>(jump_idx);
                        break;
                    }
                } catch (...) {
                    std::cout << "\033[1;31mInvalid number.\033[0m\n";
                }
            }
        }

        if (requires_rewind) continue;
        eventID++;
    }

    std::cout << "\n\033[1;32m[ Interactive Display Terminated ]\033[0m\n";
    std::fclose(fp);
    return 0;
}
