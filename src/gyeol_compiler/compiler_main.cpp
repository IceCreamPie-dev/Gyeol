#include "gyeol_graph_tools.h"
#include "gyeol_json_export.h"
#include "gyeol_json_ir_reader.h"
#include "gyeol_json_ir_tooling.h"
#include "gyeol_parser.h"

#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using json = nlohmann::json;
using namespace ICPDev::Gyeol::Schema;

namespace {

static const char* VERSION = "0.2.0";

enum class OutputFormat {
    Text,
    Json,
};

bool parseOutputFormat(const std::string& text, OutputFormat& out) {
    if (text == "text") {
        out = OutputFormat::Text;
        return true;
    }
    if (text == "json") {
        out = OutputFormat::Json;
        return true;
    }
    return false;
}

bool writeTextFile(const std::string& path, const std::string& text) {
    std::ofstream ofs(path);
    if (!ofs.is_open()) return false;
    ofs << text;
    return ofs.good();
}

bool parseGyeolStory(const std::string& path, Gyeol::Parser& outParser) {
    if (outParser.parse(path)) return true;
    for (const auto& err : outParser.getErrors()) {
        std::cerr << "error: " << err << std::endl;
    }
    return false;
}

bool loadStoryFromJsonIr(const std::string& path, StoryT& outStory) {
    std::string error;
    if (!Gyeol::JsonIrReader::fromFile(path, outStory, &error)) {
        std::cerr << "error: " << error << std::endl;
        return false;
    }
    return true;
}

void printDiagnostics(const std::vector<Gyeol::JsonIrDiagnostic>& diagnostics, OutputFormat format) {
    if (format == OutputFormat::Json) {
        std::cout << Gyeol::JsonIrTooling::diagnosticsToJson(diagnostics).dump(2) << std::endl;
        return;
    }
    std::cout << Gyeol::JsonIrTooling::diagnosticsToText(diagnostics) << std::endl;
}

void printPatchPreviewText(const json& preview) {
    const json summary = preview.value("summary", json::object());
    std::cout
        << "Patch preview (ops=" << preview.value("patch_op_count", 0) << ")\n"
        << "  Nodes: " << summary.value("nodes_before", 0) << " -> " << summary.value("nodes_after", 0)
        << " (delta=" << summary.value("nodes_delta", 0) << ")\n"
        << "  Instructions: " << summary.value("instructions_before", 0) << " -> "
        << summary.value("instructions_after", 0)
        << " (delta=" << summary.value("instructions_delta", 0) << ")\n"
        << "  Edges: " << summary.value("edges_before", 0) << " -> " << summary.value("edges_after", 0)
        << " (delta=" << summary.value("edges_delta", 0) << ")" << std::endl;

    auto printList = [](const char* label, const json& arr) {
        if (!arr.is_array() || arr.empty()) return;
        std::cout << "  " << label << ": ";
        for (size_t i = 0; i < arr.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << arr[i].get<std::string>();
        }
        std::cout << std::endl;
    };
    printList("Added nodes", preview.value("added_nodes", json::array()));
    printList("Removed nodes", preview.value("removed_nodes", json::array()));
    printList("Changed nodes", preview.value("changed_nodes", json::array()));
}

void printUsage() {
    std::cout
        << "Gyeol Compiler v" << VERSION << "\n"
        << "Public (.gyeol authoring):\n"
        << "  GyeolCompiler --validate <story.gyeol>\n"
        << "  GyeolCompiler --export-json-ir <story.gyeol> -o <story.json>\n"
        << "\n"
        << "Advanced/hidden:\n"
        << "  --validate-json-ir / --lint-json-ir / --format-json-ir\n"
        << "  --export-graph-json / --preview-graph-patch / --apply-graph-patch\n"
        << "  --export-strings-po-from-json-ir / --export-locale-template\n"
        << "  --po-to-locale-json / --validate-locale-json / --build-locale-catalog\n"
        << "  --po-to-json (legacy locale v1)\n"
        << "\n";
}

} // namespace

