#include <iostream>
#include <string>
#include <vector>
#include "Processor.h"
#include "TApplication.h"

void print_usage() {
    std::cerr << "Usage: production_500_mini <mode> -i <input_file>" << std::endl;
    std::cerr << "Modes (cannot be combined):" << std::endl;
    std::cerr << "  -w : Write mode. Calculates charge and saves to a new file (*.prod.root)." << std::endl;
    std::cerr << "  -d : Display mode. Interactively view waveforms and histograms." << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        print_usage();
        return 1;
    }

    std::string mode = argv[1];
    std::string option_i = argv[2];
    std::string infile_name = argv[3];

    if (option_i != "-i") {
        print_usage();
        return 1;
    }

    if (mode != "-w" && mode != "-d") {
        std::cerr << "Error: Invalid mode '" << mode << "'" << std::endl;
        print_usage();
        return 1;
    }

    Processor proc(infile_name);
    if (!proc.isValid()) {
        std::cerr << "Error: Failed to open file or find 'fadc_tree' in '" << infile_name << "'." << std::endl;
        return 1;
    }

    if (mode == "-w") {
        proc.processAndWrite();
    } else if (mode == "-d") {
        TApplication app("App", &argc, argv);
        proc.displayInteractive();
        std::cout << "Displaying canvases. Close all ROOT windows to exit." << std::endl;
        app.Run();
    }

    return 0;
}
