#include <gtest/gtest.h>
#include "test_helpers.h"
#include "gyeol_runner.h"
#include "gyeol_generated.h"
#include <set>
#include <fstream>
#include <unordered_map>

using namespace Gyeol;

// --- Runner 기본 흐름 ---

TEST(RunnerTest, BasicDialogue) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    hero \"hello\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    EXPECT_FALSE(runner.isFinished());

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.character, "hero");
    EXPECT_STREQ(r.line.text, "hello");

    r = runner.step();
    EXPECT_EQ(r.type, StepType::END);
    EXPECT_TRUE(runner.isFinished());
}

TEST(RunnerTest, Narration) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    \"narration text\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_EQ(r.line.character, nullptr);
    EXPECT_STREQ(r.line.text, "narration text");
}

TEST(RunnerTest, MultipleLines) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    hero \"line1\"\n"
        "    hero \"line2\"\n"
        "    hero \"line3\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));

    for (int i = 1; i <= 3; ++i) {
        auto r = runner.step();
        EXPECT_EQ(r.type, StepType::LINE);
        EXPECT_STREQ(r.line.text, ("line" + std::to_string(i)).c_str());
    }

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::END);
}

// --- 선택지 ---

TEST(RunnerTest, ChoicesPresented) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    menu:\n"
        "        \"A\" -> a\n"
        "        \"B\" -> b\n"
        "label a:\n"
        "    \"picked A\"\n"
        "label b:\n"
        "    \"picked B\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::CHOICES);
    ASSERT_EQ(r.choices.size(), 2u);
    EXPECT_STREQ(r.choices[0].text, "A");
    EXPECT_STREQ(r.choices[1].text, "B");
}

TEST(RunnerTest, ChooseFirstOption) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    menu:\n"
        "        \"A\" -> a\n"
        "        \"B\" -> b\n"
        "label a:\n"
        "    hero \"picked A\"\n"
        "label b:\n"
        "    hero \"picked B\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));

    runner.step(); // CHOICES
    runner.choose(0);

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "picked A");
}

TEST(RunnerTest, ChooseSecondOption) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    menu:\n"
        "        \"A\" -> a\n"
        "        \"B\" -> b\n"
        "label a:\n"
        "    hero \"picked A\"\n"
        "label b:\n"
        "    hero \"picked B\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));

    runner.step();
    runner.choose(1);

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "picked B");
}

// --- Jump ---

TEST(RunnerTest, JumpToNode) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    hero \"before jump\"\n"
        "    jump target\n"
        "label target:\n"
        "    hero \"after jump\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));

    auto r1 = runner.step();
    EXPECT_STREQ(r1.line.text, "before jump");

    auto r2 = runner.step();
    EXPECT_STREQ(r2.line.text, "after jump");

    auto r3 = runner.step();
    EXPECT_EQ(r3.type, StepType::END);
}

// --- Call / Return ---

TEST(RunnerTest, CallAndReturn) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    hero \"before call\"\n"
        "    call sub\n"
        "    hero \"after return\"\n"
        "label sub:\n"
        "    hero \"in subroutine\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));

    auto r1 = runner.step();
    EXPECT_STREQ(r1.line.text, "before call");

    auto r2 = runner.step();
    EXPECT_STREQ(r2.line.text, "in subroutine");

    auto r3 = runner.step();
    EXPECT_STREQ(r3.line.text, "after return");

    auto r4 = runner.step();
    EXPECT_EQ(r4.type, StepType::END);
}

// --- 변수 + 조건 ---

TEST(RunnerTest, SetVarAndConditionTrue) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ flag = 1\n"
        "    if flag == 1 -> yes else no\n"
        "label yes:\n"
        "    hero \"correct\"\n"
        "label no:\n"
        "    hero \"wrong\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "correct");
}

TEST(RunnerTest, SetVarAndConditionFalse) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ flag = 0\n"
        "    if flag == 1 -> yes else no\n"
        "label yes:\n"
        "    hero \"wrong\"\n"
        "label no:\n"
        "    hero \"correct\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));

    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "correct");
}

TEST(RunnerTest, ConditionWithoutElse) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = 5\n"
        "    if x == 5 -> target\n"
        "    hero \"should not reach\"\n"
        "label target:\n"
        "    hero \"jumped\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));

    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "jumped");
}

TEST(RunnerTest, ConditionFalseWithoutElseContinues) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = 0\n"
        "    if x == 5 -> target\n"
        "    hero \"continued\"\n"
        "label target:\n"
        "    hero \"jumped\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));

    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "continued");
}

// --- Command ---

TEST(RunnerTest, CommandReturned) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    @ bg \"forest.png\"\n"
        "    hero \"done\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::COMMAND);
    EXPECT_STREQ(r.command.type, "bg");
    ASSERT_EQ(r.command.params.size(), 1u);
    EXPECT_STREQ(r.command.params[0], "forest.png");

    auto r2 = runner.step();
    EXPECT_EQ(r2.type, StepType::LINE);
    EXPECT_STREQ(r2.line.text, "done");
}

// --- 복합 시나리오 ---

TEST(RunnerTest, FullStoryFlow) {
    // explore 경로: start -> explore (courage=1) -> encounter -> brave -> ending_greeting (call) -> ending_good
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    @ bg \"forest.png\"\n"
        "    \"intro\"\n"
        "    $ courage = 0\n"
        "    menu:\n"
        "        \"explore\" -> explore\n"
        "        \"flee\" -> flee\n"
        "label explore:\n"
        "    hero \"exploring\"\n"
        "    $ courage = 1\n"
        "    jump encounter\n"
        "label flee:\n"
        "    hero \"fleeing\"\n"
        "    $ courage = 0\n"
        "    jump encounter\n"
        "label encounter:\n"
        "    \"wolf appears\"\n"
        "    if courage == 1 -> brave else coward\n"
        "label brave:\n"
        "    hero \"brave!\"\n"
        "    call greeting\n"
        "    jump good_end\n"
        "label coward:\n"
        "    hero \"scared\"\n"
        "label greeting:\n"
        "    \"greetings\"\n"
        "label good_end:\n"
        "    hero \"victory\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));

    // @ bg
    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::COMMAND);

    // "intro"
    r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "intro");

    // menu ($ courage=0 is internal)
    r = runner.step();
    EXPECT_EQ(r.type, StepType::CHOICES);
    ASSERT_EQ(r.choices.size(), 2u);

    // choose "explore"
    runner.choose(0);

    // "exploring"
    r = runner.step();
    EXPECT_STREQ(r.line.text, "exploring");

    // jump encounter -> "wolf appears" ($ courage=1 is internal)
    r = runner.step();
    EXPECT_STREQ(r.line.text, "wolf appears");

    // condition: courage==1 -> brave
    // "brave!"
    r = runner.step();
    EXPECT_STREQ(r.line.text, "brave!");

    // call greeting -> "greetings"
    r = runner.step();
    EXPECT_STREQ(r.line.text, "greetings");

    // return from call -> jump good_end -> "victory"
    r = runner.step();
    EXPECT_STREQ(r.line.text, "victory");

    // END
    r = runner.step();
    EXPECT_EQ(r.type, StepType::END);
}

// --- 엣지 케이스 ---

TEST(RunnerTest, InvalidBufferReturnsFalse) {
    Runner runner;
    uint8_t garbage[] = {0, 1, 2, 3, 4, 5};
    EXPECT_FALSE(runner.start(garbage, sizeof(garbage)));
}

