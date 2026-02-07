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

// CallWithReturn 프레임의 return_var_name 저장/복원
TEST_F(SaveLoadTest, SaveWithCallReturnFrame) {
    auto buf = compileScript(R"(
label start:
    $ result = call helper
    narrator "{result}"

label helper:
    narrator "In helper"
    return 42
)");
    ASSERT_FALSE(buf.empty());

    Runner r1;
    ASSERT_TRUE(startRunner(r1, buf));

    // step → helper의 "In helper" (call stack에 returnVarName="result")
    auto res = r1.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "In helper");

    // 이 상태에서 저장 (call stack에 return_var_name 포함)
    ASSERT_TRUE(r1.saveState(SAVE_PATH));

    // 새 Runner에 로드
    Runner r2;
    ASSERT_TRUE(r2.start(buf.data(), buf.size()));
    ASSERT_TRUE(r2.loadState(SAVE_PATH));

    // step → return 42 실행 → call stack pop → result = 42 → narrator "{result}"
    res = r2.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "42");

    EXPECT_EQ(r2.getVariable("result").type, Variant::INT);
    EXPECT_EQ(r2.getVariable("result").i, 42);

    res = r2.step();
    EXPECT_EQ(res.type, StepType::END);
}

// 이전 포맷 호환 (return_var_name 없는 SavedCallFrame)
TEST_F(SaveLoadTest, SaveLoadReturnBackwardCompat) {
    // 기존 call (return capture 없음)으로 저장 → 로드해도 정상 동작
    auto buf = compileScript(R"(
label start:
    call helper
    narrator "back"

label helper:
    narrator "in helper"
)");
    ASSERT_FALSE(buf.empty());

    Runner r1;
    ASSERT_TRUE(startRunner(r1, buf));

    // "in helper"
    auto res = r1.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "in helper");

    // 저장 (return_var_name은 빈 문자열)
    ASSERT_TRUE(r1.saveState(SAVE_PATH));

    // 로드 후 계속
    Runner r2;
    ASSERT_TRUE(r2.start(buf.data(), buf.size()));
    ASSERT_TRUE(r2.loadState(SAVE_PATH));

    // helper 끝 → call stack pop → "back"
    res = r2.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "back");

    res = r2.step();
    EXPECT_EQ(res.type, StepType::END);
}

// ===================================================================
// Function Parameters Save/Load 테스트
// ===================================================================

TEST_F(SaveLoadTest, SaveWithParamFrame) {
    auto buf = compileScript(R"(
label start:
    $ x = 100
    $ result = call calc(42)
    narrator "{result} {x}"

label calc(x):
    narrator "computing {x}"
    return x * 2
)");
    ASSERT_FALSE(buf.empty());

    Runner r1;
    ASSERT_TRUE(startRunner(r1, buf));

    // step → "computing 42" (calc 안에서, x=42, 섀도된 x=100)
    auto res = r1.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "computing 42");

    // 이 상태에서 저장 (call stack에 shadowedVars 포함)
    ASSERT_TRUE(r1.saveState(SAVE_PATH));

    // 새 Runner에 로드
    Runner r2;
    ASSERT_TRUE(r2.start(buf.data(), buf.size()));
    ASSERT_TRUE(r2.loadState(SAVE_PATH));

    // step → return x*2=84 → call stack pop → x 복원(100) → result=84
    // → narrator "{result} {x}" → "84 100"
    res = r2.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "84 100");

    EXPECT_EQ(r2.getVariable("result").i, 84);
    EXPECT_EQ(r2.getVariable("x").i, 100);

    res = r2.step();
    EXPECT_EQ(res.type, StepType::END);
}

TEST_F(SaveLoadTest, SaveLoadParamBackwardCompat) {
    // 매개변수 없는 기존 call로 저장 → 로드 정상 동작
    auto buf = compileScript(R"(
label start:
    call sub
    narrator "back"

label sub:
    narrator "in sub"
)");
    ASSERT_FALSE(buf.empty());

    Runner r1;
    ASSERT_TRUE(startRunner(r1, buf));

    auto res = r1.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "in sub");

    ASSERT_TRUE(r1.saveState(SAVE_PATH));

    Runner r2;
    ASSERT_TRUE(r2.start(buf.data(), buf.size()));
    ASSERT_TRUE(r2.loadState(SAVE_PATH));

    res = r2.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "back");

    res = r2.step();
    EXPECT_EQ(res.type, StepType::END);
}

