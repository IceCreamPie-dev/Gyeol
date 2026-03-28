#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <cstdio>
#include <fstream>
#include <string>

#include "gyeol_graph_tools.h"
#include "gyeol_parser.h"

using json = nlohmann::json;

namespace {

Gyeol::Parser parseScript(const std::string& script) {
    Gyeol::Parser parser;
    EXPECT_TRUE(parser.parseString(script));
    return parser;
}

bool hasNode(const json& doc, const std::string& name) {
    for (const auto& node : doc["nodes"]) {
        if (node["name"] == name) return true;
    }
    return false;
}

std::string makeTempPath(const std::string& filename) {
    static int counter = 0;
    ++counter;
    return "test_tmp_graph_tools_" + std::to_string(counter) + "_" + filename;
}

} // namespace

TEST(GraphToolsTest, ExportGraphDocDeterministic) {
    auto parser = parseScript(
        "label start:\n"
        "    menu:\n"
        "        \"A\" -> a\n"
        "        \"B\" -> b\n"
        "label a:\n"
        "    \"A\"\n"
        "label b:\n"
        "    \"B\"\n");

    auto j1 = Gyeol::GraphTools::buildGraphDoc(parser.getStory());
    auto j2 = Gyeol::GraphTools::buildGraphDoc(parser.getStory());

    EXPECT_EQ(j1.dump(), j2.dump());
    EXPECT_EQ(j1["format"], "gyeol-graph-doc");
    EXPECT_EQ(j1["version"], 1);
    ASSERT_TRUE(j1["edges"].is_array());
    ASSERT_FALSE(j1["edges"].empty());
    EXPECT_TRUE(j1["edges"][0].contains("edge_id"));
    ASSERT_TRUE(j1["nodes"].is_array());
    ASSERT_FALSE(j1["nodes"].empty());
    EXPECT_TRUE(j1["nodes"][0].contains("instructions"));
    ASSERT_TRUE(j1["nodes"][0]["instructions"].is_array());
    ASSERT_FALSE(j1["nodes"][0]["instructions"].empty());
    EXPECT_TRUE(j1["nodes"][0]["instructions"][0].contains("instruction_id"));
}

TEST(GraphToolsTest, RejectsUnsupportedPatchOp) {
    auto parser = parseScript(
        "label start:\n"
        "    jump end\n"
        "label end:\n"
        "    \"end\"\n");

    json patch = {
        {"format", "gyeol-graph-patch"},
        {"version", 1},
        {"ops", json::array({
            {{"op", "unsupported"}}
        })}
    };

    std::string error;
    EXPECT_FALSE(Gyeol::GraphTools::validateGraphPatchJson(parser.getStory(), patch, &error));
    EXPECT_NE(error.find("Unsupported op"), std::string::npos);
}

TEST(GraphToolsTest, RenameNodeRetargetsAllReferences) {
    auto parser = parseScript(
        "label start:\n"
        "    $ flag = 1\n"
        "    jump mid\n"
        "    menu:\n"
        "        \"Go\" -> mid\n"
        "    if flag == 1 -> mid else end\n"
        "    random:\n"
        "        1 -> mid\n"
        "        1 -> end\n"
        "    $ out = call mid()\n"
        "label mid:\n"
        "    \"M\"\n"
        "label end:\n"
        "    \"E\"\n");

    json patch = {
        {"format", "gyeol-graph-patch"},
        {"version", 1},
        {"ops", json::array({
            {{"op", "rename_node"}, {"from", "mid"}, {"to", "renamed_mid"}}
        })}
    };

    std::string error;
    ASSERT_TRUE(Gyeol::GraphTools::applyGraphPatchJson(parser.getStoryMutable(), patch, &error)) << error;
    auto doc = Gyeol::GraphTools::buildGraphDoc(parser.getStory());

    EXPECT_TRUE(hasNode(doc, "renamed_mid"));
    EXPECT_FALSE(hasNode(doc, "mid"));

    bool foundRetargeted = false;
    for (const auto& edge : doc["edges"]) {
        EXPECT_NE(edge["to"].get<std::string>(), "mid");
        if (edge["to"] == "renamed_mid") {
            foundRetargeted = true;
        }
    }
    EXPECT_TRUE(foundRetargeted);
}

