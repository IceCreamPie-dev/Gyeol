#include <gtest/gtest.h>
#include "test_helpers.h"
#include "gyeol_parser.h"
#include "gyeol_generated.h"
#include <fstream>
#include <cstdio>

using namespace Gyeol;
using namespace ICPDev::Gyeol::Schema;

// --- 파서 기본 기능 ---

TEST(ParserTest, EmptyFileReturnsError) {
    std::string path = "test_empty.gyeol";
    { std::ofstream ofs(path); ofs << ""; }

    Parser parser;
    EXPECT_FALSE(parser.parse(path));
    EXPECT_FALSE(parser.getError().empty());
    std::remove(path.c_str());
}

TEST(ParserTest, CommentOnlyFileReturnsError) {
    std::string path = "test_comment.gyeol";
    { std::ofstream ofs(path); ofs << "# comment only\n# another\n"; }

    Parser parser;
    EXPECT_FALSE(parser.parse(path));
    std::remove(path.c_str());
}

TEST(ParserTest, SingleLabel) {
    std::string path = "test_label.gyeol";
    {
        std::ofstream ofs(path);
        ofs << "label start:\n    hero \"hello\"\n";
    }

    Parser parser;
    EXPECT_TRUE(parser.parse(path));
    EXPECT_TRUE(parser.compile("test_label.gyb"));

    std::remove(path.c_str());
    std::remove("test_label.gyb");
}

TEST(ParserTest, NarrationLine) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    \"This is narration\"\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    ASSERT_NE(story, nullptr);
    ASSERT_EQ(story->nodes()->size(), 1u);

    auto* node = story->nodes()->Get(0);
    ASSERT_EQ(node->lines()->size(), 1u);

    auto* instr = node->lines()->Get(0);
    ASSERT_EQ(instr->data_type(), OpData::Line);

    auto* line = instr->data_as_Line();
    EXPECT_EQ(line->character_id(), -1); // narration
    EXPECT_STREQ(story->string_pool()->Get(
        static_cast<uint32_t>(line->text_id()))->c_str(), "This is narration");
}

TEST(ParserTest, CharacterDialogue) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    hero \"Hello world\"\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* instr = story->nodes()->Get(0)->lines()->Get(0);
    auto* line = instr->data_as_Line();

    EXPECT_GE(line->character_id(), 0);
    EXPECT_STREQ(story->string_pool()->Get(
        static_cast<uint32_t>(line->character_id()))->c_str(), "hero");
}

TEST(ParserTest, MenuChoices) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    menu:\n"
        "        \"Choice A\" -> nodeA\n"
        "        \"Choice B\" -> nodeB\n"
        "label nodeA:\n"
        "    \"A\"\n"
        "label nodeB:\n"
        "    \"B\"\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* startNode = story->nodes()->Get(0);
    ASSERT_EQ(startNode->lines()->size(), 2u); // 2 choices

    auto* c0 = startNode->lines()->Get(0);
    auto* c1 = startNode->lines()->Get(1);
    EXPECT_EQ(c0->data_type(), OpData::Choice);
    EXPECT_EQ(c1->data_type(), OpData::Choice);
}

TEST(ParserTest, JumpInstruction) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    jump other\n"
        "label other:\n"
        "    \"end\"\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* instr = story->nodes()->Get(0)->lines()->Get(0);
    EXPECT_EQ(instr->data_type(), OpData::Jump);

    auto* jump = instr->data_as_Jump();
    EXPECT_FALSE(jump->is_call());
}

TEST(ParserTest, CallInstruction) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    call sub\n"
        "label sub:\n"
        "    \"sub content\"\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* instr = story->nodes()->Get(0)->lines()->Get(0);
    auto* jump = instr->data_as_Jump();
    EXPECT_TRUE(jump->is_call());
}

TEST(ParserTest, SetVarBool) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ flag = true\n"
        "    \"done\"\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* instr = story->nodes()->Get(0)->lines()->Get(0);
    EXPECT_EQ(instr->data_type(), OpData::SetVar);

    auto* sv = instr->data_as_SetVar();
    EXPECT_EQ(sv->value_type(), ValueData::BoolValue);
}

TEST(ParserTest, SetVarInt) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ score = 42\n"
        "    \"done\"\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* sv = story->nodes()->Get(0)->lines()->Get(0)->data_as_SetVar();
    EXPECT_EQ(sv->value_type(), ValueData::IntValue);
    EXPECT_EQ(sv->value_as_IntValue()->val(), 42);
}

TEST(ParserTest, SetVarFloat) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ rate = 3.14\n"
        "    \"done\"\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* sv = story->nodes()->Get(0)->lines()->Get(0)->data_as_SetVar();
    EXPECT_EQ(sv->value_type(), ValueData::FloatValue);
    EXPECT_FLOAT_EQ(sv->value_as_FloatValue()->val(), 3.14f);
}

TEST(ParserTest, SetVarString) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ name = \"hero\"\n"
        "    \"done\"\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* sv = story->nodes()->Get(0)->lines()->Get(0)->data_as_SetVar();
    EXPECT_EQ(sv->value_type(), ValueData::StringRef);
}

TEST(ParserTest, ConditionInstruction) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = 1\n"
        "    if x == 1 -> yes else no\n"
        "label yes:\n"
        "    \"yes\"\n"
        "label no:\n"
        "    \"no\"\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* instr = story->nodes()->Get(0)->lines()->Get(1);
    EXPECT_EQ(instr->data_type(), OpData::Condition);

    auto* cond = instr->data_as_Condition();
    EXPECT_EQ(cond->op(), Operator::Equal);
    EXPECT_GE(cond->true_jump_node_id(), 0);
    EXPECT_GE(cond->false_jump_node_id(), 0);
}

TEST(ParserTest, CommandInstruction) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    @ bg \"forest.png\"\n"
        "    \"done\"\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* instr = story->nodes()->Get(0)->lines()->Get(0);
    EXPECT_EQ(instr->data_type(), OpData::Command);

    auto* cmd = instr->data_as_Command();
    EXPECT_STREQ(story->string_pool()->Get(
        static_cast<uint32_t>(cmd->type_id()))->c_str(), "bg");
    ASSERT_EQ(cmd->params()->size(), 1u);
}

TEST(ParserTest, StringPoolDedup) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    hero \"hello\"\n"
        "    hero \"world\"\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    // "hero" should appear only once in pool, "hello", "world", "start" = unique entries
    auto* pool = story->string_pool();
    int heroCount = 0;
    for (uint32_t i = 0; i < pool->size(); ++i) {
        if (std::string(pool->Get(i)->c_str()) == "hero") heroCount++;
    }
    EXPECT_EQ(heroCount, 1); // dedup
}

TEST(ParserTest, MultipleLabels) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    \"a\"\n"
        "label mid:\n"
        "    \"b\"\n"
        "label end:\n"
        "    \"c\"\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    EXPECT_EQ(story->nodes()->size(), 3u);
    EXPECT_STREQ(story->start_node_name()->c_str(), "start");
}

TEST(ParserTest, EscapeSequences) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    \"line1\\nline2\\ttab\"\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* line = story->nodes()->Get(0)->lines()->Get(0)->data_as_Line();
    std::string text = story->string_pool()->Get(
        static_cast<uint32_t>(line->text_id()))->c_str();
    EXPECT_NE(text.find('\n'), std::string::npos);
    EXPECT_NE(text.find('\t'), std::string::npos);
}

TEST(ParserTest, StartNodeIsFirstLabel) {
    auto buf = GyeolTest::compileScript(
        "label intro:\n"
        "    \"hello\"\n"
        "label main:\n"
        "    \"world\"\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    EXPECT_STREQ(story->start_node_name()->c_str(), "intro");
}

// --- 새 기능: voice_asset_id ---

TEST(ParserTest, VoiceAssetTag) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    hero \"hello\" #voice:hero_01.wav\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* line = story->nodes()->Get(0)->lines()->Get(0)->data_as_Line();
    EXPECT_GE(line->voice_asset_id(), 0);
    EXPECT_STREQ(story->string_pool()->Get(
        static_cast<uint32_t>(line->voice_asset_id()))->c_str(), "hero_01.wav");
}

TEST(ParserTest, VoiceAssetNarration) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    \"narration\" #voice:nar_01.ogg\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* line = story->nodes()->Get(0)->lines()->Get(0)->data_as_Line();
    EXPECT_EQ(line->character_id(), -1);
    EXPECT_GE(line->voice_asset_id(), 0);
    EXPECT_STREQ(story->string_pool()->Get(
        static_cast<uint32_t>(line->voice_asset_id()))->c_str(), "nar_01.ogg");
}

TEST(ParserTest, NoVoiceAsset) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    hero \"hello\"\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* line = story->nodes()->Get(0)->lines()->Get(0)->data_as_Line();
    EXPECT_EQ(line->voice_asset_id(), -1);
}

// --- 태그 시스템 ---

TEST(ParserTest, SingleTag) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    hero \"hello\" #mood:angry\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* line = story->nodes()->Get(0)->lines()->Get(0)->data_as_Line();
    ASSERT_NE(line->tags(), nullptr);
    ASSERT_EQ(line->tags()->size(), 1u);
    auto* tag = line->tags()->Get(0);
    EXPECT_STREQ(story->string_pool()->Get(
        static_cast<uint32_t>(tag->key_id()))->c_str(), "mood");
    EXPECT_STREQ(story->string_pool()->Get(
        static_cast<uint32_t>(tag->value_id()))->c_str(), "angry");
}

