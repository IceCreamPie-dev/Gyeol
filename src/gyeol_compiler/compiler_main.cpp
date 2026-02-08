#include "gyeol_parser.h"
#include "gyeol_comp_analyzer.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>

static const char* VERSION = "0.2.0";

static void printUsage() {
    std::cout << "Gyeol Compiler v" << VERSION << "\n"
              << "Usage: GyeolCompiler <input.gyeol> [-o output.gyb]\n"
              << "\n"
              << "Options:\n"
              << "  -o <path>    Output file path (default: story.gyb)\n"
              << "  --export-strings <path>  Export translatable strings to CSV\n"
              << "  --analyze [path]  Run analysis report (default: stdout)\n"
              << "  -O           Apply optimizations (constant folding, dead code removal)\n"
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
    std::string analyzePath;
    bool doAnalyze = false;
    bool doOptimize = false;

    // 옵션 파싱
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            outputPath = argv[++i];
        } else if (std::strcmp(argv[i], "--export-strings") == 0 && i + 1 < argc) {
            exportPath = argv[++i];
        } else if (std::strcmp(argv[i], "--analyze") == 0) {
            doAnalyze = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                analyzePath = argv[++i];
            }
        } else if (std::strcmp(argv[i], "-O") == 0) {
            doOptimize = true;
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

    // 경고 출력
    if (parser.hasWarnings()) {
        for (const auto& warn : parser.getWarnings()) {
            std::cerr << "warning: " << warn << std::endl;
        }
    }

    // 분석 리포트
    if (doAnalyze) {
        Gyeol::CompilerAnalyzer analyzer;
        auto report = analyzer.analyze(parser.getStory());

        if (analyzePath.empty()) {
            Gyeol::CompilerAnalyzer::printReport(report, std::cout);
        } else {
            std::ofstream ofs(analyzePath);
            if (ofs.is_open()) {
                Gyeol::CompilerAnalyzer::printReport(report, ofs);
                std::cout << "Analysis report: " << analyzePath << std::endl;
            } else {
                std::cerr << "Failed to write analysis report: " << analyzePath << std::endl;
                return 1;
            }
        }
    }

    // 최적화 적용
    if (doOptimize) {
        Gyeol::CompilerAnalyzer analyzer;
        int optimizations = analyzer.optimize(parser.getStoryMutable());
        if (optimizations > 0) {
            std::cout << "Applied " << optimizations << " optimization(s)." << std::endl;
        }
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
