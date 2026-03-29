#include "runtime_contract_harness.h"
#include "runtime_perf_tools.h"

#include <filesystem>
#include <iostream>
#include <string>

using json = nlohmann::json;

namespace {

struct RunArgs {
    std::string suitePath;
    std::string outputPath;
};

struct CompareArgs {
    std::string baselinePath;
    std::string actualPath;
    std::string reportPath;
    double threshold = 0.15;
};

void printUsage() {
    std::cerr
        << "Usage:\n"
        << "  GyeolRuntimePerfCLI run --suite <runtime_perf_suite_core.json> --output <perf.json>\n"
        << "  GyeolRuntimePerfCLI compare --baseline <baseline.json> --actual <actual.json> "
           "[--threshold <ratio>] [--report-out <report.json>]\n";
}

bool parseDoubleArg(const std::string& text, double& out, std::string& error) {
    try {
        const double v = std::stod(text);
        if (v < 0.0 || v > 1.0) {
            error = "Threshold must be between 0.0 and 1.0: " + text;
            return false;
        }
        out = v;
        return true;
    } catch (...) {
        error = "Invalid floating argument: " + text;
        return false;
    }
}

bool ensureParentDir(const std::string& outputPath, std::string* errorOut) {
    const auto parent = std::filesystem::path(outputPath).parent_path();
    if (parent.empty()) return true;
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec) {
        if (errorOut) *errorOut = "Failed to create output directory '" + parent.string() + "': " + ec.message();
        return false;
    }
    return true;
}

bool parseRunArgs(int argc, char** argv, RunArgs& out, std::string& error) {
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--suite" || arg == "--output")) {
            if (i + 1 >= argc) {
                error = "Missing value for argument: " + arg;
                return false;
            }
            const std::string value = argv[++i];
            if (arg == "--suite") out.suitePath = value;
            if (arg == "--output") out.outputPath = value;
            continue;
        }
        error = "Unknown argument for run: " + arg;
        return false;
    }
    if (out.suitePath.empty() || out.outputPath.empty()) {
        error = "run requires --suite and --output.";
        return false;
    }
    return true;
}

bool parseCompareArgs(int argc, char** argv, CompareArgs& out, std::string& error) {
    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if ((arg == "--baseline" || arg == "--actual" || arg == "--threshold" || arg == "--report-out")) {
            if (i + 1 >= argc) {
                error = "Missing value for argument: " + arg;
                return false;
            }
            const std::string value = argv[++i];
            if (arg == "--baseline") out.baselinePath = value;
            if (arg == "--actual") out.actualPath = value;
            if (arg == "--report-out") out.reportPath = value;
            if (arg == "--threshold" && !parseDoubleArg(value, out.threshold, error)) return false;
            continue;
        }
        error = "Unknown argument for compare: " + arg;
        return false;
    }
    if (out.baselinePath.empty() || out.actualPath.empty()) {
        error = "compare requires --baseline and --actual.";
        return false;
    }
    return true;
}

int commandRun(const RunArgs& args) {
    RuntimePerf::SuiteConfig suite;
    std::string error;
    if (!RuntimePerf::loadSuiteFile(args.suitePath, suite, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    RuntimePerf::RunReport report;
    if (!RuntimePerf::runSuite(suite, report, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    const json output = RuntimePerf::runReportToJson(report);
    if (!ensureParentDir(args.outputPath, &error)) {
        std::cerr << error << "\n";
        return 1;
    }
    if (!RuntimeContract::writeJsonFile(args.outputPath, output, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    std::cout << "Generated runtime perf report: " << args.outputPath << "\n";
    return 0;
}

int commandCompare(const CompareArgs& args) {
    std::string error;
    json baselineJson;
    json actualJson;
    if (!RuntimeContract::loadJsonFile(args.baselinePath, baselineJson, &error)) {
        std::cerr << error << "\n";
        return 1;
    }
    if (!RuntimeContract::loadJsonFile(args.actualPath, actualJson, &error)) {
        std::cerr << error << "\n";
        return 1;
    }

    RuntimePerf::RunReport baseline;
    RuntimePerf::RunReport actual;
    if (!RuntimePerf::parseRunReportJson(baselineJson, baseline, &error)) {
        std::cerr << "Invalid baseline: " << error << "\n";
        return 1;
    }
    if (!RuntimePerf::parseRunReportJson(actualJson, actual, &error)) {
        std::cerr << "Invalid actual: " << error << "\n";
        return 1;
    }

    RuntimePerf::CompareReport report;
    if (!RuntimePerf::compareReports(baseline, actual, args.threshold, report, &error)) {
        std::cerr << error << "\n";
        return 1;
    }
    const json reportJson = RuntimePerf::compareReportToJson(report);

    if (!args.reportPath.empty()) {
        if (!ensureParentDir(args.reportPath, &error)) {
            std::cerr << error << "\n";
            return 1;
        }
        if (!RuntimeContract::writeJsonFile(args.reportPath, reportJson, &error)) {
            std::cerr << error << "\n";
            return 1;
        }
    }

    std::cout << reportJson.dump(2) << "\n";
    if (report.status == "fail") {
        std::cerr << "Runtime performance regression detected (threshold=" << args.threshold << ").\n";
        return 1;
    }
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

    if (command == "run") {
        RunArgs args;
        if (!parseRunArgs(argc, argv, args, error)) {
            std::cerr << error << "\n";
            printUsage();
            return 2;
        }
        return commandRun(args);
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
