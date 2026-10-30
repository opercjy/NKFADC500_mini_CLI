#include "ConfigParser.hh"
#include "ELog.hh"
#include "NoticeFadc500Spec.hh"

#include <algorithm>
#include <array>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
// Supports decimal (e.g. 65534) AND hexadecimal (e.g. 0xFFFE from TLT Builder)
bool ParseIntToken(const std::string& tok, int& out) {
    try {
        size_t idx = 0;
        long long v = std::stoll(tok, &idx, 0);
        if (idx != tok.size()) return false;
        if (v < -2147483648LL) v = -2147483648LL;
        if (v >  2147483647LL) v =  2147483647LL;
        out = static_cast<int>(v);
        return true;
    } catch (...) {
        return false;
    }
}

int ClampWithWarn(const char* key, int line_num, int val, int lo, int hi) {
    if (val < lo || val > hi) {
        const int clamped = std::clamp(val, lo, hi);
        ELog::Print(ELog::WARNING,
            Form("Line %d: %s=%d out of hardware range [%d, %d]; clamped to %d.",
                 line_num, key, val, lo, hi, clamped));
        return clamped;
    }
    return val;
}

struct DeferredBoardLines {
    std::array<int, 4> thr_line = {0, 0, 0, 0};
    std::array<bool, 4> thr_seen = {false, false, false, false};
    std::array<int, 4> dly_line = {0, 0, 0, 0};
};
} // namespace

