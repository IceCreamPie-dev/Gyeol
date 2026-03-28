#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <random>
#include <set>

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
enum class StepType { LINE, CHOICES, COMMAND, WAIT, YIELD, END };

struct LineData {
    const char* character = nullptr; // nullptr이면 narration
    const char* text = nullptr;
    std::vector<std::pair<const char*, const char*>> tags; // key-value 메타데이터
};

struct ChoiceData {
    const char* text = nullptr;
    int index = 0;
};

enum class CommandArgType { STRING, INT, FLOAT, BOOL, IDENTIFIER };

struct CommandArgData {
    CommandArgType type = CommandArgType::STRING;
    std::string text; // STRING / IDENTIFIER
    int32_t intValue = 0;
    float floatValue = 0.0f;
    bool boolValue = false;
};

struct CommandData {
    const char* type = nullptr;
    std::vector<CommandArgData> args;
};

struct WaitData {
    const char* tag = nullptr; // nullptr이면 태그 없음
};

struct StepResult {
    StepType type = StepType::END;
    LineData line;
    std::vector<ChoiceData> choices;
    CommandData command;
    WaitData wait;
    // 보간된 문자열의 소유권 (const char*가 이 버퍼를 가리킴)
    std::vector<std::string> ownedStrings_;
};

// --- Runner (VM) ---
class Runner {
public:
    struct Snapshot {
        std::vector<uint8_t> bytes;
    };

    struct TraceEvent {
        std::string kind;
        std::string nodeName;
        uint32_t pc = 0;
        std::string detail;
    };

    struct ExecutionMetrics {
        uint64_t stepCalls = 0;
        uint64_t instructionsExecuted = 0;
        uint64_t lineResults = 0;
        uint64_t choiceResults = 0;
        uint64_t commandResults = 0;
        uint64_t endResults = 0;
        uint64_t jumps = 0;
        uint64_t calls = 0;
        uint64_t returns = 0;
        uint64_t conditionsEvaluated = 0;
        uint64_t randomRolls = 0;
        uint64_t choicesMade = 0;
        uint64_t snapshotsCreated = 0;
        uint64_t snapshotsRestored = 0;
        uint64_t saveOperations = 0;
        uint64_t loadOperations = 0;
        uint64_t errors = 0;
        uint64_t traceEvents = 0;
    };

    bool start(const uint8_t* buffer, size_t size);
    bool startAtNode(const uint8_t* buffer, size_t size, const std::string& nodeName);
    StepResult step();
    bool resume();
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
    Snapshot snapshot() const;
    bool restore(const Snapshot& snapshot);

    // RNG seed (deterministic testing)
    void setSeed(uint32_t seed);
    uint32_t getSeed() const;

    // Locale (다국어) API
    bool loadLocale(const std::string& path);
    bool loadLocaleCatalog(const std::string& path);
    bool setLocale(const std::string& localeCode);
    void clearLocale();
    std::string getLocale() const;
    std::string getResolvedLocale() const;

    // Visit tracking API
    int32_t getVisitCount(const std::string& nodeName) const;
    bool hasVisited(const std::string& nodeName) const;

    // Character API (캐릭터 정의 조회)
    std::string getCharacterProperty(const std::string& characterId, const std::string& key) const;
    std::vector<std::string> getCharacterNames() const;
    std::string getCharacterDisplayName(const std::string& characterId) const;

    // Node tag API (노드 메타데이터 태그 조회)
    std::string getNodeTag(const std::string& nodeName, const std::string& key) const;
    std::vector<std::pair<std::string, std::string>> getNodeTags(const std::string& nodeName) const;
    bool hasNodeTag(const std::string& nodeName, const std::string& key) const;

    // --- Graph Data API (비주얼 에디터용) ---
    struct GraphNodeSummary {
        int lineCount = 0;
        int choiceCount = 0;
        bool hasJump = false;
        bool hasCondition = false;
        bool hasRandom = false;
        bool hasCommand = false;
        std::vector<std::string> characters;
        std::string firstLine;
    };

    struct GraphNode {
        std::string name;
        int instructionCount = 0;
        std::vector<std::string> params;
        std::vector<std::pair<std::string, std::string>> tags;
        GraphNodeSummary summary;
    };

    struct GraphEdge {
        std::string from;
        std::string to;
        std::string type;   // "jump", "call", "choice", "condition_true", "condition_false", "random", "call_return"
        std::string label;
    };

    struct GraphData {
        std::string startNode;
        std::vector<GraphNode> nodes;
        std::vector<GraphEdge> edges;
    };

    GraphData getGraphData() const;

    // --- Debug API ---
    struct DebugLocation {
        std::string nodeName;
        uint32_t pc = 0;
        std::string instructionType;  // "Line", "Choice", "Jump", "Command", "SetVar", "Condition", "Random", "Return", "CallWithReturn"
    };

