#include <iostream>
#include <string>
#include <vector>
#include <csignal>
#include "DaqSystem.h"

// 이 파일은 이전의 main.cpp와 내용이 동일합니다.
// frontend_500_mini 실행 파일의 시작점입니다.

std::unique_ptr<DaqSystem> g_daq_system = nullptr;

void signal_handler(int signal) {
    if (g_daq_system) {
        g_daq_system->stop();
    }
}

void print_usage() {
    std::cerr << "Usage: frontend_500_mini -f <config_file> -o <output_file_base> -n <num_events>" << std::endl;
}

int main(int argc, char* argv[]) {
    std::string config_file;
    std::string outfile_base;
    int n_events = 10000;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-f") {
            if (i + 1 < argc) config_file = argv[++i];
        } else if (arg == "-o") {
            if (i + 1 < argc) outfile_base = argv[++i];
        } else if (arg == "-n") {
            if (i + 1 < argc) n_events = std::stoi(argv[++i]);
        }
    }

    if (config_file.empty() || outfile_base.empty()) {
        print_usage();
        return 1;
    }

    g_daq_system = std::make_unique<DaqSystem>();
    signal(SIGINT, signal_handler);

    if (!g_daq_system->loadConfig(config_file)) return 1;
    if (!g_daq_system->initialize()) return 1;

    g_daq_system->run(n_events, outfile_base);
    g_daq_system->shutdown();

    return 0;
}
