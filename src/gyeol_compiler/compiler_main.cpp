#include "gyeol_parser.h"
#include "gyeol_comp_analyzer.h"
#include "gyeol_json_export.h"
#include "gyeol_graph_tools.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cstring>

static const char* VERSION = "0.2.0";

static void printUsage() {
    std::cout << "Gyeol Compiler v" << VERSION << "\n"
              << "Usage: GyeolCompiler <input.gyeol> [-o output] [options]\n"
              << "       GyeolCompiler --po-to-json <input.po> -o <output.json> [--locale <code>]\n"
              << "       GyeolCompiler <input.gyeol> --export-graph-json <output.graph.json>\n"
              << "       GyeolCompiler <input.gyeol> --validate-graph-patch <patch.json>\n"
              << "       GyeolCompiler <input.gyeol> --apply-graph-patch <patch.json> -o <output.gyeol>\n"
              << "\n"
              << "Options:\n"
              << "  -o <path>           Output file path (default: story.gyb)\n"
              << "  --format <fmt>      Output format: gyb (default) or json\n"
              << "  --export-strings <path>  Export translatable strings to CSV\n"
              << "  --export-strings-po <path>  Export translatable strings to POT\n"
              << "  --po-to-json <path> Convert PO to runtime locale JSON\n"
              << "  --locale <code>     Locale code for --po-to-json output\n"
              << "  --export-graph-json <path>  Export graph contract JSON (gyeol-graph-doc v1)\n"
              << "  --validate-graph-patch <path>  Validate graph patch JSON (v1/v2) without writing files\n"
              << "  --apply-graph-patch <path>  Apply graph patch (v1/v2) and write canonical .gyeol\n"
              << "  --preserve-line-id   With --apply-graph-patch, emit <output>.lineidmap.json\n"
              << "  --line-id-map <path> Apply line-id map during compile (gyeol-line-id-map v1)\n"
              << "  --analyze [path]    Run analysis report (default: stdout)\n"
              << "  -O                  Apply optimizations (constant folding, dead code removal)\n"
              << "  -h, --help          Show this help message\n"
              << "  --version           Show version number\n";
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

    if (std::strcmp(argv[1], "--po-to-json") == 0) {
        std::string poInputPath;
        std::string outputPath = "locale.json";
        std::string localeHint;

        for (int i = 2; i < argc; ++i) {
            if (std::strcmp(argv[i], "-o") == 0) {
                if (i + 1 >= argc) {
                    std::cerr << "error: missing value for -o" << std::endl;
                    return 1;
                }
                outputPath = argv[++i];
            } else if (std::strcmp(argv[i], "--locale") == 0) {
                if (i + 1 >= argc) {
                    std::cerr << "error: missing value for --locale" << std::endl;
                    return 1;
                }
                localeHint = argv[++i];
            } else if (!poInputPath.empty() && argv[i][0] != '-') {
                std::cerr << "error: unexpected extra positional argument '" << argv[i] << "'" << std::endl;
                return 1;
            } else if (argv[i][0] == '-') {
                std::cerr << "error: unknown option '" << argv[i] << "'" << std::endl;
                return 1;
            } else if (poInputPath.empty()) {
                poInputPath = argv[i];
            } else {
                std::cerr << "error: unknown argument '" << argv[i] << "'" << std::endl;
                return 1;
            }
        }

        if (poInputPath.empty()) {
            std::cerr << "error: --po-to-json requires an input .po path" << std::endl;
            return 1;
        }

        std::string error;
        if (!Gyeol::LocaleTools::convertPoToJson(poInputPath, outputPath, localeHint, &error)) {
            std::cerr << "error: " << error << std::endl;
            return 1;
        }
        std::cout << "Converted: " << poInputPath << " -> " << outputPath << std::endl;
        return 0;
    }

    std::string inputPath = argv[1];
    std::string outputPath;
    std::string outputFormat = "gyb";
    std::string exportPath;
    std::string exportPoPath;
    std::string exportGraphPath;
    std::string validateGraphPatchPath;
    std::string applyGraphPatchPath;
    std::string lineIdMapPath;
    std::string analyzePath;
    bool doAnalyze = false;
    bool doOptimize = false;
    bool outputPathSet = false;
    bool preserveLineId = false;

    // 옵션 파싱
    for (int i = 2; i < argc; ++i) {
        if (std::strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "error: missing value for -o" << std::endl;
                return 1;
            }
            outputPath = argv[++i];
            outputPathSet = true;
        } else if (std::strcmp(argv[i], "--format") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "error: missing value for --format" << std::endl;
                return 1;
            }
            outputFormat = argv[++i];
            if (outputFormat != "gyb" && outputFormat != "json") {
                std::cerr << "error: unknown format '" << outputFormat
                          << "'. Supported: gyb, json" << std::endl;
                return 1;
            }
        } else if (std::strcmp(argv[i], "--export-strings") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "error: missing value for --export-strings" << std::endl;
                return 1;
            }
            exportPath = argv[++i];
        } else if (std::strcmp(argv[i], "--export-strings-po") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "error: missing value for --export-strings-po" << std::endl;
                return 1;
            }
            exportPoPath = argv[++i];
        } else if (std::strcmp(argv[i], "--export-graph-json") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "error: missing value for --export-graph-json" << std::endl;
                return 1;
            }
            exportGraphPath = argv[++i];
        } else if (std::strcmp(argv[i], "--validate-graph-patch") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "error: missing value for --validate-graph-patch" << std::endl;
                return 1;
            }
            validateGraphPatchPath = argv[++i];
        } else if (std::strcmp(argv[i], "--apply-graph-patch") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "error: missing value for --apply-graph-patch" << std::endl;
                return 1;
            }
            applyGraphPatchPath = argv[++i];
        } else if (std::strcmp(argv[i], "--line-id-map") == 0) {
            if (i + 1 >= argc) {
                std::cerr << "error: missing value for --line-id-map" << std::endl;
                return 1;
            }
            lineIdMapPath = argv[++i];
        } else if (std::strcmp(argv[i], "--preserve-line-id") == 0) {
            preserveLineId = true;
        } else if (std::strcmp(argv[i], "--analyze") == 0) {
            doAnalyze = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                analyzePath = argv[++i];
            }
        } else if (std::strcmp(argv[i], "-O") == 0) {
            doOptimize = true;
        } else if (std::strcmp(argv[i], "--locale") == 0) {
            std::cerr << "error: --locale is only valid with --po-to-json" << std::endl;
            return 1;
        } else if (std::strcmp(argv[i], "--po-to-json") == 0) {
            std::cerr << "error: --po-to-json must be used as the primary mode" << std::endl;
            return 1;
        } else if (argv[i][0] == '-') {
            std::cerr << "error: unknown option '" << argv[i] << "'" << std::endl;
            return 1;
        } else {
            std::cerr << "error: unexpected positional argument '" << argv[i] << "'" << std::endl;
            return 1;
        }
    }

    const int graphModeCount =
        (!exportGraphPath.empty() ? 1 : 0) +
        (!validateGraphPatchPath.empty() ? 1 : 0) +
        (!applyGraphPatchPath.empty() ? 1 : 0);

    if (graphModeCount > 1) {
        std::cerr << "error: choose only one graph mode among "
                  << "--export-graph-json, --validate-graph-patch, --apply-graph-patch" << std::endl;
        return 1;
    }

    const bool hasGraphMode = (graphModeCount == 1);
    if (hasGraphMode) {
        if (outputFormat != "gyb") {
            std::cerr << "error: --format is not supported with graph modes" << std::endl;
            return 1;
        }
        if (!exportPath.empty() || !exportPoPath.empty() || doAnalyze || doOptimize) {
            std::cerr << "error: graph modes cannot be combined with --export-strings, --export-strings-po, --analyze, or -O" << std::endl;
            return 1;
        }
        if (!applyGraphPatchPath.empty() && !outputPathSet) {
            std::cerr << "error: --apply-graph-patch requires -o <output.gyeol>" << std::endl;
            return 1;
        }
        if (preserveLineId && applyGraphPatchPath.empty()) {
            std::cerr << "error: --preserve-line-id is only valid with --apply-graph-patch" << std::endl;
            return 1;
        }
        if (!lineIdMapPath.empty()) {
            std::cerr << "error: --line-id-map is only valid in compile mode (not graph modes)" << std::endl;
            return 1;
        }
    } else if (preserveLineId) {
        std::cerr << "error: --preserve-line-id is only valid with --apply-graph-patch" << std::endl;
        return 1;
    }

    // 기본 출력 경로 설정 (포맷에 따라, 그래프 모드 제외)
    if (!hasGraphMode && !outputPathSet) {
        outputPath = (outputFormat == "json") ? "story.json" : "story.gyb";
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
    if (!exportPoPath.empty()) {
        if (!parser.exportStringsPO(exportPoPath)) {
            return 1;
        }
    }

    if (!exportGraphPath.empty()) {
        std::string error;
        if (!Gyeol::GraphTools::exportGraphJson(parser.getStory(), exportGraphPath, &error)) {
            std::cerr << "error: " << error << std::endl;
            return 1;
        }
        std::cout << "Exported graph JSON: " << exportGraphPath << std::endl;
        return 0;
    }

    if (!validateGraphPatchPath.empty()) {
        std::string error;
        if (!Gyeol::GraphTools::validateGraphPatchFile(parser.getStory(), validateGraphPatchPath, &error)) {
            std::cerr << "error: " << error << std::endl;
            return 1;
        }
        std::cout << "Graph patch is valid: " << validateGraphPatchPath << std::endl;
        return 0;
    }

    if (!applyGraphPatchPath.empty()) {
        std::string error;
        std::string mapOutputPath;
        if (preserveLineId) {
            mapOutputPath = outputPath + ".lineidmap.json";
        }
        if (!Gyeol::GraphTools::applyGraphPatchFileWithOptions(
                parser.getStoryMutable(),
                applyGraphPatchPath,
                preserveLineId,
                mapOutputPath,
                &error)) {
            std::cerr << "error: " << error << std::endl;
            return 1;
        }
        if (!Gyeol::GraphTools::writeCanonicalScript(parser.getStory(), outputPath, &error)) {
            std::cerr << "error: " << error << std::endl;
            return 1;
        }
        std::cout << "Applied graph patch: " << applyGraphPatchPath << " -> " << outputPath << std::endl;
        if (preserveLineId) {
            std::cout << "Generated line-id map: " << mapOutputPath << std::endl;
        }
        return 0;
    }

    if (!lineIdMapPath.empty()) {
        std::string error;
        if (!parser.applyLineIdMap(lineIdMapPath, &error)) {
            std::cerr << "error: " << error << std::endl;
            return 1;
        }
        std::cout << "Applied line-id map: " << lineIdMapPath << std::endl;
    }

    if (outputFormat == "json") {
        // JSON IR output
        std::string jsonStr = Gyeol::JsonExport::toJsonString(parser.getStory());
        std::ofstream ofs(outputPath);
        if (!ofs.is_open()) {
            std::cerr << "error: failed to write JSON output: " << outputPath << std::endl;
            return 1;
        }
        ofs << jsonStr;
        ofs.close();
    } else {
        // Binary .gyb output (default)
        if (!parser.compile(outputPath)) {
            const auto& errors = parser.getErrors();
            for (const auto& err : errors) {
                std::cerr << "error: " << err << std::endl;
            }
            std::cerr << "\n" << errors.size() << " error(s). Compilation failed." << std::endl;
            return 1;
        }
    }

    return 0;
}
