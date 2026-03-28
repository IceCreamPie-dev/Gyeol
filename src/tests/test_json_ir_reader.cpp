#include <gtest/gtest.h>

#include "gyeol_json_export.h"
#include "gyeol_json_ir_reader.h"
#include "gyeol_parser.h"
#include "gyeol_runner.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

TEST(JsonIrReaderTest, RoundTripFromExportAndRunnerParity) {
    const std::string script =
        "label start:\n"
        "    hero \"Hello\"\n"
        "    @ sfx \"ding.wav\" 0.7 true\n"
        "    menu:\n"
        "        \"Go\" -> next if can_go #once\n"
        "        \"Stop\" -> end\n"
        "label next:\n"
        "    \"Next\"\n"
        "label end:\n"
        "    \"End\"\n";

    Gyeol::Parser parser;
    ASSERT_TRUE(parser.parseString(script));
    const auto baselineBuffer = parser.compileToBuffer();
    ASSERT_FALSE(baselineBuffer.empty());

    const std::string jsonText = Gyeol::JsonExport::toJsonString(parser.getStory());

    ICPDev::Gyeol::Schema::StoryT importedStory;
    std::string error;
    ASSERT_TRUE(Gyeol::JsonIrReader::fromJsonString(jsonText, importedStory, &error)) << error;
    EXPECT_EQ(importedStory.start_node_name, "start");
    EXPECT_FALSE(importedStory.nodes.empty());
    EXPECT_EQ(importedStory.nodes[0]->name, "start");

    const auto importedBuffer = Gyeol::JsonIrReader::compileToBuffer(importedStory);
    ASSERT_FALSE(importedBuffer.empty());

    Gyeol::Runner baselineRunner;
    Gyeol::Runner importedRunner;
    ASSERT_TRUE(baselineRunner.start(baselineBuffer.data(), baselineBuffer.size()));
    ASSERT_TRUE(importedRunner.start(importedBuffer.data(), importedBuffer.size()));

    auto a1 = baselineRunner.step();
    auto b1 = importedRunner.step();
    ASSERT_EQ(a1.type, Gyeol::StepType::LINE);
    ASSERT_EQ(b1.type, Gyeol::StepType::LINE);
    EXPECT_STREQ(a1.line.text, b1.line.text);

    auto a2 = baselineRunner.step();
    auto b2 = importedRunner.step();
    ASSERT_EQ(a2.type, Gyeol::StepType::COMMAND);
    ASSERT_EQ(b2.type, Gyeol::StepType::COMMAND);
    EXPECT_STREQ(a2.command.type, b2.command.type);
    ASSERT_EQ(a2.command.args.size(), b2.command.args.size());
    EXPECT_EQ(a2.command.args[0].type, b2.command.args[0].type);
}

TEST(JsonIrReaderTest, RejectsInvalidFormatVersion) {
    json doc = {
        {"format", "gyeol-json-ir"},
        {"format_version", 1},
        {"version", "x"},
        {"start_node_name", "start"},
        {"string_pool", json::array({"start"})},
        {"line_ids", json::array({""})},
        {"nodes", json::array({
            {
                {"name", "start"},
                {"instructions", json::array({
                    {
                        {"type", "Line"},
                        {"character", nullptr},
                        {"text", "Hello"}
                    }
                })}
            }
        })}
    };

    ICPDev::Gyeol::Schema::StoryT story;
    std::string error;
    EXPECT_FALSE(Gyeol::JsonIrReader::fromJson(doc, story, &error));
    EXPECT_NE(error.find("format_version"), std::string::npos);
}