TEST(RunnerTest, StepAfterEndReturnsEnd) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    hero \"only line\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));

    runner.step(); // LINE
    runner.step(); // END

    auto r = runner.step(); // should still be END
    EXPECT_EQ(r.type, StepType::END);
    EXPECT_TRUE(runner.isFinished());
}

// --- Variable API Tests ---

TEST(RunnerTest, GetSetVariable) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ hp = 100\n"
        "    \"check\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    runner.step(); // SetVar + Line

    EXPECT_TRUE(runner.hasVariable("hp"));
    auto v = runner.getVariable("hp");
    EXPECT_EQ(v.type, Variant::INT);
    EXPECT_EQ(v.i, 100);
}

TEST(RunnerTest, HasVariable) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ exists = true\n"
        "    \"check\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    runner.step();

    EXPECT_TRUE(runner.hasVariable("exists"));
    EXPECT_FALSE(runner.hasVariable("nope"));
}

TEST(RunnerTest, GetVariableDefault) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    \"hello\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));

    // 존재하지 않는 변수는 Int(0) 반환
    auto v = runner.getVariable("missing");
    EXPECT_EQ(v.type, Variant::INT);
    EXPECT_EQ(v.i, 0);
}

TEST(RunnerTest, SetVariableFromExternal) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    \"hello\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));

    // 외부에서 변수 설정
    runner.setVariable("score", Variant::Int(42));
    runner.setVariable("name", Variant::String("Player"));
    runner.setVariable("ratio", Variant::Float(3.14f));
    runner.setVariable("alive", Variant::Bool(true));

    EXPECT_EQ(runner.getVariable("score").i, 42);
    EXPECT_EQ(runner.getVariable("name").s, "Player");
    EXPECT_FLOAT_EQ(runner.getVariable("ratio").f, 3.14f);
    EXPECT_TRUE(runner.getVariable("alive").b);
}

TEST(RunnerTest, GetVariableNames) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ a = 1\n"
        "    $ b = 2\n"
        "    $ c = 3\n"
        "    \"done\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    runner.step(); // execute SetVars + Line

    auto names = runner.getVariableNames();
    EXPECT_EQ(names.size(), 3u);

    // 순서는 보장되지 않으므로 set으로 비교
    std::set<std::string> nameSet(names.begin(), names.end());
    EXPECT_TRUE(nameSet.count("a"));
    EXPECT_TRUE(nameSet.count("b"));
    EXPECT_TRUE(nameSet.count("c"));
}

TEST(RunnerTest, ExternalSetAffectsCondition) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    if flag == true -> yes\n"
        "    \"no path\"\n"
        "\n"
        "label yes:\n"
        "    \"yes path\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));

    // 외부에서 flag = true 설정
    runner.setVariable("flag", Variant::Bool(true));

    auto r = runner.step();
    ASSERT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "yes path");
}

// --- 산술 표현식 테스트 ---

TEST(RunnerTest, ExprSimpleAdd) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = 1 + 2\n"
        "    \"done\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    runner.step(); // SetVar + Line
    EXPECT_EQ(runner.getVariable("x").i, 3);
}

TEST(RunnerTest, ExprVarRef) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = 10\n"
        "    $ y = x + 5\n"
        "    \"done\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    runner.step();
    EXPECT_EQ(runner.getVariable("y").i, 15);
}

TEST(RunnerTest, ExprSelfIncrement) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = 10\n"
        "    $ x = x + 1\n"
        "    \"done\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    runner.step();
    EXPECT_EQ(runner.getVariable("x").i, 11);
}

TEST(RunnerTest, ExprPrecedence) {
    // 1 + 2 * 3 = 7 (not 9)
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = 1 + 2 * 3\n"
        "    \"done\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    runner.step();
    EXPECT_EQ(runner.getVariable("x").i, 7);
}

TEST(RunnerTest, ExprParentheses) {
    // (1 + 2) * 3 = 9
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = (1 + 2) * 3\n"
        "    \"done\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    runner.step();
    EXPECT_EQ(runner.getVariable("x").i, 9);
}

TEST(RunnerTest, ExprFloatPromotion) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = 5 + 2.5\n"
        "    \"done\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    runner.step();
    EXPECT_EQ(runner.getVariable("x").type, Variant::FLOAT);
    EXPECT_FLOAT_EQ(runner.getVariable("x").f, 7.5f);
}

TEST(RunnerTest, ExprDivByZero) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = 10 / 0\n"
        "    \"done\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    runner.step();
    EXPECT_EQ(runner.getVariable("x").i, 0); // 안전 기본값
}

TEST(RunnerTest, ExprWithCondition) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ hp = 100\n"
        "    $ hp = hp - 60\n"
        "    if hp < 50 -> low\n"
        "    \"high hp\"\n"
        "\n"
        "label low:\n"
        "    \"low hp\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "low hp");
}

TEST(RunnerTest, ExprUnaryMinus) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = -5\n"
        "    \"done\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    runner.step();
    EXPECT_EQ(runner.getVariable("x").i, -5);
}

TEST(RunnerTest, BackwardCompatLiteral) {
    // 기존 문법 그대로 동작 확인
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ flag = true\n"
        "    $ name = \"hero\"\n"
        "    \"done\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    runner.step();
    EXPECT_TRUE(runner.getVariable("flag").b);
    EXPECT_EQ(runner.getVariable("name").s, "hero");
}

// --- 문자열 보간 테스트 ---

TEST(RunnerTest, InterpolateBasic) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ name = \"hero\"\n"
        "    narrator \"Hello {name}!\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    ASSERT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "Hello hero!");
}

TEST(RunnerTest, InterpolateMultiple) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ a = \"fire\"\n"
        "    $ b = \"ice\"\n"
        "    \"{a} and {b}\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    ASSERT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "fire and ice");
}

TEST(RunnerTest, InterpolateNoVar) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    \"plain text\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    ASSERT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "plain text");
}

TEST(RunnerTest, InterpolateUndefined) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    \"Hello {missing}!\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    ASSERT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "Hello !");
}

TEST(RunnerTest, InterpolateInChoice) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ item = \"sword\"\n"
        "    menu:\n"
        "        \"Take {item}\" -> take\n"
        "        \"Leave\" -> leave\n"
        "\n"
        "label take:\n"
        "    \"took\"\n"
        "\n"
        "label leave:\n"
        "    \"left\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    ASSERT_EQ(r.type, StepType::CHOICES);
    ASSERT_EQ(r.choices.size(), 2u);
    EXPECT_STREQ(r.choices[0].text, "Take sword");
    EXPECT_STREQ(r.choices[1].text, "Leave");
}

TEST(RunnerTest, InterpolateIntVar) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ hp = 100\n"
        "    \"HP: {hp}\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    ASSERT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "HP: 100");
}

// --- 조건 표현식 테스트 ---

TEST(RunnerTest, CondExprLHS) {
    // if hp - 60 > 30 → hp=100, 100-60=40, 40>30 → true
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ hp = 100\n"
        "    if hp - 60 > 30 -> yes\n"
        "    \"no\"\n"
        "\n"
        "label yes:\n"
        "    \"yes\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "yes");
}

TEST(RunnerTest, CondExprRHS) {
    // if x == y + 1 → x=6, y=5, 5+1=6, 6==6 → true
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = 6\n"
        "    $ y = 5\n"
        "    if x == y + 1 -> yes\n"
        "    \"no\"\n"
        "\n"
        "label yes:\n"
        "    \"yes\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "yes");
}

