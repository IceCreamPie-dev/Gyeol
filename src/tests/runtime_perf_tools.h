#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace RuntimePerf {

struct ScenarioConfig {
    std::string name;
    std::string storyPath;
    int warmup = 5;
    int iterations = 20;
    int maxSteps = 200000;
    std::string localeCatalogPath;
    std::string locale;
};

struct SuiteConfig {
    std::string format = "gyeol-runtime-perf-suite";
    int version = 1;
    std::vector<ScenarioConfig> scenarios;
    std::string sourcePath;
};

struct ScenarioMetrics {
    std::string name;
    int warmup = 0;
    int iterations = 0;
    uint64_t medianNs = 0;
    uint64_t p95Ns = 0;
    uint64_t medianStepCalls = 0;
    uint64_t medianInstructionsExecuted = 0;
    double throughputStepCallsPerSec = 0.0;
};

struct RunReport {
    std::string format = "gyeol-runtime-perf";
    int version = 1;
    std::string engine = "core";
    std::string suitePath;
    std::vector<ScenarioMetrics> scenarios;
};

struct ScenarioComparison {
    std::string name;
    uint64_t baselineMedianNs = 0;
    uint64_t baselineP95Ns = 0;
    uint64_t actualMedianNs = 0;
    double regressionRatio = 0.0;
    double allowedRatio = 0.15;
    bool regressed = false;
};

struct CompareReport {
    std::string format = "gyeol-runtime-perf-compare";
    int version = 1;
    double threshold = 0.15;
    std::string status = "pass";
    std::vector<ScenarioComparison> scenarios;
};

bool loadSuiteFile(const std::string& path, SuiteConfig& outSuite, std::string* errorOut = nullptr);
bool parseSuiteJson(const nlohmann::json& jsonDoc,
                    const std::string& sourcePath,
                    SuiteConfig& outSuite,
                    std::string* errorOut = nullptr);

bool runSuite(const SuiteConfig& suite, RunReport& outReport, std::string* errorOut = nullptr);

nlohmann::json runReportToJson(const RunReport& report);
bool parseRunReportJson(const nlohmann::json& jsonDoc, RunReport& outReport, std::string* errorOut = nullptr);

bool compareReports(const RunReport& baseline,
                    const RunReport& actual,
                    double threshold,
                    CompareReport& outReport,
                    std::string* errorOut = nullptr);

nlohmann::json compareReportToJson(const CompareReport& report);

} // namespace RuntimePerf
