#include "runtime_contract_harness.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <vector>

using json = nlohmann::json;

namespace {

struct GenerateArgs {
    std::string engine;
    std::string storyPath;
    std::string actionsPath;
    std::string outputPath;
};

struct CompareArgs {
    std::string expectedPath;
    std::string actualPath;
    std::string expectedOutPath;
    std::string actualOutPath;
    std::string diffOutPath;
    std::string expectedEngineOverride;
};

std::string jsonTypeName(json::value_t type) {
    switch (type) {
    case json::value_t::null:
        return "null";
    case json::value_t::object:
        return "object";
    case json::value_t::array:
        return "array";
    case json::value_t::string:
        return "string";
    case json::value_t::boolean:
        return "boolean";
    case json::value_t::number_integer:
        return "number_integer";
    case json::value_t::number_unsigned:
        return "number_unsigned";
    case json::value_t::number_float:
        return "number_float";
    case json::value_t::binary:
        return "binary";
    case json::value_t::discarded:
        return "discarded";
    }
    return "unknown";
}

void printUsage() {
    std::cerr
        << "Usage:\n"
        << "  GyeolRuntimeContractCLI generate --engine <core> --story <story.json> "
           "--actions <actions.json> --output <transcript.json>\n"
        << "  GyeolRuntimeContractCLI compare --expected <golden.json> --actual <actual.json> "
           "--diff-out <diff.json> [--expected-out <path>] [--actual-out <path>] "
           "[--expected-engine <engine>]\n";
}

bool ensureParentDir(const std::string& outputPath, std::string* errorOut) {
    const auto parent = std::filesystem::path(outputPath).parent_path();
    if (parent.empty()) return true;
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec) {
        if (errorOut) {
            *errorOut = "Failed to create output directory '" + parent.string() + "': " + ec.message();
        }
        return false;
    }
    return true;
}

bool parseGenerateArgs(int argc, char** argv, GenerateArgs& out, std::string& error) {
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--engine" || arg == "--story" || arg == "--actions" || arg == "--output")) {
            if (i + 1 >= argc) {
                error = "Missing value for argument: " + arg;
                return false;
            }
            const std::string value = argv[++i];
            if (arg == "--engine") out.engine = value;
            if (arg == "--story") out.storyPath = value;
            if (arg == "--actions") out.actionsPath = value;
            if (arg == "--output") out.outputPath = value;
            continue;
        }

        error = "Unknown argument for generate: " + arg;
        return false;
    }

    if (out.engine.empty() || out.storyPath.empty() || out.actionsPath.empty() || out.outputPath.empty()) {
        error = "generate requires --engine, --story, --actions, and --output.";
        return false;
    }
    if (out.engine != "core") {
        error = "Unsupported engine '" + out.engine + "'. Runtime contract CLI supports core only.";
        return false;
    }

    return true;
}

bool parseCompareArgs(int argc, char** argv, CompareArgs& out, std::string& error) {
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--expected" ||
            arg == "--actual" ||
            arg == "--expected-out" ||
            arg == "--actual-out" ||
            arg == "--diff-out" ||
            arg == "--expected-engine") {
            if (i + 1 >= argc) {
                error = "Missing value for argument: " + arg;
                return false;
            }
            const std::string value = argv[++i];
            if (arg == "--expected") out.expectedPath = value;
            if (arg == "--actual") out.actualPath = value;
            if (arg == "--expected-out") out.expectedOutPath = value;
            if (arg == "--actual-out") out.actualOutPath = value;
            if (arg == "--diff-out") out.diffOutPath = value;
            if (arg == "--expected-engine") out.expectedEngineOverride = value;
            continue;
        }

        error = "Unknown argument for compare: " + arg;
        return false;
    }

    if (out.expectedPath.empty() || out.actualPath.empty() || out.diffOutPath.empty()) {
        error = "compare requires --expected, --actual, and --diff-out.";
        return false;
    }

    return true;
}

void pushDiff(json& diffs,
              const std::string& path,
              const std::string& kind,
              const json* expected,
              const json* actual) {
    json item = {
        {"path", path},
        {"kind", kind}
    };
    if (expected) item["expected"] = *expected;
    if (actual) item["actual"] = *actual;
    diffs.push_back(std::move(item));
}

