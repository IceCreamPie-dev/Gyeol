#include "gyeol_parser.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: GyeolCompiler <input.gyeol> [-o output.gyb]" << std::endl;
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = "story.gyb";

    // -o 옵션
    for (int i = 2; i < argc - 1; ++i) {
        if (std::string(argv[i]) == "-o") {
            outputPath = argv[i + 1];
            break;
        }
    }

    Gyeol::Parser parser;

    if (!parser.parse(inputPath)) {
        std::cerr << "Parse error: " << parser.getError() << std::endl;
        return 1;
    }

    if (!parser.compile(outputPath)) {
        std::cerr << "Compile error: " << parser.getError() << std::endl;
        return 1;
    }

    return 0;
}
