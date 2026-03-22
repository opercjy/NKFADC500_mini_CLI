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

        // 주석(#) 제거
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }

        std::istringstream iss(line);
        std::string key;
        if (!(iss >> key)) continue; 

        if (key == "RUN_NUMBER") {
            int run;
            if (iss >> run) runInfo->SetRunNumber(run);
        }
        else if (key == "BOARD") {
            int mid;
            if (iss >> mid) current_bd = runInfo->AddFadcBD(mid);
        }
        else if (key == "THR" || key == "DACOFF" || key == "POL" || key == "DLY") {
            if (!current_bd) {
                ELog::Print(ELog::WARNING, Form("Line %d: '%s' ignored (No BOARD defined).", line_num, key.c_str()));
                continue;
            }

            // 남은 값들을 모두 읽어 벡터에 저장
            std::vector<int> values;
            int val;
            while (iss >> val) {
                values.push_back(val);
            }

            if (values.empty()) continue;

            // 💡 [핵심] 값이 1개면 4채널에 일괄 적용, 값이 4개면 개별 적용
            if (values.size() == 1) {
                for (int i = 0; i < current_bd->NCHANNEL(); i++) {
                    if (key == "THR")         current_bd->SetTHR(i, values[0]);
                    else if (key == "DACOFF") current_bd->SetDACOFF(i, values[0]);
                    else if (key == "POL")    current_bd->SetPOL(i, values[0]);
                    else if (key == "DLY")    current_bd->SetDLY(i, values[0]);
                }
            } 
            else {
                for (size_t i = 0; i < values.size() && i < (size_t)current_bd->NCHANNEL(); i++) {
                    if (key == "THR")         current_bd->SetTHR(i, values[i]);
                    else if (key == "DACOFF") current_bd->SetDACOFF(i, values[i]);
                    else if (key == "POL")    current_bd->SetPOL(i, values[i]);
                    else if (key == "DLY")    current_bd->SetDLY(i, values[i]);
                }
            }
        }
        else {
            ELog::Print(ELog::WARNING, Form("Line %d: Unknown config key '%s'.", line_num, key.c_str()));
        }
    }

    file.close();
    return true;
}