int main(int argc, char* argv[]) {
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

    if (std::strcmp(argv[1], "--validate") == 0) {
        if (argc != 3) {
            std::cerr << "error: usage --validate <story.gyeol>" << std::endl;
            return 1;
        }
        Gyeol::Parser parser;
        if (!parseGyeolStory(argv[2], parser)) return 1;
        for (const auto& warning : parser.getWarnings()) {
            std::cout << "warning: " << warning << std::endl;
        }
        std::cout << "Valid script: " << argv[2] << std::endl;
        return 0;
    }

    if (std::strcmp(argv[1], "--export-json-ir") == 0) {
        if (argc < 5) {
            std::cerr << "error: usage --export-json-ir <story.gyeol> -o <story.json>" << std::endl;
            return 1;
        }
        std::string inputPath = argv[2];
        std::string outputPath;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                outputPath = argv[++i];
            } else {
                std::cerr << "error: unknown option '" << argv[i] << "'" << std::endl;
                return 1;
            }
        }
        if (outputPath.empty()) {
            std::cerr << "error: missing -o <story.json>" << std::endl;
            return 1;
        }

        Gyeol::Parser parser;
        if (!parseGyeolStory(inputPath, parser)) return 1;
        if (!writeTextFile(outputPath, Gyeol::JsonExport::toJsonString(parser.getStory()))) {
            std::cerr << "error: failed to write JSON IR output: " << outputPath << std::endl;
            return 1;
        }
        for (const auto& warning : parser.getWarnings()) {
            std::cout << "warning: " << warning << std::endl;
        }
        std::cout << "Exported JSON IR: " << inputPath << " -> " << outputPath << std::endl;
        return 0;
    }

    if (std::strcmp(argv[1], "--init-json-ir") == 0) {
        if (argc != 3) {
            std::cerr << "error: usage --init-json-ir <story.json>" << std::endl;
            return 1;
        }
        std::string error;
        if (!Gyeol::JsonIrTooling::writeInitTemplate(argv[2], &error)) {
            std::cerr << "error: " << error << std::endl;
            return 1;
        }
        std::cout << "Initialized JSON IR template: " << argv[2] << std::endl;
        return 0;
    }

    if (std::strcmp(argv[1], "--lint-json-ir") == 0) {
        if (argc < 3) {
            std::cerr << "error: usage --lint-json-ir <story.json> [--format text|json]" << std::endl;
            return 1;
        }
        std::string storyPath = argv[2];
        OutputFormat outputFormat = OutputFormat::Text;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "--format") == 0 && i + 1 < argc) {
                if (!parseOutputFormat(argv[++i], outputFormat)) {
                    std::cerr << "error: --format must be text or json." << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "error: unknown option '" << argv[i] << "'" << std::endl;
                return 1;
            }
        }

        std::vector<Gyeol::JsonIrDiagnostic> diagnostics;
        std::string error;
        Gyeol::JsonIrTooling::lintFile(storyPath, diagnostics, nullptr, &error);
        printDiagnostics(diagnostics, outputFormat);
        return Gyeol::JsonIrTooling::hasErrors(diagnostics) ? 1 : 0;
    }

    if (std::strcmp(argv[1], "--po-to-json") == 0) {
        if (argc < 5) {
            std::cerr << "error: --po-to-json requires <input.po> and -o <output.json>" << std::endl;
            return 1;
        }
        std::string poPath = argv[2];
        std::string outputPath;
        std::string locale;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                outputPath = argv[++i];
            } else if (std::strcmp(argv[i], "--locale") == 0 && i + 1 < argc) {
                locale = argv[++i];
            } else {
                std::cerr << "error: unknown option '" << argv[i] << "'" << std::endl;
                return 1;
            }
        }
        if (outputPath.empty()) {
            std::cerr << "error: missing -o <output.json>" << std::endl;
            return 1;
        }
        std::string error;
        if (!Gyeol::LocaleTools::convertPoToJson(poPath, outputPath, locale, &error)) {
            std::cerr << "error: " << error << std::endl;
            return 1;
        }
        std::cout << "Converted: " << poPath << " -> " << outputPath << std::endl;
        return 0;
    }

    if (std::strcmp(argv[1], "--validate-json-ir") == 0) {
        if (argc != 3) {
            std::cerr << "error: usage --validate-json-ir <story.json>" << std::endl;
            return 1;
        }
        std::vector<Gyeol::JsonIrDiagnostic> diagnostics;
        std::string error;
        Gyeol::JsonIrTooling::lintFile(argv[2], diagnostics, nullptr, &error);
        if (Gyeol::JsonIrTooling::hasErrors(diagnostics)) {
            printDiagnostics(diagnostics, OutputFormat::Text);
            return 1;
        }
        std::cout << "Valid JSON IR: " << argv[2] << std::endl;
        return 0;
    }

    if (std::strcmp(argv[1], "--format-json-ir") == 0) {
        if (argc < 3) {
            std::cerr << "error: usage --format-json-ir <story.json> [-o <story.json>]" << std::endl;
            return 1;
        }
        std::string inputPath = argv[2];
        std::string outputPath = inputPath;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                outputPath = argv[++i];
            } else {
                std::cerr << "error: unknown option '" << argv[i] << "'" << std::endl;
                return 1;
            }
        }
        std::string error;
        if (!Gyeol::JsonIrTooling::formatFile(inputPath, outputPath, &error)) {
            std::cerr << "error: " << error << std::endl;
            return 1;
        }
        std::cout << "Formatted JSON IR: " << inputPath << " -> " << outputPath << std::endl;
        return 0;
    }

    if (std::strcmp(argv[1], "--export-graph-json") == 0) {
        if (argc < 5) {
            std::cerr << "error: usage --export-graph-json <story.json> -o <story.graph.json>" << std::endl;
            return 1;
        }
        std::string storyPath = argv[2];
        std::string outputPath;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                outputPath = argv[++i];
            } else {
                std::cerr << "error: unknown option '" << argv[i] << "'" << std::endl;
                return 1;
            }
        }
        if (outputPath.empty()) {
            std::cerr << "error: missing -o <story.graph.json>" << std::endl;
            return 1;
        }
        StoryT story;
        if (!loadStoryFromJsonIr(storyPath, story)) return 1;
        std::string error;
        if (!Gyeol::GraphTools::exportGraphJson(story, outputPath, &error)) {
            std::cerr << "error: " << error << std::endl;
            return 1;
        }
        std::cout << "Exported graph JSON: " << outputPath << std::endl;
        return 0;
    }

    if (std::strcmp(argv[1], "--preview-graph-patch") == 0) {
        if (argc < 5) {
            std::cerr << "error: usage --preview-graph-patch <story.json> --patch <patch.json> [--format text|json]" << std::endl;
            return 1;
        }
        std::string storyPath = argv[2];
        std::string patchPath;
        OutputFormat outputFormat = OutputFormat::Text;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "--patch") == 0 && i + 1 < argc) {
                patchPath = argv[++i];
            } else if (std::strcmp(argv[i], "--format") == 0 && i + 1 < argc) {
                if (!parseOutputFormat(argv[++i], outputFormat)) {
                    std::cerr << "error: --format must be text or json." << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "error: unknown option '" << argv[i] << "'" << std::endl;
                return 1;
            }
        }
        if (patchPath.empty()) {
            std::cerr << "error: --patch is required." << std::endl;
            return 1;
        }

        StoryT story;
        if (!loadStoryFromJsonIr(storyPath, story)) return 1;

        json patch;
        std::string error;
        if (!Gyeol::JsonIrTooling::loadJsonFile(patchPath, patch, &error)) {
            std::cerr << "error: " << error << std::endl;
            return 1;
        }

        json preview;
        std::vector<Gyeol::JsonIrDiagnostic> diagnostics;
        if (!Gyeol::JsonIrTooling::previewGraphPatch(story, storyPath, patch, preview, &diagnostics, &error)) {
            printDiagnostics(diagnostics, outputFormat);
            return 1;
        }

        if (outputFormat == OutputFormat::Json) {
            std::cout << preview.dump(2) << std::endl;
        } else {
            printPatchPreviewText(preview);
        }
        return 0;
    }

    if (std::strcmp(argv[1], "--apply-graph-patch") == 0) {
        if (argc < 7) {
            std::cerr << "error: usage --apply-graph-patch <story.json> --patch <patch.json> -o <story.json>" << std::endl;
            return 1;
        }
        std::string storyPath = argv[2];
        std::string patchPath;
        std::string outputPath;
        bool preserveLineId = false;
        std::string lineIdMapPath;

        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "--patch") == 0 && i + 1 < argc) {
                patchPath = argv[++i];
            } else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                outputPath = argv[++i];
            } else if (std::strcmp(argv[i], "--preserve-line-id") == 0) {
                preserveLineId = true;
            } else if (std::strcmp(argv[i], "--line-id-map-out") == 0 && i + 1 < argc) {
                lineIdMapPath = argv[++i];
            } else {
                std::cerr << "error: unknown option '" << argv[i] << "'" << std::endl;
                return 1;
            }
        }

        if (patchPath.empty() || outputPath.empty()) {
            std::cerr << "error: --patch and -o are required." << std::endl;
            return 1;
        }
        if (preserveLineId && lineIdMapPath.empty()) {
            lineIdMapPath = outputPath + ".lineidmap.json";
        }

        StoryT story;
        if (!loadStoryFromJsonIr(storyPath, story)) return 1;

        std::string error;
        if (!Gyeol::GraphTools::applyGraphPatchFileWithOptions(
                story,
                patchPath,
                preserveLineId,
                lineIdMapPath,
                &error)) {
            std::cerr << "error: " << error << std::endl;
            return 1;
        }

        if (!writeTextFile(outputPath, Gyeol::JsonExport::toJsonString(story))) {
            std::cerr << "error: failed to write patched JSON IR: " << outputPath << std::endl;
            return 1;
        }
        std::cout << "Applied graph patch: " << patchPath << " -> " << outputPath << std::endl;
        if (preserveLineId) {
            std::cout << "Generated line-id map: " << lineIdMapPath << std::endl;
        }
        return 0;
    }

    if (std::strcmp(argv[1], "--export-strings-po-from-json-ir") == 0) {
        if (argc < 5) {
            std::cerr << "error: usage --export-strings-po-from-json-ir <story.json> -o <strings.pot>" << std::endl;
            return 1;
        }
        std::string storyPath = argv[2];
        std::string outputPath;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                outputPath = argv[++i];
            } else {
                std::cerr << "error: unknown option '" << argv[i] << "'" << std::endl;
                return 1;
            }
        }
        if (outputPath.empty()) {
            std::cerr << "error: missing -o <strings.pot>" << std::endl;
            return 1;
        }
        StoryT story;
        if (!loadStoryFromJsonIr(storyPath, story)) return 1;
        std::string error;
        if (!Gyeol::LocaleTools::exportStringsPOFromStory(story, outputPath, &error)) {
            std::cerr << "error: " << error << std::endl;
            return 1;
        }
        std::cout << "Exported: " << outputPath << std::endl;
        return 0;
    }

    if (std::strcmp(argv[1], "--export-locale-template") == 0) {
        if (argc < 5) {
            std::cerr << "error: usage --export-locale-template <story.json> -o <locale.template.json>" << std::endl;
            return 1;
        }
        std::string storyPath = argv[2];
        std::string outputPath;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                outputPath = argv[++i];
            } else {
                std::cerr << "error: unknown option '" << argv[i] << "'" << std::endl;
                return 1;
            }
        }
        if (outputPath.empty()) {
            std::cerr << "error: missing -o <locale.template.json>" << std::endl;
            return 1;
        }
        StoryT story;
        if (!loadStoryFromJsonIr(storyPath, story)) return 1;
        std::string error;
        if (!Gyeol::LocaleTools::exportLocaleTemplateFromStory(story, outputPath, &error)) {
            std::cerr << "error: " << error << std::endl;
            return 1;
        }
        std::cout << "Exported locale template: " << outputPath << std::endl;
        return 0;
    }

    if (std::strcmp(argv[1], "--po-to-locale-json") == 0) {
        if (argc < 7) {
            std::cerr << "error: usage --po-to-locale-json <strings.po> --story <story.json> -o <locale.json> [--locale <code>]" << std::endl;
            return 1;
        }
        std::string poPath = argv[2];
        std::string storyPath;
        std::string outputPath;
        std::string localeCode;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "--story") == 0 && i + 1 < argc) {
                storyPath = argv[++i];
            } else if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                outputPath = argv[++i];
            } else if (std::strcmp(argv[i], "--locale") == 0 && i + 1 < argc) {
                localeCode = argv[++i];
            } else {
                std::cerr << "error: unknown option '" << argv[i] << "'" << std::endl;
                return 1;
            }
        }
        if (storyPath.empty() || outputPath.empty()) {
            std::cerr << "error: --story and -o are required." << std::endl;
            return 1;
        }
        StoryT story;
        if (!loadStoryFromJsonIr(storyPath, story)) return 1;
        std::string error;
        if (!Gyeol::LocaleTools::convertPoToLocaleJson(poPath, story, outputPath, localeCode, &error)) {
            std::cerr << "error: " << error << std::endl;
            return 1;
        }
        std::cout << "Converted: " << poPath << " -> " << outputPath << std::endl;
        return 0;
    }

    if (std::strcmp(argv[1], "--validate-locale-json") == 0) {
        if (argc < 5) {
            std::cerr << "error: usage --validate-locale-json <locale.json> --story <story.json>" << std::endl;
            return 1;
        }
        std::string localePath = argv[2];
        std::string storyPath;
        for (int i = 3; i < argc; ++i) {
            if (std::strcmp(argv[i], "--story") == 0 && i + 1 < argc) {
                storyPath = argv[++i];
            } else {
                std::cerr << "error: unknown option '" << argv[i] << "'" << std::endl;
                return 1;
            }
        }
        if (storyPath.empty()) {
            std::cerr << "error: --story is required." << std::endl;
            return 1;
        }
        StoryT story;
        if (!loadStoryFromJsonIr(storyPath, story)) return 1;
        std::string error;
        if (!Gyeol::LocaleTools::validateLocaleJsonFile(localePath, story, &error)) {
            std::cerr << "error: " << error << std::endl;
            return 1;
        }
        std::cout << "Locale JSON is valid: " << localePath << std::endl;
        return 0;
    }

    if (std::strcmp(argv[1], "--build-locale-catalog") == 0) {
        if (argc < 6) {
            std::cerr << "error: usage --build-locale-catalog <localeA.json> <localeB.json> ... -o <catalog.json> [--default-locale <code>]" << std::endl;
            return 1;
        }
        std::vector<std::string> localePaths;
        std::string outputPath;
        std::string defaultLocale;
        for (int i = 2; i < argc; ++i) {
            if (std::strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
                outputPath = argv[++i];
            } else if (std::strcmp(argv[i], "--default-locale") == 0 && i + 1 < argc) {
                defaultLocale = argv[++i];
            } else if (argv[i][0] == '-') {
                std::cerr << "error: unknown option '" << argv[i] << "'" << std::endl;
                return 1;
            } else {
                localePaths.emplace_back(argv[i]);
            }
        }
        if (outputPath.empty()) {
            std::cerr << "error: missing -o <catalog.json>" << std::endl;
            return 1;
        }
        std::string error;
        if (!Gyeol::LocaleTools::buildLocaleCatalog(localePaths, outputPath, defaultLocale, &error)) {
            std::cerr << "error: " << error << std::endl;
            return 1;
        }
        std::cout << "Built locale catalog: " << outputPath << std::endl;
        return 0;
    }

    printUsage();
    return 1;
}