TEST(GraphToolsTest, DeleteNodeRedirectsInboundEdges) {
    auto parser = parseScript(
        "label start:\n"
        "    jump victim\n"
        "label other:\n"
        "    jump victim\n"
        "label victim:\n"
        "    \"victim\"\n"
        "label sink:\n"
        "    \"sink\"\n");

    json patch = {
        {"format", "gyeol-graph-patch"},
        {"version", 1},
        {"ops", json::array({
            {{"op", "delete_node"}, {"node", "victim"}, {"redirect_target", "sink"}}
        })}
    };

    std::string error;
    ASSERT_TRUE(Gyeol::GraphTools::applyGraphPatchJson(parser.getStoryMutable(), patch, &error)) << error;
    auto doc = Gyeol::GraphTools::buildGraphDoc(parser.getStory());

    EXPECT_FALSE(hasNode(doc, "victim"));
    for (const auto& edge : doc["edges"]) {
        EXPECT_NE(edge["to"], "victim");
    }
}

TEST(GraphToolsTest, RetargetEdgeAffectsOnlySelectedEdge) {
    auto parser = parseScript(
        "label start:\n"
        "    menu:\n"
        "        \"A\" -> a\n"
        "        \"B\" -> b\n"
        "label a:\n"
        "    \"A\"\n"
        "label b:\n"
        "    \"B\"\n"
        "label c:\n"
        "    \"C\"\n");

    auto doc = Gyeol::GraphTools::buildGraphDoc(parser.getStory());
    std::string edgeIdA;
    for (const auto& edge : doc["edges"]) {
        if (edge["type"] == "choice" && edge.contains("label") && edge["label"] == "A") {
            edgeIdA = edge["edge_id"].get<std::string>();
            break;
        }
    }
    ASSERT_FALSE(edgeIdA.empty());

    json patch = {
        {"format", "gyeol-graph-patch"},
        {"version", 1},
        {"ops", json::array({
            {{"op", "retarget_edge"}, {"edge_id", edgeIdA}, {"to", "c"}}
        })}
    };

    std::string error;
    ASSERT_TRUE(Gyeol::GraphTools::applyGraphPatchJson(parser.getStoryMutable(), patch, &error)) << error;
    auto after = Gyeol::GraphTools::buildGraphDoc(parser.getStory());

    int toC = 0;
    int toB = 0;
    for (const auto& edge : after["edges"]) {
        if (edge["from"] != "start" || edge["type"] != "choice") continue;
        if (edge["to"] == "c") toC++;
        if (edge["to"] == "b") toB++;
    }
    EXPECT_EQ(toC, 1);
    EXPECT_EQ(toB, 1);
}

TEST(GraphToolsTest, ApplyPatchIsAtomicOnFailure) {
    auto parser = parseScript(
        "label start:\n"
        "    jump a\n"
        "label a:\n"
        "    \"A\"\n");

    const std::string before = Gyeol::GraphTools::buildGraphDoc(parser.getStory()).dump();

    json invalidPatch = {
        {"format", "gyeol-graph-patch"},
        {"version", 1},
        {"ops", json::array({
            {{"op", "retarget_edge"}, {"edge_id", "n999:i999:jump:0"}, {"to", "a"}}
        })}
    };

    std::string error;
    EXPECT_FALSE(Gyeol::GraphTools::applyGraphPatchJson(parser.getStoryMutable(), invalidPatch, &error));
    const std::string after = Gyeol::GraphTools::buildGraphDoc(parser.getStory()).dump();
    EXPECT_EQ(before, after);
}

TEST(GraphToolsTest, DeleteStartRequiresSetStartNodeFirst) {
    auto parser = parseScript(
        "label start:\n"
        "    jump a\n"
        "label a:\n"
        "    \"A\"\n");

    json badPatch = {
        {"format", "gyeol-graph-patch"},
        {"version", 1},
        {"ops", json::array({
            {{"op", "delete_node"}, {"node", "start"}, {"redirect_target", "a"}}
        })}
    };

    std::string error;
    EXPECT_FALSE(Gyeol::GraphTools::applyGraphPatchJson(parser.getStoryMutable(), badPatch, &error));

    json goodPatch = {
        {"format", "gyeol-graph-patch"},
        {"version", 1},
        {"ops", json::array({
            {{"op", "set_start_node"}, {"node", "a"}},
            {{"op", "delete_node"}, {"node", "start"}, {"redirect_target", "a"}}
        })}
    };

    ASSERT_TRUE(Gyeol::GraphTools::applyGraphPatchJson(parser.getStoryMutable(), goodPatch, &error)) << error;
    EXPECT_EQ(parser.getStory().start_node_name, "a");
}

