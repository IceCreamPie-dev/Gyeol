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