void collectDiffs(const json& expected,
                  const json& actual,
                  const std::string& path,
                  json& diffs,
                  size_t maxDiffs,
                  bool& truncated) {
    if (diffs.size() >= maxDiffs) {
        truncated = true;
        return;
    }

    if (expected.type() != actual.type()) {
        json expectedType = jsonTypeName(expected.type());
        json actualType = jsonTypeName(actual.type());
        pushDiff(diffs, path, "type_mismatch", &expectedType, &actualType);
        return;
    }

    if (expected.is_object()) {
        std::set<std::string> keys;
        for (auto it = expected.begin(); it != expected.end(); ++it) keys.insert(it.key());
        for (auto it = actual.begin(); it != actual.end(); ++it) keys.insert(it.key());

        for (const auto& key : keys) {
            if (diffs.size() >= maxDiffs) {
                truncated = true;
                return;
            }
            const std::string childPath = path + "." + key;
            const bool hasExpected = expected.contains(key);
            const bool hasActual = actual.contains(key);
            if (!hasExpected) {
                pushDiff(diffs, childPath, "unexpected_in_actual", nullptr, &actual.at(key));
                continue;
            }
            if (!hasActual) {
                pushDiff(diffs, childPath, "missing_in_actual", &expected.at(key), nullptr);
                continue;
            }
            collectDiffs(expected.at(key), actual.at(key), childPath, diffs, maxDiffs, truncated);
        }
        return;
    }

    if (expected.is_array()) {
        if (expected.size() != actual.size()) {
            json expectedSize = expected.size();
            json actualSize = actual.size();
            pushDiff(diffs, path, "array_size_mismatch", &expectedSize, &actualSize);
            if (diffs.size() >= maxDiffs) {
                truncated = true;
                return;
            }
        }
        const size_t minSize = std::min(expected.size(), actual.size());
        for (size_t i = 0; i < minSize; ++i) {
            if (diffs.size() >= maxDiffs) {
                truncated = true;
                return;
            }
            collectDiffs(expected.at(i),
                         actual.at(i),
                         path + "[" + std::to_string(i) + "]",
                         diffs,
                         maxDiffs,
                         truncated);
        }
        return;
    }

    if (expected != actual) {
        pushDiff(diffs, path, "value_mismatch", &expected, &actual);
    }
}

json makeDiffReport(const json& expected,
                    const json& actual,
                    const std::string& expectedPath,
                    const std::string& actualPath) {
    constexpr size_t kMaxDiffs = 200;
    json diffs = json::array();
    bool truncated = false;
    collectDiffs(expected, actual, "$", diffs, kMaxDiffs, truncated);

    return {
        {"format", "gyeol-runtime-transcript-diff"},
        {"version", 1},
        {"equal", expected == actual},
        {"expected_path", expectedPath},
        {"actual_path", actualPath},
        {"difference_count", diffs.size()},
        {"truncated", truncated},
        {"differences", std::move(diffs)}
    };
}

int commandGenerate(const GenerateArgs& args) {
    std::string error;
    json actionsDoc;
    if (!RuntimeContract::loadJsonFile(args.actionsPath, actionsDoc, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    std::vector<uint8_t> storyBuffer;
    if (!RuntimeContract::compileStoryToBuffer(args.storyPath, storyBuffer, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    json transcript;
    RuntimeContract::RunOptions options;
    options.engine = "core";
    if (!RuntimeContract::runCoreActions(storyBuffer, actionsDoc, options, transcript, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    if (!ensureParentDir(args.outputPath, &error)) {
        std::cerr << error << "\n";
        return 1;
    }
    if (!RuntimeContract::writeJsonFile(args.outputPath, transcript, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    std::cout << "Generated runtime transcript: " << args.outputPath << "\n";
    return 0;
}

int commandCompare(const CompareArgs& args) {
    std::string error;
    json expected;
    json actual;
    if (!RuntimeContract::loadJsonFile(args.expectedPath, expected, &error)) {
        std::cerr << error << "\n";
        return 1;
    }
    if (!RuntimeContract::loadJsonFile(args.actualPath, actual, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    if (!args.expectedEngineOverride.empty()) {
        expected["engine"] = args.expectedEngineOverride;
    }

    if (!args.expectedOutPath.empty()) {
        if (!ensureParentDir(args.expectedOutPath, &error) ||
            !RuntimeContract::writeJsonFile(args.expectedOutPath, expected, &error)) {
            std::cerr << error << "\n";
            return 1;
        }
    }
    if (!args.actualOutPath.empty()) {
        if (!ensureParentDir(args.actualOutPath, &error) ||
            !RuntimeContract::writeJsonFile(args.actualOutPath, actual, &error)) {
            std::cerr << error << "\n";
            return 1;
        }
    }

    const json diffReport = makeDiffReport(expected, actual, args.expectedPath, args.actualPath);
    if (!ensureParentDir(args.diffOutPath, &error) ||
        !RuntimeContract::writeJsonFile(args.diffOutPath, diffReport, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    if (!diffReport.value("equal", false)) {
        std::cerr << "Runtime transcript mismatch. See diff report: " << args.diffOutPath << "\n";
        return 1;
    }

    std::cout << "Runtime transcript comparison passed.\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 2;
    }

    const std::string command = argv[1];
    std::string error;

    if (command == "generate") {
        GenerateArgs args;
        if (!parseGenerateArgs(argc, argv, args, error)) {
            std::cerr << error << "\n";
            printUsage();
            return 2;
        }
        return commandGenerate(args);
    }

    if (command == "compare") {
        CompareArgs args;
        if (!parseCompareArgs(argc, argv, args, error)) {
            std::cerr << error << "\n";
            printUsage();
            return 2;
        }
        return commandCompare(args);
    }

    std::cerr << "Unknown command: " << command << "\n";
    printUsage();
    return 2;
}
