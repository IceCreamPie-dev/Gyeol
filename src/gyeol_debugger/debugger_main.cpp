#include "gyeol_debugger.h"

#include <iostream>
#include <cstring>

int main(int argc, char* argv[]) {
    // --version / --help 처리
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--version") == 0) {
            std::cout << "GyeolDebugger 0.1.0" << std::endl;
            return 0;
        }
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::cout << "GyeolDebugger - Interactive debugger for .gyb stories\n"
                      << "Usage: GyeolDebugger <story.gyb>\n"
                      << "\n"
                      << "Options:\n"
                      << "  --version  Show version\n"
                      << "  -h, --help Show this help\n";
            return 0;
        }
    }

    if (argc < 2) {
        std::cerr << "Usage: GyeolDebugger <story.gyb>\n"
                  << "Try 'GyeolDebugger --help' for more information.\n";
        return 1;
    }

    Gyeol::Debugger debugger;

    if (!debugger.loadStory(argv[1])) {
        return 1;
    }

    debugger.run();
    return 0;
}