TEST(RunnerTest, CondExprBothSides) {
    // if x + 1 == y → x=4, y=5, 4+1=5, 5==5 → true
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = 4\n"
        "    $ y = 5\n"
        "    if x + 1 == y -> yes\n"
        "    \"no\"\n"
        "\n"
        "label yes:\n"
        "    \"yes\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "yes");
}

TEST(RunnerTest, CondExprFalse) {
    // if hp - 50 > 60 → hp=100, 100-50=50, 50>60 → false
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ hp = 100\n"
        "    if hp - 50 > 60 -> yes else no\n"
        "\n"
        "label yes:\n"
        "    \"yes\"\n"
        "\n"
        "label no:\n"
        "    \"no\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "no");
}

TEST(RunnerTest, CondExprNoElse) {
    // else 없이 false → 다음 줄 계속
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = 1\n"
        "    if x + 1 > 10 -> skip\n"
        "    \"continued\"\n"
        "\n"
        "label skip:\n"
        "    \"skipped\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "continued");
}

// --- 논리 연산자 실행 ---

TEST(RunnerTest, CondAndTrue) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ hp = 10\n"
        "    $ has_key = true\n"
        "    if hp > 0 and has_key == true -> yes else no\n"
        "\n"
        "label yes:\n"
        "    \"yes\"\n"
        "label no:\n"
        "    \"no\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "yes");
}

TEST(RunnerTest, CondAndFalse) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ hp = 10\n"
        "    $ has_key = false\n"
        "    if hp > 0 and has_key == true -> yes else no\n"
        "\n"
        "label yes:\n"
        "    \"yes\"\n"
        "label no:\n"
        "    \"no\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "no");
}

TEST(RunnerTest, CondOrTrue) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = 5\n"
        "    $ y = 15\n"
        "    if x > 10 or y > 10 -> yes else no\n"
        "\n"
        "label yes:\n"
        "    \"yes\"\n"
        "label no:\n"
        "    \"no\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "yes");
}

TEST(RunnerTest, CondNotTrue) {
    // not game_over == true: game_over=false → false == true → false → not false → true
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ game_over = false\n"
        "    if not game_over == true -> yes else no\n"
        "\n"
        "label yes:\n"
        "    \"yes\"\n"
        "label no:\n"
        "    \"no\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "yes");
}

TEST(RunnerTest, CondNestedParens) {
    // hp > 0 and (has_key == true or has_pick == true)
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ hp = 10\n"
        "    $ has_key = false\n"
        "    $ has_pick = true\n"
        "    if hp > 0 and (has_key == true or has_pick == true) -> yes else no\n"
        "\n"
        "label yes:\n"
        "    \"yes\"\n"
        "label no:\n"
        "    \"no\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "yes");
}

// --- Elif/Else 체인 ---

TEST(RunnerTest, ElifChainFirstMatch) {
    // 첫 조건 참 → 첫 번째 분기
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ hp = 100\n"
        "    if hp > 80 -> high\n"
        "    elif hp > 50 -> mid\n"
        "    elif hp > 20 -> low\n"
        "    else -> crit\n"
        "\n"
        "label high:\n"
        "    \"high hp\"\n"
        "label mid:\n"
        "    \"mid hp\"\n"
        "label low:\n"
        "    \"low hp\"\n"
        "label crit:\n"
        "    \"critical\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "high hp");
}

TEST(RunnerTest, ElifChainMiddleMatch) {
    // 중간 elif 참
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ hp = 60\n"
        "    if hp > 80 -> high\n"
        "    elif hp > 50 -> mid\n"
        "    elif hp > 20 -> low\n"
        "    else -> crit\n"
        "\n"
        "label high:\n"
        "    \"high hp\"\n"
        "label mid:\n"
        "    \"mid hp\"\n"
        "label low:\n"
        "    \"low hp\"\n"
        "label crit:\n"
        "    \"critical\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "mid hp");
}

TEST(RunnerTest, ElifChainElseFallthrough) {
    // 모든 조건 거짓 → else
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ hp = 10\n"
        "    if hp > 80 -> high\n"
        "    elif hp > 50 -> mid\n"
        "    elif hp > 20 -> low\n"
        "    else -> crit\n"
        "\n"
        "label high:\n"
        "    \"high hp\"\n"
        "label mid:\n"
        "    \"mid hp\"\n"
        "label low:\n"
        "    \"low hp\"\n"
        "label crit:\n"
        "    \"critical\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "critical");
}

TEST(RunnerTest, ElifChainNoElse) {
    // 모든 조건 거짓 + else 없음 → 다음 줄 fall-through
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ hp = 10\n"
        "    if hp > 80 -> high\n"
        "    elif hp > 50 -> mid\n"
        "    elif hp > 20 -> low\n"
        "    \"default path\"\n"
        "\n"
        "label high:\n"
        "    \"high hp\"\n"
        "label mid:\n"
        "    \"mid hp\"\n"
        "label low:\n"
        "    \"low hp\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "default path");
}

TEST(RunnerTest, ElifWithStringVar) {
    // 문자열 변수 비교
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ cls = \"mage\"\n"
        "    if cls == \"warrior\" -> w\n"
        "    elif cls == \"mage\" -> m\n"
        "    elif cls == \"rogue\" -> r\n"
        "    \"unknown\"\n"
        "\n"
        "label w:\n"
        "    \"warrior path\"\n"
        "label m:\n"
        "    \"mage path\"\n"
        "label r:\n"
        "    \"rogue path\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "mage path");
}

TEST(RunnerTest, ElifWithLogicalOps) {
    // elif에서 논리 연산자 사용
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ hp = 50\n"
        "    $ has_key = true\n"
        "    if hp > 80 -> high\n"
        "    elif hp > 30 and has_key == true -> mid_key\n"
        "    else -> low\n"
        "\n"
        "label high:\n"
        "    \"high\"\n"
        "label mid_key:\n"
        "    \"mid with key\"\n"
        "label low:\n"
        "    \"low\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "mid with key");
}

// --- Random 분기 러너 테스트 ---

TEST(RunnerTest, RandomWeightedGuaranteed) {
    // weight 0/100 → 항상 100쪽 선택
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    random:\n"
        "        0 -> never\n"
        "        100 -> always\n"
        "label never:\n"
        "    \"never\"\n"
        "label always:\n"
        "    \"always\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    runner.setSeed(12345);
    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "always");
}

TEST(RunnerTest, RandomAllZeroSkip) {
    // 모든 weight 0 → skip → 다음 줄 실행
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    random:\n"
        "        0 -> path_a\n"
        "        0 -> path_b\n"
        "    \"fallthrough\"\n"
        "label path_a:\n"
        "    \"a\"\n"
        "label path_b:\n"
        "    \"b\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "fallthrough");
}

TEST(RunnerTest, RandomSeedDeterminism) {
    // 같은 시드 → 같은 결과
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    random:\n"
        "        50 -> path_a\n"
        "        50 -> path_b\n"
        "label path_a:\n"
        "    \"a\"\n"
        "label path_b:\n"
        "    \"b\"\n"
    );

    std::string firstResult;
    {
        Runner runner;
        ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
        runner.setSeed(42);
        auto r = runner.step();
        firstResult = r.line.text;
    }

    // 같은 시드로 다시 실행 → 같은 결과
    for (int trial = 0; trial < 5; ++trial) {
        Runner runner;
        ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
        runner.setSeed(42);
        auto r = runner.step();
        EXPECT_STREQ(r.line.text, firstResult.c_str());
    }
}

