#include "ConfigParser.hh"
#include "ELog.hh"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {
// Supports decimal (e.g. 65534) AND hexadecimal (e.g. 0xFFFE from TLT Builder)
bool ParseIntToken(const std::string& tok, int& out) {
    try {
        size_t idx = 0;
        long v = std::stol(tok, &idx, 0);
        if (idx != tok.size()) return false;
        out = static_cast<int>(v);
        return true;
    } catch (...) {
        return false;
    }
}

int ClampWithWarn(const char* key, int line_num, int val, int lo, int hi) {
    if (val < lo || val > hi) {
        int clamped = std::clamp(val, lo, hi);
        ELog::Print(ELog::WARNING,
            Form("Line %d: %s=%d out of hardware range [%d, %d]; clamped to %d.",
                 line_num, key, val, lo, hi, clamped));
        return clamped;
    }
    return val;
}
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

    while (std::getline(file, line)) {
        line_num++;

        size_t comment_pos = line.find('#');
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
            if (readSingleInt(run)) runInfo->SetRunNumber(run);
        } else if (key == "BOARD") {
            int mid = 0;
            if (readSingleInt(mid)) current_bd = runInfo->AddFadcBD(mid);
        } else if (key == "SAMPLING_RATE") {
            int val = 1;
            if (readSingleInt(val) && current_bd) {
                current_bd->SetSAMPLING(ClampWithWarn("SAMPLING_RATE", line_num, val, 1, 32));
            }
        } else if (key == "RECORD_LEN") {
            int val = 8;
            if (readSingleInt(val) && current_bd) {
                val = ClampWithWarn("RECORD_LEN", line_num, val, 1, 128);
                current_bd->SetRL(val);
            }
        } else if (key == "PRESCALE") {
            int val = 1;
            if (readSingleInt(val) && current_bd) current_bd->SetPRESCALE(std::max(0, val));
        } else if (key == "TRIG_TLT") {
            int val = 0xFFFE;
            if (readSingleInt(val) && current_bd) {
                current_bd->SetTLT(ClampWithWarn("TRIG_TLT", line_num, val, 0, 0xFFFF));
            }
        } else if (key == "TRIG_ENABLE") {
            int val = 1;
            if (readSingleInt(val) && current_bd) {
                current_bd->SetTRIGEN(ClampWithWarn("TRIG_ENABLE", line_num, val, 0, 0x0F));
            }
        } else if (key == "PTRIG_INT") {
            int val = 0;
            if (readSingleInt(val) && current_bd) current_bd->SetPTRIG(std::max(0, val));
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

            for (int i = 0; i < loop_end; i++) {
                int apply_val = isGlobal ? values[0] : values[i];

                if      (key == "THR")    current_bd->SetTHR(i, ClampWithWarn("THR", line_num, apply_val, 0, 4095));
                else if (key == "DACOFF") current_bd->SetDACOFF(i, ClampWithWarn("DACOFF", line_num, apply_val, 0, 65535));
                else if (key == "POL")    current_bd->SetPOL(i, ClampWithWarn("POL", line_num, apply_val, 0, 1));
                else if (key == "DLY")    current_bd->SetDLY(i, std::max(0, apply_val));
                else if (key == "CW")     current_bd->SetCW(i, std::max(0, apply_val));
                else if (key == "AMODE")  current_bd->SetAMODE(i, ClampWithWarn("AMODE", line_num, apply_val, 0, 1));
                else if (key == "TMODE")  current_bd->SetTMODE(i, ClampWithWarn("TMODE", line_num, apply_val, 0, 3));
                else if (key == "DT")     current_bd->SetDT(i, std::max(0, apply_val));
                else if (key == "PSW")    current_bd->SetPSW(i, std::max(0, apply_val));
                else if (key == "PCT")    current_bd->SetPCT(i, std::max(0, apply_val));
                else if (key == "PCI")    current_bd->SetPCI(i, std::max(0, apply_val));
                else if (key == "PWT")    current_bd->SetPWT(i, std::max(0, apply_val));
                else {
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

    return true;
}
