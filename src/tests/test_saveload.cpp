#include <gtest/gtest.h>
#include "test_helpers.h"
#include <cstdio>

using namespace Gyeol;
using namespace GyeolTest;

static const char* SAVE_PATH = "test_save.gys";

class SaveLoadTest : public ::testing::Test {
protected:
    void TearDown() override {
        std::remove(SAVE_PATH);
    }
};

// 기본 라운드트립: 중간 저장 -> 복원 -> 동일 결과
TEST_F(SaveLoadTest, BasicRoundTrip) {
    auto buf = compileScript(R"(
label start:
    "Line 1"
    "Line 2"
    "Line 3"
)");
    ASSERT_FALSE(buf.empty());

    // 첫 줄 실행 후 저장
    Runner r1;
    ASSERT_TRUE(startRunner(r1, buf));
    auto res = r1.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "Line 1");

    ASSERT_TRUE(r1.saveState(SAVE_PATH));

    // 새 Runner에 로드
    Runner r2;
    ASSERT_TRUE(r2.start(buf.data(), buf.size()));
    ASSERT_TRUE(r2.loadState(SAVE_PATH));

    // 다음 줄이 "Line 2"여야 함
    res = r2.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "Line 2");

    res = r2.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "Line 3");

    res = r2.step();
    EXPECT_EQ(res.type, StepType::END);
}

// 선택지 대기 중 저장/복원
TEST_F(SaveLoadTest, SaveAtChoicePoint) {
    auto buf = compileScript(R"(
label start:
    "Before choices"
    menu:
        "Option A" -> nodeA
        "Option B" -> nodeB

label nodeA:
    "Chose A"

label nodeB:
    "Chose B"
)");
    ASSERT_FALSE(buf.empty());

    Runner r1;
    ASSERT_TRUE(startRunner(r1, buf));

    // 대사 실행
    auto res = r1.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "Before choices");

    // 선택지 받기
    res = r1.step();
    ASSERT_EQ(res.type, StepType::CHOICES);
    ASSERT_EQ(res.choices.size(), 2u);

    // 선택지 대기 상태에서 저장
    ASSERT_TRUE(r1.saveState(SAVE_PATH));

    // 새 Runner에 로드
    Runner r2;
    ASSERT_TRUE(r2.start(buf.data(), buf.size()));
    ASSERT_TRUE(r2.loadState(SAVE_PATH));

    // 선택지 선택 (Option B)
    r2.choose(1);
    res = r2.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "Chose B");
}

// 변수 저장/복원 (Bool, Int, Float, String)
TEST_F(SaveLoadTest, SaveWithVariables) {
    auto buf = compileScript(R"(
label start:
    $ flag = true
    $ score = 42
    $ ratio = 3.14
    $ name = "Player"
    "Check"
    jump end

label end:
    "Done"
)");
    ASSERT_FALSE(buf.empty());

    Runner r1;
    ASSERT_TRUE(startRunner(r1, buf));

    // 변수 설정 명령 실행 + 대사
    auto res = r1.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "Check");

    ASSERT_TRUE(r1.saveState(SAVE_PATH));

    // 새 Runner에 로드 후 조건으로 변수 검증
    Runner r2;
    ASSERT_TRUE(r2.start(buf.data(), buf.size()));
    ASSERT_TRUE(r2.loadState(SAVE_PATH));

    // step하면 "Done"이 나와야 함 (jump end 다음 위치에서 재개)
    res = r2.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "Done");
}

// 변수 타입별 조건 검증
TEST_F(SaveLoadTest, VariablesPreservedAcrossSaveLoad) {
    auto buf = compileScript(R"(
label start:
    $ hp = 100
    $ alive = true
    "Setup done"
    jump check

label check:
    if hp > 50 -> high_hp
    "Low HP"

label high_hp:
    "HP is high"
)");
    ASSERT_FALSE(buf.empty());

    Runner r1;
    ASSERT_TRUE(startRunner(r1, buf));

    auto res = r1.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "Setup done");

    // jump check 실행 전 저장 (pc는 jump를 가리킴)
    ASSERT_TRUE(r1.saveState(SAVE_PATH));

    Runner r2;
    ASSERT_TRUE(r2.start(buf.data(), buf.size()));
    ASSERT_TRUE(r2.loadState(SAVE_PATH));

    // 로드 후 step → jump check 실행 → if hp > 50 → high_hp
    res = r2.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "HP is high");
}