// visit_count 저장/복원 라운드트립
TEST_F(SaveLoadTest, SaveLoadVisitCounts) {
    auto buf = compileScript(R"(
label start:
    jump shop

label shop:
    jump shop2

label shop2:
    jump shop3

label shop3:
    narrator "{visit_count(shop)}"
)");
    ASSERT_FALSE(buf.empty());

    Runner r1;
    ASSERT_TRUE(startRunner(r1, buf));

    // start→shop→shop2→shop3 순서로 진행
    // visit_count: start=1, shop=1, shop2=1, shop3=1
    // shop3에서 "{visit_count(shop)}" 출력 → "1"
    auto res = r1.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "1");

    // 저장
    EXPECT_EQ(r1.getVisitCount("shop"), 1);
    EXPECT_EQ(r1.getVisitCount("start"), 1);
    ASSERT_TRUE(r1.saveState(SAVE_PATH));

    // 새 Runner에 로드
    Runner r2;
    ASSERT_TRUE(r2.start(buf.data(), buf.size()));
    ASSERT_TRUE(r2.loadState(SAVE_PATH));

    // 방문 횟수 복원 확인
    EXPECT_EQ(r2.getVisitCount("shop"), 1);
    EXPECT_EQ(r2.getVisitCount("start"), 1);
    EXPECT_EQ(r2.getVisitCount("shop2"), 1);
    EXPECT_EQ(r2.getVisitCount("shop3"), 1);
    EXPECT_TRUE(r2.hasVisited("shop"));

    // 미방문 노드
    EXPECT_EQ(r2.getVisitCount("nonexistent"), 0);
    EXPECT_FALSE(r2.hasVisited("nonexistent"));

    res = r2.step();
    EXPECT_EQ(res.type, StepType::END);
}

// visit_counts 필드 없는 기존 .gys 하위 호환
TEST_F(SaveLoadTest, SaveLoadVisitCountBackwardCompat) {
    // visit_count 기능 없는 단순 스크립트로 저장 → 로드 시 visitCounts_ 비어있지만 정상 동작
    auto buf = compileScript(R"(
label start:
    narrator "hello"
    narrator "world"
)");
    ASSERT_FALSE(buf.empty());

    Runner r1;
    ASSERT_TRUE(startRunner(r1, buf));

    auto res = r1.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "hello");

    ASSERT_TRUE(r1.saveState(SAVE_PATH));

    // 로드 후 정상 진행
    Runner r2;
    ASSERT_TRUE(r2.start(buf.data(), buf.size()));
    ASSERT_TRUE(r2.loadState(SAVE_PATH));

    // start 노드는 방문 카운트 복원됨 (저장 시점에 start=1)
    EXPECT_EQ(r2.getVisitCount("start"), 1);

    res = r2.step();
    ASSERT_EQ(res.type, StepType::LINE);
    EXPECT_STREQ(res.line.text, "world");

    res = r2.step();
    EXPECT_EQ(res.type, StepType::END);
}

// ==========================================================================
// List Save/Load 테스트
// ==========================================================================

TEST_F(SaveLoadTest, SaveLoadListVariable) {
    auto buf = compileScript(R"(
label start:
    $ items = ["sword", "shield"]
    $ items += "potion"
    narrator "checkpoint"
    narrator "after"
)");
    ASSERT_FALSE(buf.empty());

    Runner r1;
    ASSERT_TRUE(startRunner(r1, buf));
    auto res = r1.step();
    EXPECT_STREQ(res.line.text, "checkpoint");

    ASSERT_TRUE(r1.saveState(SAVE_PATH));

    Runner r2;
    ASSERT_TRUE(r2.start(buf.data(), buf.size()));
    ASSERT_TRUE(r2.loadState(SAVE_PATH));

    auto v = r2.getVariable("items");
    EXPECT_EQ(v.type, Variant::LIST);
    ASSERT_EQ(v.list.size(), 3u);
    EXPECT_EQ(v.list[0], "sword");
    EXPECT_EQ(v.list[1], "shield");
    EXPECT_EQ(v.list[2], "potion");

    res = r2.step();
    EXPECT_STREQ(res.line.text, "after");
}

TEST_F(SaveLoadTest, SaveLoadEmptyList) {
    auto buf = compileScript(R"(
label start:
    $ items = []
    narrator "checkpoint"
    narrator "after"
)");
    ASSERT_FALSE(buf.empty());

    Runner r1;
    ASSERT_TRUE(startRunner(r1, buf));
    auto res = r1.step();
    EXPECT_STREQ(res.line.text, "checkpoint");

    ASSERT_TRUE(r1.saveState(SAVE_PATH));

    Runner r2;
    ASSERT_TRUE(r2.start(buf.data(), buf.size()));
    ASSERT_TRUE(r2.loadState(SAVE_PATH));

    auto v = r2.getVariable("items");
    EXPECT_EQ(v.type, Variant::LIST);
    EXPECT_EQ(v.list.size(), 0u);
}

TEST_F(SaveLoadTest, SaveLoadListAfterModification) {
    // append/remove 후 저장 → 로드 시 수정된 상태 유지
    auto buf = compileScript(R"(
label start:
    $ inv = ["sword", "shield", "potion"]
    $ inv -= "shield"
    $ inv += "bow"
    narrator "checkpoint"
    narrator "after"
)");
    ASSERT_FALSE(buf.empty());

    Runner r1;
    ASSERT_TRUE(startRunner(r1, buf));
    auto res = r1.step();
    EXPECT_STREQ(res.line.text, "checkpoint");

    ASSERT_TRUE(r1.saveState(SAVE_PATH));

    Runner r2;
    ASSERT_TRUE(r2.start(buf.data(), buf.size()));
    ASSERT_TRUE(r2.loadState(SAVE_PATH));

    auto v = r2.getVariable("inv");
    EXPECT_EQ(v.type, Variant::LIST);
    ASSERT_EQ(v.list.size(), 3u);
    EXPECT_EQ(v.list[0], "sword");
    EXPECT_EQ(v.list[1], "potion");
    EXPECT_EQ(v.list[2], "bow");
}