    struct CallFrameInfo {
        std::string nodeName;
        uint32_t pc;
        std::string returnVarName;
        std::vector<std::string> paramNames;
    };

    // Breakpoint management
    void addBreakpoint(const std::string& nodeName, uint32_t pc);
    void removeBreakpoint(const std::string& nodeName, uint32_t pc);
    void clearBreakpoints();
    bool hasBreakpoint(const std::string& nodeName, uint32_t pc) const;
    std::vector<std::pair<std::string, uint32_t>> getBreakpoints() const;

    // Step control
    void setStepMode(bool enabled);  // When true, step() pauses after each instruction
    bool isStepMode() const;

    // Debug info
    DebugLocation getLocation() const;  // Current node + PC + instruction type
    std::vector<CallFrameInfo> getCallStack() const;  // Human-readable call stack
    std::string getCurrentNodeName() const;  // Convenience wrapper
    uint32_t getCurrentPC() const;
    const std::string& getLastError() const;
    void clearLastError();
    const ExecutionMetrics& getMetrics() const;
    void resetMetrics();
    void setTraceEnabled(bool enabled, size_t maxEvents = 256);
    bool isTraceEnabled() const;
    const std::vector<TraceEvent>& getTrace() const;
    void clearTrace();

    // Node inspection
    std::vector<std::string> getNodeNames() const;  // List all node names in story
    uint32_t getNodeInstructionCount(const std::string& nodeName) const;  // # of instructions in a node
    std::string getInstructionInfo(const std::string& nodeName, uint32_t pc) const;  // Human-readable instruction description

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
        int8_t choice_modifier = 0; // 0=Default, 1=Once, 2=Sticky, 3=Fallback
        std::string once_key;       // "nodeName:pc" key for once tracking
    };
    std::vector<PendingChoice> pendingChoices_;

    // Once 선택지 추적 (한번 선택 후 재표시 안 됨)
    std::unordered_set<std::string> chosenOnceChoices_;

    // Pending return value (set by explicit 'return expr', consumed after call stack pop)
    bool hasPendingReturn_ = false;
    Variant pendingReturnValue_;

    // WAIT 상태
    bool waitBlocked_ = false;
    std::string waitTag_;

    // RNG for random branches
    std::mt19937 rng_;

    // Locale 오버레이 (다국어)
    std::string currentLocale_;   // requested locale (or loaded single-locale id)
    std::string resolvedLocale_;  // exact/base/default resolved locale
    std::vector<std::string> localePool_; // string_pool과 병렬, 비어있으면 원본 사용
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> localeCharacterProps_;
    bool hasLocaleCatalog_ = false;
    std::string catalogDefaultLocale_;
    std::unordered_map<std::string, std::unordered_map<int32_t, std::string>> catalogLineEntriesByLocale_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, std::string>>> catalogCharacterEntriesByLocale_;

    // 노드 방문 횟수
    std::unordered_map<std::string, uint32_t> visitCounts_;

    // 캐릭터 정의 캐시: characterId → [(key, value), ...]
    std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> characterProps_;

    // 노드 메타데이터 태그 캐시: nodeName → [(key, value), ...]
    std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> nodeTags_;

    // Debug state
    std::set<std::pair<std::string, uint32_t>> breakpoints_;
    bool stepMode_ = false;
    bool hitBreakpoint_ = false;
    bool hasExplicitSeed_ = false;
    uint32_t currentSeed_ = 0;
    mutable std::string lastError_;
    mutable ExecutionMetrics metrics_;
    bool traceEnabled_ = false;
    size_t traceLimit_ = 256;
    mutable std::vector<TraceEvent> trace_;

    // 헬퍼
    const char* poolStr(int32_t index) const;
    void jumpToNode(const char* name);
    void jumpToNodeById(int32_t nameId);
    void setError(const std::string& message) const;
    void clearErrorInternal() const;
    void recordTrace(const std::string& kind, const std::string& detail = "") const;
    void recordTrace(const std::string& kind, const std::string& nodeName, uint32_t pc, const std::string& detail) const;
    void seedRngForStart();
    std::string exportRngState() const;
    void importRngState(const std::string& state);
    std::vector<uint8_t> serializeStateBuffer() const;
    bool deserializeStateBuffer(const uint8_t* data, size_t size);

    // 표현식 평가 (RPN 스택 머신)
    Variant evaluateExpression(const void* exprPtr) const;

    // 문자열 보간 ({변수명} → 값 치환, {if cond}...{else}...{endif} 지원)
    std::string interpolateText(const char* text, int depth = 0) const;
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
    std::string baseLocaleCode(const std::string& localeCode) const;
    bool applyLocaleSelection(const std::string& requestedLocale, bool recordTraceEvent);
};

} // namespace Gyeol