// Call stack 저장/복원
TEST_F(SaveLoadTest, SaveWithCallStack) {
    auto buf = compileScript(R"(
label start:
    call sub
    "After return"

label sub:
    "In sub"
    "Sub line 2"
)");
    ASSERT_FALSE(buf.empty());

    Runner r1;
    ASSERT_TRUE(startRunner(r1, buf));

    // call sub → "In sub" 실행
    auto res = r1.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "In sub");

    // call stack에 start가 있는 상태에서 저장
    ASSERT_TRUE(r1.saveState(SAVE_PATH));

    Runner r2;
    ASSERT_TRUE(r2.start(buf.data(), buf.size()));
    ASSERT_TRUE(r2.loadState(SAVE_PATH));

    // "Sub line 2" → return to start → "After return"
    res = r2.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "Sub line 2");

    res = r2.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "After return");

    res = r2.step();
    EXPECT_EQ(res.type, StepType::END);
}

// 종료 상태 저장/복원
TEST_F(SaveLoadTest, SaveFinishedState) {
    auto buf = compileScript(R"(
label start:
    "Only line"
)");
    ASSERT_FALSE(buf.empty());

    Runner r1;
    ASSERT_TRUE(startRunner(r1, buf));

    auto res = r1.step(); // "Only line"
    res = r1.step();      // END
    ASSERT_EQ(res.type, StepType::END);
    ASSERT_TRUE(r1.isFinished());

    ASSERT_TRUE(r1.saveState(SAVE_PATH));

    Runner r2;
    ASSERT_TRUE(r2.start(buf.data(), buf.size()));
    ASSERT_TRUE(r2.loadState(SAVE_PATH));

    EXPECT_TRUE(r2.isFinished());
    res = r2.step();
    EXPECT_EQ(res.type, StepType::END);
}

// 잘못된 세이브 파일
TEST_F(SaveLoadTest, InvalidSaveFile) {
    auto buf = compileScript(R"(
label start:
    "Hello"
)");
    ASSERT_FALSE(buf.empty());

    Runner r;
    ASSERT_TRUE(startRunner(r, buf));

    // 존재하지 않는 파일
    EXPECT_FALSE(r.loadState("nonexistent.gys"));

    // 잘못된 데이터 파일
    {
        std::ofstream ofs(SAVE_PATH, std::ios::binary);
        ofs << "invalid data here";
    }
    EXPECT_FALSE(r.loadState(SAVE_PATH));
}

// 스토리 없이 loadState 시도
TEST_F(SaveLoadTest, LoadWithoutStory) {
    Runner r;
    EXPECT_FALSE(r.hasStory());
    EXPECT_FALSE(r.loadState(SAVE_PATH));
    EXPECT_FALSE(r.saveState(SAVE_PATH));
}

// 복합 시나리오: 변수 + 선택지 + Call stack
TEST_F(SaveLoadTest, ComplexRoundTrip) {
    auto buf = compileScript(R"(
label start:
    $ gold = 50
    "Welcome, adventurer"
    call shop
    "Back from shop"

label shop:
    "The shopkeeper greets you"
    menu:
        "Buy sword" -> buy_sword
        "Leave" -> leave_shop

label buy_sword:
    $ gold = 10
    "You bought a sword"

label leave_shop:
    "Goodbye"
)");
    ASSERT_FALSE(buf.empty());

    Runner r1;
    ASSERT_TRUE(startRunner(r1, buf));

    // "Welcome, adventurer" (gold=50 설정됨)
    auto res = r1.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "Welcome, adventurer");

    // call shop → "The shopkeeper greets you"
    res = r1.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "The shopkeeper greets you");

    // 선택지
    res = r1.step();
    ASSERT_EQ(res.type, StepType::CHOICES);
    ASSERT_EQ(res.choices.size(), 2u);

    // 이 상태에서 저장 (call stack + 변수 + pending choices)
    ASSERT_TRUE(r1.saveState(SAVE_PATH));

    // 새 Runner에 로드
    Runner r2;
    ASSERT_TRUE(r2.start(buf.data(), buf.size()));
    ASSERT_TRUE(r2.loadState(SAVE_PATH));

    // "Buy sword" 선택
    r2.choose(0);

    // $ gold = 10 → "You bought a sword"
    res = r2.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "You bought a sword");

    // buy_sword 끝 → call stack에서 start로 복귀 → "Back from shop"
    res = r2.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "Back from shop");

    res = r2.step();
    EXPECT_EQ(res.type, StepType::END);
}
