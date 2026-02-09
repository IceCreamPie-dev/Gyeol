#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include "gyeol_parser.h"
#include "gyeol_json_export.h"
#include <string>

using json = nlohmann::json;

// Helper: parse script string and return JSON IR
static json compileToJson(const std::string& script) {
    Gyeol::Parser parser;
    EXPECT_TRUE(parser.parseString(script));
    return Gyeol::JsonExport::toJson(parser.getStory());
}

// ============================================================
// Basic structure tests
// ============================================================

TEST(JsonExportTest, BasicStructure) {
    auto j = compileToJson(R"(
label start:
    "Hello world"
)");

    EXPECT_EQ(j["format"], "gyeol-json-ir");
    EXPECT_EQ(j["format_version"], 1);
    EXPECT_EQ(j["start_node_name"], "start");
    EXPECT_TRUE(j["string_pool"].is_array());
    EXPECT_TRUE(j["nodes"].is_array());
    EXPECT_EQ(j["nodes"].size(), 1u);
}

TEST(JsonExportTest, NodeName) {
    auto j = compileToJson(R"(
label my_node:
    "text"
)");

    EXPECT_EQ(j["nodes"][0]["name"], "my_node");
}

// ============================================================
// Line tests
// ============================================================

TEST(JsonExportTest, NarrationLine) {
    auto j = compileToJson(R"(
label start:
    "Hello world"
)");

    auto& instr = j["nodes"][0]["instructions"][0];
    EXPECT_EQ(instr["type"], "Line");
    EXPECT_TRUE(instr["character"].is_null());
    EXPECT_EQ(instr["text"], "Hello world");
}

TEST(JsonExportTest, CharacterLine) {
    auto j = compileToJson(R"(
label start:
    hero "I am the hero"
)");

    auto& instr = j["nodes"][0]["instructions"][0];
    EXPECT_EQ(instr["type"], "Line");
    EXPECT_EQ(instr["character"], "hero");
    EXPECT_EQ(instr["text"], "I am the hero");
}

TEST(JsonExportTest, LineWithTags) {
    auto j = compileToJson(R"(
label start:
    hero "Hello" #mood:happy #pose:standing
)");

    auto& instr = j["nodes"][0]["instructions"][0];
    EXPECT_EQ(instr["type"], "Line");
    EXPECT_TRUE(instr.contains("tags"));
    EXPECT_EQ(instr["tags"].size(), 2u);
    EXPECT_EQ(instr["tags"][0]["key"], "mood");
    EXPECT_EQ(instr["tags"][0]["value"], "happy");
    EXPECT_EQ(instr["tags"][1]["key"], "pose");
    EXPECT_EQ(instr["tags"][1]["value"], "standing");
}

TEST(JsonExportTest, LineWithVoiceAsset) {
    auto j = compileToJson(R"(
label start:
    hero "Hello" #voice:hero_greeting.wav
)");

    auto& instr = j["nodes"][0]["instructions"][0];
    EXPECT_EQ(instr["type"], "Line");
    EXPECT_EQ(instr["voice_asset"], "hero_greeting.wav");
}

// ============================================================
// Choice tests
// ============================================================

TEST(JsonExportTest, BasicChoice) {
    auto j = compileToJson(R"(
label start:
    menu:
        "Go left" -> left
        "Go right" -> right
label left:
    "Left path"
label right:
    "Right path"
)");

    auto& instrs = j["nodes"][0]["instructions"];
    bool foundChoice = false;
    for (auto& instr : instrs) {
        if (instr["type"] == "Choice") {
            foundChoice = true;
            EXPECT_FALSE(instr["text"].get<std::string>().empty());
            EXPECT_FALSE(instr["target_node"].get<std::string>().empty());
            break;
        }
    }
    EXPECT_TRUE(foundChoice);
}

TEST(JsonExportTest, ChoiceModifier) {
    auto j = compileToJson(R"(
label start:
    menu:
        "Once choice" -> start #once
        "Fallback" -> start #fallback
label end:
    "done"
)");

    auto& instrs = j["nodes"][0]["instructions"];
    bool foundOnce = false;
    bool foundFallback = false;
    for (auto& instr : instrs) {
        if (instr["type"] == "Choice") {
            if (instr.contains("choice_modifier")) {
                if (instr["choice_modifier"] == "Once") foundOnce = true;
                if (instr["choice_modifier"] == "Fallback") foundFallback = true;
            }
        }
    }
    EXPECT_TRUE(foundOnce);
    EXPECT_TRUE(foundFallback);
}

// ============================================================
// Jump/Call tests
// ============================================================

TEST(JsonExportTest, JumpInstruction) {
    auto j = compileToJson(R"(
label start:
    jump target
label target:
    "arrived"
)");

    auto& instrs = j["nodes"][0]["instructions"];
    bool foundJump = false;
    for (auto& instr : instrs) {
        if (instr["type"] == "Jump") {
            foundJump = true;
            EXPECT_EQ(instr["target_node"], "target");
            EXPECT_EQ(instr["is_call"], false);
            break;
        }
    }
    EXPECT_TRUE(foundJump);
}