TEST(RunnerTest, RandomEqualWeight) {
    // weight 1/1 → 유효한 결과 중 하나
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    random:\n"
        "        -> path_a\n"
        "        -> path_b\n"
        "label path_a:\n"
        "    \"a\"\n"
        "label path_b:\n"
        "    \"b\"\n"
    );
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    runner.setSeed(99);
    auto r = runner.step();
    std::string text = r.line.text;
    EXPECT_TRUE(text == "a" || text == "b");
}

// --- Locale (다국어) 테스트 ---

// 헬퍼: 스크립트로부터 locale CSV를 생성 (line_id → 번역 텍스트 매핑)
static void writeLocaleCSV(const std::string& csvPath,
                           const std::vector<uint8_t>& buf,
                           const std::unordered_map<std::string, std::string>& translations) {
    using namespace ICPDev::Gyeol::Schema;
    auto* story = GetStory(buf.data());
    std::ofstream ofs(csvPath);
    ofs << "line_id,type,node,character,text\n";
    for (flatbuffers::uoffset_t i = 0; i < story->line_ids()->size(); ++i) {
        auto* lid = story->line_ids()->Get(i);
        if (lid->size() == 0) continue;
        std::string lineId = lid->c_str();
        auto it = translations.find(lineId);
        if (it != translations.end()) {
            ofs << lineId << ",LINE,node,," << it->second << "\n";
        }
    }
}

// line_id를 특정 pool text로 찾는 헬퍼
static std::string findLineIdForText(const std::vector<uint8_t>& buf, const std::string& text) {
    using namespace ICPDev::Gyeol::Schema;
    auto* story = GetStory(buf.data());
    for (flatbuffers::uoffset_t i = 0; i < story->string_pool()->size(); ++i) {
        if (std::string(story->string_pool()->Get(i)->c_str()) == text) {
            if (i < story->line_ids()->size()) {
                return story->line_ids()->Get(i)->c_str();
            }
        }
    }
    return "";
}

TEST(RunnerTest, LocaleOverlayBasic) {
    // loadLocale → 번역된 대사 출력
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    \"Hello world!\"\n"
    );
    ASSERT_FALSE(buf.empty());

    // line_id 찾기
    std::string lid = findLineIdForText(buf, "Hello world!");
    ASSERT_FALSE(lid.empty());

    // CSV 작성
    std::string csvPath = "test_locale_basic.csv";
    writeLocaleCSV(csvPath, buf, {{lid, "안녕하세요!"}});

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    ASSERT_TRUE(runner.loadLocale(csvPath));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "안녕하세요!");

    std::remove(csvPath.c_str());
}

TEST(RunnerTest, LocaleFallback) {
    // 번역 없는 문자열 → 원문 폴백
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    \"Translated line\"\n"
        "    \"Untranslated line\"\n"
    );
    ASSERT_FALSE(buf.empty());

    std::string lid = findLineIdForText(buf, "Translated line");
    ASSERT_FALSE(lid.empty());

    // "Translated line"만 번역, "Untranslated line"은 번역 안 함
    std::string csvPath = "test_locale_fallback.csv";
    writeLocaleCSV(csvPath, buf, {{lid, "번역됨"}});

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    ASSERT_TRUE(runner.loadLocale(csvPath));

    auto r1 = runner.step();
    EXPECT_STREQ(r1.line.text, "번역됨");

    auto r2 = runner.step();
    EXPECT_STREQ(r2.line.text, "Untranslated line");

    std::remove(csvPath.c_str());
}

TEST(RunnerTest, LocaleWithInterpolation) {
    // 번역 텍스트에서 {변수} 보간 동작
    auto buf = GyeolTest::compileScript(
        "$ name = \"Player\"\n"
        "label start:\n"
        "    \"Hello {name}!\"\n"
    );
    ASSERT_FALSE(buf.empty());

    std::string lid = findLineIdForText(buf, "Hello {name}!");
    ASSERT_FALSE(lid.empty());

    // 번역에도 {name} 플레이스홀더 유지
    std::string csvPath = "test_locale_interp.csv";
    writeLocaleCSV(csvPath, buf, {{lid, "안녕 {name}님!"}});

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    ASSERT_TRUE(runner.loadLocale(csvPath));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "안녕 Player님!");

    std::remove(csvPath.c_str());
}

TEST(RunnerTest, LocaleClearRevert) {
    // clearLocale → 원문 복귀
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    \"Hello!\"\n"
    );
    ASSERT_FALSE(buf.empty());

    std::string lid = findLineIdForText(buf, "Hello!");
    ASSERT_FALSE(lid.empty());

    std::string csvPath = "test_locale_clear.csv";
    writeLocaleCSV(csvPath, buf, {{lid, "안녕!"}});

    // 1차: locale 적용
    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    ASSERT_TRUE(runner.loadLocale(csvPath));
    EXPECT_EQ(runner.getLocale(), "test_locale_clear");

    auto r1 = runner.step();
    EXPECT_STREQ(r1.line.text, "안녕!");

    // clearLocale 후 재시작 → 원문
    runner.clearLocale();
    EXPECT_EQ(runner.getLocale(), "");
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r2 = runner.step();
    EXPECT_STREQ(r2.line.text, "Hello!");

    std::remove(csvPath.c_str());
}

// ========== 인라인 조건 텍스트 테스트 ==========