TEST(ParserTest, MultipleTags) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    hero \"hello\" #mood:angry #pose:arms_crossed\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* line = story->nodes()->Get(0)->lines()->Get(0)->data_as_Line();
    ASSERT_NE(line->tags(), nullptr);
    ASSERT_EQ(line->tags()->size(), 2u);
    EXPECT_STREQ(story->string_pool()->Get(
        static_cast<uint32_t>(line->tags()->Get(0)->key_id()))->c_str(), "mood");
    EXPECT_STREQ(story->string_pool()->Get(
        static_cast<uint32_t>(line->tags()->Get(0)->value_id()))->c_str(), "angry");
    EXPECT_STREQ(story->string_pool()->Get(
        static_cast<uint32_t>(line->tags()->Get(1)->key_id()))->c_str(), "pose");
    EXPECT_STREQ(story->string_pool()->Get(
        static_cast<uint32_t>(line->tags()->Get(1)->value_id()))->c_str(), "arms_crossed");
}

TEST(ParserTest, VoiceTagBackwardCompat) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    hero \"hello\" #voice:hero.wav #mood:happy\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* line = story->nodes()->Get(0)->lines()->Get(0)->data_as_Line();
    // voice_asset_id 하위 호환
    EXPECT_GE(line->voice_asset_id(), 0);
    EXPECT_STREQ(story->string_pool()->Get(
        static_cast<uint32_t>(line->voice_asset_id()))->c_str(), "hero.wav");
    // tags에도 voice 포함
    ASSERT_NE(line->tags(), nullptr);
    ASSERT_EQ(line->tags()->size(), 2u);
    EXPECT_STREQ(story->string_pool()->Get(
        static_cast<uint32_t>(line->tags()->Get(0)->key_id()))->c_str(), "voice");
    EXPECT_STREQ(story->string_pool()->Get(
        static_cast<uint32_t>(line->tags()->Get(1)->key_id()))->c_str(), "mood");
}

TEST(ParserTest, TagWithoutValue) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    hero \"hello\" #important\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* line = story->nodes()->Get(0)->lines()->Get(0)->data_as_Line();
    ASSERT_NE(line->tags(), nullptr);
    ASSERT_EQ(line->tags()->size(), 1u);
    EXPECT_STREQ(story->string_pool()->Get(
        static_cast<uint32_t>(line->tags()->Get(0)->key_id()))->c_str(), "important");
    EXPECT_STREQ(story->string_pool()->Get(
        static_cast<uint32_t>(line->tags()->Get(0)->value_id()))->c_str(), "");
}

TEST(ParserTest, NarrationWithTags) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    \"narration text\" #effect:fade_in\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* line = story->nodes()->Get(0)->lines()->Get(0)->data_as_Line();
    EXPECT_EQ(line->character_id(), -1);
    ASSERT_NE(line->tags(), nullptr);
    ASSERT_EQ(line->tags()->size(), 1u);
    EXPECT_STREQ(story->string_pool()->Get(
        static_cast<uint32_t>(line->tags()->Get(0)->key_id()))->c_str(), "effect");
    EXPECT_STREQ(story->string_pool()->Get(
        static_cast<uint32_t>(line->tags()->Get(0)->value_id()))->c_str(), "fade_in");
}

// --- 새 기능: global_vars ---

TEST(ParserTest, GlobalVarInt) {
    auto buf = GyeolTest::compileScript(
        "$ hp = 100\n"
        "label start:\n"
        "    \"hello\"\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    ASSERT_NE(story->global_vars(), nullptr);
    ASSERT_GE(story->global_vars()->size(), 1u);

    auto* gv = story->global_vars()->Get(0);
    EXPECT_EQ(gv->value_type(), ValueData::IntValue);
    EXPECT_EQ(gv->value_as_IntValue()->val(), 100);
}

TEST(ParserTest, GlobalVarBool) {
    auto buf = GyeolTest::compileScript(
        "$ debug = true\n"
        "label start:\n"
        "    \"hello\"\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    ASSERT_NE(story->global_vars(), nullptr);
    ASSERT_GE(story->global_vars()->size(), 1u);

    auto* gv = story->global_vars()->Get(0);
    EXPECT_EQ(gv->value_type(), ValueData::BoolValue);
    EXPECT_TRUE(gv->value_as_BoolValue()->val());
}

TEST(ParserTest, MultipleGlobalVars) {
    auto buf = GyeolTest::compileScript(
        "$ hp = 100\n"
        "$ name = \"hero\"\n"
        "$ speed = 1.5\n"
        "label start:\n"
        "    \"hello\"\n"
    );
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    ASSERT_NE(story->global_vars(), nullptr);
    EXPECT_EQ(story->global_vars()->size(), 3u);
}

TEST(ParserTest, GlobalVarsInitializedInRunner) {
    auto buf = GyeolTest::compileScript(
        "$ courage = 5\n"
        "label start:\n"
        "    if courage == 5 -> yes else no\n"
        "label yes:\n"
        "    hero \"correct\"\n"
        "label no:\n"
        "    hero \"wrong\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));

    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "correct");
}

// --- 새 기능: 에러 복구 ---

TEST(ParserErrorTest, MultipleErrorsCollected) {
    std::string path = "test_multi_err.gyeol";
    {
        std::ofstream ofs(path);
        ofs << "label start:\n"
            << "    hero missing_quote\n"     // error 1
            << "    hero \"valid line\"\n"     // ok
            << "    hero another_missing\n";  // error 2
    }

    Parser parser;
    EXPECT_FALSE(parser.parse(path));
    EXPECT_GE(parser.getErrors().size(), 2u); // 2개 이상 에러 수집
    std::remove(path.c_str());
}

TEST(ParserErrorTest, ErrorRecoveryStillParses) {
    std::string path = "test_recovery.gyeol";
    {
        std::ofstream ofs(path);
        ofs << "label start:\n"
            << "    hero missing_quote\n"     // error - but continue
            << "    hero \"valid\"\n";        // should be parsed
    }

    Parser parser;
    EXPECT_FALSE(parser.parse(path));
    EXPECT_TRUE(parser.hasErrors());
    std::remove(path.c_str());
}

// --- 파서 에러 케이스 ---

TEST(ParserErrorTest, MissingFile) {
    Parser parser;
    EXPECT_FALSE(parser.parse("nonexistent_file.gyeol"));
}

TEST(ParserErrorTest, DialogueOutsideLabel) {
    std::string path = "test_err1.gyeol";
    { std::ofstream ofs(path); ofs << "hero \"hello\"\n"; }

    Parser parser;
    EXPECT_FALSE(parser.parse(path));
    std::remove(path.c_str());
}

TEST(ParserErrorTest, MissingQuoteAfterCharacter) {
    std::string path = "test_err2.gyeol";
    { std::ofstream ofs(path); ofs << "label start:\n    hero missing_quote\n"; }

    Parser parser;
    EXPECT_FALSE(parser.parse(path));
    std::remove(path.c_str());
}

TEST(ParserErrorTest, ChoiceOutsideMenu) {
    std::string path = "test_err3.gyeol";
    {
        std::ofstream ofs(path);
        ofs << "label start:\n"
            << "    hero \"hi\"\n"
            << "        \"choice\" -> target\n"; // indent 8 but no menu:
    }

    Parser parser;
    EXPECT_FALSE(parser.parse(path));
    std::remove(path.c_str());
}

// --- Jump Target Validation Tests ---

TEST(ParserErrorTest, InvalidJumpTarget) {
    std::string path = "test_jump_err.gyeol";
    {
        std::ofstream ofs(path);
        ofs << "label start:\n"
            << "    jump nonexistent\n";
    }

    Parser parser;
    EXPECT_FALSE(parser.parse(path));
    EXPECT_TRUE(parser.hasErrors());

    // 에러 메시지에 타겟 이름 포함 확인
    bool found = false;
    for (const auto& err : parser.getErrors()) {
        if (err.find("nonexistent") != std::string::npos &&
            err.find("does not exist") != std::string::npos) {
            found = true;
        }
    }
    EXPECT_TRUE(found);
    std::remove(path.c_str());
}

TEST(ParserErrorTest, InvalidChoiceTarget) {
    std::string path = "test_choice_err.gyeol";
    {
        std::ofstream ofs(path);
        ofs << "label start:\n"
            << "    menu:\n"
            << "        \"Go\" -> missing_node\n";
    }

    Parser parser;
    EXPECT_FALSE(parser.parse(path));

    bool found = false;
    for (const auto& err : parser.getErrors()) {
        if (err.find("missing_node") != std::string::npos &&
            err.find("does not exist") != std::string::npos) {
            found = true;
        }
    }
    EXPECT_TRUE(found);
    std::remove(path.c_str());
}

TEST(ParserErrorTest, InvalidConditionTarget) {
    std::string path = "test_cond_err.gyeol";
    {
        std::ofstream ofs(path);
        ofs << "label start:\n"
            << "    if x == 1 -> ghost_node else another_ghost\n";
    }

    Parser parser;
    EXPECT_FALSE(parser.parse(path));

    // 두 타겟 모두 에러
    const auto& errors = parser.getErrors();
    bool foundTrue = false, foundFalse = false;
    for (const auto& err : errors) {
        if (err.find("ghost_node") != std::string::npos) foundTrue = true;
        if (err.find("another_ghost") != std::string::npos) foundFalse = true;
    }
    EXPECT_TRUE(foundTrue);
    EXPECT_TRUE(foundFalse);
    std::remove(path.c_str());
}

TEST(ParserTest, ValidTargetsNoError) {
    // 모든 타겟이 유효한 스크립트
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    jump nodeA\n"
        "\n"
        "label nodeA:\n"
        "    menu:\n"
        "        \"Go B\" -> nodeB\n"
        "\n"
        "label nodeB:\n"
        "    if x == 1 -> start\n"
        "    \"done\"\n"
    );
    EXPECT_FALSE(buf.empty()); // 에러 없이 컴파일 성공
}

TEST(ParserTest, ConditionNoElseNoError) {
    // else 없는 조건문 (false_jump = -1) → 검증 에러 아님
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    if x == 1 -> nodeA\n"
        "    \"fallthrough\"\n"
        "\n"
        "label nodeA:\n"
        "    \"in A\"\n"
    );
    EXPECT_FALSE(buf.empty());
}