TEST(JsonExportTest, CallInstruction) {
    auto j = compileToJson(R"(
label start:
    call subroutine
label subroutine:
    "in sub"
    return
)");

    auto& instrs = j["nodes"][0]["instructions"];
    bool foundCall = false;
    for (auto& instr : instrs) {
        if (instr["type"] == "Jump" && instr["is_call"] == true) {
            foundCall = true;
            EXPECT_EQ(instr["target_node"], "subroutine");
            break;
        }
    }
    EXPECT_TRUE(foundCall);
}

// ============================================================
// Variable tests
// ============================================================

TEST(JsonExportTest, SetVarInt) {
    auto j = compileToJson(R"(
label start:
    $ hp = 100
)");

    auto& instr = j["nodes"][0]["instructions"][0];
    EXPECT_EQ(instr["type"], "SetVar");
    EXPECT_EQ(instr["var_name"], "hp");
    EXPECT_EQ(instr["assign_op"], "Assign");
    EXPECT_EQ(instr["value"]["type"], "Int");
    EXPECT_EQ(instr["value"]["val"], 100);
}

TEST(JsonExportTest, SetVarBool) {
    auto j = compileToJson(R"(
label start:
    $ flag = true
)");

    auto& instr = j["nodes"][0]["instructions"][0];
    EXPECT_EQ(instr["value"]["type"], "Bool");
    EXPECT_EQ(instr["value"]["val"], true);
}

TEST(JsonExportTest, SetVarString) {
    auto j = compileToJson(R"(
label start:
    $ name = "Alice"
)");

    auto& instr = j["nodes"][0]["instructions"][0];
    EXPECT_EQ(instr["value"]["type"], "String");
    EXPECT_EQ(instr["value"]["val"], "Alice");
}

TEST(JsonExportTest, SetVarExpression) {
    auto j = compileToJson(R"(
label start:
    $ hp = 10
    $ hp = hp + 5
)");

    auto& instr = j["nodes"][0]["instructions"][1];
    EXPECT_EQ(instr["type"], "SetVar");
    EXPECT_EQ(instr["var_name"], "hp");
    EXPECT_FALSE(instr["expr"].is_null());
    EXPECT_TRUE(instr["expr"]["tokens"].is_array());
    EXPECT_GT(instr["expr"]["tokens"].size(), 0u);
}

TEST(JsonExportTest, GlobalVars) {
    auto j = compileToJson(R"(
$ score = 0
$ name = "Player"
label start:
    "Hello"
)");

    EXPECT_TRUE(j.contains("global_vars"));
    EXPECT_EQ(j["global_vars"].size(), 2u);
    EXPECT_EQ(j["global_vars"][0]["var_name"], "score");
    EXPECT_EQ(j["global_vars"][1]["var_name"], "name");
}

// ============================================================
// Condition tests
// ============================================================

TEST(JsonExportTest, ConditionSimple) {
    auto j = compileToJson(R"(
label start:
    $ hp = 10
    if hp > 0 -> alive else dead
label alive:
    "alive"
label dead:
    "dead"
)");

    auto& instrs = j["nodes"][0]["instructions"];
    bool foundCond = false;
    for (auto& instr : instrs) {
        if (instr["type"] == "Condition") {
            foundCond = true;
            EXPECT_EQ(instr["true_jump_node"], "alive");
            EXPECT_EQ(instr["false_jump_node"], "dead");
            break;
        }
    }
    EXPECT_TRUE(foundCond);
}

// ============================================================
// Command tests
// ============================================================

TEST(JsonExportTest, CommandInstruction) {
    auto j = compileToJson(R"(
label start:
    @ bg forest.png
)");

    auto& instr = j["nodes"][0]["instructions"][0];
    EXPECT_EQ(instr["type"], "Command");
    EXPECT_EQ(instr["command_type"], "bg");
    EXPECT_EQ(instr["params"].size(), 1u);
    EXPECT_EQ(instr["params"][0], "forest.png");
}

// ============================================================
// Random tests
// ============================================================

TEST(JsonExportTest, RandomBranch) {
    auto j = compileToJson(R"(
label start:
    random:
        50 -> a
        30 -> b
        -> c
label a:
    "A"
label b:
    "B"
label c:
    "C"
)");

    auto& instrs = j["nodes"][0]["instructions"];
    bool foundRandom = false;
    for (auto& instr : instrs) {
        if (instr["type"] == "Random") {
            foundRandom = true;
            EXPECT_EQ(instr["branches"].size(), 3u);
            EXPECT_EQ(instr["branches"][0]["weight"], 50);
            EXPECT_EQ(instr["branches"][1]["weight"], 30);
            EXPECT_EQ(instr["branches"][2]["weight"], 1);
            break;
        }
    }
    EXPECT_TRUE(foundRandom);
}

