#include "gyeol_runner.h"
#include "gyeol_generated.h"
#include <set>

using namespace ICPDev::Gyeol::Schema;

namespace Gyeol {

namespace {
static const Story* asStory(const void* p) { return static_cast<const Story*>(p); }
static const Node* asNode(const void* p) { return static_cast<const Node*>(p); }
}

void Runner::addBreakpoint(const std::string& nodeName, uint32_t pc) {
    breakpoints_.insert({nodeName, pc});
}

void Runner::removeBreakpoint(const std::string& nodeName, uint32_t pc) {
    breakpoints_.erase({nodeName, pc});
}

void Runner::clearBreakpoints() {
    breakpoints_.clear();
}

bool Runner::hasBreakpoint(const std::string& nodeName, uint32_t pc) const {
    return breakpoints_.count({nodeName, pc}) > 0;
}

std::vector<std::pair<std::string, uint32_t>> Runner::getBreakpoints() const {
    std::vector<std::pair<std::string, uint32_t>> result;
    result.reserve(breakpoints_.size());
    for (const auto& bp : breakpoints_) {
        result.push_back(bp);
    }
    return result;
}

void Runner::setStepMode(bool enabled) {
    stepMode_ = enabled;
}

bool Runner::isStepMode() const {
    return stepMode_;
}

Runner::DebugLocation Runner::getLocation() const {
    DebugLocation loc;
    if (!currentNode_) return loc;

    loc.nodeName = currentNodeName();
    loc.pc = pc_;

    auto* node = asNode(currentNode_);
    if (node && node->lines() && pc_ < node->lines()->size()) {
        auto* instr = node->lines()->Get(pc_);
        switch (instr->data_type()) {
            case OpData::Line:            loc.instructionType = "Line"; break;
            case OpData::Choice:          loc.instructionType = "Choice"; break;
            case OpData::Jump:            loc.instructionType = "Jump"; break;
            case OpData::Command:         loc.instructionType = "Command"; break;
            case OpData::Wait:            loc.instructionType = "Wait"; break;
            case OpData::Yield:           loc.instructionType = "Yield"; break;
            case OpData::SetVar:          loc.instructionType = "SetVar"; break;
            case OpData::Condition:       loc.instructionType = "Condition"; break;
            case OpData::Random:          loc.instructionType = "Random"; break;
            case OpData::Return:          loc.instructionType = "Return"; break;
            case OpData::CallWithReturn:  loc.instructionType = "CallWithReturn"; break;
            default:                      loc.instructionType = "Unknown"; break;
        }
    }

    return loc;
}

std::vector<Runner::CallFrameInfo> Runner::getCallStack() const {
    std::vector<CallFrameInfo> result;
    result.reserve(callStack_.size());
    for (const auto& frame : callStack_) {
        CallFrameInfo info;
        info.nodeName = nodeNameFromPtr(frame.node);
        info.pc = frame.pc;
        info.returnVarName = frame.returnVarName;
        info.paramNames = frame.paramNames;
        result.push_back(std::move(info));
    }
    return result;
}

std::string Runner::getCurrentNodeName() const {
    return currentNodeName();
}

uint32_t Runner::getCurrentPC() const {
    return pc_;
}

const std::string& Runner::getLastError() const {
    return lastError_;
}

void Runner::clearLastError() {
    clearErrorInternal();
}

const Runner::ExecutionMetrics& Runner::getMetrics() const {
    return metrics_;
}

void Runner::resetMetrics() {
    metrics_ = {};
}

void Runner::setTraceEnabled(bool enabled, size_t maxEvents) {
    traceEnabled_ = enabled;
    traceLimit_ = maxEvents;
    if (!traceEnabled_ || traceLimit_ == 0) {
        trace_.clear();
    } else if (trace_.size() > traceLimit_) {
        trace_.erase(trace_.begin(), trace_.end() - static_cast<std::ptrdiff_t>(traceLimit_));
    }
}

bool Runner::isTraceEnabled() const {
    return traceEnabled_;
}

const std::vector<Runner::TraceEvent>& Runner::getTrace() const {
    return trace_;
}

void Runner::clearTrace() {
    trace_.clear();
}

std::vector<std::string> Runner::getNodeNames() const {
    std::vector<std::string> result;
    auto* story = asStory(story_);
    if (!story || !story->nodes()) return result;

    auto* nodes = story->nodes();
    result.reserve(nodes->size());
    for (flatbuffers::uoffset_t i = 0; i < nodes->size(); ++i) {
        auto* node = nodes->Get(i);
        if (node->name()) {
            result.push_back(node->name()->c_str());
        }
    }
    return result;
}

uint32_t Runner::getNodeInstructionCount(const std::string& nodeName) const {
    const void* nodePtr = findNodeByName(nodeName.c_str());
    if (!nodePtr) return 0;
    auto* node = asNode(nodePtr);
    if (!node->lines()) return 0;
    return node->lines()->size();
}

std::string Runner::getInstructionInfo(const std::string& nodeName, uint32_t pc) const {
    if (!story_) return "";
    const void* nodePtr = findNodeByName(nodeName.c_str());
    if (!nodePtr) return "";
    auto* node = asNode(nodePtr);
    if (!node->lines() || pc >= node->lines()->size()) return "";

    auto* instr = node->lines()->Get(pc);
    switch (instr->data_type()) {
        case OpData::Line: {
            auto* line = instr->data_as_Line();
            std::string chr = (line->character_id() >= 0)
                ? poolStr(line->character_id()) : "(narration)";
            std::string txt = poolStr(line->text_id());
            return "Line: " + chr + " \"" + txt + "\"";
        }
        case OpData::Choice: {
            auto* choice = instr->data_as_Choice();
            std::string txt = poolStr(choice->text_id());
            std::string target = poolStr(choice->target_node_name_id());
            std::string info = "Choice: \"" + txt + "\" -> " + target;
            if (choice->choice_modifier() == ChoiceModifier::Once) info += " #once";
            else if (choice->choice_modifier() == ChoiceModifier::Sticky) info += " #sticky";
            else if (choice->choice_modifier() == ChoiceModifier::Fallback) info += " #fallback";
            return info;
        }
        case OpData::Jump: {
            auto* jump = instr->data_as_Jump();
            std::string target = poolStr(jump->target_node_name_id());
            if (jump->is_call()) {
                return "Call: -> " + target;
            }
            return "Jump: -> " + target;
        }
        case OpData::Command: {
            auto* cmd = instr->data_as_Command();
            std::string info = "Command: @ " + std::string(poolStr(cmd->type_id()));
            auto* params = cmd->params();
            if (params) {
                for (flatbuffers::uoffset_t k = 0; k < params->size(); ++k) {
                    info += " " + std::string(poolStr(params->Get(k)));
                }
            }
            return info;
        }
        case OpData::Wait: {
            auto* wait = instr->data_as_Wait();
            if (wait && wait->tag_id() >= 0) {
                return "Wait: \"" + std::string(poolStr(wait->tag_id())) + "\"";
            }
            return "Wait";
        }
        case OpData::Yield:
            return "Yield";
        case OpData::SetVar: {
            auto* sv = instr->data_as_SetVar();
            return "SetVar: $ " + std::string(poolStr(sv->var_name_id())) + " = ...";
        }
        case OpData::Condition: {
            auto* cond = instr->data_as_Condition();
            std::string info = "Condition: if ...";
            if (cond->true_jump_node_id() >= 0) {
                info += " -> " + std::string(poolStr(cond->true_jump_node_id()));
            }
            if (cond->false_jump_node_id() >= 0) {
                info += " else -> " + std::string(poolStr(cond->false_jump_node_id()));
            }
            return info;
        }
        case OpData::Random: {
            auto* random = instr->data_as_Random();
            uint32_t count = (random->branches()) ? random->branches()->size() : 0;
            return "Random: " + std::to_string(count) + " branches";
        }
        case OpData::Return: {
            auto* ret = instr->data_as_Return();
            if (ret->expr() || (ret->value() && ret->value_type() != ValueData::NONE)) {
                return "Return: <expr>";
            }
            return "Return";
        }
        case OpData::CallWithReturn: {
            auto* cwr = instr->data_as_CallWithReturn();
            std::string var = poolStr(cwr->return_var_name_id());
            std::string target = poolStr(cwr->target_node_name_id());
            return "CallWithReturn: $ " + var + " = call " + target;
        }
        default:
            return "Unknown";
    }
}

Runner::GraphData Runner::getGraphData() const {
    GraphData data;
    auto* story = asStory(story_);
    if (!story) return data;

    if (story->start_node_name()) {
        data.startNode = story->start_node_name()->c_str();
    }

    auto* nodes = story->nodes();
    if (!nodes) return data;

    for (flatbuffers::uoffset_t ni = 0; ni < nodes->size(); ++ni) {
        auto* node = nodes->Get(ni);
        if (!node->name()) continue;

        GraphNode gn;
        gn.name = node->name()->c_str();
        gn.instructionCount = node->lines() ? static_cast<int>(node->lines()->size()) : 0;

        if (node->param_ids()) {
            for (flatbuffers::uoffset_t pi = 0; pi < node->param_ids()->size(); ++pi) {
                gn.params.push_back(poolStr(node->param_ids()->Get(pi)));
            }
        }

        if (node->tags()) {
            for (flatbuffers::uoffset_t ti = 0; ti < node->tags()->size(); ++ti) {
                auto* tag = node->tags()->Get(ti);
                gn.tags.emplace_back(poolStr(tag->key_id()), poolStr(tag->value_id()));
            }
        }

        std::set<std::string> charSet;
        if (node->lines()) {
            for (flatbuffers::uoffset_t li = 0; li < node->lines()->size(); ++li) {
                auto* instr = node->lines()->Get(li);
                switch (instr->data_type()) {
                case OpData::Line: {
                    auto* line = instr->data_as_Line();
                    gn.summary.lineCount++;
                    if (line->character_id() >= 0) {
                        charSet.insert(poolStr(line->character_id()));
                    }
                    if (gn.summary.firstLine.empty()) {
                        std::string txt = poolStr(line->text_id());
                        if (txt.length() > 40) txt = txt.substr(0, 40) + "...";
                        if (line->character_id() >= 0) {
                            gn.summary.firstLine = std::string(poolStr(line->character_id())) + ": \"" + txt + "\"";
                        } else {
                            gn.summary.firstLine = "\"" + txt + "\"";
                        }
                    }
                    break;
                }
                case OpData::Choice: {
                    auto* choice = instr->data_as_Choice();
                    gn.summary.choiceCount++;
                    GraphEdge edge;
                    edge.from = gn.name;
                    edge.to = poolStr(choice->target_node_name_id());
                    edge.type = "choice";
                    std::string txt = poolStr(choice->text_id());
                    if (txt.length() > 30) txt = txt.substr(0, 30) + "...";
                    edge.label = txt;
                    data.edges.push_back(std::move(edge));
                    break;
                }
                case OpData::Jump: {
                    auto* jump = instr->data_as_Jump();
                    gn.summary.hasJump = true;
                    GraphEdge edge;
                    edge.from = gn.name;
                    edge.to = poolStr(jump->target_node_name_id());
                    edge.type = jump->is_call() ? "call" : "jump";
                    data.edges.push_back(std::move(edge));
                    break;
                }
                case OpData::Command:
                    gn.summary.hasCommand = true;
                    break;
                case OpData::Condition: {
                    auto* cond = instr->data_as_Condition();
                    gn.summary.hasCondition = true;
                    if (cond->true_jump_node_id() >= 0) {
                        GraphEdge edge;
                        edge.from = gn.name;
                        edge.to = poolStr(cond->true_jump_node_id());
                        edge.type = "condition_true";
                        data.edges.push_back(std::move(edge));
                    }
                    if (cond->false_jump_node_id() >= 0) {
                        GraphEdge edge;
                        edge.from = gn.name;
                        edge.to = poolStr(cond->false_jump_node_id());
                        edge.type = "condition_false";
                        edge.label = "else";
                        data.edges.push_back(std::move(edge));
                    }
                    break;
                }
                case OpData::Random: {
                    auto* random = instr->data_as_Random();
                    gn.summary.hasRandom = true;
                    if (random->branches()) {
                        for (flatbuffers::uoffset_t bi = 0; bi < random->branches()->size(); ++bi) {
                            auto* branch = random->branches()->Get(bi);
                            GraphEdge edge;
                            edge.from = gn.name;
                            edge.to = poolStr(branch->target_node_name_id());
                            edge.type = "random";
                            edge.label = std::to_string(branch->weight());
                            data.edges.push_back(std::move(edge));
                        }
                    }
                    break;
                }
                case OpData::CallWithReturn: {
                    auto* cwr = instr->data_as_CallWithReturn();
                    GraphEdge edge;
                    edge.from = gn.name;
                    edge.to = poolStr(cwr->target_node_name_id());
                    edge.type = "call_return";
                    edge.label = "$ " + std::string(poolStr(cwr->return_var_name_id()));
                    data.edges.push_back(std::move(edge));
                    break;
                }
                default:
                    break;
                }
            }
        }

        gn.summary.characters.assign(charSet.begin(), charSet.end());
        data.nodes.push_back(std::move(gn));
    }

    return data;
}

} // namespace Gyeol