// --- 표현식 파싱 테스트 ---

TEST(ParserTest, SetVarSimpleLiteralBackwardCompat) {
    // 단순 리터럴은 기존 value 필드 사용, expr 없음
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = 42\n"
        "    \"done\"\n"
    );
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* sv = story->nodes()->Get(0)->lines()->Get(0)->data_as_SetVar();
    EXPECT_EQ(sv->value_type(), ValueData::IntValue);
    EXPECT_EQ(sv->value_as_IntValue()->val(), 42);
    EXPECT_EQ(sv->expr(), nullptr); // 단순 리터럴은 expr 없음
}

TEST(ParserTest, SetVarExpressionAddition) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = 1 + 2\n"
        "    \"done\"\n"
    );
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* sv = story->nodes()->Get(0)->lines()->Get(0)->data_as_SetVar();
    ASSERT_NE(sv->expr(), nullptr);
    // RPN: [PushLiteral(1), PushLiteral(2), Add]
    ASSERT_EQ(sv->expr()->tokens()->size(), 3u);
    EXPECT_EQ(sv->expr()->tokens()->Get(0)->op(), ExprOp::PushLiteral);
    EXPECT_EQ(sv->expr()->tokens()->Get(1)->op(), ExprOp::PushLiteral);
    EXPECT_EQ(sv->expr()->tokens()->Get(2)->op(), ExprOp::Add);
}

TEST(ParserTest, SetVarExpressionWithVariable) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = y + 1\n"
        "    \"done\"\n"
    );
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* sv = story->nodes()->Get(0)->lines()->Get(0)->data_as_SetVar();
    ASSERT_NE(sv->expr(), nullptr);
    // RPN: [PushVar(y), PushLiteral(1), Add]
    ASSERT_EQ(sv->expr()->tokens()->size(), 3u);
    EXPECT_EQ(sv->expr()->tokens()->Get(0)->op(), ExprOp::PushVar);
    EXPECT_GE(sv->expr()->tokens()->Get(0)->var_name_id(), 0);
    EXPECT_EQ(sv->expr()->tokens()->Get(1)->op(), ExprOp::PushLiteral);
    EXPECT_EQ(sv->expr()->tokens()->Get(2)->op(), ExprOp::Add);
}

TEST(ParserTest, SetVarExpressionPrecedence) {
    // $ x = 1 + 2 * 3 → RPN: [1, 2, 3, Mul, Add]
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = 1 + 2 * 3\n"
        "    \"done\"\n"
    );
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* sv = story->nodes()->Get(0)->lines()->Get(0)->data_as_SetVar();
    ASSERT_NE(sv->expr(), nullptr);
    // RPN: [1, 2, 3, Mul, Add]
    ASSERT_EQ(sv->expr()->tokens()->size(), 5u);
    EXPECT_EQ(sv->expr()->tokens()->Get(0)->op(), ExprOp::PushLiteral);
    EXPECT_EQ(sv->expr()->tokens()->Get(1)->op(), ExprOp::PushLiteral);
    EXPECT_EQ(sv->expr()->tokens()->Get(2)->op(), ExprOp::PushLiteral);
    EXPECT_EQ(sv->expr()->tokens()->Get(3)->op(), ExprOp::Mul);
    EXPECT_EQ(sv->expr()->tokens()->Get(4)->op(), ExprOp::Add);
}

// --- 조건 표현식 파싱 테스트 ---

TEST(ParserTest, ConditionExprLHS) {
    // if hp - 10 > 0 → lhs_expr 존재
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    if hp - 10 > 0 -> target\n"
        "    \"fallthrough\"\n"
        "\n"
        "label target:\n"
        "    \"hit\"\n"
    );
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* cond = story->nodes()->Get(0)->lines()->Get(0)->data_as_Condition();
    ASSERT_NE(cond, nullptr);
    EXPECT_NE(cond->lhs_expr(), nullptr); // LHS는 표현식
    EXPECT_EQ(cond->op(), Operator::Greater);
    // RHS는 리터럴 0
    EXPECT_EQ(cond->compare_value_type(), ValueData::IntValue);
    EXPECT_EQ(cond->compare_value_as_IntValue()->val(), 0);
}

TEST(ParserTest, ConditionExprBothSides) {
    // if x + y == z → 양쪽 표현식
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    if x + y == z -> target\n"
        "    \"fallthrough\"\n"
        "\n"
        "label target:\n"
        "    \"hit\"\n"
    );
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* cond = story->nodes()->Get(0)->lines()->Get(0)->data_as_Condition();
    ASSERT_NE(cond, nullptr);
    EXPECT_NE(cond->lhs_expr(), nullptr); // LHS 표현식 (x + y)
    EXPECT_EQ(cond->op(), Operator::Equal);
    EXPECT_NE(cond->rhs_expr(), nullptr); // RHS 표현식 (z 변수)
}

TEST(ParserTest, ConditionSimpleBackwardCompat) {
    // if x == 1 → 기존 방식 (var_name_id + compare_value)
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    if x == 1 -> target\n"
        "    \"fallthrough\"\n"
        "\n"
        "label target:\n"
        "    \"hit\"\n"
    );
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* cond = story->nodes()->Get(0)->lines()->Get(0)->data_as_Condition();
    ASSERT_NE(cond, nullptr);
    EXPECT_EQ(cond->cond_expr(), nullptr); // 논리 연산자 없음 → cond_expr 미사용
    EXPECT_EQ(cond->lhs_expr(), nullptr); // 단순 변수 → var_name_id
    EXPECT_GE(cond->var_name_id(), 0);
    EXPECT_EQ(cond->op(), Operator::Equal);
    EXPECT_EQ(cond->compare_value_type(), ValueData::IntValue);
    EXPECT_EQ(cond->compare_value_as_IntValue()->val(), 1);
}

// --- 논리 연산자 파싱 ---

TEST(ParserTest, ConditionAndOp) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    if hp > 0 and has_key == true -> target\n"
        "    \"fallthrough\"\n"
        "\n"
        "label target:\n"
        "    \"hit\"\n"
    );
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* cond = story->nodes()->Get(0)->lines()->Get(0)->data_as_Condition();
    ASSERT_NE(cond, nullptr);
    ASSERT_NE(cond->cond_expr(), nullptr); // 논리 연산자 사용 → cond_expr
    // And 토큰이 있는지 확인
    bool foundAnd = false;
    for (flatbuffers::uoffset_t i = 0; i < cond->cond_expr()->tokens()->size(); ++i) {
        if (cond->cond_expr()->tokens()->Get(i)->op() == ExprOp::And) foundAnd = true;
    }
    EXPECT_TRUE(foundAnd);
}

TEST(ParserTest, ConditionOrOp) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    if x > 10 or y > 10 -> target\n"
        "    \"fallthrough\"\n"
        "\n"
        "label target:\n"
        "    \"hit\"\n"
    );
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* cond = story->nodes()->Get(0)->lines()->Get(0)->data_as_Condition();
    ASSERT_NE(cond, nullptr);
    ASSERT_NE(cond->cond_expr(), nullptr);
    bool foundOr = false;
    for (flatbuffers::uoffset_t i = 0; i < cond->cond_expr()->tokens()->size(); ++i) {
        if (cond->cond_expr()->tokens()->Get(i)->op() == ExprOp::Or) foundOr = true;
    }
    EXPECT_TRUE(foundOr);
}

TEST(ParserTest, ConditionNotOp) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    if not game_over == true -> target\n"
        "    \"fallthrough\"\n"
        "\n"
        "label target:\n"
        "    \"hit\"\n"
    );
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* cond = story->nodes()->Get(0)->lines()->Get(0)->data_as_Condition();
    ASSERT_NE(cond, nullptr);
    ASSERT_NE(cond->cond_expr(), nullptr);
    bool foundNot = false;
    for (flatbuffers::uoffset_t i = 0; i < cond->cond_expr()->tokens()->size(); ++i) {
        if (cond->cond_expr()->tokens()->Get(i)->op() == ExprOp::Not) foundNot = true;
    }
    EXPECT_TRUE(foundNot);
}

TEST(ParserTest, ConditionLogicalPrecedence) {
    // and가 or보다 우선순위 높음: a == 1 or b == 2 and c == 3
    // = a == 1 or (b == 2 and c == 3)
    // RPN: [a, 1, CmpEq, b, 2, CmpEq, c, 3, CmpEq, And, Or]
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    if a == 1 or b == 2 and c == 3 -> target\n"
        "    \"fallthrough\"\n"
        "\n"
        "label target:\n"
        "    \"hit\"\n"
    );
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* cond = story->nodes()->Get(0)->lines()->Get(0)->data_as_Condition();
    ASSERT_NE(cond, nullptr);
    ASSERT_NE(cond->cond_expr(), nullptr);
    auto* toks = cond->cond_expr()->tokens();
    ASSERT_GE(toks->size(), 2u);
    // 마지막 토큰이 Or, 그 전이 And (and가 먼저 실행)
    EXPECT_EQ(toks->Get(toks->size() - 1)->op(), ExprOp::Or);
    EXPECT_EQ(toks->Get(toks->size() - 2)->op(), ExprOp::And);
}

