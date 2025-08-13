#include <iostream>
#include <string>
#include <vector>
#include <csignal>
#include "DaqSystem.h"

std::unique_ptr<DaqSystem> g_daq_system = nullptr;

void signal_handler(int signal) {
    if (g_daq_system) {
        g_daq_system->stop();
    }
}

void print_usage() {
    std::cerr << "Usage: frontend_500_mini -f <config_file> -o <output_file_base> [-n <num_events> | -t <seconds>]" << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  -n <num_events> : Set the number of events to acquire." << std::endl;
    std::cerr << "  -t <seconds>    : Set the duration of acquisition in seconds." << std::endl;
    std::cerr << "  Note: -n and -t are mutually exclusive." << std::endl;
}

int main(int argc, char* argv[]) {
    std::string config_file;
    std::string outfile_base;
    int n_events = 0;
    int duration_sec = 0;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-f") {
            if (i + 1 < argc) config_file = argv[++i];
        } else if (arg == "-o") {
            if (i + 1 < argc) outfile_base = argv[++i];
        } else if (arg == "-n") {
            if (i + 1 < argc) n_events = std::stoi(argv[++i]);
        } else if (arg == "-t") {
            if (i + 1 < argc) duration_sec = std::stoi(argv[++i]);
        }
    }

    if (config_file.empty() || outfile_base.empty()) {
        print_usage();
        return 1;
    }

    if (n_events > 0 && duration_sec > 0) {
        std::cerr << "Error: -n and -t options cannot be used at the same time." << std::endl;
        print_usage();
        return 1;
    }

    if (n_events <= 0 && duration_sec <= 0) {
        std::cerr << "Error: You must specify a run condition using -n or -t." << std::endl;
        print_usage();
        return 1;
    }

    g_daq_system = std::make_unique<DaqSystem>();
    signal(SIGINT, signal_handler);

    if (!g_daq_system->loadConfig(config_file)) return 1;
    if (!g_daq_system->initialize()) return 1;

    g_daq_system->run(n_events, duration_sec, outfile_base);
    g_daq_system->shutdown();

    return 0;
}