// ============================================================
// Character definition tests
// ============================================================

TEST(JsonExportTest, CharacterDef) {
    auto j = compileToJson(R"(
character hero:
    name: "The Hero"
    color: "#FF0000"

label start:
    hero "Hello"
)");

    EXPECT_TRUE(j.contains("characters"));
    EXPECT_EQ(j["characters"].size(), 1u);
    EXPECT_EQ(j["characters"][0]["name"], "hero");
    EXPECT_EQ(j["characters"][0]["properties"].size(), 2u);
}

// ============================================================
// Node tags tests
// ============================================================

TEST(JsonExportTest, NodeTags) {
    auto j = compileToJson(R"(
label start #repeatable #difficulty=hard:
    "Hello"
)");

    EXPECT_TRUE(j["nodes"][0].contains("tags"));
    auto& tags = j["nodes"][0]["tags"];
    EXPECT_EQ(tags.size(), 2u);
}

// ============================================================
// Function parameters tests
// ============================================================

TEST(JsonExportTest, FunctionParams) {
    auto j = compileToJson(R"(
label add(a, b):
    return a
label start:
    $ result = call add(1, 2)
)");

    // Check param names
    auto& addNode = j["nodes"][0];
    EXPECT_TRUE(addNode.contains("params"));
    EXPECT_EQ(addNode["params"].size(), 2u);
    EXPECT_EQ(addNode["params"][0], "a");
    EXPECT_EQ(addNode["params"][1], "b");

    // Check CallWithReturn
    auto& startInstrs = j["nodes"][1]["instructions"];
    bool foundCWR = false;
    for (auto& instr : startInstrs) {
        if (instr["type"] == "CallWithReturn") {
            foundCWR = true;
            EXPECT_EQ(instr["target_node"], "add");
            EXPECT_EQ(instr["return_var"], "result");
            EXPECT_TRUE(instr.contains("arg_exprs"));
            EXPECT_EQ(instr["arg_exprs"].size(), 2u);
            break;
        }
    }
    EXPECT_TRUE(foundCWR);
}

// ============================================================
// Return instruction tests
// ============================================================

TEST(JsonExportTest, ReturnValue) {
    auto j = compileToJson(R"(
label func:
    return 42
label start:
    "Hello"
)");

    auto& instrs = j["nodes"][0]["instructions"];
    bool foundReturn = false;
    for (auto& instr : instrs) {
        if (instr["type"] == "Return") {
            foundReturn = true;
            break;
        }
    }
    EXPECT_TRUE(foundReturn);
}

// ============================================================
// Expression token tests
// ============================================================

TEST(JsonExportTest, ExpressionTokens) {
    auto j = compileToJson(R"(
label start:
    $ x = 1 + 2 * 3
)");

    auto& instr = j["nodes"][0]["instructions"][0];
    EXPECT_EQ(instr["type"], "SetVar");
    auto& tokens = instr["expr"]["tokens"];
    EXPECT_TRUE(tokens.is_array());

    // Verify token structure (each should have at least "op")
    for (auto& token : tokens) {
        EXPECT_TRUE(token.contains("op"));
    }
}

// ============================================================
// toJsonString test
// ============================================================

TEST(JsonExportTest, ToJsonString) {
    Gyeol::Parser parser;
    EXPECT_TRUE(parser.parseString(R"(
label start:
    "Hello"
)"));

    std::string jsonStr = Gyeol::JsonExport::toJsonString(parser.getStory());
    EXPECT_FALSE(jsonStr.empty());

    // Should be valid JSON
    auto parsed = json::parse(jsonStr);
    EXPECT_EQ(parsed["format"], "gyeol-json-ir");
}

// ============================================================
// Multi-node story
// ============================================================

TEST(JsonExportTest, MultiNodeStory) {
    auto j = compileToJson(R"(
$ score = 0

character npc:
    name: "NPC"

label start:
    npc "Welcome!" #mood:friendly
    $ score = score + 10
    menu:
        "Fight" -> battle
        "Talk" -> dialogue
    jump ending

label battle:
    @ sfx sword.wav
    "You fight bravely"
    jump ending

label dialogue:
    npc "Let's talk"
    jump ending

label ending:
    "The end"
)");

    // Verify overall structure
    EXPECT_EQ(j["format"], "gyeol-json-ir");
    EXPECT_EQ(j["start_node_name"], "start");
    EXPECT_EQ(j["nodes"].size(), 4u);
    EXPECT_TRUE(j.contains("global_vars"));
    EXPECT_TRUE(j.contains("characters"));

    // Verify all string pool references are resolved (no raw indices in output)
    std::string jsonStr = j.dump();
    // The JSON should contain actual text, not just numbers for string pool indices
    EXPECT_NE(jsonStr.find("Welcome!"), std::string::npos);
    EXPECT_NE(jsonStr.find("NPC"), std::string::npos);
    EXPECT_NE(jsonStr.find("sword.wav"), std::string::npos);
}