TEST(ParserTest, ConditionSimpleNoRegression) {
    // 논리 연산자 없는 단순 조건은 여전히 기존 방식 (cond_expr=null)
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    if hp > 10 -> target\n"
        "    \"fallthrough\"\n"
        "\n"
        "label target:\n"
        "    \"hit\"\n"
    );
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* cond = story->nodes()->Get(0)->lines()->Get(0)->data_as_Condition();
    ASSERT_NE(cond, nullptr);
    EXPECT_EQ(cond->cond_expr(), nullptr); // 논리 없음 → 기존 경로
    EXPECT_EQ(cond->op(), Operator::Greater);
}

// --- Elif/Else 체인 ---

TEST(ParserTest, ElifBasicChain) {
    // if/elif/else → Condition + Condition + Jump
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    if x == 1 -> a\n"
        "    elif x == 2 -> b\n"
        "    else -> c\n"
        "\n"
        "label a:\n"
        "    \"a\"\n"
        "label b:\n"
        "    \"b\"\n"
        "label c:\n"
        "    \"c\"\n"
    );
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* lines = story->nodes()->Get(0)->lines();
    ASSERT_EQ(lines->size(), 3u);

    // 첫 번째: Condition (if)
    EXPECT_EQ(lines->Get(0)->data_type(), OpData::Condition);
    auto* cond0 = lines->Get(0)->data_as_Condition();
    EXPECT_GE(cond0->true_jump_node_id(), 0);
    EXPECT_EQ(cond0->false_jump_node_id(), -1); // fall-through

    // 두 번째: Condition (elif)
    EXPECT_EQ(lines->Get(1)->data_type(), OpData::Condition);
    auto* cond1 = lines->Get(1)->data_as_Condition();
    EXPECT_GE(cond1->true_jump_node_id(), 0);
    EXPECT_EQ(cond1->false_jump_node_id(), -1); // fall-through

    // 세 번째: Jump (else)
    EXPECT_EQ(lines->Get(2)->data_type(), OpData::Jump);
}

TEST(ParserTest, ElifWithoutElse) {
    // if/elif/elif (else 없음) → 3개 Condition
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    if x == 1 -> a\n"
        "    elif x == 2 -> b\n"
        "    elif x == 3 -> c\n"
        "    \"default\"\n"
        "\n"
        "label a:\n"
        "    \"a\"\n"
        "label b:\n"
        "    \"b\"\n"
        "label c:\n"
        "    \"c\"\n"
    );
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* lines = story->nodes()->Get(0)->lines();
    ASSERT_EQ(lines->size(), 4u); // 3 conditions + 1 dialogue

    for (flatbuffers::uoffset_t i = 0; i < 3; ++i) {
        EXPECT_EQ(lines->Get(i)->data_type(), OpData::Condition);
        EXPECT_EQ(lines->Get(i)->data_as_Condition()->false_jump_node_id(), -1);
    }
    // 마지막은 대사
    EXPECT_EQ(lines->Get(3)->data_type(), OpData::Line);
}

TEST(ParserTest, ElifWithLogicalOps) {
    // elif에서 논리 연산자 사용
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    if x == 1 -> a\n"
        "    elif hp > 0 and key == true -> b\n"
        "    \"default\"\n"
        "\n"
        "label a:\n"
        "    \"a\"\n"
        "label b:\n"
        "    \"b\"\n"
    );
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* cond = story->nodes()->Get(0)->lines()->Get(1)->data_as_Condition();
    ASSERT_NE(cond, nullptr);
    ASSERT_NE(cond->cond_expr(), nullptr); // 논리 연산자 → cond_expr
    bool foundAnd = false;
    for (flatbuffers::uoffset_t i = 0; i < cond->cond_expr()->tokens()->size(); ++i) {
        if (cond->cond_expr()->tokens()->Get(i)->op() == ExprOp::And) foundAnd = true;
    }
    EXPECT_TRUE(foundAnd);
}

TEST(ParserErrorTest, ElifAfterInlineElse) {
    // if에 inline else가 있는데 elif 사용 → 에러
    std::string path = "test_elif_inline_err.gyeol";
    {
        std::ofstream ofs(path);
        ofs << "label start:\n"
            << "    if x == 1 -> a else b\n"
            << "    elif x == 2 -> c\n"
            << "label a:\n"
            << "    \"a\"\n"
            << "label b:\n"
            << "    \"b\"\n"
            << "label c:\n"
            << "    \"c\"\n";
    }
    Parser parser;
    EXPECT_FALSE(parser.parse(path));
    bool foundError = false;
    for (const auto& err : parser.getErrors()) {
        if (err.find("elif") != std::string::npos && err.find("else") != std::string::npos) {
            foundError = true;
        }
    }
    EXPECT_TRUE(foundError);
    std::remove(path.c_str());
}

TEST(ParserErrorTest, ElifWithoutIf) {
    // elif이 if 없이 등장 → 에러
    std::string path = "test_elif_noif_err.gyeol";
    {
        std::ofstream ofs(path);
        ofs << "label start:\n"
            << "    \"hello\"\n"
            << "    elif x == 1 -> a\n"
            << "label a:\n"
            << "    \"a\"\n";
    }
    Parser parser;
    EXPECT_FALSE(parser.parse(path));
    bool foundError = false;
    for (const auto& err : parser.getErrors()) {
        if (err.find("elif") != std::string::npos && err.find("must follow") != std::string::npos) {
            foundError = true;
        }
    }
    EXPECT_TRUE(foundError);
    std::remove(path.c_str());
}

// --- Random 블록 파서 테스트 ---

TEST(ParserTest, RandomBlockBasic) {
    // 가중치 50/30/20 → Random 명령어 1개, 3개 분기
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    random:\n"
        "        50 -> path_a\n"
        "        30 -> path_b\n"
        "        20 -> path_c\n"
        "label path_a:\n"
        "    \"a\"\n"
        "label path_b:\n"
        "    \"b\"\n"
        "label path_c:\n"
        "    \"c\"\n"
    );
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* startNode = story->nodes()->Get(0);
    ASSERT_EQ(startNode->lines()->size(), 1u); // 1 Random instruction

    auto* instr = startNode->lines()->Get(0);
    EXPECT_EQ(instr->data_type(), OpData::Random);
    auto* random = instr->data_as_Random();
    ASSERT_NE(random, nullptr);
    ASSERT_EQ(random->branches()->size(), 3u);
    EXPECT_EQ(random->branches()->Get(0)->weight(), 50);
    EXPECT_EQ(random->branches()->Get(1)->weight(), 30);
    EXPECT_EQ(random->branches()->Get(2)->weight(), 20);
}

TEST(ParserTest, RandomBlockEqualWeight) {
    // -> 만 사용 → 모든 weight=1
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    random:\n"
        "        -> path_a\n"
        "        -> path_b\n"
        "        -> path_c\n"
        "label path_a:\n"
        "    \"a\"\n"
        "label path_b:\n"
        "    \"b\"\n"
        "label path_c:\n"
        "    \"c\"\n"
    );
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* startNode = story->nodes()->Get(0);
    ASSERT_EQ(startNode->lines()->size(), 1u);

    auto* random = startNode->lines()->Get(0)->data_as_Random();
    ASSERT_NE(random, nullptr);
    ASSERT_EQ(random->branches()->size(), 3u);
    EXPECT_EQ(random->branches()->Get(0)->weight(), 1);
    EXPECT_EQ(random->branches()->Get(1)->weight(), 1);
    EXPECT_EQ(random->branches()->Get(2)->weight(), 1);
}

TEST(ParserTest, RandomBlockMixedWeight) {
    // 가중치 혼합: 5, 기본1, 3
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    random:\n"
        "        5 -> path_a\n"
        "        -> path_b\n"
        "        3 -> path_c\n"
        "label path_a:\n"
        "    \"a\"\n"
        "label path_b:\n"
        "    \"b\"\n"
        "label path_c:\n"
        "    \"c\"\n"
    );
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* startNode = story->nodes()->Get(0);
    auto* random = startNode->lines()->Get(0)->data_as_Random();
    ASSERT_NE(random, nullptr);
    ASSERT_EQ(random->branches()->size(), 3u);
    EXPECT_EQ(random->branches()->Get(0)->weight(), 5);
    EXPECT_EQ(random->branches()->Get(1)->weight(), 1);
    EXPECT_EQ(random->branches()->Get(2)->weight(), 3);
}

TEST(ParserErrorTest, RandomInvalidTargetError) {
    // 존재하지 않는 타겟 → 에러
    std::string path = "test_random_invalid_target.gyeol";
    {
        std::ofstream ofs(path);
        ofs << "label start:\n"
            << "    random:\n"
            << "        50 -> nonexistent\n"
            << "        50 -> also_missing\n";
    }
    Parser parser;
    EXPECT_FALSE(parser.parse(path));
    // 2개의 잘못된 타겟 에러
    int errorCount = 0;
    for (const auto& err : parser.getErrors()) {
        if (err.find("does not exist") != std::string::npos) {
            errorCount++;
        }
    }
    EXPECT_GE(errorCount, 2);
    std::remove(path.c_str());
}

// --- Line ID 테스트 ---