TEST(GraphToolsTest, CanonicalEmitterRoundTripAfterPatch) {
    auto parser = parseScript(
        "label start:\n"
        "    menu:\n"
        "        \"A\" -> next\n"
        "    random:\n"
        "        2 -> next\n"
        "        1 -> end\n"
        "    $ x = 1\n"
        "    if x == 1 -> next else end\n"
        "label next:\n"
        "    hero \"Hello\" #voice=v1.ogg\n"
        "label end:\n"
        "    return\n");

    json patch = {
        {"format", "gyeol-graph-patch"},
        {"version", 1},
        {"ops", json::array({
            {{"op", "rename_node"}, {"from", "next"}, {"to", "next2"}}
        })}
    };

    std::string error;
    ASSERT_TRUE(Gyeol::GraphTools::applyGraphPatchJson(parser.getStoryMutable(), patch, &error)) << error;

    std::string script = Gyeol::GraphTools::toCanonicalScript(parser.getStory(), &error);
    ASSERT_FALSE(script.empty()) << error;

    Gyeol::Parser roundTrip;
    ASSERT_TRUE(roundTrip.parseString(script));
    auto doc = Gyeol::GraphTools::buildGraphDoc(roundTrip.getStory());
    EXPECT_TRUE(hasNode(doc, "next2"));
}

TEST(GraphToolsTest, PatchV2SupportsInstructionEditingOps) {
    auto parser = parseScript(
        "label start:\n"
        "    \"Hello\"\n"
        "    @ bg \"forest.png\"\n"
        "    $ hp = 1\n"
        "    if hp == 1 -> next else end\n"
        "    menu:\n"
        "        \"Go\" -> next\n"
        "        \"Stop\" -> end\n"
        "label next:\n"
        "    \"Next\"\n"
        "    \"extra\"\n"
        "label end:\n"
        "    \"End\"\n");

    json patch = {
        {"format", "gyeol-graph-patch"},
        {"version", 2},
        {"ops", json::array({
            {{"op", "update_line_text"}, {"instruction_id", "n0:i0"}, {"text", "HELLO_V2"}},
            {{"op", "update_choice_text"}, {"instruction_id", "n0:i4"}, {"text", "Go now"}},
            {{"op", "update_command"}, {"instruction_id", "n0:i1"}, {"command_type", "sfx"}, {"args", json::array({
                {{"kind", "String"}, {"value", "hit.wav"}},
                {{"kind", "Float"}, {"value", 0.8}}
            })}},
            {{"op", "update_expression"}, {"instruction_id", "n0:i2"}, {"expr_text", "2 + 3"}},
            {{"op", "update_expression"}, {"instruction_id", "n0:i3"}, {"expr_text", "hp > 2"}},
            {{"op", "insert_instruction"}, {"node", "start"}, {"index", 0}, {"instruction", {{"type", "Line"}, {"text", "Prelude"}}}},
            {{"op", "move_instruction"}, {"instruction_id", "n0:i5"}, {"before_instruction_id", "n0:i4"}},
            {{"op", "delete_instruction"}, {"instruction_id", "n1:i1"}}
        })}
    };

    std::string error;
    ASSERT_TRUE(Gyeol::GraphTools::applyGraphPatchJson(parser.getStoryMutable(), patch, &error)) << error;

    const std::string script = Gyeol::GraphTools::toCanonicalScript(parser.getStory(), &error);
    ASSERT_FALSE(script.empty()) << error;

    EXPECT_NE(script.find("\"Prelude\""), std::string::npos);
    EXPECT_NE(script.find("\"HELLO_V2\""), std::string::npos);
    EXPECT_NE(script.find("\"Go now\" -> next"), std::string::npos);
    EXPECT_NE(script.find("@ sfx \"hit.wav\" 0.8"), std::string::npos);
    EXPECT_NE(script.find("$ hp = "), std::string::npos);
    EXPECT_EQ(script.find("$ hp = 1"), std::string::npos);
    EXPECT_NE(script.find("if hp > 2 -> next else end"), std::string::npos);
    EXPECT_EQ(script.find("\"extra\""), std::string::npos);

    const size_t stopPos = script.find("\"Stop\" -> end");
    const size_t goPos = script.find("\"Go now\" -> next");
    ASSERT_NE(stopPos, std::string::npos);
    ASSERT_NE(goPos, std::string::npos);
    EXPECT_LT(stopPos, goPos);
}

