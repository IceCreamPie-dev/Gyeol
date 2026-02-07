#include "gyeol_parser.h"
#include <iostream>
#include <string>
#include <cstring>

static const char* VERSION = "0.1.0";

static void printUsage() {
    std::cout << "Gyeol Compiler v" << VERSION << "\n"
              << "Usage: GyeolCompiler <input.gyeol> [-o output.gyb]\n"
              << "\n"
              << "Options:\n"
              << "  -o <path>    Output file path (default: story.gyb)\n"
              << "  --export-strings <path>  Export translatable strings to CSV\n"
              << "  -h, --help   Show this help message\n"
              << "  --version    Show version number\n";
}

int main(int argc, char* argv[]) {
    // help/version 플래그 체크 (위치 무관)
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "-h") == 0 || std::strcmp(argv[i], "--help") == 0) {
            printUsage();
            return 0;
        }
        if (std::strcmp(argv[i], "--version") == 0) {
            std::cout << "GyeolCompiler " << VERSION << std::endl;
            return 0;
        }
    }

    if (argc < 2) {
        printUsage();
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath = "story.gyb";
    std::string exportPath;

    // 옵션 파싱
    for (int i = 2; i < argc - 1; ++i) {
        if (std::strcmp(argv[i], "-o") == 0) {
            outputPath = argv[i + 1];
        } else if (std::strcmp(argv[i], "--export-strings") == 0) {
            exportPath = argv[i + 1];
        }
    }

    Gyeol::Parser parser;

    if (!parser.parse(inputPath)) {
        const auto& errors = parser.getErrors();
        for (const auto& err : errors) {
            std::cerr << "error: " << err << std::endl;
        }
        std::cerr << "\n" << errors.size() << " error(s). Compilation aborted." << std::endl;
        return 1;
    }

    // CSV 추출 (--export-strings)
    if (!exportPath.empty()) {
        if (!parser.exportStrings(exportPath)) {
            return 1;
        }
    }

    if (!parser.compile(outputPath)) {
        const auto& errors = parser.getErrors();
        for (const auto& err : errors) {
            std::cerr << "error: " << err << std::endl;
        }
        std::cerr << "\n" << errors.size() << " error(s). Compilation failed." << std::endl;
        return 1;
    }

    return 0;
}