TEST(ParserTest, LineIdGenerated) {
    // 대사와 선택지에 line_id 생성, 구조적 문자열(노드명 등)은 빈 line_id
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    hero \"Hello world!\"\n"
        "    menu:\n"
        "        \"Go left\" -> start\n"
    );
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    ASSERT_NE(story->line_ids(), nullptr);
    EXPECT_EQ(story->line_ids()->size(), story->string_pool()->size());

    // line_id 형식 확인: 번역 대상(text)은 비어있지 않아야 함
    // "Hello world!"의 pool index 찾기
    int helloIdx = -1;
    int choiceIdx = -1;
    for (flatbuffers::uoffset_t i = 0; i < story->string_pool()->size(); ++i) {
        std::string s = story->string_pool()->Get(i)->c_str();
        if (s == "Hello world!") helloIdx = static_cast<int>(i);
        if (s == "Go left") choiceIdx = static_cast<int>(i);
    }
    ASSERT_GE(helloIdx, 0);
    ASSERT_GE(choiceIdx, 0);

    // 대사 line_id: "start:N:hash" 형식
    std::string helloLid = story->line_ids()->Get(static_cast<flatbuffers::uoffset_t>(helloIdx))->c_str();
    EXPECT_FALSE(helloLid.empty());
    EXPECT_EQ(helloLid.substr(0, 6), "start:");

    // 선택지 line_id
    std::string choiceLid = story->line_ids()->Get(static_cast<flatbuffers::uoffset_t>(choiceIdx))->c_str();
    EXPECT_FALSE(choiceLid.empty());
    EXPECT_EQ(choiceLid.substr(0, 6), "start:");

    // 구조적 문자열(노드명 "start", 캐릭터명 "hero")은 빈 line_id
    int startIdx = -1, heroIdx = -1;
    for (flatbuffers::uoffset_t i = 0; i < story->string_pool()->size(); ++i) {
        std::string s = story->string_pool()->Get(i)->c_str();
        if (s == "start") startIdx = static_cast<int>(i);
        if (s == "hero") heroIdx = static_cast<int>(i);
    }
    if (startIdx >= 0) {
        EXPECT_EQ(std::string(story->line_ids()->Get(static_cast<flatbuffers::uoffset_t>(startIdx))->c_str()), "");
    }
    if (heroIdx >= 0) {
        EXPECT_EQ(std::string(story->line_ids()->Get(static_cast<flatbuffers::uoffset_t>(heroIdx))->c_str()), "");
    }
}

TEST(ParserTest, LineIdStability) {
    // 같은 텍스트 → 같은 hash, 다른 텍스트 → 다른 hash
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    \"Hello\"\n"
        "    \"World\"\n"
    );
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());

    // "Hello"와 "World"의 line_id hash 부분이 다른지 확인
    int helloIdx = -1, worldIdx = -1;
    for (flatbuffers::uoffset_t i = 0; i < story->string_pool()->size(); ++i) {
        std::string s = story->string_pool()->Get(i)->c_str();
        if (s == "Hello") helloIdx = static_cast<int>(i);
        if (s == "World") worldIdx = static_cast<int>(i);
    }
    ASSERT_GE(helloIdx, 0);
    ASSERT_GE(worldIdx, 0);

    std::string helloLid = story->line_ids()->Get(static_cast<flatbuffers::uoffset_t>(helloIdx))->c_str();
    std::string worldLid = story->line_ids()->Get(static_cast<flatbuffers::uoffset_t>(worldIdx))->c_str();
    EXPECT_NE(helloLid, worldLid);

    // hash 부분 (마지막 4자리) 추출
    std::string helloHash = helloLid.substr(helloLid.rfind(':') + 1);
    std::string worldHash = worldLid.substr(worldLid.rfind(':') + 1);
    EXPECT_EQ(helloHash.size(), 4u);
    EXPECT_EQ(worldHash.size(), 4u);
    EXPECT_NE(helloHash, worldHash);
}

TEST(ParserTest, LineIdInExportedGyb) {
    // .gyb에 line_ids 배열이 포함되어 있는지 확인
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    \"Test line\"\n"
    );
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    ASSERT_NE(story, nullptr);
    ASSERT_NE(story->line_ids(), nullptr);
    EXPECT_GT(story->line_ids()->size(), 0u);

    // string_pool과 line_ids 크기 동일
    EXPECT_EQ(story->string_pool()->size(), story->line_ids()->size());
}

TEST(ParserTest, ExportStringsCSV) {
    // exportStrings() → CSV 파일 생성 및 내용 검증
    std::string inPath = "test_export_strings.gyeol";
    std::string csvPath = "test_export_strings.csv";
    {
        std::ofstream ofs(inPath);
        ofs << "label start:\n"
            << "    hero \"Hello world!\"\n"
            << "    menu:\n"
            << "        \"Go left\" -> start\n";
    }

    Parser parser;
    ASSERT_TRUE(parser.parse(inPath));
    ASSERT_TRUE(parser.exportStrings(csvPath));

    // CSV 읽기
    std::ifstream ifs(csvPath);
    ASSERT_TRUE(ifs.is_open());

    std::string header;
    std::getline(ifs, header);
    EXPECT_EQ(header, "line_id,type,node,character,text");

    // 최소 2행 (대사 + 선택지)
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(ifs, line)) {
        if (!line.empty()) lines.push_back(line);
    }
    EXPECT_GE(lines.size(), 2u);

    // LINE 타입 행이 있는지
    bool foundLine = false;
    bool foundChoice = false;
    for (const auto& l : lines) {
        if (l.find(",LINE,") != std::string::npos && l.find("Hello world!") != std::string::npos) {
            foundLine = true;
        }
        if (l.find(",CHOICE,") != std::string::npos && l.find("Go left") != std::string::npos) {
            foundChoice = true;
        }
    }
    EXPECT_TRUE(foundLine);
    EXPECT_TRUE(foundChoice);

    std::remove(inPath.c_str());
    std::remove(csvPath.c_str());
}

// =================================================================
// --- Import 테스트 ---
// =================================================================

TEST(ParserTest, ImportBasicMerge) {
    // common.gyeol에 label 하나, main에서 import + 자체 label
    std::vector<std::pair<std::string, std::string>> files = {
        {"test_import_common.gyeol",
         "label common_node:\n"
         "    narrator \"Common text\"\n"},
        {"test_import_main.gyeol",
         "import \"test_import_common.gyeol\"\n"
         "\n"
         "label start:\n"
         "    hero \"Main text\"\n"
         "    jump common_node\n"}
    };

    auto buf = GyeolTest::compileMultiFileScript(files, "test_import_main.gyeol");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    ASSERT_NE(story, nullptr);
    EXPECT_EQ(story->nodes()->size(), 2u);

    // start_node는 메인 파일의 첫 label
    EXPECT_STREQ(story->start_node_name()->c_str(), "start");
}

TEST(ParserTest, ImportMultipleFiles) {
    std::vector<std::pair<std::string, std::string>> files = {
        {"test_import_a.gyeol",
         "label node_a:\n"
         "    narrator \"A\"\n"},
        {"test_import_b.gyeol",
         "label node_b:\n"
         "    narrator \"B\"\n"},
        {"test_import_multi_main.gyeol",
         "import \"test_import_a.gyeol\"\n"
         "import \"test_import_b.gyeol\"\n"
         "\n"
         "label start:\n"
         "    hero \"Main\"\n"}
    };

    auto buf = GyeolTest::compileMultiFileScript(files, "test_import_multi_main.gyeol");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    ASSERT_NE(story, nullptr);
    EXPECT_EQ(story->nodes()->size(), 3u);
}

TEST(ParserTest, ImportGlobalVars) {
    std::vector<std::pair<std::string, std::string>> files = {
        {"test_import_gv_common.gyeol",
         "$ shared_var = 42\n"
         "\n"
         "label common:\n"
         "    narrator \"common\"\n"},
        {"test_import_gv_main.gyeol",
         "import \"test_import_gv_common.gyeol\"\n"
         "\n"
         "label start:\n"
         "    narrator \"main\"\n"}
    };

    auto buf = GyeolTest::compileMultiFileScript(files, "test_import_gv_main.gyeol");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    ASSERT_NE(story, nullptr);
    ASSERT_GE(story->global_vars()->size(), 1u);

    auto* gv = story->global_vars()->Get(0);
    auto varNameId = gv->var_name_id();
    EXPECT_STREQ(story->string_pool()->Get(varNameId)->c_str(), "shared_var");
    EXPECT_EQ(gv->value_type(), ValueData::IntValue);
    EXPECT_EQ(gv->value_as_IntValue()->val(), 42);
}

TEST(ParserTest, ImportStringPoolShared) {
    // 양쪽 파일에 같은 문자열 → pool에서 중복 제거
    std::vector<std::pair<std::string, std::string>> files = {
        {"test_import_sp_common.gyeol",
         "label common:\n"
         "    hero \"shared text\"\n"},
        {"test_import_sp_main.gyeol",
         "import \"test_import_sp_common.gyeol\"\n"
         "\n"
         "label start:\n"
         "    hero \"shared text\"\n"}
    };

    auto buf = GyeolTest::compileMultiFileScript(files, "test_import_sp_main.gyeol");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    ASSERT_NE(story, nullptr);

    // "shared text"와 "hero"는 각각 1번만 pool에 존재해야 함
    int count = 0;
    for (unsigned i = 0; i < story->string_pool()->size(); i++) {
        if (std::string(story->string_pool()->Get(i)->c_str()) == "shared text") {
            count++;
        }
    }
    EXPECT_EQ(count, 1);
}

