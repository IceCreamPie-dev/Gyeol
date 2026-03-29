#include <gtest/gtest.h>

#include "runtime_perf_tools.h"

#include <filesystem>

using json = nlohmann::json;

namespace {

std::string sourcePath(const std::string& relPath) {
    return std::string(GYEOL_SOURCE_DIR) + "/" + relPath;
}

RuntimePerf::RunReport makeRunReport(std::initializer_list<std::pair<std::string, uint64_t>> scenarios) {
    RuntimePerf::RunReport report;
    for (const auto& entry : scenarios) {
        RuntimePerf::ScenarioMetrics metric;
        metric.name = entry.first;
        metric.medianNs = entry.second;
        report.scenarios.push_back(metric);
    }
    return report;
}

} // namespace

TEST(RuntimePerfSuiteTest, LoadsCoreSuiteFile) {
    RuntimePerf::SuiteConfig suite;
    std::string error;
    ASSERT_TRUE(RuntimePerf::loadSuiteFile(
        sourcePath("src/tests/perf/runtime_perf_suite_core.json"), suite, &error))
        << error;

    ASSERT_EQ(suite.scenarios.size(), 4u);
    EXPECT_EQ(suite.scenarios[0].name, "line_loop");
    EXPECT_TRUE(std::filesystem::path(suite.scenarios[0].storyPath).is_absolute());
    EXPECT_EQ(std::filesystem::path(suite.scenarios[0].storyPath).extension(), ".json");

    const auto& localeScenario = suite.scenarios[3];
    EXPECT_EQ(localeScenario.name, "locale_overlay");
    EXPECT_FALSE(localeScenario.localeCatalogPath.empty());
    EXPECT_EQ(localeScenario.locale, "ko-KR");
}

TEST(RuntimePerfSuiteTest, RejectsDuplicateScenarioName) {
    json doc = {
        {"format", "gyeol-runtime-perf-suite"},
        {"version", 1},
        {"scenarios", json::array({
            {{"name", "dup"}, {"story_path", "a.json"}, {"warmup", 1}, {"iterations", 1}, {"max_steps", 10}},
            {{"name", "dup"}, {"story_path", "b.json"}, {"warmup", 1}, {"iterations", 1}, {"max_steps", 10}}
        })}
    };

    RuntimePerf::SuiteConfig suite;
    std::string error;
    EXPECT_FALSE(RuntimePerf::parseSuiteJson(doc, "<mem>", suite, &error));
    EXPECT_NE(error.find("Duplicate scenario name"), std::string::npos);
}

TEST(RuntimePerfSuiteTest, RejectsLocaleWithoutCatalog) {
    json doc = {
        {"format", "gyeol-runtime-perf-suite"},
        {"version", 1},
        {"scenarios", json::array({
            {{"name", "locale_only"}, {"story_path", "a.json"}, {"warmup", 1}, {"iterations", 1}, {"max_steps", 10}, {"locale", "ko"}}
        })}
    };

    RuntimePerf::SuiteConfig suite;
    std::string error;
    EXPECT_FALSE(RuntimePerf::parseSuiteJson(doc, "<mem>", suite, &error));
    EXPECT_NE(error.find("locale requires locale_catalog"), std::string::npos);
}

TEST(RuntimePerfSuiteTest, RunsSingleScenarioFromJsonIrSource) {
    RuntimePerf::SuiteConfig suite;
    suite.sourcePath = sourcePath("src/tests/perf/runtime_perf_suite_core.json");
    suite.scenarios.push_back({
        "line_loop",
        sourcePath("src/tests/perf/line_loop.json"),
        1,
        3,
        10000,
        "",
        ""
    });

    RuntimePerf::RunReport report;
    std::string error;
    ASSERT_TRUE(RuntimePerf::runSuite(suite, report, &error)) << error;

    ASSERT_EQ(report.scenarios.size(), 1u);
    const auto& scenario = report.scenarios[0];
    EXPECT_EQ(scenario.name, "line_loop");
    EXPECT_EQ(scenario.warmup, 1);
    EXPECT_EQ(scenario.iterations, 3);
    EXPECT_GT(scenario.medianNs, 0u);
    EXPECT_GT(scenario.p95Ns, 0u);
    EXPECT_GT(scenario.medianStepCalls, 0u);
    EXPECT_GT(scenario.medianInstructionsExecuted, 0u);
    EXPECT_GT(scenario.throughputStepCallsPerSec, 0.0);
}

TEST(RuntimePerfCompareTest, PassesWithinThreshold) {
    const auto baseline = makeRunReport({
        {"line_loop", 100},
        {"choice_filter", 200}
    });
    const auto actual = makeRunReport({
        {"line_loop", 110},
        {"choice_filter", 210}
    });

    RuntimePerf::CompareReport report;
    std::string error;
    ASSERT_TRUE(RuntimePerf::compareReports(baseline, actual, 0.15, report, &error)) << error;
    EXPECT_EQ(report.status, "pass");
}

TEST(RuntimePerfCompareTest, FailsOnRegressionAboveThreshold) {
    const auto baseline = makeRunReport({
        {"line_loop", 100}
    });
    const auto actual = makeRunReport({
        {"line_loop", 130}
    });

    RuntimePerf::CompareReport report;
    std::string error;
    ASSERT_TRUE(RuntimePerf::compareReports(baseline, actual, 0.15, report, &error)) << error;
    EXPECT_EQ(report.status, "fail");
    ASSERT_EQ(report.scenarios.size(), 1u);
    EXPECT_TRUE(report.scenarios[0].regressed);
}

TEST(RuntimePerfCompareTest, UsesP95JitterAllowanceForNoiseMitigation) {
    RuntimePerf::RunReport baseline;
    RuntimePerf::ScenarioMetrics baselineMetric;
    baselineMetric.name = "line_loop";
    baselineMetric.medianNs = 100;
    baselineMetric.p95Ns = 130; // +30% jitter -> allowance capped to +10%
    baseline.scenarios.push_back(baselineMetric);

    const auto actual = makeRunReport({
        {"line_loop", 124}
    });

    RuntimePerf::CompareReport report;
    std::string error;
    ASSERT_TRUE(RuntimePerf::compareReports(baseline, actual, 0.15, report, &error)) << error;
    ASSERT_EQ(report.scenarios.size(), 1u);
    EXPECT_DOUBLE_EQ(report.scenarios[0].allowedRatio, 0.25);
    EXPECT_FALSE(report.scenarios[0].regressed);
    EXPECT_EQ(report.status, "pass");
}

TEST(RuntimePerfCompareTest, FailsWhenScenarioIsMissing) {
    const auto baseline = makeRunReport({
        {"line_loop", 100},
        {"choice_filter", 200}
    });
    const auto actual = makeRunReport({
        {"line_loop", 100}
    });

    RuntimePerf::CompareReport report;
    std::string error;
    EXPECT_FALSE(RuntimePerf::compareReports(baseline, actual, 0.15, report, &error));
    EXPECT_NE(error.find("missing scenario"), std::string::npos);
}

TEST(RuntimePerfCompareTest, FailsWhenScenarioIsExtra) {
    const auto baseline = makeRunReport({
        {"line_loop", 100}
    });
    const auto actual = makeRunReport({
        {"line_loop", 100},
        {"choice_filter", 200}
    });

    RuntimePerf::CompareReport report;
    std::string error;
    EXPECT_FALSE(RuntimePerf::compareReports(baseline, actual, 0.15, report, &error));
    EXPECT_NE(error.find("extra scenario"), std::string::npos);
}
