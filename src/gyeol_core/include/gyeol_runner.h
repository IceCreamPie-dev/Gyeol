#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>

namespace Gyeol {

// --- 변수 값 타입 ---
struct Variant {
    enum Type { BOOL, INT, FLOAT, STRING };
    Type type = INT;
    union {
        bool b;
        int32_t i;
        float f;
    };
    std::string s;

    static Variant Bool(bool v) { Variant r; r.type = BOOL; r.b = v; return r; }
    static Variant Int(int32_t v) { Variant r; r.type = INT; r.i = v; return r; }
    static Variant Float(float v) { Variant r; r.type = FLOAT; r.f = v; return r; }
    static Variant String(const std::string& v) { Variant r; r.type = STRING; r.s = v; return r; }
};

// --- step() 결과 ---
enum class StepType { LINE, CHOICES, COMMAND, END };

struct LineData {
    const char* character = nullptr; // nullptr이면 narration
    const char* text = nullptr;
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
    struct CallFrame {
        const void* node;
        uint32_t pc;
    };
    std::vector<CallFrame> callStack_;

    // 대기 중인 선택지
    struct PendingChoice {
        int32_t text_id;
        int32_t target_node_name_id;
    };
    std::vector<PendingChoice> pendingChoices_;

    // 헬퍼
    const char* poolStr(int32_t index) const;
    void jumpToNode(const char* name);
    void jumpToNodeById(int32_t nameId);

    // Save/Load 헬퍼
    std::string currentNodeName() const;
    std::string nodeNameFromPtr(const void* nodePtr) const;
    const void* findNodeByName(const char* name) const;
    int32_t findStringInPool(const char* str) const;
};

} // namespace Gyeol