TEST(ParserTest, ImportStartNodeFromMainFile) {
    // import가 먼저 와도 main 파일의 첫 label이 start_node
    std::vector<std::pair<std::string, std::string>> files = {
        {"test_import_sn_common.gyeol",
         "label imported_first:\n"
         "    narrator \"I was imported\"\n"},
        {"test_import_sn_main.gyeol",
         "import \"test_import_sn_common.gyeol\"\n"
         "\n"
         "label main_start:\n"
         "    narrator \"I am main\"\n"}
    };

    auto buf = GyeolTest::compileMultiFileScript(files, "test_import_sn_main.gyeol");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    ASSERT_NE(story, nullptr);
    EXPECT_STREQ(story->start_node_name()->c_str(), "main_start");
}

TEST(ParserTest, ImportPreservesOrder) {
    // import된 노드가 먼저, 그 다음 main 노드
    std::vector<std::pair<std::string, std::string>> files = {
        {"test_import_ord_common.gyeol",
         "label alpha:\n"
         "    narrator \"A\"\n"},
        {"test_import_ord_main.gyeol",
         "import \"test_import_ord_common.gyeol\"\n"
         "\n"
         "label beta:\n"
         "    narrator \"B\"\n"}
    };

    auto buf = GyeolTest::compileMultiFileScript(files, "test_import_ord_main.gyeol");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    ASSERT_NE(story, nullptr);
    ASSERT_EQ(story->nodes()->size(), 2u);
    EXPECT_STREQ(story->nodes()->Get(0)->name()->c_str(), "alpha");
    EXPECT_STREQ(story->nodes()->Get(1)->name()->c_str(), "beta");
}

TEST(ParserTest, ImportNestedFiles) {
    // A imports B, B imports C → 3단 중첩
    std::vector<std::pair<std::string, std::string>> files = {
        {"test_import_nest_c.gyeol",
         "label node_c:\n"
         "    narrator \"C\"\n"},
        {"test_import_nest_b.gyeol",
         "import \"test_import_nest_c.gyeol\"\n"
         "\n"
         "label node_b:\n"
         "    narrator \"B\"\n"},
        {"test_import_nest_a.gyeol",
         "import \"test_import_nest_b.gyeol\"\n"
         "\n"
         "label node_a:\n"
         "    narrator \"A\"\n"}
    };

    auto buf = GyeolTest::compileMultiFileScript(files, "test_import_nest_a.gyeol");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    ASSERT_NE(story, nullptr);
    EXPECT_EQ(story->nodes()->size(), 3u);
    // 순서: C → B → A (깊이 우선)
    EXPECT_STREQ(story->nodes()->Get(0)->name()->c_str(), "node_c");
    EXPECT_STREQ(story->nodes()->Get(1)->name()->c_str(), "node_b");
    EXPECT_STREQ(story->nodes()->Get(2)->name()->c_str(), "node_a");
}

// --- Import 에러 테스트 ---

TEST(ParserErrorTest, ImportCircularDetection) {
    // A imports B, B imports A → 순환 에러
    {
        std::ofstream ofs("test_import_circ_a.gyeol");
        ofs << "import \"test_import_circ_b.gyeol\"\n"
               "\n"
               "label node_a:\n"
               "    narrator \"A\"\n";
    }
    {
        std::ofstream ofs("test_import_circ_b.gyeol");
        ofs << "import \"test_import_circ_a.gyeol\"\n"
               "\n"
               "label node_b:\n"
               "    narrator \"B\"\n";
    }

    Parser parser;
    EXPECT_FALSE(parser.parse("test_import_circ_a.gyeol"));

    bool foundCircular = false;
    for (const auto& err : parser.getErrors()) {
        if (err.find("circular import") != std::string::npos) {
            foundCircular = true;
            break;
        }
    }
    EXPECT_TRUE(foundCircular);

    std::remove("test_import_circ_a.gyeol");
    std::remove("test_import_circ_b.gyeol");
}

TEST(ParserErrorTest, ImportSelfCircular) {
    {
        std::ofstream ofs("test_import_self.gyeol");
        ofs << "import \"test_import_self.gyeol\"\n"
               "\n"
               "label start:\n"
               "    narrator \"hello\"\n";
    }

    Parser parser;
    EXPECT_FALSE(parser.parse("test_import_self.gyeol"));

    bool foundCircular = false;
    for (const auto& err : parser.getErrors()) {
        if (err.find("circular import") != std::string::npos) {
            foundCircular = true;
            break;
        }
    }
    EXPECT_TRUE(foundCircular);

    std::remove("test_import_self.gyeol");
}

TEST(ParserErrorTest, ImportFileNotFound) {
    {
        std::ofstream ofs("test_import_notfound.gyeol");
        ofs << "import \"nonexistent_file.gyeol\"\n"
               "\n"
               "label start:\n"
               "    narrator \"hello\"\n";
    }

    Parser parser;
    EXPECT_FALSE(parser.parse("test_import_notfound.gyeol"));

    bool foundNotFound = false;
    for (const auto& err : parser.getErrors()) {
        if (err.find("imported file not found") != std::string::npos) {
            foundNotFound = true;
            break;
        }
    }
    EXPECT_TRUE(foundNotFound);

    std::remove("test_import_notfound.gyeol");
}

TEST(ParserErrorTest, ImportDuplicateLabel) {
    std::vector<std::pair<std::string, std::string>> files = {
        {"test_import_dup_common.gyeol",
         "label shared_name:\n"
         "    narrator \"common\"\n"},
        {"test_import_dup_main.gyeol",
         "import \"test_import_dup_common.gyeol\"\n"
         "\n"
         "label shared_name:\n"
         "    narrator \"main\"\n"}
    };

    // 파일 작성
    for (const auto& file : files) {
        std::ofstream ofs(file.first);
        ofs << file.second;
    }

    Parser parser;
    EXPECT_FALSE(parser.parse("test_import_dup_main.gyeol"));

    bool foundDup = false;
    for (const auto& err : parser.getErrors()) {
        if (err.find("duplicate label") != std::string::npos) {
            foundDup = true;
            break;
        }
    }
    EXPECT_TRUE(foundDup);

    for (const auto& file : files) {
        std::remove(file.first.c_str());
    }
}

TEST(ParserErrorTest, ImportWithoutQuotedPath) {
    {
        std::ofstream ofs("test_import_noquote.gyeol");
        ofs << "import nopath\n"
               "\n"
               "label start:\n"
               "    narrator \"hello\"\n";
    }

    Parser parser;
    EXPECT_FALSE(parser.parse("test_import_noquote.gyeol"));

    bool foundErr = false;
    for (const auto& err : parser.getErrors()) {
        if (err.find("import requires a quoted file path") != std::string::npos) {
            foundErr = true;
            break;
        }
    }
    EXPECT_TRUE(foundErr);

    std::remove("test_import_noquote.gyeol");
}

TEST(ParserErrorTest, ImportEmptyPath) {
    {
        std::ofstream ofs("test_import_empty.gyeol");
        ofs << "import \"\"\n"
               "\n"
               "label start:\n"
               "    narrator \"hello\"\n";
    }

    Parser parser;
    EXPECT_FALSE(parser.parse("test_import_empty.gyeol"));

    bool foundErr = false;
    for (const auto& err : parser.getErrors()) {
        if (err.find("import requires a non-empty file path") != std::string::npos) {
            foundErr = true;
            break;
        }
    }
    EXPECT_TRUE(foundErr);

    std::remove("test_import_empty.gyeol");
}

// --- Return / CallWithReturn 파서 테스트 ---

TEST(ParserTest, ReturnLiteral) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    return 42\n");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* node = story->nodes()->Get(0);
    ASSERT_EQ(node->lines()->size(), 1u);

    auto* instr = node->lines()->Get(0);
    EXPECT_EQ(instr->data_type(), OpData::Return);
    auto* ret = instr->data_as_Return();
    EXPECT_EQ(ret->value_type(), ValueData::IntValue);
    EXPECT_EQ(ret->value_as_IntValue()->val(), 42);
    EXPECT_EQ(ret->expr(), nullptr);
}

TEST(ParserTest, ReturnVariable) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ result = 10\n"
        "    return result\n");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* node = story->nodes()->Get(0);
    ASSERT_EQ(node->lines()->size(), 2u);

    auto* instr = node->lines()->Get(1);
    EXPECT_EQ(instr->data_type(), OpData::Return);
    auto* ret = instr->data_as_Return();
    // 변수 참조는 Expression으로 파싱됨 (PushVar)
    EXPECT_NE(ret->expr(), nullptr);
    EXPECT_GE(ret->expr()->tokens()->size(), 1u);
    EXPECT_EQ(ret->expr()->tokens()->Get(0)->op(), ExprOp::PushVar);
}

TEST(ParserTest, ReturnExpression) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    return 2 + 3\n");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* node = story->nodes()->Get(0);
    auto* instr = node->lines()->Get(0);
    EXPECT_EQ(instr->data_type(), OpData::Return);
    auto* ret = instr->data_as_Return();
    EXPECT_NE(ret->expr(), nullptr);
    // RPN: 2 3 Add
    EXPECT_EQ(ret->expr()->tokens()->size(), 3u);
}

TEST(ParserTest, ReturnStringLiteral) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    return \"hello\"\n");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* node = story->nodes()->Get(0);
    auto* instr = node->lines()->Get(0);
    EXPECT_EQ(instr->data_type(), OpData::Return);
    auto* ret = instr->data_as_Return();
    EXPECT_EQ(ret->value_type(), ValueData::StringRef);
}

TEST(ParserTest, ReturnBoolLiteral) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    return true\n");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* node = story->nodes()->Get(0);
    auto* instr = node->lines()->Get(0);
    EXPECT_EQ(instr->data_type(), OpData::Return);
    auto* ret = instr->data_as_Return();
    EXPECT_EQ(ret->value_type(), ValueData::BoolValue);
    EXPECT_TRUE(ret->value_as_BoolValue()->val());
}