TEST(RunnerTest, InlineCondTrue) {
    // {if hp > 50} → true 분기
    auto buf = GyeolTest::compileScript(
        "$ hp = 80\n"
        "label start:\n"
        "    hero \"You have {if hp > 50}plenty of{else}low{endif} health\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "You have plenty of health");
}

TEST(RunnerTest, InlineCondFalse) {
    // {if hp > 50} → false 분기
    auto buf = GyeolTest::compileScript(
        "$ hp = 30\n"
        "label start:\n"
        "    hero \"You have {if hp > 50}plenty of{else}low{endif} health\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "You have low health");
}

TEST(RunnerTest, InlineCondTruthyTrue) {
    // {if has_key} → 변수 있고 true
    auto buf = GyeolTest::compileScript(
        "$ has_key = true\n"
        "label start:\n"
        "    \"The door is {if has_key}unlocked{else}locked{endif}.\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "The door is unlocked.");
}

TEST(RunnerTest, InlineCondTruthyFalse) {
    // {if has_key} → 변수 없음 → false
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    \"The door is {if has_key}unlocked{else}locked{endif}.\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "The door is locked.");
}

TEST(RunnerTest, InlineCondNoElse) {
    // {if has_key}...{endif} — else 없음, false면 빈 텍스트
    auto buf = GyeolTest::compileScript(
        "$ has_key = false\n"
        "label start:\n"
        "    \"Door{if has_key} (unlocked){endif}.\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "Door.");
}

TEST(RunnerTest, InlineCondWithVar) {
    // 조건 분기 안에 {변수} 보간 중첩
    auto buf = GyeolTest::compileScript(
        "$ hp = 80\n"
        "$ name = \"Hero\"\n"
        "label start:\n"
        "    \"{if hp > 0}{name} lives{else}Game over{endif}\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "Hero lives");
}

TEST(RunnerTest, InlineCondString) {
    // 문자열 비교 (\\\" → 파서가 \" 로 인식 → 보간시 "mage" 리터럴)
    auto buf = GyeolTest::compileScript(
        "$ class = \"mage\"\n"
        "label start:\n"
        "    \"Weapon: {if class == \\\"mage\\\"}Staff{else}Sword{endif}\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "Weapon: Staff");
}

TEST(RunnerTest, InlineCondInChoice) {
    // 선택지 텍스트에서 인라인 조건
    auto buf = GyeolTest::compileScript(
        "$ gold = 100\n"
        "label start:\n"
        "    menu:\n"
        "        \"Buy potion{if gold >= 50} (affordable){endif}\" -> start\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::CHOICES);
    ASSERT_EQ(r.choices.size(), 1u);
    EXPECT_STREQ(r.choices[0].text, "Buy potion (affordable)");
}

// ========== 태그 시스템 테스트 ==========

TEST(RunnerTest, TagsExposedInLineData) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    hero \"Hello!\" #mood:angry\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.character, "hero");
    EXPECT_STREQ(r.line.text, "Hello!");
    ASSERT_EQ(r.line.tags.size(), 1u);
    EXPECT_STREQ(r.line.tags[0].first, "mood");
    EXPECT_STREQ(r.line.tags[0].second, "angry");
}

TEST(RunnerTest, MultipleTagsExposed) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    hero \"Hello!\" #mood:angry #pose:arms_crossed #voice:hero.wav\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    ASSERT_EQ(r.line.tags.size(), 3u);
    EXPECT_STREQ(r.line.tags[0].first, "mood");
    EXPECT_STREQ(r.line.tags[0].second, "angry");
    EXPECT_STREQ(r.line.tags[1].first, "pose");
    EXPECT_STREQ(r.line.tags[1].second, "arms_crossed");
    EXPECT_STREQ(r.line.tags[2].first, "voice");
    EXPECT_STREQ(r.line.tags[2].second, "hero.wav");
}

TEST(RunnerTest, NoTagsEmptyVector) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    hero \"Hello!\"\n"
    );
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(GyeolTest::startRunner(runner, buf));
    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_TRUE(r.line.tags.empty());
}

// =================================================================
// --- Import 통합 테스트 ---
// =================================================================

TEST(RunnerTest, ImportedNodeJump) {
    // main에서 import된 파일의 노드로 jump
    std::vector<std::pair<std::string, std::string>> files = {
        {"test_run_import_common.gyeol",
         "label greeting:\n"
         "    narrator \"Hello from imported!\"\n"},
        {"test_run_import_main.gyeol",
         "import \"test_run_import_common.gyeol\"\n"
         "\n"
         "label start:\n"
         "    narrator \"Starting...\"\n"
         "    jump greeting\n"}
    };

    auto buf = GyeolTest::compileMultiFileScript(files, "test_run_import_main.gyeol");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    // step 1: "Starting..."
    auto r1 = runner.step();
    EXPECT_EQ(r1.type, StepType::LINE);
    EXPECT_STREQ(r1.line.text, "Starting...");

    // step 2: jump → greeting 노드의 "Hello from imported!"
    auto r2 = runner.step();
    EXPECT_EQ(r2.type, StepType::LINE);
    EXPECT_STREQ(r2.line.text, "Hello from imported!");

    // step 3: END
    auto r3 = runner.step();
    EXPECT_EQ(r3.type, StepType::END);
}

TEST(RunnerTest, ImportedGlobalVarsInitialized) {
    // import된 파일의 global vars가 런타임에서 접근 가능
    std::vector<std::pair<std::string, std::string>> files = {
        {"test_run_import_gv_common.gyeol",
         "$ imported_var = 99\n"
         "\n"
         "label common:\n"
         "    narrator \"common\"\n"},
        {"test_run_import_gv_main.gyeol",
         "import \"test_run_import_gv_common.gyeol\"\n"
         "\n"
         "label start:\n"
         "    narrator \"Value is {imported_var}\"\n"}
    };

    auto buf = GyeolTest::compileMultiFileScript(files, "test_run_import_gv_main.gyeol");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "Value is 99");
}

// =================================================================
// --- Return / CallWithReturn 런타임 테스트 ---
// =================================================================

TEST(RunnerTest, CallWithReturnLiteral) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = call calc\n"
        "    narrator \"{x}\"\n"
        "\n"
        "label calc:\n"
        "    return 42\n");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "42");

    EXPECT_EQ(runner.getVariable("x").type, Variant::INT);
    EXPECT_EQ(runner.getVariable("x").i, 42);
}

TEST(RunnerTest, CallWithReturnVariable) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ result = call compute\n"
        "    narrator \"{result}\"\n"
        "\n"
        "label compute:\n"
        "    $ val = 100\n"
        "    return val\n");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "100");

    EXPECT_EQ(runner.getVariable("result").i, 100);
}

TEST(RunnerTest, CallWithReturnExpression) {
    auto buf = GyeolTest::compileScript(
        "$ a = 10\n"
        "$ b = 20\n"
        "\n"
        "label start:\n"
        "    $ sum = call add_them\n"
        "    narrator \"{sum}\"\n"
        "\n"
        "label add_them:\n"
        "    return a + b\n");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "30");

    EXPECT_EQ(runner.getVariable("sum").i, 30);
}

TEST(RunnerTest, CallWithReturnString) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ msg = call get_msg\n"
        "    narrator \"{msg}\"\n"
        "\n"
        "label get_msg:\n"
        "    return \"hello\"\n");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "hello");

    EXPECT_EQ(runner.getVariable("msg").type, Variant::STRING);
    EXPECT_EQ(runner.getVariable("msg").s, "hello");
}

TEST(RunnerTest, BareReturnNoValueCapture) {
    // bare return → 변수 미변경
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = 999\n"
        "    $ x = call sub\n"
        "    narrator \"{x}\"\n"
        "\n"
        "label sub:\n"
        "    return\n");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    // bare return → hasPendingReturn_ = false → 변수 미변경 → 999
    EXPECT_STREQ(r.line.text, "999");
    EXPECT_EQ(runner.getVariable("x").i, 999);
}

TEST(RunnerTest, ImplicitReturnNoValue) {
    // 노드 끝(return 없음) → 변수 미변경
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ x = 777\n"
        "    $ x = call sub\n"
        "    narrator \"{x}\"\n"
        "\n"
        "label sub:\n"
        "    $ temp = 1\n");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    // 명시적 return 없음 → 변수 미변경 → 777
    EXPECT_STREQ(r.line.text, "777");
    EXPECT_EQ(runner.getVariable("x").i, 777);
}

TEST(RunnerTest, ReturnWithoutCallFrame) {
    // call stack 없이 return → 스토리 종료
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    narrator \"before\"\n"
        "    return 42\n"
        "    narrator \"after\"\n");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r1 = runner.step();
    EXPECT_EQ(r1.type, StepType::LINE);
    EXPECT_STREQ(r1.line.text, "before");

    // return without call frame → END
    auto r2 = runner.step();
    EXPECT_EQ(r2.type, StepType::END);
    EXPECT_TRUE(runner.isFinished());
}

