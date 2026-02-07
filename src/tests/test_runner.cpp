#include <gtest/gtest.h>
#include "test_helpers.h"
#include "gyeol_runner.h"
#include <set>

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