TEST(ParserTest, BareReturn) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    return\n");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* node = story->nodes()->Get(0);
    auto* instr = node->lines()->Get(0);
    EXPECT_EQ(instr->data_type(), OpData::Return);
    auto* ret = instr->data_as_Return();
    EXPECT_EQ(ret->expr(), nullptr);
    EXPECT_EQ(ret->value_type(), ValueData::NONE);
}

TEST(ParserTest, CallWithReturnVar) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ result = call calc\n"
        "\n"
        "label calc:\n"
        "    return 42\n");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* node = story->nodes()->Get(0);
    ASSERT_EQ(node->lines()->size(), 1u);

    auto* instr = node->lines()->Get(0);
    EXPECT_EQ(instr->data_type(), OpData::CallWithReturn);
    auto* cwr = instr->data_as_CallWithReturn();

    // target node name
    auto targetName = story->string_pool()->Get(cwr->target_node_name_id())->c_str();
    EXPECT_STREQ(targetName, "calc");

    // return var name
    auto retVarName = story->string_pool()->Get(cwr->return_var_name_id())->c_str();
    EXPECT_STREQ(retVarName, "result");
}

TEST(ParserTest, CallWithReturnValidation) {
    // target이 존재하면 에러 없이 컴파일됨
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = call helper\n"
        "\n"
        "label helper:\n"
        "    return 1\n");
    EXPECT_FALSE(buf.empty());
}

// --- Return / CallWithReturn 에러 테스트 ---

TEST(ParserErrorTest, ReturnOutsideLabel) {
    // indent 0에서 return은 에러 (label/import/global 만 가능)
    auto buf = GyeolTest::compileScript(
        "return 42\n"
        "\n"
        "label start:\n"
        "    narrator \"hello\"\n");
    EXPECT_TRUE(buf.empty());
}

TEST(ParserErrorTest, CallWithReturnInvalidTarget) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = call nonexistent\n");
    EXPECT_TRUE(buf.empty());
}

TEST(ParserErrorTest, ReturnInvalidExpression) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    return !!!\n");
    EXPECT_TRUE(buf.empty());
}

// ===================================================================
// Function Parameters (함수 매개변수) 파서 테스트
// ===================================================================

TEST(ParserTest, LabelWithParams) {
    auto buf = GyeolTest::compileScript(
        "label greet(name, title):\n"
        "    narrator \"Hello\"\n");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* node = story->nodes()->Get(0);
    EXPECT_STREQ(node->name()->c_str(), "greet");

    // param_ids 확인
    ASSERT_TRUE(node->param_ids() != nullptr);
    ASSERT_EQ(node->param_ids()->size(), 2u);
    EXPECT_STREQ(story->string_pool()->Get(node->param_ids()->Get(0))->c_str(), "name");
    EXPECT_STREQ(story->string_pool()->Get(node->param_ids()->Get(1))->c_str(), "title");
}

TEST(ParserTest, LabelNoParams) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    narrator \"Hello\"\n");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* node = story->nodes()->Get(0);
    // param_ids 비어있거나 nullptr (하위 호환)
    EXPECT_TRUE(node->param_ids() == nullptr || node->param_ids()->size() == 0u);
}

TEST(ParserTest, LabelEmptyParens) {
    auto buf = GyeolTest::compileScript(
        "label func():\n"
        "    narrator \"test\"\n");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* node = story->nodes()->Get(0);
    EXPECT_STREQ(node->name()->c_str(), "func");
    // 빈 괄호 → 0 params
    EXPECT_TRUE(node->param_ids() == nullptr || node->param_ids()->size() == 0u);
}

TEST(ParserTest, LabelSingleParam) {
    auto buf = GyeolTest::compileScript(
        "label func(x):\n"
        "    narrator \"{x}\"\n");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* node = story->nodes()->Get(0);
    ASSERT_TRUE(node->param_ids() != nullptr);
    ASSERT_EQ(node->param_ids()->size(), 1u);
    EXPECT_STREQ(story->string_pool()->Get(node->param_ids()->Get(0))->c_str(), "x");
}

TEST(ParserTest, CallWithArgs) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    call greet(\"Hero\", \"Mr\")\n"
        "\n"
        "label greet(name, title):\n"
        "    narrator \"Hello\"\n");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* startNode = story->nodes()->Get(0);
    ASSERT_EQ(startNode->lines()->size(), 1u);

    auto* instr = startNode->lines()->Get(0);
    EXPECT_EQ(instr->data_type(), OpData::Jump);
    auto* jump = instr->data_as_Jump();
    EXPECT_TRUE(jump->is_call());

    // arg_exprs 확인
    ASSERT_TRUE(jump->arg_exprs() != nullptr);
    EXPECT_EQ(jump->arg_exprs()->size(), 2u);
}

TEST(ParserTest, CallNoArgs) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    call sub\n"
        "\n"
        "label sub:\n"
        "    narrator \"test\"\n");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* instr = story->nodes()->Get(0)->lines()->Get(0);
    auto* jump = instr->data_as_Jump();
    // arg_exprs 없음 (하위 호환)
    EXPECT_TRUE(jump->arg_exprs() == nullptr || jump->arg_exprs()->size() == 0u);
}

TEST(ParserTest, CallWithReturnAndArgs) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ r = call calc(10, 20)\n"
        "\n"
        "label calc(a, b):\n"
        "    return a + b\n");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* instr = story->nodes()->Get(0)->lines()->Get(0);
    EXPECT_EQ(instr->data_type(), OpData::CallWithReturn);
    auto* cwr = instr->data_as_CallWithReturn();

    ASSERT_TRUE(cwr->arg_exprs() != nullptr);
    EXPECT_EQ(cwr->arg_exprs()->size(), 2u);
}

TEST(ParserTest, CallArgExpression) {
    auto buf = GyeolTest::compileScript(
        "$ x = 10\n"
        "$ y = 5\n"
        "\n"
        "label start:\n"
        "    call func(x + 1, y * 2)\n"
        "\n"
        "label func(a, b):\n"
        "    narrator \"test\"\n");
    ASSERT_FALSE(buf.empty());

    auto* story = GetStory(buf.data());
    auto* instr = story->nodes()->Get(0)->lines()->Get(0);
    auto* jump = instr->data_as_Jump();
    ASSERT_TRUE(jump->arg_exprs() != nullptr);
    EXPECT_EQ(jump->arg_exprs()->size(), 2u);

    // 첫 번째 인자: x + 1 → 다중 토큰 Expression
    auto* arg0 = jump->arg_exprs()->Get(0);
    EXPECT_GT(arg0->tokens()->size(), 1u);  // PushVar, PushLiteral, Add 등
}

// --- Function Parameters 에러 테스트 ---

TEST(ParserErrorTest, LabelDuplicateParam) {
    auto buf = GyeolTest::compileScript(
        "label func(a, a):\n"
        "    narrator \"test\"\n");
    EXPECT_TRUE(buf.empty());
}

TEST(ParserErrorTest, CallUnclosedParen) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    call func(1, 2\n"
        "\n"
        "label func(a, b):\n"
        "    narrator \"test\"\n");
    EXPECT_TRUE(buf.empty());
}

TEST(ParserErrorTest, JumpWithArgs) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    jump func(1, 2)\n"
        "\n"
        "label func(a, b):\n"
        "    narrator \"test\"\n");
    EXPECT_TRUE(buf.empty());
}

TEST(ParserErrorTest, CallEmptyArg) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    call func(, 2)\n"
        "\n"
        "label func(a, b):\n"
        "    narrator \"test\"\n");
    EXPECT_TRUE(buf.empty());
}

// ===================================================================
// Visit Count 파서 테스트
// ===================================================================

TEST(ParserTest, VisitCountInExpression) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = visit_count(\"shop\")\n"
        "    \"done\"\n"
        "label shop:\n"
        "    \"shop\"\n");
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* sv = story->nodes()->Get(0)->lines()->Get(0)->data_as_SetVar();
    ASSERT_NE(sv->expr(), nullptr);
    ASSERT_EQ(sv->expr()->tokens()->size(), 1u);
    EXPECT_EQ(sv->expr()->tokens()->Get(0)->op(), ExprOp::PushVisitCount);
    // var_name_id는 "shop"을 가리킴
    int32_t nameId = sv->expr()->tokens()->Get(0)->var_name_id();
    EXPECT_STREQ(story->string_pool()->Get(nameId)->c_str(), "shop");
}

TEST(ParserTest, VisitedInExpression) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ seen = visited(\"shop\")\n"
        "    \"done\"\n"
        "label shop:\n"
        "    \"shop\"\n");
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* sv = story->nodes()->Get(0)->lines()->Get(0)->data_as_SetVar();
    ASSERT_NE(sv->expr(), nullptr);
    ASSERT_EQ(sv->expr()->tokens()->size(), 1u);
    EXPECT_EQ(sv->expr()->tokens()->Get(0)->op(), ExprOp::PushVisited);
}

TEST(ParserTest, VisitCountBareArg) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = visit_count(shop)\n"
        "    \"done\"\n"
        "label shop:\n"
        "    \"shop\"\n");
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* sv = story->nodes()->Get(0)->lines()->Get(0)->data_as_SetVar();
    ASSERT_NE(sv->expr(), nullptr);
    ASSERT_EQ(sv->expr()->tokens()->size(), 1u);
    EXPECT_EQ(sv->expr()->tokens()->Get(0)->op(), ExprOp::PushVisitCount);
    int32_t nameId = sv->expr()->tokens()->Get(0)->var_name_id();
    EXPECT_STREQ(story->string_pool()->Get(nameId)->c_str(), "shop");
}

