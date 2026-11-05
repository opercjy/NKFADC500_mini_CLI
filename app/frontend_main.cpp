#include <algorithm>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <string>
#include <thread>

#include "BinaryDaqManager.hh"
#include "ConfigParser.hh"
#include "ELog.hh"
#include "NoticeFadc500Spec.hh"
#include "RunInfo.hh"
#include "TFile.h"
#include "TString.h"

namespace {
// Async-signal-safe termination flag (no mutex/malloc/stdio inside POSIX signal handler!)
volatile std::sig_atomic_t gStopRequested = 0;

void SignalHandler(int /*signum*/) {
    gStopRequested = 1;
}

void PrintUsage(const char* progName) {
    std::cout << "\n\033[1;36m======================================================================\033[0m\n";
    std::cout << "\033[1;32m      NKFADC500 Mini - High-Speed Data Acquisition Frontend\033[0m\n";
    std::cout << "\033[1;36m======================================================================\033[0m\n";
    std::cout << "\033[1;33mUsage:\033[0m " << progName << " [options]\n\n";
    std::cout << "\033[1;37m[Options]\033[0m\n";
    std::cout << "  -f <config>   : Path to settings.cfg (default: config/settings.cfg)\n";
    std::cout << "  -o <file>     : Output raw data file (default: data/test_noise.dat)\n";
    std::cout << "  -n <events>   : Stop after N events (default: 0 = infinite)\n";
    std::cout << "  -t <sec>      : Stop after T seconds (default: 0 = infinite)\n";
    std::cout << "  --thr <adc>   : Override hardware trigger threshold THR (1..4095 height, 1..67108863 peak-sum)\n";
    std::cout << "  -h, --help    : Print this help message\n";
    std::cout << "\033[1;36m======================================================================\033[0m\n\n";
}
} // namespace

int main(int argc, char** argv) {
    std::string configFile = "config/settings.cfg";
    std::string outFile = "data/test_noise.dat";
    int maxEvents = 0;
    int maxTime = 0;
    int thrOverride = -1;

    static struct option long_options[] = {
        {"config", required_argument, nullptr, 'f'},
        {"output", required_argument, nullptr, 'o'},
        {"events", required_argument, nullptr, 'n'},
        {"time",   required_argument, nullptr, 't'},
        {"thr",    required_argument, nullptr, 'T'},
        {"help",   no_argument,       nullptr, 'h'},
        {nullptr,  0,                 nullptr, 0}
    };

    int opt = 0;
    while ((opt = getopt_long(argc, argv, "f:o:n:t:T:h", long_options, nullptr)) != -1) {
        switch (opt) {
            case 'f': configFile = optarg; break;
            case 'o': outFile = optarg; break;
            case 'n': maxEvents = std::max(0, std::atoi(optarg)); break;
            case 't': maxTime = std::max(0, std::atoi(optarg)); break;
            case 'T': thrOverride = std::clamp(std::atoi(optarg), 0, NoticeFadc500Spec::kMaxThrPeakSum); break;
            case 'h': PrintUsage(argv[0]); return 0;
            default:  PrintUsage(argv[0]); return 1;
        }
    }

    if (argc == 1) {
        PrintUsage(argv[0]);
        return 1;
    }

    // Fallback check if launched from bin/ or an arbitrary external directory without -f
    if (!std::filesystem::exists(configFile)) {
        const std::string resolvedCfg = NoticeFadc500Spec::ResolveCompanionConfigPath(configFile, "", argv[0]);
        if (!resolvedCfg.empty() && std::filesystem::exists(resolvedCfg)) {
            configFile = resolvedCfg;
        } else if (const char* envRoot = std::getenv("NKFADC500_ROOT")) {
            std::filesystem::path cand = std::filesystem::path(envRoot) / "config" / "settings.cfg";
            if (std::filesystem::exists(cand)) {
                configFile = cand.string();
            }
        }
    }

    std::signal(SIGINT, SignalHandler);
    std::signal(SIGTERM, SignalHandler);

    RunInfo runInfo;
    if (!ConfigParser::Parse(configFile, &runInfo)) {
        return 1;
    }

    // Apply optional CLI threshold override (used by GUI Threshold Auto Scan)
    int appliedThrLog = thrOverride;
    if (thrOverride >= 0) {
        if (thrOverride < NoticeFadc500Spec::kMinThrHeight) {
            ELog::Print(ELog::WARNING, "--thr=0 causes continuous noise trigger lockup; clamped to 1.");
        }
        for (int b = 0; b < runInfo.GetNFadcBD(); ++b) {
            FadcBD* bd = runInfo.GetFadcBD(b);
            if (!bd) continue;
            for (int ch = 0; ch < bd->NCHANNEL(); ++ch) {
                const int maxThr = NoticeFadc500Spec::IsPeakSumTriggerMode(bd->GetTMODE(ch))
                    ? NoticeFadc500Spec::kMaxThrPeakSum
                    : NoticeFadc500Spec::kMaxThrHeight;
                const int clamped = std::clamp(thrOverride, NoticeFadc500Spec::kMinThrHeight, maxThr);
                bd->SetTHR(ch, clamped);
                appliedThrLog = clamped;
            }
        }
        ELog::Print(ELog::INFO, Form("Applied CLI threshold override: THR = %d on all channels.", appliedThrLog));
    }

    // Ensure output parent directory exists before hardware init & DAQ start
    std::filesystem::path outPath(outFile);
    if (outPath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(outPath.parent_path(), ec);
    }

    BinaryDaqManager daqManager(&runInfo);

    // Serialize hardware-verified RunInfo / FadcBD data structure (<run>_cfg.root) + text backup (<run>.cfg)
    try {
        std::filesystem::path backupCfgPath = outPath;
        backupCfgPath.replace_extension(".cfg");
        std::filesystem::copy_file(configFile, backupCfgPath, std::filesystem::copy_options::overwrite_existing);
        if (thrOverride >= 0) {
            std::ofstream ofs(backupCfgPath, std::ios::app);
            ofs << "\n# CLI/Scan Threshold Override\nTHR " << appliedThrLog << "\n";
        }

        std::filesystem::path cfgRootPath = outPath;
        cfgRootPath.replace_extension("");
        cfgRootPath += "_cfg.root";
        TFile* cfgRootFile = TFile::Open(cfgRootPath.string().c_str(), "RECREATE");
        if (cfgRootFile && !cfgRootFile->IsZombie()) {
            runInfo.Write("RunInfo", TObject::kSingleKey);
            if (FadcBD* bd0 = runInfo.GetFadcBD(0)) {
                bd0->Write("FadcBD", TObject::kSingleKey);
            }
            cfgRootFile->Close();
            delete cfgRootFile;
        }
        ELog::Print(ELog::INFO,
            Form("Configuration serialized to: %s (and %s)",
                 cfgRootPath.string().c_str(), backupCfgPath.string().c_str()));
    } catch (const std::exception& e) {
        ELog::Print(ELog::WARNING, std::string("Could not serialize configuration metadata: ") + e.what());
    }

    daqManager.Start(outFile, maxEvents, maxTime);

    while (daqManager.IsRunning()) {
        if (gStopRequested != 0) {
            std::cout << "\n";
            ELog::Print(ELog::WARNING, "Interrupt signal received! Shutting down DAQ gracefully...");
            daqManager.Stop();
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    daqManager.Stop();
    ELog::Print(ELog::INFO, "DAQ System fully stopped and safely exited.");
    return 0;
}