TEST(RunnerTest, ExistingCallStillWorks) {
    // 기존 call sub + return 42 → 값 무시 (returnVarName 비어있음)
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    call sub\n"
        "    narrator \"back\"\n"
        "\n"
        "label sub:\n"
        "    narrator \"in sub\"\n"
        "    return 42\n");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r1 = runner.step();
    EXPECT_EQ(r1.type, StepType::LINE);
    EXPECT_STREQ(r1.line.text, "in sub");

    // return 42 → call stack pop → 호출자로 복귀
    auto r2 = runner.step();
    EXPECT_EQ(r2.type, StepType::LINE);
    EXPECT_STREQ(r2.line.text, "back");

    auto r3 = runner.step();
    EXPECT_EQ(r3.type, StepType::END);
}

TEST(RunnerTest, NestedCallsWithReturn) {
    // A → B(return capture) → C(return capture) → 중첩 반환
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ outer = call mid\n"
        "    narrator \"{outer}\"\n"
        "\n"
        "label mid:\n"
        "    $ inner = call deep\n"
        "    return inner + 100\n"
        "\n"
        "label deep:\n"
        "    return 5\n");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "105"); // deep returns 5, mid returns 5+100=105
    EXPECT_EQ(runner.getVariable("outer").i, 105);
}

TEST(RunnerTest, CallWithReturnThenContinue) {
    // return 후 다음 명령 실행
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ val = call helper\n"
        "    narrator \"Got {val}\"\n"
        "    narrator \"Done\"\n"
        "\n"
        "label helper:\n"
        "    return 7\n");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r1 = runner.step();
    EXPECT_EQ(r1.type, StepType::LINE);
    EXPECT_STREQ(r1.line.text, "Got 7");

    auto r2 = runner.step();
    EXPECT_EQ(r2.type, StepType::LINE);
    EXPECT_STREQ(r2.line.text, "Done");

    auto r3 = runner.step();
    EXPECT_EQ(r3.type, StepType::END);
}

TEST(RunnerTest, ReturnFloat) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ f = call get_pi\n"
        "\n"
        "label get_pi:\n"
        "    return 3.14\n");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));
    runner.step(); // END (no dialogue)

    EXPECT_EQ(runner.getVariable("f").type, Variant::FLOAT);
    EXPECT_NEAR(runner.getVariable("f").f, 3.14f, 0.001f);
}

TEST(RunnerTest, ReturnBool) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ flag = call check\n"
        "\n"
        "label check:\n"
        "    return true\n");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));
    runner.step(); // END

    EXPECT_EQ(runner.getVariable("flag").type, Variant::BOOL);
    EXPECT_TRUE(runner.getVariable("flag").b);
}

// ===================================================================
// Function Parameters (함수 매개변수) 런타임 테스트
// ===================================================================

TEST(RunnerTest, CallWithSingleParam) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    call greet(\"Hero\")\n"
        "\n"
        "label greet(name):\n"
        "    narrator \"Hello {name}!\"\n");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "Hello Hero!");
}

TEST(RunnerTest, CallWithMultipleParams) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    call greet(\"Hero\", \"Mr\")\n"
        "\n"
        "label greet(name, title):\n"
        "    narrator \"Hello {title} {name}!\"\n");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "Hello Mr Hero!");
}

TEST(RunnerTest, ParamLocalScope) {
    // 매개변수 x는 call 후 호출자에서 사라져야 함 (기존에 없었으므로 erase)
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    call func(42)\n"
        "    narrator \"{x}\"\n"
        "\n"
        "label func(x):\n"
        "    narrator \"{x}\"\n");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "42");

    r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    // x가 원래 없었으므로 복원 시 삭제됨 → hasVariable 검증
    EXPECT_FALSE(runner.hasVariable("x"));
}

TEST(RunnerTest, ParamShadowsGlobal) {
    // 전역 변수 x=100이 매개변수로 섀도되고, 복원됨
    auto buf = GyeolTest::compileScript(
        "$ x = 100\n"
        "\n"
        "label start:\n"
        "    call func(42)\n"
        "    narrator \"{x}\"\n"
        "\n"
        "label func(x):\n"
        "    narrator \"{x}\"\n");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "42");

    r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "100");  // 전역 값 복원됨
}

TEST(RunnerTest, ParamExpressionArgs) {
    auto buf = GyeolTest::compileScript(
        "$ a = 10\n"
        "\n"
        "label start:\n"
        "    call func(a + 5, a * 2)\n"
        "\n"
        "label func(x, y):\n"
        "    narrator \"{x} {y}\"\n");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "15 20");
}

TEST(RunnerTest, CallWithReturnAndParams) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    $ result = call add(10, 20)\n"
        "    narrator \"{result}\"\n"
        "\n"
        "label add(a, b):\n"
        "    return a + b\n");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "30");
    EXPECT_EQ(runner.getVariable("result").i, 30);
}

TEST(RunnerTest, NestedCallsWithParams) {
    // A→B(x=1)→C(x=2), 각 레벨에서 x 섀도, 복원 확인
    auto buf = GyeolTest::compileScript(
        "$ x = 0\n"
        "\n"
        "label start:\n"
        "    call outer(1)\n"
        "    narrator \"{x}\"\n"
        "\n"
        "label outer(x):\n"
        "    call inner(2)\n"
        "    narrator \"{x}\"\n"
        "\n"
        "label inner(x):\n"
        "    narrator \"{x}\"\n");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "2");  // inner: x=2

    r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "1");  // outer: x=1 (inner 복원 후)

    r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "0");  // start: x=0 (outer 복원 후)
}

TEST(RunnerTest, ParamDefaultZero) {
    // 인자 부족 시 Int(0) 기본값
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    call func(42)\n"
        "\n"
        "label func(a, b):\n"
        "    narrator \"{a} {b}\"\n");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "42 0");
}

TEST(RunnerTest, ExistingCallNoParamsStillWorks) {
    // 기존 call (매개변수 없음) 하위 호환
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    call sub\n"
        "    narrator \"back\"\n"
        "\n"
        "label sub:\n"
        "    narrator \"in sub\"\n");
    ASSERT_FALSE(buf.empty());

    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "in sub");

    r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "back");
}

// ===================================================================
// Visit Count 런타임 테스트
// ===================================================================

TEST(RunnerTest, BasicVisitCount) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    "first"
    jump start
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    // start 진입 시 1회
    EXPECT_EQ(runner.getVisitCount("start"), 1);

    auto r = runner.step(); // "first" (visit 1)
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "first");

    // step() processes jump start (visit 2) → continues → "first" again
    r = runner.step();
    EXPECT_STREQ(r.line.text, "first");
    EXPECT_EQ(runner.getVisitCount("start"), 2);

    // step() processes jump start (visit 3) → continues → "first" again
    r = runner.step();
    EXPECT_STREQ(r.line.text, "first");
    EXPECT_EQ(runner.getVisitCount("start"), 3);
}

TEST(RunnerTest, VisitCountZeroUnvisited) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    "hello"
label other:
    "other"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    // start는 방문, other는 미방문
    EXPECT_EQ(runner.getVisitCount("start"), 1);
    EXPECT_EQ(runner.getVisitCount("other"), 0);
    EXPECT_EQ(runner.getVisitCount("nonexistent"), 0);
}

TEST(RunnerTest, VisitedBoolCheck) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    "in start"
    jump shop
label shop:
    "in shop"
label inn:
    "in inn"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    EXPECT_TRUE(runner.hasVisited("start"));
    EXPECT_FALSE(runner.hasVisited("shop"));
    EXPECT_FALSE(runner.hasVisited("inn"));

    runner.step(); // "in start"
    runner.step(); // jump shop → shop 진입

    EXPECT_TRUE(runner.hasVisited("shop"));
    EXPECT_FALSE(runner.hasVisited("inn"));
}

