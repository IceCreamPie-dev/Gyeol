#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <random>

namespace Gyeol {

// --- 변수 값 타입 ---
struct Variant {
    enum Type { BOOL, INT, FLOAT, STRING, LIST };
    Type type = INT;
    union {
        bool b;
        int32_t i;
        float f;
    };
    std::string s;
    std::vector<std::string> list; // LIST 타입 항목들

    static Variant Bool(bool v) { Variant r; r.type = BOOL; r.b = v; return r; }
    static Variant Int(int32_t v) { Variant r; r.type = INT; r.i = v; return r; }
    static Variant Float(float v) { Variant r; r.type = FLOAT; r.f = v; return r; }
    static Variant String(const std::string& v) { Variant r; r.type = STRING; r.s = v; return r; }
    static Variant List(const std::vector<std::string>& v) { Variant r; r.type = LIST; r.list = v; return r; }
    static Variant List(std::vector<std::string>&& v) { Variant r; r.type = LIST; r.list = std::move(v); return r; }
};

// --- step() 결과 ---
enum class StepType { LINE, CHOICES, COMMAND, END };

struct LineData {
    const char* character = nullptr; // nullptr이면 narration
    const char* text = nullptr;
    std::vector<std::pair<const char*, const char*>> tags; // key-value 메타데이터
};

struct ChoiceData {
    const char* text = nullptr;
    int index = 0;
};

struct CommandData {
    const char* type = nullptr;
    std::vector<const char*> params;
};

struct StepResult {
    StepType type = StepType::END;
    LineData line;
    std::vector<ChoiceData> choices;
    CommandData command;
    // 보간된 문자열의 소유권 (const char*가 이 버퍼를 가리킴)
    std::vector<std::string> ownedStrings_;
};

// --- Runner (VM) ---
class Runner {
public:
    bool start(const uint8_t* buffer, size_t size);
    StepResult step();
    void choose(int index);
    bool isFinished() const;

    // Variable access API
    Variant getVariable(const std::string& name) const;
    void setVariable(const std::string& name, const Variant& value);
    bool hasVariable(const std::string& name) const;
    std::vector<std::string> getVariableNames() const;

    // Save/Load API
    bool saveState(const std::string& filepath) const;
    bool loadState(const std::string& filepath);
    bool hasStory() const { return story_ != nullptr; }

    // RNG seed (deterministic testing)
    void setSeed(uint32_t seed);

    // Locale (다국어) API
    bool loadLocale(const std::string& csvPath);
    void clearLocale();
    std::string getLocale() const;

    // Visit tracking API
    int32_t getVisitCount(const std::string& nodeName) const;
    bool hasVisited(const std::string& nodeName) const;

private:
    // forward declarations — 실제 FlatBuffers 타입은 cpp에서 사용
    const void* story_ = nullptr;
    const void* currentNode_ = nullptr;
    const void* pool_ = nullptr;
    uint32_t pc_ = 0;
    bool finished_ = true;

    // 변수 상태
    std::unordered_map<std::string, Variant> variables_;

    // Call stack
    struct ShadowedVar {
        std::string name;
        Variant value;
        bool existed; // true = 변수가 기존에 존재했음
    };

    struct CallFrame {
        const void* node;
        uint32_t pc;
        std::string returnVarName; // empty = 반환값 무시
        std::vector<ShadowedVar> shadowedVars; // 함수 매개변수로 섀도된 변수들
        std::vector<std::string> paramNames;   // 매개변수 이름들
    };
    std::vector<CallFrame> callStack_;

    // 대기 중인 선택지
    struct PendingChoice {
        int32_t text_id;
        int32_t target_node_name_id;
    };
    std::vector<PendingChoice> pendingChoices_;

    // Pending return value (set by explicit 'return expr', consumed after call stack pop)
    bool hasPendingReturn_ = false;
    Variant pendingReturnValue_;

    // RNG for random branches
    std::mt19937 rng_;

    // Locale 오버레이 (다국어)
    std::string currentLocale_;
    std::vector<std::string> localePool_; // string_pool과 병렬, 비어있으면 원본 사용

    // 노드 방문 횟수
    std::unordered_map<std::string, uint32_t> visitCounts_;

    // 헬퍼
    const char* poolStr(int32_t index) const;
    void jumpToNode(const char* name);
    void jumpToNodeById(int32_t nameId);

    // 표현식 평가 (RPN 스택 머신)
    Variant evaluateExpression(const void* exprPtr) const;

    // 문자열 보간 ({변수명} → 값 치환, {if cond}...{else}...{endif} 지원)
    std::string interpolateText(const char* text) const;
    static std::string variantToString(const Variant& v);

    // 인라인 조건 평가 ({if cond}...{else}...{endif})
    bool evaluateInlineCondition(const std::string& condStr) const;

    // 함수 매개변수 바인딩/복원 헬퍼
    void bindParameters(const void* targetNode, const std::vector<Variant>& argValues, CallFrame& frame);
    void restoreShadowedVars(const CallFrame& frame);

    // Save/Load 헬퍼
    std::string currentNodeName() const;
    std::string nodeNameFromPtr(const void* nodePtr) const;
    const void* findNodeByName(const char* name) const;
    int32_t findStringInPool(const char* str) const;
};

} // namespace Gyeol
