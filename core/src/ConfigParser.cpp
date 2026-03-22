#include "ConfigParser.hh"
#include "ELog.hh"
#include <fstream>
#include <sstream>
#include <vector>

bool ConfigParser::Parse(const std::string& filename, RunInfo* runInfo) {
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

        // 글로벌 설정
        if (key == "RUN_NUMBER") {
            int run; if (iss >> run) runInfo->SetRunNumber(run);
        }
        else if (key == "BOARD") {
            int mid; if (iss >> mid) current_bd = runInfo->AddFadcBD(mid);
        }
        else if (key == "SAMPLING_RATE") {
            int val; if (iss >> val && current_bd) current_bd->SetSAMPLING(val);
        }
        else if (key == "RECORD_LEN") {
            int val; if (iss >> val && current_bd) current_bd->SetRL(val);
        }
        else if (key == "PRESCALE") {
            int val; if (iss >> val && current_bd) current_bd->SetPRESCALE(val);
        }
        else if (key == "TRIG_TLT") {
            int val; if (iss >> val && current_bd) current_bd->SetTLT(val);
        }
        else if (key == "TRIG_ENABLE") {
            int val; if (iss >> val && current_bd) current_bd->SetTRIGEN(val);
        }
        else if (key == "PTRIG_INT") {
            int val; if (iss >> val && current_bd) current_bd->SetPTRIG(val);
        }
        // 채널별 배열 설정
        else {
            if (!current_bd) {
                ELog::Print(ELog::WARNING, Form("Line %d: '%s' ignored (No BOARD defined).", line_num, key.c_str()));
                continue;
            }

            std::vector<int> values;
            int val;
            while (iss >> val) values.push_back(val);
            if (values.empty()) continue;

            bool isGlobal = (values.size() == 1);
            int loop_end = isGlobal ? current_bd->NCHANNEL() : values.size();

            for (int i = 0; i < loop_end && i < current_bd->NCHANNEL(); i++) {
                int apply_val = isGlobal ? values[0] : values[i];

                if      (key == "THR")    current_bd->SetTHR(i, apply_val);
                else if (key == "DACOFF") current_bd->SetDACOFF(i, apply_val);
                else if (key == "POL")    current_bd->SetPOL(i, apply_val);
                else if (key == "DLY")    current_bd->SetDLY(i, apply_val);
                else if (key == "CW")     current_bd->SetCW(i, apply_val);
                else if (key == "AMODE")  current_bd->SetAMODE(i, apply_val);
                else if (key == "TMODE")  current_bd->SetTMODE(i, apply_val);
                else if (key == "DT")     current_bd->SetDT(i, apply_val);
                else if (key == "PSW")    current_bd->SetPSW(i, apply_val);
                else if (key == "PCT")    current_bd->SetPCT(i, apply_val);
                else if (key == "PCI")    current_bd->SetPCI(i, apply_val);
                else if (key == "PWT")    current_bd->SetPWT(i, apply_val);
                else {
                    ELog::Print(ELog::WARNING, Form("Line %d: Unknown key '%s'.", line_num, key.c_str()));
                    break; 
                }
            }
        }
    }
    file.close();
    return true;
}