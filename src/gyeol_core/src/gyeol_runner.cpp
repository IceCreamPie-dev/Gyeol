#include "gyeol_runner.h"
#include "gyeol_generated.h"
#include <iostream>
#include <fstream>
#include <cstring>

using namespace ICPDev::Gyeol::Schema;

namespace Gyeol {

// --- 타입 캐스팅 헬퍼 (void* → FlatBuffers 타입) ---
static const Story* asStory(const void* p) { return static_cast<const Story*>(p); }
static const Node* asNode(const void* p) { return static_cast<const Node*>(p); }

static const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>*
asPool(const void* p) {
    return static_cast<const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>*>(p);
}

// --- poolStr ---
const char* Runner::poolStr(int32_t index) const {
    auto* pool = asPool(pool_);
    if (!pool || index < 0 || index >= static_cast<int32_t>(pool->size())) {
        return "";
    }
    return pool->Get(static_cast<flatbuffers::uoffset_t>(index))->c_str();
}

// --- 노드 검색 및 이동 ---
void Runner::jumpToNode(const char* name) {
    auto* story = asStory(story_);
    auto* nodes = story->nodes();
    if (!nodes) {
        finished_ = true;
        return;
    }

    for (flatbuffers::uoffset_t i = 0; i < nodes->size(); ++i) {
        auto* node = nodes->Get(i);
        if (node->name() && std::strcmp(node->name()->c_str(), name) == 0) {
            currentNode_ = node;
            pc_ = 0;
            return;
        }
    }

    std::cerr << "[Gyeol] Node not found: " << name << std::endl;
    finished_ = true;
}

void Runner::jumpToNodeById(int32_t nameId) {
    jumpToNode(poolStr(nameId));
}

// --- Variant로부터 ValueData 읽기 헬퍼 ---
static Variant readValueData(
    const void* valuePtr, ValueData valueType,
    const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>* pool)
{
    switch (valueType) {
        case ValueData::BoolValue:
            return Variant::Bool(static_cast<const BoolValue*>(valuePtr)->val());
        case ValueData::IntValue:
            return Variant::Int(static_cast<const IntValue*>(valuePtr)->val());
        case ValueData::FloatValue:
            return Variant::Float(static_cast<const FloatValue*>(valuePtr)->val());
        case ValueData::StringRef: {
            int32_t idx = static_cast<const StringRef*>(valuePtr)->index();
            if (pool && idx >= 0 && idx < static_cast<int32_t>(pool->size())) {
                return Variant::String(pool->Get(static_cast<flatbuffers::uoffset_t>(idx))->c_str());
            }
            return Variant::String("");
        }
        default:
            return Variant::Int(0);
    }
}

// --- 조건 비교 ---
static bool compareVariants(const Variant& lhs, Operator op, const Variant& rhs) {
    // 타입이 다르면 INT로 비교 시도
    if (lhs.type == Variant::BOOL || rhs.type == Variant::BOOL) {
        bool a = (lhs.type == Variant::BOOL) ? lhs.b : (lhs.i != 0);
        bool b = (rhs.type == Variant::BOOL) ? rhs.b : (rhs.i != 0);
        switch (op) {
            case Operator::Equal:          return a == b;
            case Operator::NotEqual:       return a != b;
            default:                       return false;
        }
    }

    if (lhs.type == Variant::STRING || rhs.type == Variant::STRING) {
        switch (op) {
            case Operator::Equal:          return lhs.s == rhs.s;
            case Operator::NotEqual:       return lhs.s != rhs.s;
            default:                       return false;
        }
    }

    if (lhs.type == Variant::FLOAT || rhs.type == Variant::FLOAT) {
        float a = (lhs.type == Variant::FLOAT) ? lhs.f : static_cast<float>(lhs.i);
        float b = (rhs.type == Variant::FLOAT) ? rhs.f : static_cast<float>(rhs.i);
        switch (op) {
            case Operator::Equal:          return a == b;
            case Operator::NotEqual:       return a != b;
            case Operator::Greater:        return a > b;
            case Operator::Less:           return a < b;
            case Operator::GreaterOrEqual: return a >= b;
            case Operator::LessOrEqual:    return a <= b;
        }
    }

    // INT 비교
    int32_t a = lhs.i;
    int32_t b = rhs.i;
    switch (op) {
        case Operator::Equal:          return a == b;
        case Operator::NotEqual:       return a != b;
        case Operator::Greater:        return a > b;
        case Operator::Less:           return a < b;
        case Operator::GreaterOrEqual: return a >= b;
        case Operator::LessOrEqual:    return a <= b;
    }
    return false;
}

// --- start ---
bool Runner::start(const uint8_t* buffer, size_t size) {
    flatbuffers::Verifier verifier(buffer, size);
    if (!VerifyStoryBuffer(verifier)) {
        std::cerr << "[Gyeol] Invalid buffer" << std::endl;
        return false;
    }

    story_ = GetStory(buffer);
    auto* story = asStory(story_);
    pool_ = story->string_pool();

    // global_vars 초기화
    variables_.clear();
    auto* globalVars = story->global_vars();
    if (globalVars) {
        auto* pool = asPool(pool_);
        for (flatbuffers::uoffset_t i = 0; i < globalVars->size(); ++i) {
            auto* sv = globalVars->Get(i);
            std::string varName = poolStr(sv->var_name_id());
            if (sv->value() && sv->value_type() != ValueData::NONE) {
                variables_[varName] = readValueData(sv->value(), sv->value_type(), pool);
            }
        }
    }

    // start_node로 이동
    callStack_.clear();
    pendingChoices_.clear();
    finished_ = false;

    if (story->start_node_name()) {
        jumpToNode(story->start_node_name()->c_str());
    } else {
        finished_ = true;
        return false;
    }

    return !finished_;
}

// --- step ---
StepResult Runner::step() {
    StepResult result;
    result.type = StepType::END;

    if (finished_) return result;

    auto* node = asNode(currentNode_);
    auto* pool = asPool(pool_);

    while (true) {
        // 노드 끝 도달
        if (!node || !node->lines() || pc_ >= node->lines()->size()) {
            // call stack에서 복귀
            if (!callStack_.empty()) {
                auto frame = callStack_.back();
                callStack_.pop_back();
                currentNode_ = frame.node;
                pc_ = frame.pc;
                node = asNode(currentNode_);
                continue;
            }
            // 스토리 종료
            finished_ = true;
            result.type = StepType::END;
            return result;
        }

        auto* instr = node->lines()->Get(pc_);
        pc_++;

        switch (instr->data_type()) {
            case OpData::Line: {
                auto* line = instr->data_as_Line();
                result.type = StepType::LINE;
                result.line.character = (line->character_id() >= 0)
                    ? poolStr(line->character_id()) : nullptr;
                result.line.text = poolStr(line->text_id());
                return result;
            }

            case OpData::Choice: {
                // Choice를 연속으로 수집
                auto* choice = instr->data_as_Choice();
                pendingChoices_.clear();

                // condition_var_id 체크 (조건부 선택지)
                bool visible = true;
                if (choice->condition_var_id() >= 0) {
                    std::string condVar = poolStr(choice->condition_var_id());
                    auto it = variables_.find(condVar);
                    if (it != variables_.end()) {
                        visible = (it->second.type == Variant::BOOL) ? it->second.b : (it->second.i != 0);
                    } else {
                        visible = false;
                    }
                }
                if (visible) {
                    pendingChoices_.push_back({choice->text_id(), choice->target_node_name_id()});
                }

                // 다음 instruction도 Choice면 계속 수집
                while (node->lines() && pc_ < node->lines()->size()) {
                    auto* next = node->lines()->Get(pc_);
                    if (next->data_type() != OpData::Choice) break;
                    pc_++;

                    auto* nextChoice = next->data_as_Choice();
                    bool nextVisible = true;
                    if (nextChoice->condition_var_id() >= 0) {
                        std::string condVar = poolStr(nextChoice->condition_var_id());
                        auto it = variables_.find(condVar);
                        if (it != variables_.end()) {
                            nextVisible = (it->second.type == Variant::BOOL) ? it->second.b : (it->second.i != 0);
                        } else {
                            nextVisible = false;
                        }
                    }
                    if (nextVisible) {
                        pendingChoices_.push_back({nextChoice->text_id(), nextChoice->target_node_name_id()});
                    }
                }

                // 결과 반환
                result.type = StepType::CHOICES;
                for (int k = 0; k < static_cast<int>(pendingChoices_.size()); ++k) {
                    ChoiceData cd;
                    cd.text = poolStr(pendingChoices_[k].text_id);
                    cd.index = k;
                    result.choices.push_back(cd);
                }
                return result;
            }

            case OpData::Jump: {
                auto* jump = instr->data_as_Jump();
                if (jump->is_call()) {
                    callStack_.push_back({currentNode_, pc_});
                }
                jumpToNodeById(jump->target_node_name_id());
                node = asNode(currentNode_);
                if (finished_) {
                    result.type = StepType::END;
                    return result;
                }
                continue; // 다음 instruction 계속
            }

            case OpData::SetVar: {
                auto* setvar = instr->data_as_SetVar();
                std::string varName = poolStr(setvar->var_name_id());
                if (setvar->value() && setvar->value_type() != ValueData::NONE) {
                    variables_[varName] = readValueData(setvar->value(), setvar->value_type(), pool);
                }
                continue;
            }

            case OpData::Condition: {
                auto* cond = instr->data_as_Condition();
                std::string varName = poolStr(cond->var_name_id());

                Variant lhs = Variant::Int(0);
                auto it = variables_.find(varName);
                if (it != variables_.end()) {
                    lhs = it->second;
                }

                Variant rhs = Variant::Int(0);
                if (cond->compare_value() && cond->compare_value_type() != ValueData::NONE) {
                    rhs = readValueData(cond->compare_value(), cond->compare_value_type(), pool);
                }

                bool condResult = compareVariants(lhs, cond->op(), rhs);

                int32_t targetId = condResult ? cond->true_jump_node_id() : cond->false_jump_node_id();
                if (targetId >= 0) {
                    jumpToNodeById(targetId);
                    node = asNode(currentNode_);
                    if (finished_) {
                        result.type = StepType::END;
                        return result;
                    }
                }
                // targetId < 0이면 다음 줄로 계속
                continue;
            }

            case OpData::Command: {
                auto* cmd = instr->data_as_Command();
                result.type = StepType::COMMAND;
                result.command.type = poolStr(cmd->type_id());
                result.command.params.clear();
                auto* params = cmd->params();
                if (params) {
                    for (flatbuffers::uoffset_t k = 0; k < params->size(); ++k) {
                        result.command.params.push_back(poolStr(params->Get(k)));
                    }
                }
                return result;
            }

            default:
                continue;
        }
    }
}

// --- choose ---
void Runner::choose(int index) {
    if (index < 0 || index >= static_cast<int>(pendingChoices_.size())) {
        std::cerr << "[Gyeol] Invalid choice index: " << index << std::endl;
        return;
    }

    jumpToNodeById(pendingChoices_[index].target_node_name_id);
    pendingChoices_.clear();
}

// --- isFinished ---
bool Runner::isFinished() const {
    return finished_;
}

// --- Variable access API ---
Variant Runner::getVariable(const std::string& name) const {
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        return it->second;
    }
    return Variant::Int(0);
}