TEST(ParserTest, VisitCountInCondition) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    if visit_count(\"shop\") > 2 -> frequent\n"
        "    \"normal\"\n"
        "label frequent:\n"
        "    \"frequent\"\n"
        "label shop:\n"
        "    \"shop\"\n");
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* cond = story->nodes()->Get(0)->lines()->Get(0)->data_as_Condition();
    ASSERT_NE(cond, nullptr);
    ASSERT_NE(cond->cond_expr(), nullptr);
    bool foundVisitCount = false;
    for (flatbuffers::uoffset_t i = 0; i < cond->cond_expr()->tokens()->size(); ++i) {
        if (cond->cond_expr()->tokens()->Get(i)->op() == ExprOp::PushVisitCount)
            foundVisitCount = true;
    }
    EXPECT_TRUE(foundVisitCount);
}

TEST(ParserTest, VisitedInCondition) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    if visited(\"shop\") -> seen\n"
        "    \"unseen\"\n"
        "label seen:\n"
        "    \"seen\"\n"
        "label shop:\n"
        "    \"shop\"\n");
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* cond = story->nodes()->Get(0)->lines()->Get(0)->data_as_Condition();
    ASSERT_NE(cond, nullptr);
    ASSERT_NE(cond->cond_expr(), nullptr);
    bool foundVisited = false;
    for (flatbuffers::uoffset_t i = 0; i < cond->cond_expr()->tokens()->size(); ++i) {
        if (cond->cond_expr()->tokens()->Get(i)->op() == ExprOp::PushVisited)
            foundVisited = true;
    }
    EXPECT_TRUE(foundVisited);
}

TEST(ParserTest, VisitCountInArithmeticExpr) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ total = visit_count(\"a\") + visit_count(\"b\")\n"
        "    \"done\"\n"
        "label a:\n"
        "    \"a\"\n"
        "label b:\n"
        "    \"b\"\n");
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* sv = story->nodes()->Get(0)->lines()->Get(0)->data_as_SetVar();
    ASSERT_NE(sv->expr(), nullptr);
    // RPN: [PushVisitCount("a"), PushVisitCount("b"), Add]
    ASSERT_EQ(sv->expr()->tokens()->size(), 3u);
    EXPECT_EQ(sv->expr()->tokens()->Get(0)->op(), ExprOp::PushVisitCount);
    EXPECT_EQ(sv->expr()->tokens()->Get(1)->op(), ExprOp::PushVisitCount);
    EXPECT_EQ(sv->expr()->tokens()->Get(2)->op(), ExprOp::Add);
}

// ==========================================================================
// List 관련 파서 테스트
// ==========================================================================

TEST(ParserListTest, EmptyListLiteral) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ items = []\n"
        "    \"done\"\n");
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* sv = story->nodes()->Get(0)->lines()->Get(0)->data_as_SetVar();
    ASSERT_NE(sv, nullptr);
    ASSERT_EQ(sv->value_type(), ValueData::ListValue);
    auto* lv = sv->value_as_ListValue();
    ASSERT_NE(lv, nullptr);
    // 빈 리스트: items가 nullptr이거나 size == 0
    if (lv->items()) {
        EXPECT_EQ(lv->items()->size(), 0u);
    }
}

TEST(ParserListTest, ListLiteralWithStrings) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ items = [\"sword\", \"shield\", \"potion\"]\n"
        "    \"done\"\n");
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* sv = story->nodes()->Get(0)->lines()->Get(0)->data_as_SetVar();
    ASSERT_NE(sv, nullptr);
    ASSERT_EQ(sv->value_type(), ValueData::ListValue);
    auto* lv = sv->value_as_ListValue();
    ASSERT_NE(lv, nullptr);
    ASSERT_EQ(lv->items()->size(), 3u);
    // String Pool 참조 확인
    auto* pool = story->string_pool();
    EXPECT_STREQ(pool->Get(lv->items()->Get(0))->c_str(), "sword");
    EXPECT_STREQ(pool->Get(lv->items()->Get(1))->c_str(), "shield");
    EXPECT_STREQ(pool->Get(lv->items()->Get(2))->c_str(), "potion");
}

TEST(ParserListTest, ListLiteralWithBareWords) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ tags = [fire, ice, wind]\n"
        "    \"done\"\n");
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* sv = story->nodes()->Get(0)->lines()->Get(0)->data_as_SetVar();
    ASSERT_EQ(sv->value_type(), ValueData::ListValue);
    auto* lv = sv->value_as_ListValue();
    ASSERT_EQ(lv->items()->size(), 3u);
    auto* pool = story->string_pool();
    EXPECT_STREQ(pool->Get(lv->items()->Get(0))->c_str(), "fire");
    EXPECT_STREQ(pool->Get(lv->items()->Get(1))->c_str(), "ice");
    EXPECT_STREQ(pool->Get(lv->items()->Get(2))->c_str(), "wind");
}

TEST(ParserListTest, AppendOperator) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ items = []\n"
        "    $ items += \"sword\"\n"
        "    \"done\"\n");
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* sv = story->nodes()->Get(0)->lines()->Get(1)->data_as_SetVar();
    ASSERT_NE(sv, nullptr);
    EXPECT_EQ(sv->assign_op(), AssignOp::Append);
}

TEST(ParserListTest, RemoveOperator) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ items = [\"sword\"]\n"
        "    $ items -= \"sword\"\n"
        "    \"done\"\n");
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* sv = story->nodes()->Get(0)->lines()->Get(1)->data_as_SetVar();
    ASSERT_NE(sv, nullptr);
    EXPECT_EQ(sv->assign_op(), AssignOp::Remove);
}

TEST(ParserListTest, LenFunctionInExpression) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ count = len(items)\n"
        "    \"done\"\n");
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* sv = story->nodes()->Get(0)->lines()->Get(0)->data_as_SetVar();
    ASSERT_NE(sv, nullptr);
    ASSERT_NE(sv->expr(), nullptr);
    // 단일 ListLength 토큰
    ASSERT_EQ(sv->expr()->tokens()->size(), 1u);
    EXPECT_EQ(sv->expr()->tokens()->Get(0)->op(), ExprOp::ListLength);
}

TEST(ParserListTest, LenInArithmeticExpr) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ total = len(a) + len(b)\n"
        "    \"done\"\n");
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* sv = story->nodes()->Get(0)->lines()->Get(0)->data_as_SetVar();
    ASSERT_NE(sv->expr(), nullptr);
    ASSERT_EQ(sv->expr()->tokens()->size(), 3u);
    EXPECT_EQ(sv->expr()->tokens()->Get(0)->op(), ExprOp::ListLength);
    EXPECT_EQ(sv->expr()->tokens()->Get(1)->op(), ExprOp::ListLength);
    EXPECT_EQ(sv->expr()->tokens()->Get(2)->op(), ExprOp::Add);
}

TEST(ParserListTest, InOperatorInCondition) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    if \"sword\" in items -> has_sword\n"
        "    \"no sword\"\n"
        "label has_sword:\n"
        "    \"found sword\"\n");
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* cond = story->nodes()->Get(0)->lines()->Get(0)->data_as_Condition();
    ASSERT_NE(cond, nullptr);
    ASSERT_NE(cond->cond_expr(), nullptr);
    // RPN: [PushLiteral("sword"), PushVar(items), ListContains]
    bool foundContains = false;
    for (flatbuffers::uoffset_t i = 0; i < cond->cond_expr()->tokens()->size(); ++i) {
        if (cond->cond_expr()->tokens()->Get(i)->op() == ExprOp::ListContains)
            foundContains = true;
    }
    EXPECT_TRUE(foundContains);
}

TEST(ParserListTest, LenFunctionInCondition) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    if len(items) > 0 -> has_items\n"
        "    \"empty\"\n"
        "label has_items:\n"
        "    \"has items\"\n");
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* cond = story->nodes()->Get(0)->lines()->Get(0)->data_as_Condition();
    ASSERT_NE(cond, nullptr);
    ASSERT_NE(cond->cond_expr(), nullptr);
    bool foundLen = false;
    for (flatbuffers::uoffset_t i = 0; i < cond->cond_expr()->tokens()->size(); ++i) {
        if (cond->cond_expr()->tokens()->Get(i)->op() == ExprOp::ListLength)
            foundLen = true;
    }
    EXPECT_TRUE(foundLen);
}

TEST(ParserListTest, GlobalVarList) {
    auto buf = GyeolTest::compileScript(
        "$ inventory = [\"key\", \"map\"]\n"
        "label start:\n"
        "    \"done\"\n");
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    ASSERT_GE(story->global_vars()->size(), 1u);
    auto* gv = story->global_vars()->Get(0);
    EXPECT_EQ(gv->value_type(), ValueData::ListValue);
    auto* lv = gv->value_as_ListValue();
    ASSERT_NE(lv, nullptr);
    ASSERT_EQ(lv->items()->size(), 2u);
}

TEST(ParserListTest, ListLiteralInExpression) {
    // 리스트 리터럴이 표현식 컨텍스트에서 파싱됨
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ items = [\"a\", \"b\"]\n"
        "    \"done\"\n");
    ASSERT_FALSE(buf.empty());
    auto* story = GetStory(buf.data());
    auto* sv = story->nodes()->Get(0)->lines()->Get(0)->data_as_SetVar();
    ASSERT_NE(sv, nullptr);
    // 단일 리터럴이므로 value 필드에 ListValue로 저장됨
    EXPECT_EQ(sv->value_type(), ValueData::ListValue);
}
