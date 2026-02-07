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