void Runner::setVariable(const std::string& name, const Variant& value) {
    variables_[name] = value;
}

bool Runner::hasVariable(const std::string& name) const {
    return variables_.find(name) != variables_.end();
}

std::vector<std::string> Runner::getVariableNames() const {
    std::vector<std::string> names;
    names.reserve(variables_.size());
    for (const auto& pair : variables_) {
        names.push_back(pair.first);
    }
    return names;
}

// --- Save/Load 헬퍼 ---
std::string Runner::nodeNameFromPtr(const void* nodePtr) const {
    auto* node = asNode(nodePtr);
    if (node && node->name()) {
        return node->name()->c_str();
    }
    return "";
}

std::string Runner::currentNodeName() const {
    return nodeNameFromPtr(currentNode_);
}

const void* Runner::findNodeByName(const char* name) const {
    auto* story = asStory(story_);
    if (!story || !story->nodes()) return nullptr;
    auto* nodes = story->nodes();
    for (flatbuffers::uoffset_t i = 0; i < nodes->size(); ++i) {
        auto* node = nodes->Get(i);
        if (node->name() && std::strcmp(node->name()->c_str(), name) == 0) {
            return node;
        }
    }
    return nullptr;
}

int32_t Runner::findStringInPool(const char* str) const {
    auto* pool = asPool(pool_);
    if (!pool) return -1;
    for (flatbuffers::uoffset_t i = 0; i < pool->size(); ++i) {
        if (std::strcmp(pool->Get(i)->c_str(), str) == 0) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

// --- saveState ---
bool Runner::saveState(const std::string& filepath) const {
    if (!story_) {
        std::cerr << "[Gyeol] No story loaded" << std::endl;
        return false;
    }

    auto* story = asStory(story_);

    // SaveStateT 객체 생성
    SaveStateT state;
    state.version = "1.0";
    state.story_version = story->version() ? story->version()->c_str() : "";
    state.current_node_name = currentNodeName();
    state.pc = pc_;
    state.finished = finished_;

    // 변수 저장
    for (auto& pair : variables_) {
        auto sv = std::make_unique<SavedVarT>();
        sv->name = pair.first;
        switch (pair.second.type) {
            case Variant::BOOL: {
                auto bv = std::make_unique<BoolValueT>();
                bv->val = pair.second.b;
                sv->value.Set(std::move(*bv));
                break;
            }
            case Variant::INT: {
                auto iv = std::make_unique<IntValueT>();
                iv->val = pair.second.i;
                sv->value.Set(std::move(*iv));
                break;
            }
            case Variant::FLOAT: {
                auto fv = std::make_unique<FloatValueT>();
                fv->val = pair.second.f;
                sv->value.Set(std::move(*fv));
                break;
            }
            case Variant::STRING: {
                // STRING은 string_value 필드에 직접 저장 (pool index 불필요)
                sv->string_value = pair.second.s;
                // value union은 비워둠 (타입 구분용으로 StringRef 사용)
                auto sr = std::make_unique<StringRefT>();
                sr->index = -1; // 의미 없음, 타입 마커용
                sv->value.Set(std::move(*sr));
                break;
            }
        }
        state.variables.push_back(std::move(sv));
    }

    // Call stack 저장
    for (auto& frame : callStack_) {
        auto cf = std::make_unique<SavedCallFrameT>();
        cf->node_name = nodeNameFromPtr(frame.node);
        cf->pc = frame.pc;
        state.call_stack.push_back(std::move(cf));
    }

    // Pending choices 저장
    for (auto& pc : pendingChoices_) {
        auto spc = std::make_unique<SavedPendingChoiceT>();
        spc->text = poolStr(pc.text_id);
        spc->target_node_name = poolStr(pc.target_node_name_id);
        state.pending_choices.push_back(std::move(spc));
    }

    // FlatBuffers 직렬화
    flatbuffers::FlatBufferBuilder fbb;
    auto offset = SaveState::Pack(fbb, &state);
    fbb.Finish(offset);

    // 파일 쓰기
    std::ofstream ofs(filepath, std::ios::binary);
    if (!ofs) {
        std::cerr << "[Gyeol] Cannot open save file: " << filepath << std::endl;
        return false;
    }
    ofs.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()), fbb.GetSize());
    return ofs.good();
}