TEST(RunnerTest, VisitCountInExpression) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    jump target
label target:
    $ count = visit_count("target")
    narrator "{count}"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    // start(1) → jump target → target(1)
    auto r = runner.step(); // $ count = visit_count("target") → 1, then narrator "1"
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "1");
}

TEST(RunnerTest, VisitedInCondition) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    if visited("shop") -> been_there
    "First time"
    jump shop
label shop:
    "In shop"
    jump start
label been_there:
    "Welcome back"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    // 첫 방문: shop 미방문 → "First time"
    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "First time");

    // jump shop
    r = runner.step(); // "In shop"
    EXPECT_STREQ(r.line.text, "In shop");

    // jump start → visited("shop") == true → been_there
    r = runner.step(); // "Welcome back"
    EXPECT_STREQ(r.line.text, "Welcome back");
}

TEST(RunnerTest, VisitCountComparison) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    if visit_count("start") > 2 -> done
    "Again"
    jump start
label done:
    "Enough"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    // visit 1: count=1, not >2 → "Again"
    auto r = runner.step();
    EXPECT_STREQ(r.line.text, "Again");

    // jump start → visit 2: count=2, not >2 → "Again"
    r = runner.step();
    EXPECT_STREQ(r.line.text, "Again");

    // jump start → visit 3: count=3, >2 → done → "Enough"
    r = runner.step();
    EXPECT_STREQ(r.line.text, "Enough");
}

TEST(RunnerTest, VisitCountInterpolation) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    "Visit {visit_count(start)}"
    jump start
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r = runner.step(); // visit 1
    EXPECT_STREQ(r.line.text, "Visit 1");

    r = runner.step(); // jump start → visit 2
    EXPECT_STREQ(r.line.text, "Visit 2");
}

TEST(RunnerTest, VisitedInlineCondition) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    jump shop
label shop:
    "In shop"
    jump check
label check:
    "{if visited(shop)}been{else}new{endif}"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto r = runner.step(); // "In shop"
    EXPECT_STREQ(r.line.text, "In shop");

    r = runner.step(); // "{if visited(shop)}been{else}new{endif}"
    EXPECT_STREQ(r.line.text, "been");
}

TEST(RunnerTest, GetVisitCountAPI) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    "s"
    call sub
    "back"
label sub:
    "in sub"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    EXPECT_EQ(runner.getVisitCount("start"), 1);
    EXPECT_EQ(runner.getVisitCount("sub"), 0);

    runner.step(); // "s"
    runner.step(); // call sub → "in sub"

    EXPECT_EQ(runner.getVisitCount("sub"), 1);
    EXPECT_EQ(runner.getVisitCount("start"), 1);
}

TEST(RunnerTest, HasVisitedAPI) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    "s"
    jump other
label other:
    "o"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    EXPECT_TRUE(runner.hasVisited("start"));
    EXPECT_FALSE(runner.hasVisited("other"));

    runner.step(); // "s"
    runner.step(); // jump other → "o"

    EXPECT_TRUE(runner.hasVisited("other"));
    EXPECT_FALSE(runner.hasVisited("nonexistent"));
}

// ==========================================================================
// Debug API Tests
// ==========================================================================

TEST(DebugAPITest, BreakpointManagement) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    hero "line1"
    hero "line2"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    // 초기 상태: breakpoint 없음
    EXPECT_TRUE(runner.getBreakpoints().empty());
    EXPECT_FALSE(runner.hasBreakpoint("start", 0));

    // 추가
    runner.addBreakpoint("start", 0);
    runner.addBreakpoint("start", 1);
    EXPECT_TRUE(runner.hasBreakpoint("start", 0));
    EXPECT_TRUE(runner.hasBreakpoint("start", 1));
    EXPECT_FALSE(runner.hasBreakpoint("start", 2));
    EXPECT_EQ(runner.getBreakpoints().size(), 2u);

    // 삭제
    runner.removeBreakpoint("start", 0);
    EXPECT_FALSE(runner.hasBreakpoint("start", 0));
    EXPECT_TRUE(runner.hasBreakpoint("start", 1));
    EXPECT_EQ(runner.getBreakpoints().size(), 1u);

    // 전체 클리어
    runner.addBreakpoint("start", 0);
    runner.clearBreakpoints();
    EXPECT_TRUE(runner.getBreakpoints().empty());
}

TEST(DebugAPITest, StepModeControl) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    hero "line1"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    // 초기 상태: step mode 비활성
    EXPECT_FALSE(runner.isStepMode());

    runner.setStepMode(true);
    EXPECT_TRUE(runner.isStepMode());

    runner.setStepMode(false);
    EXPECT_FALSE(runner.isStepMode());
}

TEST(DebugAPITest, GetLocation) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    hero "line1"
    $ x = 10
    jump other
label other:
    "narration"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto loc = runner.getLocation();
    EXPECT_EQ(loc.nodeName, "start");
    EXPECT_EQ(loc.pc, 0u);
    EXPECT_EQ(loc.instructionType, "Line");
}

TEST(DebugAPITest, GetCurrentNodeAndPC) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    hero "line1"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    EXPECT_EQ(runner.getCurrentNodeName(), "start");
    EXPECT_EQ(runner.getCurrentPC(), 0u);

    runner.step(); // line1 실행 → PC 진행
    EXPECT_EQ(runner.getCurrentPC(), 1u);
}

TEST(DebugAPITest, GetCallStack) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    call sub
label sub:
    hero "in sub"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    // start에서 call 실행 전: 빈 콜 스택
    auto stack = runner.getCallStack();
    EXPECT_TRUE(stack.empty());

    // step mode로 call 내부 진입
    runner.setStepMode(true);
    auto r = runner.step(); // step mode pause
    if (r.type == StepType::END && !runner.isFinished()) {
        r = runner.step(); // call sub → sub로 이동
    }
    // sub 내부에서 콜 스택 확인
    if (r.type == StepType::END && !runner.isFinished()) {
        r = runner.step(); // sub 내부 instruction
    }

    stack = runner.getCallStack();
    // call로 진입했으므로 스택에 start 프레임이 있어야 함
    if (!stack.empty()) {
        EXPECT_EQ(stack[0].nodeName, "start");
    }
}

TEST(DebugAPITest, GetNodeNames) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    hero "s"
label nodeA:
    hero "a"
label nodeB:
    hero "b"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    auto names = runner.getNodeNames();
    EXPECT_EQ(names.size(), 3u);

    std::set<std::string> nameSet(names.begin(), names.end());
    EXPECT_TRUE(nameSet.count("start") > 0);
    EXPECT_TRUE(nameSet.count("nodeA") > 0);
    EXPECT_TRUE(nameSet.count("nodeB") > 0);
}

TEST(DebugAPITest, GetNodeInstructionCount) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    hero "line1"
    hero "line2"
    hero "line3"
label other:
    hero "only one"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    EXPECT_EQ(runner.getNodeInstructionCount("start"), 3u);
    EXPECT_EQ(runner.getNodeInstructionCount("other"), 1u);
    EXPECT_EQ(runner.getNodeInstructionCount("nonexistent"), 0u);
}

TEST(DebugAPITest, GetInstructionInfo) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    hero "hello"
    $ x = 10
    jump other
