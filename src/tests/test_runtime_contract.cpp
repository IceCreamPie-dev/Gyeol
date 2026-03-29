#include <gtest/gtest.h>

#include "runtime_contract_harness.h"
#include "gyeol_json_ir_reader.h"

#include <filesystem>

using json = nlohmann::json;

namespace {

std::string sourcePath(const std::string& relPath) {
    return std::string(GYEOL_SOURCE_DIR) + "/" + relPath;
}

bool getCheckpoint(const json& transcript, const std::string& label, json& outCheckpoint) {
    if (!transcript.contains("checkpoints") || !transcript["checkpoints"].is_array()) return false;
    for (const auto& checkpoint : transcript["checkpoints"]) {
        if (checkpoint.value("label", "") == label) {
            outCheckpoint = checkpoint;
            return true;
        }
    }
    return false;
}

} // namespace

TEST(RuntimeContractCoreTest, CrossScenarioMatchesGolden) {
    std::vector<uint8_t> storyBuffer;
    std::string error;
    ASSERT_TRUE(RuntimeContract::compileStoryToBuffer(
        sourcePath("src/tests/conformance/runtime_contract_v1_story.json"),
        storyBuffer,
        &error)) << error;

    json actionsDoc;
    ASSERT_TRUE(RuntimeContract::loadJsonFile(
        sourcePath("src/tests/conformance/runtime_contract_v1_actions_cross.json"),
        actionsDoc,
        &error)) << error;

    RuntimeContract::RunOptions options;
    options.engine = "core";

    json actual;
    ASSERT_TRUE(RuntimeContract::runCoreActions(storyBuffer, actionsDoc, options, actual, &error)) << error;

    json golden;
    ASSERT_TRUE(RuntimeContract::loadJsonFile(
        sourcePath("src/tests/conformance/runtime_contract_v1_golden_core_cross.json"),
        golden,
        &error)) << error;

    EXPECT_TRUE(RuntimeContract::jsonEquals(golden, actual, &error)) << error;
}

TEST(RuntimeContractCoreTest, SaveLoadAndSnapshotRestoreAreEquivalent) {
    std::vector<uint8_t> storyBuffer;
    std::string error;
    ASSERT_TRUE(RuntimeContract::compileStoryToBuffer(
        sourcePath("src/tests/conformance/runtime_contract_v1_story.json"),
        storyBuffer,
        &error)) << error;

    json actionsDoc;
    ASSERT_TRUE(RuntimeContract::loadJsonFile(
        sourcePath("src/tests/conformance/runtime_contract_v1_actions_core_extended.json"),
        actionsDoc,
        &error)) << error;

    RuntimeContract::RunOptions options;
    options.engine = "core";
    options.includeSeedInState = true;
    options.includeLastErrorInState = true;

    json transcript;
    ASSERT_TRUE(RuntimeContract::runCoreActions(storyBuffer, actionsDoc, options, transcript, &error)) << error;

    json afterLineOnce;
    json afterRestoreLine;
    ASSERT_TRUE(getCheckpoint(transcript, "after_line_once", afterLineOnce));
    ASSERT_TRUE(getCheckpoint(transcript, "after_restore_line", afterRestoreLine));
    json stateAfterLine = afterLineOnce["state"];
    json stateAfterRestore = afterRestoreLine["state"];
    stateAfterLine.erase("metrics");
    stateAfterRestore.erase("metrics");
    EXPECT_EQ(stateAfterLine, stateAfterRestore);

    json afterLoadChooseEnd;
    ASSERT_TRUE(getCheckpoint(transcript, "after_load_choose_end", afterLoadChooseEnd));
    ASSERT_EQ(afterLoadChooseEnd["state"]["variables"]["flag"]["type"], "int");
    ASSERT_EQ(afterLoadChooseEnd["state"]["variables"]["flag"]["value"], 1);

    json finished;
    ASSERT_TRUE(getCheckpoint(transcript, "finished", finished));
    EXPECT_TRUE(finished["state"]["finished"].get<bool>());

    bool sawSave = false;
    bool sawLoad = false;
    bool sawSnapshot = false;
    bool sawRestore = false;
    bool sawResume = false;
    for (const auto& step : transcript["steps"]) {
        const auto action = step.value("action", "");
        if (action == "save") {
            sawSave = true;
            EXPECT_TRUE(step.value("ok", false));
        } else if (action == "load") {
            sawLoad = true;
            EXPECT_TRUE(step.value("ok", false));
        } else if (action == "snapshot") {
            sawSnapshot = true;
            EXPECT_TRUE(step.value("ok", false));
        } else if (action == "restore") {
            sawRestore = true;
            EXPECT_TRUE(step.value("ok", false));
        } else if (action == "resume") {
            sawResume = true;
            EXPECT_TRUE(step.value("ok", false));
        }
    }
    EXPECT_TRUE(sawSave);
    EXPECT_TRUE(sawLoad);
    EXPECT_TRUE(sawSnapshot);
    EXPECT_TRUE(sawRestore);
    EXPECT_TRUE(sawResume);
}

