#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "gyeol_graph_tools.h"
#include "gyeol_json_ir_tooling.h"
#include "gyeol_parser.h"

#include <cstdio>
#include <fstream>
#include <string>

using json = nlohmann::json;
using namespace ICPDev::Gyeol::Schema;

namespace {

std::string makeTempPath(const std::string& filename) {
    static int counter = 0;
    ++counter;
    return "test_tmp_json_ir_tooling_" + std::to_string(counter) + "_" + filename;
}

int countInstructions(const StoryT& story) {
    int count = 0;
    for (const auto& node : story.nodes) {
        if (!node) continue;
        count += static_cast<int>(node->lines.size());
    }
    return count;
}

int countEdges(const StoryT& story) {
    const json doc = Gyeol::GraphTools::buildGraphDoc(story);
    if (!doc.contains("edges") || !doc["edges"].is_array()) return 0;
    return static_cast<int>(doc["edges"].size());
}

bool hasDiagnosticCode(const std::vector<Gyeol::JsonIrDiagnostic>& diagnostics, const std::string& code) {
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.code == code) return true;
    }
    return false;
}

} // namespace

TEST(JsonIrToolingTest, InitTemplateCanBeLintedWithoutErrors) {
    const std::string path = makeTempPath("init.json");
    std::string error;
    ASSERT_TRUE(Gyeol::JsonIrTooling::writeInitTemplate(path, &error)) << error;

    std::vector<Gyeol::JsonIrDiagnostic> diagnostics;
    StoryT story;
    ASSERT_TRUE(Gyeol::JsonIrTooling::lintFile(path, diagnostics, &story, &error)) << error;
    EXPECT_FALSE(Gyeol::JsonIrTooling::hasErrors(diagnostics));
    EXPECT_EQ(story.start_node_name, "start");

    std::remove(path.c_str());
}

TEST(JsonIrToolingTest, LintStoryDetectsMissingChoiceTarget) {
    Gyeol::Parser parser;
    ASSERT_TRUE(parser.parseString(
        "label start:\n"
        "    menu:\n"
        "        \"Go\" -> end\n"
        "label end:\n"
        "    \"End\"\n"));

    StoryT& story = parser.getStoryMutable();
    ASSERT_FALSE(story.nodes.empty());

    int targetInstructionIndex = -1;
    for (size_t i = 0; i < story.nodes[0]->lines.size(); ++i) {
        auto& line = story.nodes[0]->lines[i];
        if (!line || line->data.type != OpData::Choice) continue;
        auto* choice = line->data.AsChoice();
        ASSERT_NE(choice, nullptr);
        const int32_t missingTargetId = static_cast<int32_t>(story.string_pool.size());
        story.string_pool.push_back("__missing_target__");
        if (story.line_ids.size() < story.string_pool.size()) {
            story.line_ids.resize(story.string_pool.size());
        }
        choice->target_node_name_id = missingTargetId;
        targetInstructionIndex = static_cast<int>(i);
        break;
    }

    ASSERT_GE(targetInstructionIndex, 0);

    std::vector<Gyeol::JsonIrDiagnostic> diagnostics;
    ASSERT_TRUE(Gyeol::JsonIrTooling::lintStory(story, "story.json", diagnostics));
    EXPECT_TRUE(hasDiagnosticCode(diagnostics, "IR_TARGET_MISSING"));
    EXPECT_TRUE(Gyeol::JsonIrTooling::hasErrors(diagnostics));

    bool matchedLocation = false;
    for (const auto& diagnostic : diagnostics) {
        if (diagnostic.code == "IR_TARGET_MISSING" &&
            diagnostic.node == "start" &&
            diagnostic.instruction_index == targetInstructionIndex) {
            matchedLocation = true;
            break;
        }
    }
    EXPECT_TRUE(matchedLocation);
}

TEST(JsonIrToolingTest, LintFileEmitsParseDiagnosticForInvalidJsonIr) {
    const std::string path = makeTempPath("invalid.json");
    std::ofstream ofs(path);
    ofs << R"({
  "format": "gyeol-json-ir",
  "format_version": 1,
  "start_node_name": "start",
  "nodes": []
})";
    ofs.close();

    std::vector<Gyeol::JsonIrDiagnostic> diagnostics;
    std::string error;
    EXPECT_FALSE(Gyeol::JsonIrTooling::lintFile(path, diagnostics, nullptr, &error));
    EXPECT_TRUE(hasDiagnosticCode(diagnostics, "IR_PARSE_ERROR"));
    EXPECT_TRUE(Gyeol::JsonIrTooling::hasErrors(diagnostics));

    std::remove(path.c_str());
}