TEST(GraphToolsTest, PatchV2RejectsInstructionIdFromSamePatchInsertion) {
    auto parser = parseScript(
        "label start:\n"
        "    \"A\"\n"
        "    \"B\"\n");

    json patch = {
        {"format", "gyeol-graph-patch"},
        {"version", 2},
        {"ops", json::array({
            {{"op", "insert_instruction"}, {"node", "start"}, {"index", 2}, {"instruction", {{"type", "Line"}, {"text", "Inserted"}}}},
            {{"op", "update_line_text"}, {"instruction_id", "n0:i2"}, {"text", "ShouldFail"}}
        })}
    };

    std::string error;
    EXPECT_FALSE(Gyeol::GraphTools::validateGraphPatchJson(parser.getStory(), patch, &error));
    EXPECT_NE(error.find("Unknown instruction_id"), std::string::npos);
}

TEST(GraphToolsTest, PreserveLineIdGeneratesMapAndCompileRestoresIds) {
    auto parser = parseScript(
        "label start:\n"
        "    \"Hello\"\n"
        "    menu:\n"
        "        \"Go\" -> end\n"
        "label end:\n"
        "    \"End\"\n");

    const auto& beforeStory = parser.getStory();
    ASSERT_FALSE(beforeStory.nodes.empty());
    ASSERT_FALSE(beforeStory.nodes[0]->lines.empty());
    auto* originalLine = beforeStory.nodes[0]->lines[0]->data.AsLine();
    ASSERT_NE(originalLine, nullptr);
    ASSERT_GE(originalLine->text_id, 0);

    json patch = {
        {"format", "gyeol-graph-patch"},
        {"version", 2},
        {"ops", json::array({
            {{"op", "insert_instruction"}, {"node", "start"}, {"index", 0}, {"instruction", {{"type", "Line"}, {"text", "Prelude"}}}},
            {{"op", "update_line_text"}, {"instruction_id", "n0:i0"}, {"text", "Hello edited"}}
        })}
    };

    json lineIdMap;
    std::string error;
    ASSERT_TRUE(Gyeol::GraphTools::applyGraphPatchJsonWithOptions(
        parser.getStoryMutable(), patch, true, &lineIdMap, &error)) << error;

    EXPECT_EQ(lineIdMap["format"], "gyeol-line-id-map");
    EXPECT_EQ(lineIdMap["version"], 1);
    ASSERT_TRUE(lineIdMap["entries"].is_object());
    ASSERT_TRUE(lineIdMap["entries"].contains("n0:i1"));
    const std::string preservedId = lineIdMap["entries"]["n0:i1"].get<std::string>();
    ASSERT_FALSE(preservedId.empty());

    const std::string patchedPath = makeTempPath("patched.gyeol");
    const std::string mapPath = makeTempPath("patched.lineidmap.json");
    const std::string outPath = makeTempPath("patched.gyb");

    ASSERT_TRUE(Gyeol::GraphTools::writeCanonicalScript(parser.getStory(), patchedPath, &error)) << error;
    ASSERT_TRUE(Gyeol::GraphTools::writeLineIdMap(lineIdMap, mapPath, &error)) << error;

    Gyeol::Parser compileParser;
    ASSERT_TRUE(compileParser.parse(patchedPath));
    ASSERT_TRUE(compileParser.applyLineIdMap(mapPath, &error)) << error;
    ASSERT_TRUE(compileParser.compile(outPath));

    const auto& afterStory = compileParser.getStory();
    ASSERT_FALSE(afterStory.nodes.empty());
    ASSERT_GT(afterStory.nodes[0]->lines.size(), 1u);
    auto* preservedLine = afterStory.nodes[0]->lines[1]->data.AsLine();
    ASSERT_NE(preservedLine, nullptr);
    ASSERT_GE(preservedLine->text_id, 0);
    ASSERT_LT(static_cast<size_t>(preservedLine->text_id), afterStory.line_ids.size());
    EXPECT_EQ(afterStory.line_ids[static_cast<size_t>(preservedLine->text_id)], preservedId);

    std::remove(patchedPath.c_str());
    std::remove(mapPath.c_str());
    std::remove(outPath.c_str());
}