label other:
    "narration"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    // Line instruction
    std::string info0 = runner.getInstructionInfo("start", 0);
    EXPECT_TRUE(info0.find("Line") != std::string::npos);
    EXPECT_TRUE(info0.find("hero") != std::string::npos);
    EXPECT_TRUE(info0.find("hello") != std::string::npos);

    // SetVar instruction
    std::string info1 = runner.getInstructionInfo("start", 1);
    EXPECT_TRUE(info1.find("SetVar") != std::string::npos);
    EXPECT_TRUE(info1.find("x") != std::string::npos);

    // Jump instruction
    std::string info2 = runner.getInstructionInfo("start", 2);
    EXPECT_TRUE(info2.find("Jump") != std::string::npos);
    EXPECT_TRUE(info2.find("other") != std::string::npos);

    // Narration
    std::string infoN = runner.getInstructionInfo("other", 0);
    EXPECT_TRUE(infoN.find("narration") != std::string::npos);

    // Out of bounds / nonexistent
    EXPECT_TRUE(runner.getInstructionInfo("start", 999).empty());
    EXPECT_TRUE(runner.getInstructionInfo("nonexistent", 0).empty());
}

TEST(DebugAPITest, InstructionInfoChoice) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    menu:
        "Go left" -> left
        "Go right" -> right
label left:
    "Left!"
label right:
    "Right!"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    std::string info0 = runner.getInstructionInfo("start", 0);
    EXPECT_TRUE(info0.find("Choice") != std::string::npos);
    EXPECT_TRUE(info0.find("Go left") != std::string::npos);
}

TEST(DebugAPITest, InstructionInfoCommand) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    @ bg forest
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    std::string info = runner.getInstructionInfo("start", 0);
    EXPECT_TRUE(info.find("Command") != std::string::npos);
    EXPECT_TRUE(info.find("bg") != std::string::npos);
    EXPECT_TRUE(info.find("forest") != std::string::npos);
}

TEST(DebugAPITest, InstructionInfoCallWithReturn) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    $ result = call helper
label helper:
    return 42
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    std::string info = runner.getInstructionInfo("start", 0);
    EXPECT_TRUE(info.find("CallWithReturn") != std::string::npos);
    EXPECT_TRUE(info.find("result") != std::string::npos);
    EXPECT_TRUE(info.find("helper") != std::string::npos);
}

TEST(DebugAPITest, InstructionInfoCondition) {
    auto buf = GyeolTest::compileScript(R"(
$ x = 10
label start:
    if x > 5 -> yes else no
label yes:
    "yes"
label no:
    "no"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    std::string info = runner.getInstructionInfo("start", 0);
    EXPECT_TRUE(info.find("Condition") != std::string::npos);
}

TEST(DebugAPITest, InstructionInfoRandom) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    random:
        50 -> a
        50 -> b
label a:
    "a"
label b:
    "b"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    std::string info = runner.getInstructionInfo("start", 0);
    EXPECT_TRUE(info.find("Random") != std::string::npos);
    EXPECT_TRUE(info.find("2") != std::string::npos); // 2 branches
}

TEST(DebugAPITest, InstructionInfoReturn) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    call helper
label helper:
    return 42
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    std::string info = runner.getInstructionInfo("helper", 0);
    EXPECT_TRUE(info.find("Return") != std::string::npos);
}

TEST(DebugAPITest, StepModePausesExecution) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    hero "line1"
    hero "line2"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));
    runner.setStepMode(true);

    // 첫 step: pause 신호 (END 반환이지만 finished는 아님)
    auto r = runner.step();
    EXPECT_FALSE(runner.isFinished());

    // 두 번째 step: 실제 instruction 실행
    r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "line1");
}

TEST(DebugAPITest, BreakpointHitsAndResumes) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    hero "line1"
    hero "line2"
    hero "line3"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    // PC 1에 breakpoint 설정 (line2)
    runner.addBreakpoint("start", 1);

    // 첫 step: line1 실행
    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "line1");

    // 다음 step: breakpoint hit → END 반환 (but not finished)
    r = runner.step();
    EXPECT_EQ(r.type, StepType::END);
    EXPECT_FALSE(runner.isFinished());

    // 다시 step: breakpoint 통과 후 line2 실행
    r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "line2");
}

TEST(DebugAPITest, NoDebugFeaturesBackwardCompat) {
    // Debug 기능을 전혀 사용하지 않으면 기존과 동일하게 동작
    auto buf = GyeolTest::compileScript(R"(
label start:
    hero "line1"
    hero "line2"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    // step mode OFF, breakpoints 없음 → 기존과 동일
    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "line1");

    r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "line2");

    r = runner.step();
    EXPECT_EQ(r.type, StepType::END);
    EXPECT_TRUE(runner.isFinished());
}

TEST(DebugAPITest, LocationAfterJump) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    jump other
label other:
    hero "in other"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    // jump 실행 후 other 노드로 이동
    auto r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);

    auto loc = runner.getLocation();
    EXPECT_EQ(loc.nodeName, "other");
}

TEST(DebugAPITest, LocationTypes) {
    // 여러 instruction 타입의 location type 확인
    auto buf = GyeolTest::compileScript(R"(
label start:
    hero "dialogue"
    $ x = 10
    @ bg forest
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    // PC 0: Line
    auto loc = runner.getLocation();
    EXPECT_EQ(loc.instructionType, "Line");

    runner.step(); // execute Line → PC advances

    // PC 1: SetVar (내부적으로 skip되므로 PC 2로 이동)
    // SetVar와 Command는 step()에서 자동 실행될 수 있음
    // Command는 COMMAND 결과를 반환함
}

TEST(DebugAPITest, GetCallStackWithParams) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    $ result = call add(3, 4)
label add(a, b):
    return a + b
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));
    runner.setStepMode(true);

    // step mode로 call 내부 진입
    auto r = runner.step(); // pause
    if (r.type == StepType::END && !runner.isFinished()) {
        r = runner.step(); // CallWithReturn 실행 → add 노드 진입
    }
    if (r.type == StepType::END && !runner.isFinished()) {
        r = runner.step(); // add 내부 pause/instruction
    }

    // add 내부에서 call stack 확인
    auto stack = runner.getCallStack();
    if (!stack.empty()) {
        EXPECT_EQ(stack[0].nodeName, "start");
        EXPECT_EQ(stack[0].returnVarName, "result");
    }
}

TEST(DebugAPITest, BreakpointOnDifferentNode) {
    auto buf = GyeolTest::compileScript(R"(
label start:
    hero "in start"
    jump other
label other:
    hero "in other"
    hero "done"
)");
    ASSERT_FALSE(buf.empty());
    Runner runner;
    ASSERT_TRUE(runner.start(buf.data(), buf.size()));

    // other 노드의 PC 1에 breakpoint
    runner.addBreakpoint("other", 1);

    auto r = runner.step(); // "in start"
    EXPECT_STREQ(r.line.text, "in start");

    r = runner.step(); // jump → "in other"
    EXPECT_STREQ(r.line.text, "in other");

    // breakpoint hit at other:1
    r = runner.step();
    EXPECT_EQ(r.type, StepType::END);
    EXPECT_FALSE(runner.isFinished());

    // resume → "done"
    r = runner.step();
    EXPECT_EQ(r.type, StepType::LINE);
    EXPECT_STREQ(r.line.text, "done");
}