TEST(RuntimeContractCoreTest, LastErrorLifecycleForWaitMisuse) {
    std::vector<uint8_t> storyBuffer;
    std::string error;
    ASSERT_TRUE(RuntimeContract::compileStoryToBuffer(
        sourcePath("src/tests/conformance/runtime_contract_v1_story.json"),
        storyBuffer,
        &error)) << error;

    json actions = {
        {"format", "gyeol-runtime-actions"},
        {"version", 2},
        {"actions", json::array({
            {{"op", "step"}},
            {{"op", "step"}},
            {{"op", "choose"}, {"index", 0}},
            {{"op", "step"}},
            {{"op", "choose"}, {"index", 0}},
            {{"op", "checkpoint"}, {"label", "after_invalid_choose"}},
            {{"op", "resume"}},
            {{"op", "resume"}},
            {{"op", "checkpoint"}, {"label", "after_invalid_resume"}},
            {{"op", "clear_last_error"}},
            {{"op", "checkpoint"}, {"label", "after_clear"}}
        })}
    };

    RuntimeContract::RunOptions options;
    options.engine = "core";
    options.includeLastErrorInState = true;

    json transcript;
    ASSERT_TRUE(RuntimeContract::runCoreActions(storyBuffer, actions, options, transcript, &error)) << error;

    json afterInvalidChoose;
    json afterInvalidResume;
    json afterClear;
    ASSERT_TRUE(getCheckpoint(transcript, "after_invalid_choose", afterInvalidChoose));
    ASSERT_TRUE(getCheckpoint(transcript, "after_invalid_resume", afterInvalidResume));
    ASSERT_TRUE(getCheckpoint(transcript, "after_clear", afterClear));

    const std::string invalidChooseError = afterInvalidChoose["state"]["last_error"].get<std::string>();
    EXPECT_FALSE(invalidChooseError.empty());
    EXPECT_NE(invalidChooseError.find("Cannot choose while waiting"), std::string::npos);

    const std::string invalidResumeError = afterInvalidResume["state"]["last_error"].get<std::string>();
    EXPECT_FALSE(invalidResumeError.empty());
    EXPECT_NE(invalidResumeError.find("Cannot resume when runner is not waiting"), std::string::npos);

    EXPECT_TRUE(afterClear["state"]["last_error"].get<std::string>().empty());
}

TEST(RuntimeContractCoreTest, LocaleOverlayKeepsStepFlowStable) {
    std::vector<uint8_t> storyBuffer;
    std::string error;
    ASSERT_TRUE(RuntimeContract::compileStoryToBuffer(
        sourcePath("src/tests/conformance/runtime_contract_v1_story.json"),
        storyBuffer,
        &error)) << error;

    Gyeol::Runner baseRunner;
    ASSERT_TRUE(baseRunner.start(storyBuffer.data(), storyBuffer.size()));

    Gyeol::Runner localeRunner;
    ASSERT_TRUE(localeRunner.start(storyBuffer.data(), storyBuffer.size()));

    std::string localePath =
        (std::filesystem::temp_directory_path() / "gyeol_runtime_contract_v1_locale.json").string();
    {
        ICPDev::Gyeol::Schema::StoryT story;
        ASSERT_TRUE(Gyeol::JsonIrReader::fromFile(
            sourcePath("src/tests/conformance/runtime_contract_v1_story.json"),
            story,
            &error)) << error;
        json locale = {
            {"version", 1},
            {"locale", "contract-test"},
            {"entries", json::object()}
        };
        for (size_t i = 0; i < story.string_pool.size() && i < story.line_ids.size(); ++i) {
            if (!story.line_ids[i].empty()) {
                locale["entries"][story.line_ids[i]] = std::string("T_") + story.string_pool[i];
            }
        }
        ASSERT_TRUE(RuntimeContract::writeJsonFile(localePath, locale, &error)) << error;
    }

    ASSERT_TRUE(localeRunner.loadLocale(localePath));

    std::vector<std::string> baseTypes;
    std::vector<std::string> localeTypes;
    std::string baseFirstLine;
    std::string localeFirstLine;

    auto collectFlow = [](Gyeol::Runner& runner,
                          std::vector<std::string>& outTypes,
                          std::string* firstLineOut) {
        for (int i = 0; i < 8; ++i) {
            auto result = runner.step();
            const auto r = RuntimeContract::stepResultToJson(result);
            outTypes.push_back(r.value("type", ""));
            if (firstLineOut->empty() && r.value("type", "") == "LINE") {
                *firstLineOut = r["line"].value("text", "");
            }
            if (r.value("type", "") == "CHOICES") {
                runner.choose(0);
            }
            if (r.value("type", "") == "WAIT") {
                EXPECT_TRUE(runner.resume());
            }
            if (r.value("type", "") == "END") break;
        }
    };

    collectFlow(baseRunner, baseTypes, &baseFirstLine);
    collectFlow(localeRunner, localeTypes, &localeFirstLine);

    EXPECT_EQ(baseTypes, localeTypes);
    EXPECT_NE(baseFirstLine, localeFirstLine);

    std::error_code ec;
    std::filesystem::remove(localePath, ec);
}