TEST(JsonIrToolingTest, DiagnosticsJsonIncludesRequiredFields) {
    Gyeol::JsonIrDiagnostic diagnostic;
    diagnostic.code = "IR_SAMPLE";
    diagnostic.severity = "warning";
    diagnostic.path = "story.json";
    diagnostic.node = "start";
    diagnostic.instruction_index = 3;
    diagnostic.message = "샘플 진단";
    diagnostic.hint = "샘플 힌트";

    const auto encoded = Gyeol::JsonIrTooling::diagnosticsToJson({diagnostic});
    ASSERT_TRUE(encoded.contains("diagnostics"));
    ASSERT_TRUE(encoded["diagnostics"].is_array());
    ASSERT_EQ(encoded["diagnostics"].size(), 1u);

    const auto& entry = encoded["diagnostics"][0];
    EXPECT_TRUE(entry.contains("code"));
    EXPECT_TRUE(entry.contains("severity"));
    EXPECT_TRUE(entry.contains("path"));
    EXPECT_TRUE(entry.contains("node"));
    EXPECT_TRUE(entry.contains("instruction_index"));
    EXPECT_TRUE(entry.contains("message"));
    EXPECT_TRUE(entry.contains("hint"));
    EXPECT_EQ(entry["code"], "IR_SAMPLE");
    EXPECT_EQ(entry["severity"], "warning");
    EXPECT_EQ(entry["message"], "샘플 진단");
    EXPECT_EQ(entry["hint"], "샘플 힌트");
}

TEST(JsonIrToolingTest, PreviewGraphPatchSummaryMatchesApplyResult) {
    const std::string script =
        "label start:\n"
        "    jump end\n"
        "label end:\n"
        "    \"End\"\n";

    Gyeol::Parser previewParser;
    ASSERT_TRUE(previewParser.parseString(script));
    const StoryT& beforeStory = previewParser.getStory();

    const json patch = {
        {"format", "gyeol-graph-patch"},
        {"version", 1},
        {"ops", json::array({
            {{"op", "add_node"}, {"node", "bonus"}}
        })}
    };

    json preview;
    std::vector<Gyeol::JsonIrDiagnostic> diagnostics;
    std::string error;
    ASSERT_TRUE(Gyeol::JsonIrTooling::previewGraphPatch(
        beforeStory, "story.json", patch, preview, &diagnostics, &error))
        << error;
    EXPECT_FALSE(Gyeol::JsonIrTooling::hasErrors(diagnostics));

    Gyeol::Parser applyParser;
    ASSERT_TRUE(applyParser.parseString(script));
    ASSERT_TRUE(Gyeol::GraphTools::applyGraphPatchJson(applyParser.getStoryMutable(), patch, &error)) << error;
    const StoryT& afterStory = applyParser.getStory();

    const auto& summary = preview["summary"];
    EXPECT_EQ(summary["nodes_before"].get<int>(), static_cast<int>(beforeStory.nodes.size()));
    EXPECT_EQ(summary["nodes_after"].get<int>(), static_cast<int>(afterStory.nodes.size()));
    EXPECT_EQ(summary["nodes_delta"].get<int>(),
              static_cast<int>(afterStory.nodes.size() - beforeStory.nodes.size()));
    EXPECT_EQ(summary["instructions_before"].get<int>(), countInstructions(beforeStory));
    EXPECT_EQ(summary["instructions_after"].get<int>(), countInstructions(afterStory));
    EXPECT_EQ(summary["instructions_delta"].get<int>(),
              countInstructions(afterStory) - countInstructions(beforeStory));
    EXPECT_EQ(summary["edges_before"].get<int>(), countEdges(beforeStory));
    EXPECT_EQ(summary["edges_after"].get<int>(), countEdges(afterStory));
    EXPECT_EQ(summary["edges_delta"].get<int>(), countEdges(afterStory) - countEdges(beforeStory));

    ASSERT_TRUE(preview.contains("added_nodes"));
    ASSERT_TRUE(preview["added_nodes"].is_array());
    EXPECT_EQ(preview["added_nodes"].size(), 1u);
    EXPECT_EQ(preview["added_nodes"][0], "bonus");
}
