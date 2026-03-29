#include "runtime_perf_tools.h"

#include "runtime_contract_harness.h"

#include "gyeol_runner.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>

using json = nlohmann::json;

namespace RuntimePerf {
namespace {

bool ensureInt(const json& obj, const char* key, int& outValue, std::string* errorOut) {
    if (!obj.contains(key) || !obj[key].is_number_integer()) {
        if (errorOut) *errorOut = std::string("Missing integer field: ") + key;
        return false;
    }
    const int value = obj[key].get<int>();
    if (value <= 0) {
        if (errorOut) *errorOut = std::string("Field must be positive: ") + key;
        return false;
    }
    outValue = value;
    return true;
}

bool ensureString(const json& obj, const char* key, std::string& outValue, std::string* errorOut) {
    if (!obj.contains(key) || !obj[key].is_string()) {
        if (errorOut) *errorOut = std::string("Missing string field: ") + key;
        return false;
    }
    outValue = obj[key].get<std::string>();
    if (outValue.empty()) {
        if (errorOut) *errorOut = std::string("Field must not be empty: ") + key;
        return false;
    }
    return true;
}

bool hasSupportedStoryExtension(const std::string& path) {
    const auto ext = std::filesystem::path(path).extension().string();
    return ext == ".json";
}

uint64_t percentile(const std::vector<uint64_t>& sortedValues, double p) {
    if (sortedValues.empty()) return 0;
    const size_t raw = static_cast<size_t>(std::ceil((p / 100.0) * static_cast<double>(sortedValues.size())));
    const size_t index = raw == 0 ? 0 : raw - 1;
    return sortedValues[std::min(index, sortedValues.size() - 1)];
}

uint64_t median(std::vector<uint64_t> values) {
    if (values.empty()) return 0;
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

struct RunSample {
    uint64_t elapsedNs = 0;
    uint64_t stepCalls = 0;
    uint64_t instructionsExecuted = 0;
};

bool runOnce(const std::vector<uint8_t>& storyBuffer,
             const ScenarioConfig& scenario,
             RunSample& outSample,
             std::string* errorOut) {
    Gyeol::Runner runner;
    if (!runner.start(storyBuffer.data(), storyBuffer.size())) {
        if (errorOut) *errorOut = "Runner failed to start for scenario: " + scenario.name;
        return false;
    }

    if (!scenario.localeCatalogPath.empty()) {
        if (!runner.loadLocaleCatalog(scenario.localeCatalogPath)) {
            if (errorOut) *errorOut = "Failed to load locale catalog for scenario '" + scenario.name + "'";
            return false;
        }
        if (!scenario.locale.empty() && !runner.setLocale(scenario.locale)) {
            if (errorOut) *errorOut = "Failed to set locale '" + scenario.locale + "' for scenario '" + scenario.name + "'";
            return false;
        }
    } else if (!scenario.locale.empty()) {
        if (errorOut) *errorOut = "Scenario '" + scenario.name + "' has locale without locale_catalog";
        return false;
    }

    const auto start = std::chrono::steady_clock::now();
    int guard = 0;
    while (guard++ < scenario.maxSteps) {
        const auto result = runner.step();
        switch (result.type) {
        case Gyeol::StepType::LINE:
        case Gyeol::StepType::COMMAND:
        case Gyeol::StepType::YIELD:
            continue;
        case Gyeol::StepType::CHOICES:
            if (result.choices.empty()) {
                if (errorOut) *errorOut = "Scenario '" + scenario.name + "' returned empty CHOICES";
                return false;
            }
            runner.choose(0);
            continue;
        case Gyeol::StepType::WAIT:
            if (!runner.resume()) {
                if (errorOut) *errorOut = "Scenario '" + scenario.name + "' failed to resume WAIT";
                return false;
            }
            continue;
        case Gyeol::StepType::END: {
            const auto end = std::chrono::steady_clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            const auto& metrics = runner.getMetrics();
            outSample.elapsedNs = elapsed > 0 ? static_cast<uint64_t>(elapsed) : 0u;
            outSample.stepCalls = metrics.stepCalls;
            outSample.instructionsExecuted = metrics.instructionsExecuted;
            return true;
        }
        }
    }

    if (errorOut) {
        *errorOut = "Scenario '" + scenario.name + "' exceeded max_steps guard (" +
                    std::to_string(scenario.maxSteps) + ")";
    }
    return false;
}

} // namespace

bool parseSuiteJson(const json& jsonDoc,
                    const std::string& sourcePath,
                    SuiteConfig& outSuite,
                    std::string* errorOut) {
    if (!jsonDoc.is_object()) {
        if (errorOut) *errorOut = "Suite root must be an object.";
        return false;
    }
    if (!jsonDoc.contains("format") || !jsonDoc["format"].is_string() ||
        jsonDoc["format"].get<std::string>() != "gyeol-runtime-perf-suite") {
        if (errorOut) *errorOut = "Suite format must be 'gyeol-runtime-perf-suite'.";
        return false;
    }
    if (!jsonDoc.contains("version") || !jsonDoc["version"].is_number_integer() ||
        jsonDoc["version"].get<int>() != 1) {
        if (errorOut) *errorOut = "Suite version must be 1.";
        return false;
    }
    if (!jsonDoc.contains("scenarios") || !jsonDoc["scenarios"].is_array()) {
        if (errorOut) *errorOut = "Suite must include array field 'scenarios'.";
        return false;
    }
    if (jsonDoc["scenarios"].empty()) {
        if (errorOut) *errorOut = "Suite scenarios must not be empty.";
        return false;
    }

    SuiteConfig suite;
    suite.sourcePath = sourcePath;
    std::unordered_set<std::string> names;
    for (size_t i = 0; i < jsonDoc["scenarios"].size(); ++i) {
        const auto& item = jsonDoc["scenarios"][i];
        if (!item.is_object()) {
            if (errorOut) *errorOut = "Each scenario must be an object.";
            return false;
        }

        ScenarioConfig scenario;
        if (!ensureString(item, "name", scenario.name, errorOut)) return false;
        if (!ensureString(item, "story_path", scenario.storyPath, errorOut)) return false;
        if (!hasSupportedStoryExtension(scenario.storyPath)) {
            if (errorOut) *errorOut = "Scenario story_path must end with .json: " + scenario.storyPath;
            return false;
        }
        if (!ensureInt(item, "warmup", scenario.warmup, errorOut)) return false;
        if (!ensureInt(item, "iterations", scenario.iterations, errorOut)) return false;
        if (!ensureInt(item, "max_steps", scenario.maxSteps, errorOut)) return false;

        if (item.contains("locale_catalog")) {
            if (!item["locale_catalog"].is_string()) {
                if (errorOut) *errorOut = "locale_catalog must be a string.";
                return false;
            }
            scenario.localeCatalogPath = item["locale_catalog"].get<std::string>();
        }
        if (item.contains("locale")) {
            if (!item["locale"].is_string()) {
                if (errorOut) *errorOut = "locale must be a string.";
                return false;
            }
            scenario.locale = item["locale"].get<std::string>();
        }
        if (!scenario.locale.empty() && scenario.localeCatalogPath.empty()) {
            if (errorOut) *errorOut = "locale requires locale_catalog in scenario: " + scenario.name;
            return false;
        }
        if (names.find(scenario.name) != names.end()) {
            if (errorOut) *errorOut = "Duplicate scenario name: " + scenario.name;
            return false;
        }
        names.insert(scenario.name);
        suite.scenarios.push_back(std::move(scenario));
    }

    outSuite = std::move(suite);
    return true;
}

bool loadSuiteFile(const std::string& path, SuiteConfig& outSuite, std::string* errorOut) {
    json doc;
    std::string error;
    if (!RuntimeContract::loadJsonFile(path, doc, &error)) {
        if (errorOut) *errorOut = error;
        return false;
    }
    if (!parseSuiteJson(doc, path, outSuite, &error)) {
        if (errorOut) *errorOut = error;
        return false;
    }

    const auto baseDir = std::filesystem::path(path).parent_path();
    for (auto& scenario : outSuite.scenarios) {
        std::filesystem::path storyPath = scenario.storyPath;
        if (storyPath.is_relative()) {
            storyPath = baseDir / storyPath;
        }
        scenario.storyPath = storyPath.lexically_normal().string();

        if (!scenario.localeCatalogPath.empty()) {
            std::filesystem::path catalogPath = scenario.localeCatalogPath;
            if (catalogPath.is_relative()) {
                catalogPath = baseDir / catalogPath;
            }
            scenario.localeCatalogPath = catalogPath.lexically_normal().string();
        }
    }
    outSuite.sourcePath = std::filesystem::path(path).lexically_normal().string();
    return true;
}

bool runSuite(const SuiteConfig& suite, RunReport& outReport, std::string* errorOut) {
    RunReport report;
    report.suitePath = suite.sourcePath;

    for (const auto& scenario : suite.scenarios) {
        std::vector<uint8_t> storyBuffer;
        std::string error;
        if (!RuntimeContract::compileStoryToBuffer(scenario.storyPath, storyBuffer, &error)) {
            if (errorOut) *errorOut = error;
            return false;
        }

        for (int i = 0; i < scenario.warmup; ++i) {
            RunSample sample;
            if (!runOnce(storyBuffer, scenario, sample, &error)) {
                if (errorOut) *errorOut = error;
                return false;
            }
        }

        std::vector<RunSample> samples;
        samples.reserve(static_cast<size_t>(scenario.iterations));
        for (int i = 0; i < scenario.iterations; ++i) {
            RunSample sample;
            if (!runOnce(storyBuffer, scenario, sample, &error)) {
                if (errorOut) *errorOut = error;
                return false;
            }
            samples.push_back(sample);
        }

        std::vector<uint64_t> elapsed;
        std::vector<uint64_t> stepCalls;
        std::vector<uint64_t> instructions;
        elapsed.reserve(samples.size());
        stepCalls.reserve(samples.size());
        instructions.reserve(samples.size());
        for (const auto& sample : samples) {
            elapsed.push_back(sample.elapsedNs);
            stepCalls.push_back(sample.stepCalls);
            instructions.push_back(sample.instructionsExecuted);
        }
        std::sort(elapsed.begin(), elapsed.end());

        ScenarioMetrics metrics;
        metrics.name = scenario.name;
        metrics.warmup = scenario.warmup;
        metrics.iterations = scenario.iterations;
        metrics.medianNs = median(elapsed);
        metrics.p95Ns = percentile(elapsed, 95.0);
        metrics.medianStepCalls = median(stepCalls);
        metrics.medianInstructionsExecuted = median(instructions);
        metrics.throughputStepCallsPerSec = metrics.medianNs > 0
            ? static_cast<double>(metrics.medianStepCalls) * 1'000'000'000.0 / static_cast<double>(metrics.medianNs)
            : 0.0;
        report.scenarios.push_back(std::move(metrics));
    }

    outReport = std::move(report);
    return true;
}

json runReportToJson(const RunReport& report) {
    json scenarios = json::array();
    for (const auto& s : report.scenarios) {
        scenarios.push_back({
            {"name", s.name},
            {"warmup", s.warmup},
            {"iterations", s.iterations},
            {"median_ns", s.medianNs},
            {"p95_ns", s.p95Ns},
            {"median_step_calls", s.medianStepCalls},
            {"median_instructions_executed", s.medianInstructionsExecuted},
            {"throughput_step_calls_per_sec", s.throughputStepCallsPerSec},
        });
    }
    return {
        {"format", report.format},
        {"version", report.version},
        {"engine", report.engine},
        {"suite_path", report.suitePath},
        {"scenarios", std::move(scenarios)},
    };
}

bool parseRunReportJson(const json& jsonDoc, RunReport& outReport, std::string* errorOut) {
    if (!jsonDoc.is_object()) {
        if (errorOut) *errorOut = "Run report root must be an object.";
        return false;
    }
    if (!jsonDoc.contains("format") || !jsonDoc["format"].is_string() ||
        jsonDoc["format"].get<std::string>() != "gyeol-runtime-perf") {
        if (errorOut) *errorOut = "Run report format must be gyeol-runtime-perf.";
        return false;
    }
    if (!jsonDoc.contains("version") || !jsonDoc["version"].is_number_integer() ||
        jsonDoc["version"].get<int>() != 1) {
        if (errorOut) *errorOut = "Run report version must be 1.";
        return false;
    }
    if (!jsonDoc.contains("scenarios") || !jsonDoc["scenarios"].is_array()) {
        if (errorOut) *errorOut = "Run report scenarios must be an array.";
        return false;
    }

    RunReport report;
    if (jsonDoc.contains("engine") && jsonDoc["engine"].is_string()) {
        report.engine = jsonDoc["engine"].get<std::string>();
    }
    if (jsonDoc.contains("suite_path") && jsonDoc["suite_path"].is_string()) {
        report.suitePath = jsonDoc["suite_path"].get<std::string>();
    }

    std::unordered_set<std::string> names;
    for (const auto& item : jsonDoc["scenarios"]) {
        if (!item.is_object()) continue;
        ScenarioMetrics s;
        if (!ensureString(item, "name", s.name, errorOut)) return false;
        if (!item.contains("median_ns") || !item["median_ns"].is_number_unsigned()) {
            if (errorOut) *errorOut = "Scenario missing unsigned median_ns: " + s.name;
            return false;
        }
        if (names.find(s.name) != names.end()) {
            if (errorOut) *errorOut = "Duplicate scenario name in run report: " + s.name;
            return false;
        }
        names.insert(s.name);
        s.medianNs = item["median_ns"].get<uint64_t>();
        s.p95Ns = item.value("p95_ns", 0u);
        s.warmup = item.value("warmup", 0);
        s.iterations = item.value("iterations", 0);
        s.medianStepCalls = item.value("median_step_calls", 0u);
        s.medianInstructionsExecuted = item.value("median_instructions_executed", 0u);
        s.throughputStepCallsPerSec = item.value("throughput_step_calls_per_sec", 0.0);
        report.scenarios.push_back(std::move(s));
    }
    if (report.scenarios.empty()) {
        if (errorOut) *errorOut = "Run report scenarios must not be empty.";
        return false;
    }

    outReport = std::move(report);
    return true;
}

bool compareReports(const RunReport& baseline,
                    const RunReport& actual,
                    double threshold,
                    CompareReport& outReport,
                    std::string* errorOut) {
    std::unordered_map<std::string, ScenarioMetrics> baselineByName;
    std::unordered_map<std::string, ScenarioMetrics> actualByName;
    for (const auto& scenario : baseline.scenarios) baselineByName[scenario.name] = scenario;
    for (const auto& scenario : actual.scenarios) actualByName[scenario.name] = scenario;

    for (const auto& [name, _] : baselineByName) {
        if (actualByName.find(name) == actualByName.end()) {
            if (errorOut) *errorOut = "Actual report missing scenario: " + name;
            return false;
        }
    }
    for (const auto& [name, _] : actualByName) {
        if (baselineByName.find(name) == baselineByName.end()) {
            if (errorOut) *errorOut = "Actual report has extra scenario: " + name;
            return false;
        }
    }

    CompareReport report;
    report.threshold = threshold;
    bool hasRegression = false;
    for (const auto& baselineScenario : baseline.scenarios) {
        const auto it = actualByName.find(baselineScenario.name);
        const auto& actualScenario = it->second;

        const double ratio = baselineScenario.medianNs > 0
            ? (static_cast<double>(actualScenario.medianNs) - static_cast<double>(baselineScenario.medianNs)) /
                static_cast<double>(baselineScenario.medianNs)
            : 0.0;

        double jitterAllowance = 0.0;
        if (baselineScenario.medianNs > 0 && baselineScenario.p95Ns > baselineScenario.medianNs) {
            jitterAllowance =
                (static_cast<double>(baselineScenario.p95Ns) - static_cast<double>(baselineScenario.medianNs)) /
                static_cast<double>(baselineScenario.medianNs);
            jitterAllowance = std::clamp(jitterAllowance, 0.0, 0.10);
        }
        const double allowedRatio = threshold + jitterAllowance;
        const bool regressed = ratio > allowedRatio;
        if (regressed) hasRegression = true;
        report.scenarios.push_back({
            baselineScenario.name,
            baselineScenario.medianNs,
            baselineScenario.p95Ns,
            actualScenario.medianNs,
            ratio,
            allowedRatio,
            regressed,
        });
    }
    report.status = hasRegression ? "fail" : "pass";
    outReport = std::move(report);
    return true;
}

json compareReportToJson(const CompareReport& report) {
    json scenarios = json::array();
    for (const auto& s : report.scenarios) {
        scenarios.push_back({
            {"name", s.name},
            {"baseline_median_ns", s.baselineMedianNs},
            {"baseline_p95_ns", s.baselineP95Ns},
            {"actual_median_ns", s.actualMedianNs},
            {"regression_ratio", s.regressionRatio},
            {"allowed_ratio", s.allowedRatio},
            {"regressed", s.regressed},
        });
    }
    return {
        {"format", report.format},
        {"version", report.version},
        {"threshold", report.threshold},
        {"status", report.status},
        {"scenarios", std::move(scenarios)},
    };
}

} // namespace RuntimePerf