bool ConfigParser::Parse(const std::string& filename, RunInfo* runInfo) {
    if (!runInfo) {
        ELog::Print(ELog::FATAL, "ConfigParser::Parse received null RunInfo pointer.");
        return false;
    }

    std::ifstream file(filename);
    if (!file.is_open()) {
        ELog::Print(ELog::FATAL, Form("Cannot open configuration file: %s", filename.c_str()));
        return false;
    }

    ELog::Print(ELog::INFO, Form("Loading Configuration: %s", filename.c_str()));

    std::string line;
    int line_num = 0;
    FadcBD* current_bd = nullptr;
    std::unordered_map<FadcBD*, DeferredBoardLines> deferred_meta;

    while (std::getline(file, line)) {
        line_num++;

        const size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        std::istringstream iss(line);
        std::string key;
        if (!(iss >> key)) continue;

        auto readSingleInt = [&](int& out) -> bool {
            std::string tok;
            if (!(iss >> tok)) return false;
            if (!ParseIntToken(tok, out)) {
                ELog::Print(ELog::WARNING, Form("Line %d: Invalid integer token '%s' for key '%s'.",
                                                line_num, tok.c_str(), key.c_str()));
                return false;
            }
            return true;
        };

        // Global / board-level settings
        if (key == "RUN_NUMBER") {
            int run = 0;
            if (readSingleInt(run)) runInfo->SetRunNumber(std::max(0, run));
        } else if (key == "BOARD") {
            int mid = 0;
            if (readSingleInt(mid)) current_bd = runInfo->AddFadcBD(mid);
        } else if (key == "SAMPLING_RATE") {
            int val = 1;
            if (readSingleInt(val) && current_bd) {
                bool changed = false;
                const int norm = NoticeFadc500Spec::NormalizeSamplingDivider(val, &changed);
                if (changed) {
                    ELog::Print(ELog::WARNING,
                        Form("Line %d: SAMPLING_RATE=%d is not in vendor DSR set {1, 2, 4, 8}; normalized to %d.",
                             line_num, val, norm));
                }
                current_bd->SetSAMPLING(norm);
            }
        } else if (key == "RECORD_LEN") {
            int val = 8;
            if (readSingleInt(val) && current_bd) {
                bool changed = false;
                const int norm = NoticeFadc500Spec::NormalizeRecordLengthCode(val, &changed);
                if (changed) {
                    ELog::Print(ELog::WARNING,
                        Form("Line %d: RECORD_LEN=%d must be a power-of-2 in {1..256}; normalized to %d.",
                             line_num, val, norm));
                }
                current_bd->SetRL(norm);
            }
        } else if (key == "PRESCALE") {
            int val = 1;
            if (readSingleInt(val) && current_bd) {
                current_bd->SetPRESCALE(ClampWithWarn(
                    "PRESCALE", line_num, val,
                    NoticeFadc500Spec::kMinPrescale, NoticeFadc500Spec::kMaxPrescale));
            }
        } else if (key == "TRIG_TLT") {
            int val = 0xFFFE;
            if (readSingleInt(val) && current_bd) {
                current_bd->SetTLT(ClampWithWarn("TRIG_TLT", line_num, val, 0, 0xFFFF));
            }
        } else if (key == "TRIG_ENABLE") {
            int val = 1;
            if (readSingleInt(val) && current_bd) {
                current_bd->SetTRIGEN(ClampWithWarn(
                    "TRIG_ENABLE", line_num, val, 0, NoticeFadc500Spec::kMaxTrigEnMask));
            }
        } else if (key == "PTRIG_INT") {
            int val = 0;
            if (readSingleInt(val) && current_bd) {
                current_bd->SetPTRIG(ClampWithWarn(
                    "PTRIG_INT", line_num, val, 0, NoticeFadc500Spec::kMaxPtrigMs));
            }
        } else if (key == "ZERO_SUP") {
            if (!current_bd) {
                ELog::Print(ELog::WARNING, Form("Line %d: 'ZERO_SUP' ignored (No BOARD defined yet).", line_num));
                continue;
            }
            std::vector<int> vals;
            std::string tok;
            while (iss >> tok) {
                int v = 0;
                if (ParseIntToken(tok, v)) vals.push_back(v);
            }
            if (vals.size() == 1) {
                current_bd->SetZEROSUP(ClampWithWarn("ZERO_SUP", line_num, vals[0], 0, 0x0F));
            } else if (vals.size() >= 4) {
                int mask = 0;
                for (int c = 0; c < 4; ++c) {
                    if (ClampWithWarn("ZERO_SUP", line_num, vals[c], 0, 1) != 0) mask |= (1 << c);
                }
                current_bd->SetZEROSUP(mask);
            }
        } else {
            // Per-channel array or broadcast settings
            if (!current_bd) {
                ELog::Print(ELog::WARNING, Form("Line %d: '%s' ignored (No BOARD defined yet).", line_num, key.c_str()));
                continue;
            }

            std::vector<int> values;
            std::string tok;
            while (iss >> tok) {
                int val = 0;
                if (ParseIntToken(tok, val)) {
                    values.push_back(val);
                } else {
                    ELog::Print(ELog::WARNING, Form("Line %d: Invalid token '%s' in '%s'.",
                                                    line_num, tok.c_str(), key.c_str()));
                }
            }
            if (values.empty()) continue;

            const bool isGlobal = (values.size() == 1);
            const int nCh = current_bd->NCHANNEL();
            const int loop_end = isGlobal ? nCh : std::min(static_cast<int>(values.size()), nCh);
            auto& meta = deferred_meta[current_bd];

            for (int i = 0; i < loop_end; i++) {
                const int apply_val = isGlobal ? values[0] : values[i];

                if (key == "THR") {
                    // Defer upper-bound check until after all lines are parsed so THR preceding TMODE=4 works!
                    current_bd->SetTHR(i, apply_val);
                    meta.thr_line[i] = line_num;
                    meta.thr_seen[i] = true;
                } else if (key == "DACOFF") {
                    current_bd->SetDACOFF(i, ClampWithWarn(
                        "DACOFF", line_num, apply_val, 0, NoticeFadc500Spec::kMaxDacOff));
                } else if (key == "POL") {
                    current_bd->SetPOL(i, ClampWithWarn("POL", line_num, apply_val, 0, 1));
                } else if (key == "DLY") {
                    current_bd->SetDLY(i, ClampWithWarn(
                        "DLY", line_num, apply_val, 0, NoticeFadc500Spec::kMaxDlyNs));
                    meta.dly_line[i] = line_num;
                } else if (key == "CW") {
                    current_bd->SetCW(i, ClampWithWarn(
                        "CW", line_num, apply_val,
                        NoticeFadc500Spec::kMinCwNs, NoticeFadc500Spec::kMaxCwNs));
                } else if (key == "AMODE") {
                    current_bd->SetAMODE(i, ClampWithWarn("AMODE", line_num, apply_val, 0, 1));
                } else if (key == "TMODE") {
                    current_bd->SetTMODE(i, ClampWithWarn(
                        "TMODE", line_num, apply_val, 0, NoticeFadc500Spec::kMaxTmodeMask));
                } else if (key == "DT") {
                    current_bd->SetDT(i, ClampWithWarn(
                        "DT", line_num, apply_val, 0, NoticeFadc500Spec::kMaxDtNs));
                } else if (key == "PSW") {
                    current_bd->SetPSW(i, ClampWithWarn(
                        "PSW", line_num, apply_val,
                        NoticeFadc500Spec::kMinPswNs, NoticeFadc500Spec::kMaxPswNs));
                } else if (key == "PCT") {
                    current_bd->SetPCT(i, ClampWithWarn(
                        "PCT", line_num, apply_val,
                        NoticeFadc500Spec::kMinPct, NoticeFadc500Spec::kMaxPct));
                } else if (key == "PCI") {
                    current_bd->SetPCI(i, ClampWithWarn(
                        "PCI", line_num, apply_val,
                        NoticeFadc500Spec::kMinPciNs, NoticeFadc500Spec::kMaxPciNs));
                } else if (key == "PWT") {
                    current_bd->SetPWT(i, ClampWithWarn(
                        "PWT", line_num, apply_val,
                        NoticeFadc500Spec::kMinPwtNs, NoticeFadc500Spec::kMaxPwtNs));
                } else {
                    ELog::Print(ELog::WARNING, Form("Line %d: Unknown key '%s'.", line_num, key.c_str()));
                    break;
                }
            }
        }
    }

    if (runInfo->GetNFadcBD() == 0) {
        ELog::Print(ELog::ERROR, Form("No BOARD entry found in configuration file: %s", filename.c_str()));
        return false;
    }

    // Post-parse cross-register hardware validation pass (order-independent!)
    for (int b = 0; b < runInfo->GetNFadcBD(); ++b) {
        FadcBD* bd = runInfo->GetFadcBD(b);
        if (!bd) continue;
        const auto& meta = deferred_meta[bd];

        for (int ch = 0; ch < bd->NCHANNEL(); ++ch) {
            const int tmode = bd->GetTMODE(ch);
            const int maxThr = NoticeFadc500Spec::IsPeakSumTriggerMode(tmode)
                ? NoticeFadc500Spec::kMaxThrPeakSum
                : NoticeFadc500Spec::kMaxThrHeight;
            const int rawThr = bd->GetTHR(ch);
            const int clampedThr = ClampWithWarn(
                "THR", meta.thr_seen[ch] ? meta.thr_line[ch] : 0,
                rawThr, NoticeFadc500Spec::kMinThrHeight, maxThr);
            bd->SetTHR(ch, clampedThr);

            // NoticeNKFADC500.c:179 writes ((data/1000)<<10)|(data%1000) where data = CW + DLY.
            // Coarse delay field is 5 bits (0..31 us -> max CW + DLY = 31992 ns).
            int cw = bd->GetCW(ch);
            int dly = bd->GetDLY(ch);
            if (cw + dly > NoticeFadc500Spec::kMaxDlyNs) {
                if (cw > NoticeFadc500Spec::kMaxDlyNs) {
                    cw = NoticeFadc500Spec::kMaxDlyNs;
                    bd->SetCW(ch, cw);
                }
                const int newDly = std::max(0, NoticeFadc500Spec::kMaxDlyNs - cw);
                ELog::Print(ELog::WARNING,
                    Form("Line %d (Ch%d): CW(%d) + DLY(%d) = %d ns exceeds NKFADC500 5-bit coarse delay limit (%d ns); DLY clamped to %d ns.",
                         meta.dly_line[ch], ch, cw, dly, cw + dly, NoticeFadc500Spec::kMaxDlyNs, newDly));
                bd->SetDLY(ch, newDly);
            }
        }
    }

    return true;
}
