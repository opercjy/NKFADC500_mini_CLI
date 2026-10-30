#ifndef NOTICE_FADC500_SPEC_HH
#define NOTICE_FADC500_SPEC_HH

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// =============================================================================
// Authoritative Hardware Register & Packet Specification for NOTICE NKFADC500-4
// Derived directly from bundled vendor SDK (notice_nkfadc500_4):
//   - notice/src/nkfadc500/nkfadc500/NoticeNKFADC500.{h,c}
//   - notice/src/usb3com/usb3com/usb3com.c
//   - notice/test/mini_nkfadc500/nkfadc500_4_test.C
// =============================================================================
namespace NoticeFadc500Spec {

constexpr int kNumChannels = 4;
constexpr uint32_t kUsbReadErrorWord = 0xFFFFFFFFU;

// Maximum onboard DRAM buffer size: 64 MB = 65,536 KB (NoticeNKFADC500.c:538).
// IMPORTANT: Never mask BCOUNT with 0xFFFF — 65,536 KB is 0x10000 and would wrap to 0!
constexpr uint32_t kMaxDramKb = 65536U;

// USB 3.0 FX3 Slave FIFO bulk transfer granularity (usb3com.c: NBURST=16 -> 16 KB,
// nkfadc500_4_test.C:262 -> 128 KB streaming slice alignment).
constexpr uint32_t kUsbBurstAlignKb = 16U;
constexpr uint32_t kStreamSliceAlignKb = 128U;

// Hardware register valid ranges (nkfadc500_4_test.C:18-50 & NoticeNKFADC500.c)
constexpr int kMinPrescale = 1;
constexpr int kMaxPrescale = 65535;
constexpr int kMaxPtrigMs  = 65535;
constexpr int kMaxDacOff   = 4095;      // 12-bit offset DAC
constexpr int kMinThrHeight = 1;
constexpr int kMaxThrHeight = 4095;     // 12-bit pulse-height threshold
constexpr int kMaxThrPeakSum = 67108863;// 26-bit (0x3FFFFFF) peak-sum threshold
constexpr int kMaxTmodeMask = 0x0F;     // bit0:ten, bit1:tew, bit2:tep, bit3:tep2
constexpr int kMaxTrigEnMask = 0x0F;    // bit0:self, bit1:ped, bit2:soft, bit3:ext
constexpr int kMinCwNs     = 8;
constexpr int kMaxCwNs     = 32760;
constexpr int kMaxDlyNs    = 31992;     // ((data/1000)<<10)|(data%1000): 5-bit coarse 0..31!
constexpr int kMinPswNs    = 2;
constexpr int kMaxPswNs    = 16382;
constexpr int kMinPct      = 1;
constexpr int kMaxPct      = 15;
constexpr int kMinPciNs    = 32;
constexpr int kMaxPciNs    = 8160;
constexpr int kMinPwtNs    = 2;
constexpr int kMaxPwtNs    = 1022;
constexpr int kMaxDtNs     = 8355840;

// Valid sampling rate dividers DSR in {1, 2, 4, 8} (500, 250, 125, 62.5 MS/s)
inline int NormalizeSamplingDivider(int dsr, bool* changed = nullptr) {
    constexpr std::array<int, 4> kValid = {1, 2, 4, 8};
    for (int v : kValid) {
        if (dsr == v) {
            if (changed) *changed = false;
            return v;
        }
    }
    int best = 1;
    int bestDist = 999999;
    for (int v : kValid) {
        int d = (dsr > v) ? (dsr - v) : (v - dsr);
        if (d < bestDist) {
            bestDist = d;
            best = v;
        }
    }
    if (changed) *changed = true;
    return best;
}

// Valid recording length codes RL in {1, 2, 4, 8, 16, 32, 64, 128, 256}
// (1 = 128 ns [48 pts], ..., 8 = 1 us [496 pts], 16 = 2 us [1008 pts], ..., 256 = 32 us [16368 pts])
inline int NormalizeRecordLengthCode(int rl, bool* changed = nullptr) {
    constexpr std::array<int, 9> kValid = {1, 2, 4, 8, 16, 32, 64, 128, 256};
    for (int v : kValid) {
        if (rl == v) {
            if (changed) *changed = false;
            return v;
        }
    }
    int clamped = std::clamp(rl, 1, 256);
    int best = 1;
    for (int v : kValid) {
        if (v <= clamped) best = v;
    }
    if (changed) *changed = true;
    return best;
}

inline bool IsPeakSumTriggerMode(int tmode) {
    return (tmode & 0x0C) != 0; // tep (bit 2 = 4) or tep2 (bit 3 = 8)
}

// Safe 64-bit decoding of Notice NKFADC500-4 128-byte 4-channel byte-interleaved header
struct DecodedPacketHeader {
    uint32_t data_length = 0;        // Per-channel byte length (32 + 2*N_samples); total packet = data_length * 4
    uint32_t trigger_type = 0;       // Low nibble of byte 6*4
    uint32_t trigger_destination = 0;// High nibble of byte 6*4
    uint32_t tcb_trig_num = 0;       // Global TCB trigger number (bytes 7..10 * 4)
    uint32_t local_trig_num = 0;     // Local FPGA trigger number (bytes 17..20 * 4)
    uint32_t trigger_pattern = 0;    // Local trigger pattern bitmask (bytes 21..24 * 4)
    uint64_t tcb_trig_time_ns = 0;   // External TCB timestamp (24-bit coarse * 1000 ns + 8-bit fine * 8 ns)
    uint64_t local_trig_time_ns = 0; // Local FPGA 48-bit coarse * 1000 ns + 8-bit fine * 8 ns
    uint64_t best_trig_time_ns = 0;  // Prefers 48-bit monotonic local FPGA clock when non-zero
    uint8_t  mid = 0;                // Module ID in packet header (byte 15*4 = 60)
    std::array<uint8_t, 4> channel_ids = {0, 0, 0, 0}; // Expected {1, 2, 3, 4} at bytes 64..67
    std::array<int, 4> hw_pedestal = {0, 0, 0, 0};     // Hardware-measured pedestals at bytes 16..23
    bool valid_channel_tags = false;
    bool valid_geometry = false;
};

inline DecodedPacketHeader DecodeHeader128(const unsigned char* h) {
    DecodedPacketHeader d;
    d.data_length =
        static_cast<uint32_t>(h[0]) |
        (static_cast<uint32_t>(h[4]) << 8) |
        (static_cast<uint32_t>(h[8]) << 16) |
        (static_cast<uint32_t>(h[12]) << 24);

    for (int ch = 0; ch < 4; ++ch) {
        d.hw_pedestal[ch] =
            static_cast<int>(h[16 + ch]) |
            (static_cast<int>(h[20 + ch]) << 8);
        d.channel_ids[ch] = h[64 + ch];
    }

    d.trigger_type        = static_cast<uint32_t>(h[24] & 0x0F);
    d.trigger_destination = static_cast<uint32_t>((h[24] >> 4) & 0x0F);

    d.tcb_trig_num =
        static_cast<uint32_t>(h[28]) |
        (static_cast<uint32_t>(h[32]) << 8) |
        (static_cast<uint32_t>(h[36]) << 16) |
        (static_cast<uint32_t>(h[40]) << 24);

    const uint64_t tcb_fine_ns = static_cast<uint64_t>(h[44]) * 8ULL;
    const uint64_t tcb_coarse_us =
        static_cast<uint64_t>(h[48]) |
        (static_cast<uint64_t>(h[52]) << 8) |
        (static_cast<uint64_t>(h[56]) << 16);
    d.tcb_trig_time_ns = tcb_fine_ns + tcb_coarse_us * 1000ULL;

    d.mid = h[60];

    d.local_trig_num =
        static_cast<uint32_t>(h[68]) |
        (static_cast<uint32_t>(h[72]) << 8) |
        (static_cast<uint32_t>(h[76]) << 16) |
        (static_cast<uint32_t>(h[80]) << 24);

    d.trigger_pattern =
        static_cast<uint32_t>(h[84]) |
        (static_cast<uint32_t>(h[88]) << 8) |
        (static_cast<uint32_t>(h[92]) << 16) |
        (static_cast<uint32_t>(h[96]) << 24);

    // Note: Vendor macro nkfadc500_4_test.C:357 had a typo indexing data[k+29..31] instead of
    // interleaved offsets 29*4=116, 30*4=120, 31*4=124. We decode the true 4-byte-interleaved offsets.
    const uint64_t loc_fine_ns = static_cast<uint64_t>(h[100]) * 8ULL;
    const uint64_t loc_coarse_us =
        static_cast<uint64_t>(h[104]) |
        (static_cast<uint64_t>(h[108]) << 8) |
        (static_cast<uint64_t>(h[112]) << 16) |
        (static_cast<uint64_t>(h[116]) << 24) |
        (static_cast<uint64_t>(h[120]) << 32) |
        (static_cast<uint64_t>(h[124]) << 40);
    d.local_trig_time_ns = loc_fine_ns + loc_coarse_us * 1000ULL;

    d.best_trig_time_ns = (d.local_trig_time_ns > 0ULL) ? d.local_trig_time_ns : d.tcb_trig_time_ns;

    // Zero-suppressed packets legitimately have data_length == 32 (header-only, 0 samples);
    // normal RL=1..256 packets have data_length = 128..32768 (even, >= 32).
    d.valid_geometry = (d.data_length >= 32U) && (d.data_length <= 1048576U) && ((d.data_length - 32U) % 2U == 0U);
    d.valid_channel_tags =
        (d.channel_ids[0] == 1 && d.channel_ids[1] == 2 && d.channel_ids[2] == 3 && d.channel_ids[3] == 4) ||
        (d.channel_ids[0] == 0 && d.channel_ids[1] == 0 && d.channel_ids[2] == 0 && d.channel_ids[3] == 0);
    return d;
}

// Compute safe pre-trigger baseline sample count from DLY and dt_ns without overlapping the leading edge
inline int ComputeBaselineSamples(double delay_ns, double dt_ns, int record_length) {
    if (record_length <= 0) return 0;
    if (dt_ns <= 0.0) dt_ns = 2.0;
    if (delay_ns <= 0.0) {
        return std::min(8, record_length);
    }
    const int preTrigPts = static_cast<int>(delay_ns / dt_ns);
    if (preTrigPts <= 4) {
        return std::min(std::max(1, preTrigPts), record_length);
    }
    const int safeWindow = std::max(4, static_cast<int>(preTrigPts * 0.45));
    return std::clamp(safeWindow, 1, std::min(preTrigPts - 2, record_length));
}

// Lightweight shared configuration reader used by both production_nkfadc_500 and online_nkfadc500
struct StreamDecodeConfig {
    double dt_ns = 2.0;
    std::array<double, 4> delay_ns = {400.0, 400.0, 400.0, 400.0};
    std::array<int, 4> polarity = {0, 0, 0, 0}; // 0: negative pulse, 1: positive pulse
    int config_run_number = 0;
    std::string loaded_path;
};

inline StreamDecodeConfig LoadStreamDecodeConfig(const std::string& configPath) {
    StreamDecodeConfig cfg;
    if (configPath.empty()) return cfg;
    std::ifstream in(configPath);
    if (!in.is_open()) return cfg;
    cfg.loaded_path = configPath;

    std::string line;
    while (std::getline(in, line)) {
        const size_t hashPos = line.find('#');
        if (hashPos != std::string::npos) line = line.substr(0, hashPos);
        std::istringstream iss(line);
        std::string key;
        if (!(iss >> key)) continue;

        auto parseTok = [](const std::string& t, long& out) -> bool {
            try {
                size_t idx = 0;
                out = std::stol(t, &idx, 0);
                return idx == t.size();
            } catch (...) {
                return false;
            }
        };

        if (key == "RUN_NUMBER") {
            std::string tok; long v = 0;
            if ((iss >> tok) && parseTok(tok, v)) cfg.config_run_number = static_cast<int>(v);
        } else if (key == "SAMPLING_RATE") {
            std::string tok; long v = 1;
            if ((iss >> tok) && parseTok(tok, v)) {
                const int dsr = NormalizeSamplingDivider(static_cast<int>(v));
                cfg.dt_ns = 2.0 * static_cast<double>(dsr);
            }
        } else if (key == "DLY") {
            std::vector<double> vals;
            double v = 0.0;
            while (iss >> v) vals.push_back(std::max(0.0, v));
            if (vals.size() == 1) cfg.delay_ns.fill(vals[0]);
            else {
                for (size_t i = 0; i < std::min<size_t>(4, vals.size()); ++i) cfg.delay_ns[i] = vals[i];
            }
        } else if (key == "POL") {
            std::vector<int> vals;
            std::string tok;
            while (iss >> tok) {
                long v = 0;
                if (parseTok(tok, v)) vals.push_back(std::clamp(static_cast<int>(v), 0, 1));
            }
            if (vals.size() == 1) cfg.polarity.fill(vals[0]);
            else {
                for (size_t i = 0; i < std::min<size_t>(4, vals.size()); ++i) cfg.polarity[i] = vals[i];
            }
        }
    }
    return cfg;
}

// Locate companion <input_stem>.cfg first, then explicit -f <cfg>, then repo config/settings.cfg
inline std::string ResolveCompanionConfigPath(
    const std::string& explicitCfg,
    const std::string& inputFile,
    const char* argv0 = nullptr)
{
    if (!explicitCfg.empty() && std::filesystem::exists(explicitCfg)) {
        return explicitCfg;
    }
    if (!inputFile.empty()) {
        std::filesystem::path companion(inputFile);
        companion.replace_extension(".cfg");
        if (std::filesystem::exists(companion)) {
            return companion.string();
        }
    }
    if (std::filesystem::exists("config/settings.cfg")) {
        return "config/settings.cfg";
    }
    if (std::filesystem::exists("../config/settings.cfg")) {
        return "../config/settings.cfg";
    }
    if (argv0) {
        std::error_code ec;
        auto exeDir = std::filesystem::weakly_canonical(std::filesystem::path(argv0), ec).parent_path();
        auto candidate = exeDir / "../config/settings.cfg";
        if (std::filesystem::exists(candidate)) {
            return candidate.string();
        }
    }
    return explicitCfg;
}

} // namespace NoticeFadc500Spec

#endif