// --- loadState ---
bool Runner::loadState(const std::string& filepath) {
    if (!story_) {
        std::cerr << "[Gyeol] No story loaded" << std::endl;
        return false;
    }

    // 파일 읽기
    std::ifstream ifs(filepath, std::ios::binary | std::ios::ate);
    if (!ifs) {
        std::cerr << "[Gyeol] Cannot open save file: " << filepath << std::endl;
        return false;
    }

    auto size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    if (!ifs.read(reinterpret_cast<char*>(buf.data()), size)) {
        std::cerr << "[Gyeol] Failed to read save file" << std::endl;
        return false;
    }

    // 검증
    flatbuffers::Verifier verifier(buf.data(), buf.size());
    auto* saveState = flatbuffers::GetRoot<SaveState>(buf.data());
    if (!saveState->Verify(verifier)) {
        std::cerr << "[Gyeol] Invalid save file" << std::endl;
        return false;
    }

    // 상태 복원
    finished_ = saveState->finished();
    pc_ = saveState->pc();

    // 현재 노드 복원
    if (saveState->current_node_name()) {
        currentNode_ = findNodeByName(saveState->current_node_name()->c_str());
        if (!currentNode_ && !finished_) {
            std::cerr << "[Gyeol] Save state node not found: "
                      << saveState->current_node_name()->c_str() << std::endl;
            finished_ = true;
            return false;
        }
    } else {
        currentNode_ = nullptr;
    }

    // 변수 복원
    variables_.clear();
    auto* vars = saveState->variables();
    if (vars) {
        auto* pool = asPool(pool_);
        for (flatbuffers::uoffset_t i = 0; i < vars->size(); ++i) {
            auto* sv = vars->Get(i);
            if (!sv->name()) continue;
            std::string varName = sv->name()->c_str();

            if (sv->value_type() == ValueData::StringRef) {
                // STRING 타입: string_value 필드에서 읽기
                if (sv->string_value()) {
                    variables_[varName] = Variant::String(sv->string_value()->c_str());
                } else {
                    variables_[varName] = Variant::String("");
                }
            } else if (sv->value() && sv->value_type() != ValueData::NONE) {
                variables_[varName] = readValueData(sv->value(), sv->value_type(), pool);
            }
        }
    }

    // Call stack 복원
    callStack_.clear();
    auto* stack = saveState->call_stack();
    if (stack) {
        for (flatbuffers::uoffset_t i = 0; i < stack->size(); ++i) {
            auto* frame = stack->Get(i);
            if (!frame->node_name()) continue;
            const void* nodePtr = findNodeByName(frame->node_name()->c_str());
            if (nodePtr) {
                callStack_.push_back({nodePtr, frame->pc()});
            }
        }
    }

    // Pending choices 복원
    pendingChoices_.clear();
    auto* choices = saveState->pending_choices();
    if (choices) {
        for (flatbuffers::uoffset_t i = 0; i < choices->size(); ++i) {
            auto* pc = choices->Get(i);
            int32_t textId = pc->text() ? findStringInPool(pc->text()->c_str()) : -1;
            int32_t targetId = pc->target_node_name()
                ? findStringInPool(pc->target_node_name()->c_str()) : -1;
            if (textId >= 0 && targetId >= 0) {
                pendingChoices_.push_back({textId, targetId});
            }
        }
    }

    return true;
}

} // namespace Gyeol
