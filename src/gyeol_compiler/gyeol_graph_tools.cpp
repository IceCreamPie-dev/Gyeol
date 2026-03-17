#include "gyeol_graph_tools.h"
#include "gyeol_parser.h"

#include <flatbuffers/flatbuffers.h>

#include <algorithm>
#include <fstream>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

using json = nlohmann::json;
using namespace ICPDev::Gyeol::Schema;

namespace Gyeol::GraphTools {
namespace {

constexpr const char* kGraphDocFormat = "gyeol-graph-doc";
constexpr const char* kGraphPatchFormat = "gyeol-graph-patch";
constexpr const char* kLineIdMapFormat = "gyeol-line-id-map";
constexpr int kGraphDocVersion = 1;
constexpr int kGraphPatchV1 = 1;
constexpr int kGraphPatchV2 = 2;
constexpr int kLineIdMapVersion = 1;

struct EdgeView {
    std::string edgeId;
    std::string from;
    std::string to;
    std::string type;
    std::string label;
};

struct EdgeMutable {
    std::string edgeId;
    std::string from;
    std::string to;
    std::string type;
    std::string label;
    int32_t* targetId = nullptr;
};

const std::string& poolStr(const StoryT& story, int32_t index) {
    static const std::string kEmpty;
    if (index < 0 || static_cast<size_t>(index) >= story.string_pool.size()) return kEmpty;
    return story.string_pool[static_cast<size_t>(index)];
}

std::string makeEdgeId(size_t nodeIdx, size_t instrIdx, const std::string& kind, size_t slot = 0) {
    return "n" + std::to_string(nodeIdx) + ":i" + std::to_string(instrIdx) + ":" + kind + ":" + std::to_string(slot);
}

std::string makeInstructionId(size_t nodeIdx, size_t instrIdx) {
    return "n" + std::to_string(nodeIdx) + ":i" + std::to_string(instrIdx);
}

bool isValidNodeName(const std::string& name) {
    if (name.empty()) return false;
    for (char c : name) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == ':' || c == '#' || c == '(' || c == ')' || c == '"') {
            return false;
        }
    }
    return true;
}

int findNodeIndex(const StoryT& story, const std::string& nodeName) {
    for (size_t i = 0; i < story.nodes.size(); ++i) {
        if (story.nodes[i] && story.nodes[i]->name == nodeName) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

int32_t findOrAddString(StoryT& story, const std::string& value) {
    for (size_t i = 0; i < story.string_pool.size(); ++i) {
        if (story.string_pool[i] == value) {
            return static_cast<int32_t>(i);
        }
    }
    story.string_pool.push_back(value);
    if (!story.line_ids.empty()) {
        story.line_ids.push_back("");
    }
    return static_cast<int32_t>(story.string_pool.size() - 1);
}

std::unique_ptr<StoryT> cloneStory(const StoryT& story) {
    flatbuffers::FlatBufferBuilder builder;
    auto* mutableStory = const_cast<StoryT*>(&story);
    auto root = Story::Pack(builder, mutableStory);
    builder.Finish(root);
    const Story* packed = GetStory(builder.GetBufferPointer());
    return std::unique_ptr<StoryT>(packed->UnPack());
}

template <typename Fn>
void forEachEdgeConst(const StoryT& story, Fn&& fn) {
    for (size_t ni = 0; ni < story.nodes.size(); ++ni) {
        const auto& nodePtr = story.nodes[ni];
        if (!nodePtr) continue;
        const auto& node = *nodePtr;
        for (size_t ii = 0; ii < node.lines.size(); ++ii) {
            const auto& instrPtr = node.lines[ii];
            if (!instrPtr) continue;
            const auto& instr = *instrPtr;

            switch (instr.data.type) {
            case OpData::Choice: {
                const auto* choice = instr.data.AsChoice();
                if (!choice) break;
                fn(EdgeView{
                    makeEdgeId(ni, ii, "choice"),
                    node.name,
                    poolStr(story, choice->target_node_name_id),
                    "choice",
                    poolStr(story, choice->text_id),
                });
                break;
            }
            case OpData::Jump: {
                const auto* jump = instr.data.AsJump();
                if (!jump) break;
                const std::string type = jump->is_call ? "call" : "jump";
                fn(EdgeView{
                    makeEdgeId(ni, ii, type),
                    node.name,
                    poolStr(story, jump->target_node_name_id),
                    type,
                    "",
                });
                break;
            }
            case OpData::Condition: {
                const auto* cond = instr.data.AsCondition();
                if (!cond) break;
                if (cond->true_jump_node_id >= 0) {
                    fn(EdgeView{
                        makeEdgeId(ni, ii, "condition_true"),
                        node.name,
                        poolStr(story, cond->true_jump_node_id),
                        "condition_true",
                        "",
                    });
                }
                if (cond->false_jump_node_id >= 0) {
                    fn(EdgeView{
                        makeEdgeId(ni, ii, "condition_false"),
                        node.name,
                        poolStr(story, cond->false_jump_node_id),
                        "condition_false",
                        "else",
                    });
                }
                break;
            }
            case OpData::Random: {
                const auto* rnd = instr.data.AsRandom();
                if (!rnd) break;
                for (size_t bi = 0; bi < rnd->branches.size(); ++bi) {
                    const auto& branch = rnd->branches[bi];
                    if (!branch) continue;
                    fn(EdgeView{
                        makeEdgeId(ni, ii, "random", bi),
                        node.name,
                        poolStr(story, branch->target_node_name_id),
                        "random",
                        std::to_string(branch->weight),
                    });
                }
                break;
            }
            case OpData::CallWithReturn: {
                const auto* cwr = instr.data.AsCallWithReturn();
                if (!cwr) break;
                fn(EdgeView{
                    makeEdgeId(ni, ii, "call_return"),
                    node.name,
                    poolStr(story, cwr->target_node_name_id),
                    "call_return",
                    "$ " + poolStr(story, cwr->return_var_name_id),
                });
                break;
            }
            default:
                break;
            }
        }
    }
}

template <typename Fn>
void forEachEdgeMutable(StoryT& story, Fn&& fn) {
    for (size_t ni = 0; ni < story.nodes.size(); ++ni) {
        auto& nodePtr = story.nodes[ni];
        if (!nodePtr) continue;
        auto& node = *nodePtr;
        for (size_t ii = 0; ii < node.lines.size(); ++ii) {
            auto& instrPtr = node.lines[ii];
            if (!instrPtr) continue;
            auto& instr = *instrPtr;

            switch (instr.data.type) {
            case OpData::Choice: {
                auto* choice = instr.data.AsChoice();
                if (!choice) break;
                fn(EdgeMutable{
                    makeEdgeId(ni, ii, "choice"),
                    node.name,
                    poolStr(story, choice->target_node_name_id),
                    "choice",
                    poolStr(story, choice->text_id),
                    &choice->target_node_name_id,
                });
                break;
            }
            case OpData::Jump: {
                auto* jump = instr.data.AsJump();
                if (!jump) break;
                const std::string type = jump->is_call ? "call" : "jump";
                fn(EdgeMutable{
                    makeEdgeId(ni, ii, type),
                    node.name,
                    poolStr(story, jump->target_node_name_id),
                    type,
                    "",
                    &jump->target_node_name_id,
                });
                break;
            }
            case OpData::Condition: {
                auto* cond = instr.data.AsCondition();
                if (!cond) break;
                if (cond->true_jump_node_id >= 0) {
                    fn(EdgeMutable{
                        makeEdgeId(ni, ii, "condition_true"),
                        node.name,
                        poolStr(story, cond->true_jump_node_id),
                        "condition_true",
                        "",
                        &cond->true_jump_node_id,
                    });
                }
                if (cond->false_jump_node_id >= 0) {
                    fn(EdgeMutable{
                        makeEdgeId(ni, ii, "condition_false"),
                        node.name,
                        poolStr(story, cond->false_jump_node_id),
                        "condition_false",
                        "else",
                        &cond->false_jump_node_id,
                    });
                }
                break;
            }
            case OpData::Random: {
                auto* rnd = instr.data.AsRandom();
                if (!rnd) break;
                for (size_t bi = 0; bi < rnd->branches.size(); ++bi) {
                    auto& branch = rnd->branches[bi];
                    if (!branch) continue;
                    fn(EdgeMutable{
                        makeEdgeId(ni, ii, "random", bi),
                        node.name,
                        poolStr(story, branch->target_node_name_id),
                        "random",
                        std::to_string(branch->weight),
                        &branch->target_node_name_id,
                    });
                }
                break;
            }
            case OpData::CallWithReturn: {
                auto* cwr = instr.data.AsCallWithReturn();
                if (!cwr) break;
                fn(EdgeMutable{
                    makeEdgeId(ni, ii, "call_return"),
                    node.name,
                    poolStr(story, cwr->target_node_name_id),
                    "call_return",
                    "$ " + poolStr(story, cwr->return_var_name_id),
                    &cwr->target_node_name_id,
                });
                break;
            }
            default:
                break;
            }
        }
    }
}

bool loadJsonFile(const std::string& path, json& out, std::string* errorOut) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        if (errorOut) *errorOut = "Failed to open JSON file: " + path;
        return false;
    }
    try {
        ifs >> out;
    } catch (const std::exception& e) {
        if (errorOut) *errorOut = std::string("Invalid JSON: ") + e.what();
        return false;
    }
    return true;
}

bool validatePatchEnvelope(const json& patch, int* versionOut, std::string* errorOut) {
    if (!patch.is_object()) {
        if (errorOut) *errorOut = "Patch must be a JSON object.";
        return false;
    }
    if (patch.value("format", "") != kGraphPatchFormat) {
        if (errorOut) *errorOut = "Patch format must be 'gyeol-graph-patch'.";
        return false;
    }
    if (!patch.contains("version") || !patch["version"].is_number_integer()) {
        if (errorOut) *errorOut = "Patch version must be integer 1 or 2.";
        return false;
    }
    const int version = patch["version"].get<int>();
    if (version != kGraphPatchV1 && version != kGraphPatchV2) {
        if (errorOut) *errorOut = "Unsupported patch version. Expected 1 or 2.";
        return false;
    }
    if (!patch.contains("ops") || !patch["ops"].is_array()) {
        if (errorOut) *errorOut = "Patch must contain array field 'ops'.";
        return false;
    }
    if (versionOut) *versionOut = version;
    return true;
}

bool readRequiredString(const json& obj,
                        const char* field,
                        std::string& out,
                        const std::string& opName,
                        std::string* errorOut) {
    if (!obj.contains(field) || !obj[field].is_string()) {
        if (errorOut) *errorOut = "Op '" + opName + "' requires string field '" + field + "'.";
        return false;
    }
    out = obj[field].get<std::string>();
    if (out.empty()) {
        if (errorOut) *errorOut = "Op '" + opName + "' field '" + field + "' cannot be empty.";
        return false;
    }
    return true;
}

bool readOptionalInt(const json& obj, const char* field, int& out) {
    if (!obj.contains(field)) return false;
    if (!obj[field].is_number_integer()) return false;
    out = obj[field].get<int>();
    return true;
}

struct InstructionPos {
    int nodeIndex = -1;
    int instrIndex = -1;
};

bool isValidPos(const InstructionPos& pos) {
    return pos.nodeIndex >= 0 && pos.instrIndex >= 0;
}

using InstructionOriginTable = std::vector<std::vector<std::string>>;

void initializeInstructionOrigins(const StoryT& story, InstructionOriginTable& origins) {
    origins.clear();
    origins.resize(story.nodes.size());
    for (size_t ni = 0; ni < story.nodes.size(); ++ni) {
        const auto& node = story.nodes[ni];
        if (!node) continue;
        origins[ni].resize(node->lines.size());
        for (size_t ii = 0; ii < node->lines.size(); ++ii) {
            origins[ni][ii] = makeInstructionId(ni, ii);
        }
    }
}

InstructionPos findInstructionByOriginId(const InstructionOriginTable& origins, const std::string& originId) {
    for (size_t ni = 0; ni < origins.size(); ++ni) {
        for (size_t ii = 0; ii < origins[ni].size(); ++ii) {
            if (origins[ni][ii] == originId) {
                return {static_cast<int>(ni), static_cast<int>(ii)};
            }
        }
    }
    return {};
}

int resolveNodeIndexByName(const StoryT& story, const json& opObj, std::string* errorOut) {
    if (!opObj.contains("node")) return -1;
    if (!opObj["node"].is_string()) {
        if (errorOut) *errorOut = "Field 'node' must be a string.";
        return -2;
    }
    const std::string nodeName = opObj["node"].get<std::string>();
    const int idx = findNodeIndex(story, nodeName);
    if (idx < 0) {
        if (errorOut) *errorOut = "Unknown node: " + nodeName;
        return -2;
    }
    return idx;
}

bool resolveInsertionTarget(const StoryT& story,
                            const InstructionOriginTable& origins,
                            const json& opObj,
                            InstructionPos& outPos,
                            std::string* errorOut) {
    const bool hasBefore = opObj.contains("before_instruction_id");
    const bool hasAfter = opObj.contains("after_instruction_id");
    const bool hasIndex = opObj.contains("index");
    const int anchorCount = static_cast<int>(hasBefore) + static_cast<int>(hasAfter) + static_cast<int>(hasIndex);
    if (anchorCount != 1) {
        if (errorOut) *errorOut = "Specify exactly one anchor: before_instruction_id, after_instruction_id, or index.";
        return false;
    }

    const int nodeIdxByName = resolveNodeIndexByName(story, opObj, errorOut);
    if (nodeIdxByName == -2) return false;

    if (hasBefore || hasAfter) {
        const char* key = hasBefore ? "before_instruction_id" : "after_instruction_id";
        if (!opObj[key].is_string()) {
            if (errorOut) *errorOut = std::string("Field '") + key + "' must be a string.";
            return false;
        }
        const std::string originId = opObj[key].get<std::string>();
        InstructionPos ref = findInstructionByOriginId(origins, originId);
        if (!isValidPos(ref)) {
            if (errorOut) *errorOut = std::string("Unknown instruction_id: ") + originId;
            return false;
        }
        if (nodeIdxByName >= 0 && nodeIdxByName != ref.nodeIndex) {
            if (errorOut) *errorOut = "Anchor instruction is in a different node than field 'node'.";
            return false;
        }
        outPos.nodeIndex = ref.nodeIndex;
        outPos.instrIndex = hasAfter ? (ref.instrIndex + 1) : ref.instrIndex;
        return true;
    }

    if (!opObj["index"].is_number_integer()) {
        if (errorOut) *errorOut = "Field 'index' must be an integer.";
        return false;
    }
    if (nodeIdxByName < 0) {
        if (errorOut) *errorOut = "Field 'node' is required when using index anchor.";
        return false;
    }
    const int index = opObj["index"].get<int>();
    if (index < 0 || index > static_cast<int>(story.nodes[static_cast<size_t>(nodeIdxByName)]->lines.size())) {
        if (errorOut) *errorOut = "index is out of range for target node.";
        return false;
    }
    outPos = {nodeIdxByName, index};
    return true;
}

ValueDataUnion remapValueData(const ValueDataUnion& src, const StoryT& srcStory, StoryT& dstStory) {
    ValueDataUnion out;
    switch (src.type) {
    case ValueData::BoolValue:
        {
            BoolValueT value;
            value.val = src.AsBoolValue() ? src.AsBoolValue()->val : false;
            out.Set(std::move(value));
        }
        break;
    case ValueData::IntValue:
        {
            IntValueT value;
            value.val = src.AsIntValue() ? src.AsIntValue()->val : 0;
            out.Set(std::move(value));
        }
        break;
    case ValueData::FloatValue:
        {
            FloatValueT value;
            value.val = src.AsFloatValue() ? src.AsFloatValue()->val : 0.0f;
            out.Set(std::move(value));
        }
        break;
    case ValueData::StringRef: {
        StringRefT ref;
        ref.index = findOrAddString(dstStory, poolStr(srcStory, src.AsStringRef() ? src.AsStringRef()->index : -1));
        out.Set(ref);
        break;
    }
    case ValueData::ListValue: {
        ListValueT lv;
        auto* srcLv = src.AsListValue();
        if (srcLv) {
            for (int32_t id : srcLv->items) {
                lv.items.push_back(findOrAddString(dstStory, poolStr(srcStory, id)));
            }
        }
        out.Set(lv);
        break;
    }
    default:
        break;
    }
    return out;
}

std::unique_ptr<ExpressionT> remapExpression(const ExpressionT* srcExpr, const StoryT& srcStory, StoryT& dstStory) {
    if (!srcExpr) return nullptr;
    auto out = std::make_unique<ExpressionT>();
    for (const auto& token : srcExpr->tokens) {
        if (!token) continue;
        auto t = std::make_unique<ExprTokenT>();
        t->op = token->op;
        t->literal_value = remapValueData(token->literal_value, srcStory, dstStory);
        if (token->var_name_id >= 0) {
            t->var_name_id = findOrAddString(dstStory, poolStr(srcStory, token->var_name_id));
        } else {
            t->var_name_id = -1;
        }
        out->tokens.push_back(std::move(t));
    }
    return out;
}

bool parseScriptToStory(const std::string& script, StoryT& outStory, std::string* errorOut) {
    Parser parser;
    if (!parser.parseString(script, "<patch-expr>")) {
        if (errorOut) {
            std::string err = parser.getError();
            if (err.empty() && !parser.getErrors().empty()) err = parser.getErrors().front();
            *errorOut = "Failed to parse expression script: " + err;
        }
        return false;
    }
    outStory = parser.getStory();
    return true;
}

bool parseSetVarExpr(const std::string& exprText,
                     StoryT& dstStory,
                     std::unique_ptr<ExpressionT>& outExpr,
                     ValueDataUnion& outValue,
                     std::string* errorOut) {
    StoryT tmp;
    const std::string script =
        "label start:\n"
        "    $ __tmp = " + exprText + "\n";
    if (!parseScriptToStory(script, tmp, errorOut)) return false;
    if (tmp.nodes.empty() || tmp.nodes[0]->lines.empty()) {
        if (errorOut) *errorOut = "Failed to build SetVar expression.";
        return false;
    }
    auto* sv = tmp.nodes[0]->lines[0]->data.AsSetVar();
    if (!sv) {
        if (errorOut) *errorOut = "Expression did not parse as SetVar payload.";
        return false;
    }
    outExpr = remapExpression(sv->expr.get(), tmp, dstStory);
    outValue = remapValueData(sv->value, tmp, dstStory);
    return true;
}

bool parseReturnExpr(const std::string& exprText,
                     StoryT& dstStory,
                     std::unique_ptr<ExpressionT>& outExpr,
                     ValueDataUnion& outValue,
                     std::string* errorOut) {
    StoryT tmp;
    const std::string script =
        "label start:\n"
        "    return " + exprText + "\n";
    if (!parseScriptToStory(script, tmp, errorOut)) return false;
    if (tmp.nodes.empty() || tmp.nodes[0]->lines.empty()) {
        if (errorOut) *errorOut = "Failed to build Return expression.";
        return false;
    }
    auto* ret = tmp.nodes[0]->lines[0]->data.AsReturn();
    if (!ret) {
        if (errorOut) *errorOut = "Expression did not parse as Return payload.";
        return false;
    }
    outExpr = remapExpression(ret->expr.get(), tmp, dstStory);
    outValue = remapValueData(ret->value, tmp, dstStory);
    return true;
}

bool parseConditionExpr(const std::string& exprText,
                        StoryT& dstStory,
                        ConditionT& outCond,
                        std::string* errorOut) {
    StoryT tmp;
    const std::string script =
        "label start:\n"
        "    if " + exprText + " -> __t else __f\n"
        "label __t:\n"
        "    \"t\"\n"
        "label __f:\n"
        "    \"f\"\n";
    if (!parseScriptToStory(script, tmp, errorOut)) return false;
    if (tmp.nodes.empty() || tmp.nodes[0]->lines.empty()) {
        if (errorOut) *errorOut = "Failed to build Condition expression.";
        return false;
    }
    auto* cond = tmp.nodes[0]->lines[0]->data.AsCondition();
    if (!cond) {
        if (errorOut) *errorOut = "Expression did not parse as Condition payload.";
        return false;
    }

    outCond.op = cond->op;
    outCond.var_name_id = (cond->var_name_id >= 0)
        ? findOrAddString(dstStory, poolStr(tmp, cond->var_name_id))
        : -1;
    outCond.compare_value = remapValueData(cond->compare_value, tmp, dstStory);
    outCond.lhs_expr = remapExpression(cond->lhs_expr.get(), tmp, dstStory);
    outCond.rhs_expr = remapExpression(cond->rhs_expr.get(), tmp, dstStory);
    outCond.cond_expr = remapExpression(cond->cond_expr.get(), tmp, dstStory);
    return true;
}

bool parseChoiceModifier(const std::string& name, ChoiceModifier& out) {
    if (name == "Default") { out = ChoiceModifier::Default; return true; }
    if (name == "Once") { out = ChoiceModifier::Once; return true; }
    if (name == "Sticky") { out = ChoiceModifier::Sticky; return true; }
    if (name == "Fallback") { out = ChoiceModifier::Fallback; return true; }
    return false;
}

bool parseAssignOp(const std::string& name, AssignOp& out) {
    if (name == "Assign") { out = AssignOp::Assign; return true; }
    if (name == "Append") { out = AssignOp::Append; return true; }
    if (name == "Remove") { out = AssignOp::Remove; return true; }
    return false;
}

bool parseValueJson(const json& j, StoryT& story, ValueDataUnion& out, std::string* errorOut) {
    if (!j.is_object() || !j.contains("type")) {
        if (errorOut) *errorOut = "value must be an object with field 'type'.";
        return false;
    }
    const std::string type = j["type"].is_string() ? j["type"].get<std::string>() : "";
    if (type == "Bool") {
        if (!j.contains("val") || !j["val"].is_boolean()) return false;
        BoolValueT value;
        value.val = j["val"].get<bool>();
        out.Set(std::move(value));
        return true;
    }
    if (type == "Int") {
        if (!j.contains("val") || !j["val"].is_number_integer()) return false;
        IntValueT value;
        value.val = j["val"].get<int32_t>();
        out.Set(std::move(value));
        return true;
    }
    if (type == "Float") {
        if (!j.contains("val") || !j["val"].is_number()) return false;
        FloatValueT value;
        value.val = j["val"].get<float>();
        out.Set(std::move(value));
        return true;
    }
    if (type == "String") {
        if (!j.contains("val") || !j["val"].is_string()) return false;
        StringRefT ref;
        ref.index = findOrAddString(story, j["val"].get<std::string>());
        out.Set(ref);
        return true;
    }
    if (type == "List") {
        if (!j.contains("val") || !j["val"].is_array()) return false;
        ListValueT lv;
        for (const auto& item : j["val"]) {
            if (!item.is_string()) return false;
            lv.items.push_back(findOrAddString(story, item.get<std::string>()));
        }
        out.Set(lv);
        return true;
    }
    if (errorOut) *errorOut = "Unsupported value type: " + type;
    return false;
}

bool buildInstructionFromJson(const json& spec,
                              StoryT& story,
                              std::unique_ptr<InstructionT>& outInstr,
                              std::string* errorOut) {
    if (!spec.is_object() || !spec.contains("type") || !spec["type"].is_string()) {
        if (errorOut) *errorOut = "insert_instruction requires object field instruction.type";
        return false;
    }
    const std::string type = spec["type"].get<std::string>();
    auto instr = std::make_unique<InstructionT>();

    if (type == "Line") {
        if (!spec.contains("text") || !spec["text"].is_string()) {
            if (errorOut) *errorOut = "Line instruction requires string field 'text'.";
            return false;
        }
        LineT line;
        line.text_id = findOrAddString(story, spec["text"].get<std::string>());
        line.character_id = -1;
        if (spec.contains("character")) {
            if (spec["character"].is_string()) {
                line.character_id = findOrAddString(story, spec["character"].get<std::string>());
            } else if (!spec["character"].is_null()) {
                if (errorOut) *errorOut = "Line.character must be string or null.";
                return false;
            }
        }
        if (spec.contains("voice_asset")) {
            if (!spec["voice_asset"].is_string()) return false;
            line.voice_asset_id = findOrAddString(story, spec["voice_asset"].get<std::string>());
        } else {
            line.voice_asset_id = -1;
        }
        if (spec.contains("tags")) {
            if (!spec["tags"].is_array()) return false;
            for (const auto& jt : spec["tags"]) {
                if (!jt.is_object() || !jt.contains("key") || !jt["key"].is_string()) return false;
                auto tag = std::make_unique<TagT>();
                tag->key_id = findOrAddString(story, jt["key"].get<std::string>());
                const std::string value = (jt.contains("value") && jt["value"].is_string()) ? jt["value"].get<std::string>() : "";
                tag->value_id = findOrAddString(story, value);
                line.tags.push_back(std::move(tag));
            }
        }
        instr->data.Set(line);
        outInstr = std::move(instr);
        return true;
    }

    if (type == "Choice") {
        if (!spec.contains("text") || !spec["text"].is_string() ||
            !spec.contains("target_node") || !spec["target_node"].is_string()) {
            if (errorOut) *errorOut = "Choice requires 'text' and 'target_node' strings.";
            return false;
        }
        ChoiceT c;
        c.text_id = findOrAddString(story, spec["text"].get<std::string>());
        c.target_node_name_id = findOrAddString(story, spec["target_node"].get<std::string>());
        c.condition_var_id = -1;
        c.choice_modifier = ChoiceModifier::Default;
        if (spec.contains("condition_var")) {
            if (!spec["condition_var"].is_string()) return false;
            c.condition_var_id = findOrAddString(story, spec["condition_var"].get<std::string>());
        }
        if (spec.contains("choice_modifier")) {
            if (!spec["choice_modifier"].is_string()) return false;
            if (!parseChoiceModifier(spec["choice_modifier"].get<std::string>(), c.choice_modifier)) return false;
        }
        instr->data.Set(c);
        outInstr = std::move(instr);
        return true;
    }

    if (type == "Command") {
        if (!spec.contains("command_type") || !spec["command_type"].is_string()) {
            if (errorOut) *errorOut = "Command requires string field 'command_type'.";
            return false;
        }
        CommandT cmd;
        cmd.type_id = findOrAddString(story, spec["command_type"].get<std::string>());
        if (spec.contains("params")) {
            if (!spec["params"].is_array()) return false;
            for (const auto& p : spec["params"]) {
                if (!p.is_string()) return false;
                cmd.params.push_back(findOrAddString(story, p.get<std::string>()));
            }
        }
        instr->data.Set(cmd);
        outInstr = std::move(instr);
        return true;
    }

    if (type == "Jump") {
        if (!spec.contains("target_node") || !spec["target_node"].is_string()) return false;
        JumpT j;
        j.target_node_name_id = findOrAddString(story, spec["target_node"].get<std::string>());
        j.is_call = spec.contains("is_call") && spec["is_call"].is_boolean() ? spec["is_call"].get<bool>() : false;
        instr->data.Set(j);
        outInstr = std::move(instr);
        return true;
    }

    if (type == "SetVar") {
        if (!spec.contains("var_name") || !spec["var_name"].is_string()) return false;
        SetVarT sv;
        sv.var_name_id = findOrAddString(story, spec["var_name"].get<std::string>());
        sv.assign_op = AssignOp::Assign;
        if (spec.contains("assign_op")) {
            if (!spec["assign_op"].is_string()) return false;
            if (!parseAssignOp(spec["assign_op"].get<std::string>(), sv.assign_op)) return false;
        }
        if (spec.contains("expr_text")) {
            if (!spec["expr_text"].is_string()) return false;
            if (!parseSetVarExpr(spec["expr_text"].get<std::string>(), story, sv.expr, sv.value, errorOut)) return false;
        } else if (spec.contains("value")) {
            if (!parseValueJson(spec["value"], story, sv.value, errorOut)) return false;
        } else {
            if (errorOut) *errorOut = "SetVar requires either expr_text or value.";
            return false;
        }
        instr->data.Set(sv);
        outInstr = std::move(instr);
        return true;
    }

    if (type == "Condition") {
        if (!spec.contains("expr_text") || !spec["expr_text"].is_string() ||
            !spec.contains("true_target") || !spec["true_target"].is_string()) {
            if (errorOut) *errorOut = "Condition requires expr_text and true_target.";
            return false;
        }
        ConditionT c;
        c.true_jump_node_id = findOrAddString(story, spec["true_target"].get<std::string>());
        c.false_jump_node_id = -1;
        if (spec.contains("false_target")) {
            if (!spec["false_target"].is_string()) return false;
            c.false_jump_node_id = findOrAddString(story, spec["false_target"].get<std::string>());
        }
        if (!parseConditionExpr(spec["expr_text"].get<std::string>(), story, c, errorOut)) return false;
        c.true_jump_node_id = findOrAddString(story, spec["true_target"].get<std::string>());
        if (spec.contains("false_target")) {
            c.false_jump_node_id = findOrAddString(story, spec["false_target"].get<std::string>());
        } else {
            c.false_jump_node_id = -1;
        }
        instr->data.Set(c);
        outInstr = std::move(instr);
        return true;
    }

    if (type == "Return") {
        ReturnT r;
        if (spec.contains("expr_text")) {
            if (!spec["expr_text"].is_string()) return false;
            if (!parseReturnExpr(spec["expr_text"].get<std::string>(), story, r.expr, r.value, errorOut)) return false;
        } else if (spec.contains("value")) {
            if (!parseValueJson(spec["value"], story, r.value, errorOut)) return false;
        }
        instr->data.Set(r);
        outInstr = std::move(instr);
        return true;
    }

    if (errorOut) *errorOut = "Unsupported instruction type for insert_instruction: " + type;
    return false;
}

void retargetAllReferences(StoryT& story, const std::string& fromNode, const std::string& toNode) {
    const int32_t toId = findOrAddString(story, toNode);
    forEachEdgeMutable(story, [&](const EdgeMutable& edge) {
        if (!edge.targetId) return;
        if (edge.to == fromNode) {
            *edge.targetId = toId;
        }
    });
}

bool retargetEdgeById(StoryT& story,
                      const std::string& edgeId,
                      const std::string& toNode,
                      std::string* errorOut) {
    const int32_t toId = findOrAddString(story, toNode);
    size_t matched = 0;
    forEachEdgeMutable(story, [&](const EdgeMutable& edge) {
        if (edge.edgeId != edgeId || !edge.targetId) return;
        *edge.targetId = toId;
        matched++;
    });

    if (matched == 0) {
        if (errorOut) *errorOut = "Unknown edge_id: " + edgeId;
        return false;
    }
    if (matched > 1) {
        if (errorOut) *errorOut = "Non-unique edge_id encountered: " + edgeId;
        return false;
    }
    return true;
}

bool validateGraphIntegrity(const StoryT& story, std::string* errorOut) {
    std::unordered_set<std::string> nodeNames;
    for (const auto& node : story.nodes) {
        if (!node) continue;
        if (node->name.empty()) {
            if (errorOut) *errorOut = "Encountered node with empty name.";
            return false;
        }
        if (!nodeNames.insert(node->name).second) {
            if (errorOut) *errorOut = "Duplicate node name after patch: " + node->name;
            return false;
        }
    }

    if (story.start_node_name.empty()) {
        if (errorOut) *errorOut = "Story start_node_name is empty after patch.";
        return false;
    }
    if (nodeNames.find(story.start_node_name) == nodeNames.end()) {
        if (errorOut) *errorOut = "start_node_name points to missing node: " + story.start_node_name;
        return false;
    }

    bool ok = true;
    forEachEdgeConst(story, [&](const EdgeView& edge) {
        if (!ok) return;
        if (edge.to.empty() || nodeNames.find(edge.to) == nodeNames.end()) {
            ok = false;
            if (errorOut) {
                *errorOut = "Broken edge target after patch: " + edge.edgeId + " -> '" + edge.to + "'";
            }
        }
    });
    return ok;
}

bool applyPatchOps(StoryT& story,
                   const json& patch,
                   bool preserveLineIds,
                   json* outLineIdMap,
                   std::string* errorOut) {
    int patchVersion = 0;
    if (!validatePatchEnvelope(patch, &patchVersion, errorOut)) return false;
    bool sawSetStartNode = false;
    const bool enableV2 = (patchVersion >= kGraphPatchV2);

    InstructionOriginTable instructionOrigins;
    std::unordered_map<std::string, std::string> snapshotLineIdByOrigin;
    if (enableV2 || preserveLineIds) {
        initializeInstructionOrigins(story, instructionOrigins);
        for (size_t ni = 0; ni < story.nodes.size(); ++ni) {
            const auto& node = story.nodes[ni];
            if (!node) continue;
            for (size_t ii = 0; ii < node->lines.size(); ++ii) {
                const auto& instr = node->lines[ii];
                if (!instr) continue;
                int32_t textId = -1;
                if (instr->data.type == OpData::Line) {
                    auto* line = instr->data.AsLine();
                    if (line) textId = line->text_id;
                } else if (instr->data.type == OpData::Choice) {
                    auto* choice = instr->data.AsChoice();
                    if (choice) textId = choice->text_id;
                }
                if (textId < 0 || static_cast<size_t>(textId) >= story.line_ids.size()) continue;
                const std::string lid = story.line_ids[static_cast<size_t>(textId)];
                if (!lid.empty()) {
                    snapshotLineIdByOrigin[makeInstructionId(ni, ii)] = lid;
                }
            }
        }
    }

    const auto& ops = patch["ops"];
    for (size_t i = 0; i < ops.size(); ++i) {
        if (!ops[i].is_object()) {
            if (errorOut) *errorOut = "Each op must be an object (ops[" + std::to_string(i) + "]).";
            return false;
        }
        const auto& opObj = ops[i];
        if (!opObj.contains("op") || !opObj["op"].is_string()) {
            if (errorOut) *errorOut = "Op at index " + std::to_string(i) + " is missing string field 'op'.";
            return false;
        }
        const std::string opName = opObj["op"].get<std::string>();

        if (opName == "add_node") {
            std::string nodeName;
            if (!readRequiredString(opObj, "node", nodeName, opName, errorOut)) return false;
            if (!isValidNodeName(nodeName)) {
                if (errorOut) *errorOut = "Invalid node name for add_node: " + nodeName;
                return false;
            }
            if (findNodeIndex(story, nodeName) >= 0) {
                if (errorOut) *errorOut = "add_node target already exists: " + nodeName;
                return false;
            }

            auto node = std::make_unique<NodeT>();
            node->name = nodeName;

            if (opObj.contains("params")) {
                if (!opObj["params"].is_array()) {
                    if (errorOut) *errorOut = "add_node field 'params' must be an array.";
                    return false;
                }
                for (const auto& p : opObj["params"]) {
                    if (!p.is_string()) {
                        if (errorOut) *errorOut = "add_node params must be strings.";
                        return false;
                    }
                    node->param_ids.push_back(findOrAddString(story, p.get<std::string>()));
                }
            }

            if (opObj.contains("tags")) {
                if (!opObj["tags"].is_array()) {
                    if (errorOut) *errorOut = "add_node field 'tags' must be an array.";
                    return false;
                }
                for (const auto& t : opObj["tags"]) {
                    if (!t.is_object() || !t.contains("key") || !t["key"].is_string()) {
                        if (errorOut) *errorOut = "Each add_node tag requires string field 'key'.";
                        return false;
                    }
                    auto tag = std::make_unique<TagT>();
                    tag->key_id = findOrAddString(story, t["key"].get<std::string>());
                    const std::string value = t.contains("value") && t["value"].is_string()
                        ? t["value"].get<std::string>()
                        : "";
                    tag->value_id = findOrAddString(story, value);
                    node->tags.push_back(std::move(tag));
                }
            }

            story.nodes.push_back(std::move(node));
            if (enableV2 || preserveLineIds) {
                instructionOrigins.push_back({});
            }
            continue;
        }

        if (opName == "rename_node") {
            std::string fromNode;
            std::string toNode;
            if (!readRequiredString(opObj, "from", fromNode, opName, errorOut)) return false;
            if (!readRequiredString(opObj, "to", toNode, opName, errorOut)) return false;
            if (!isValidNodeName(toNode)) {
                if (errorOut) *errorOut = "Invalid rename target node name: " + toNode;
                return false;
            }

            const int fromIdx = findNodeIndex(story, fromNode);
            if (fromIdx < 0) {
                if (errorOut) *errorOut = "rename_node source does not exist: " + fromNode;
                return false;
            }
            if (findNodeIndex(story, toNode) >= 0) {
                if (errorOut) *errorOut = "rename_node target already exists: " + toNode;
                return false;
            }

            story.nodes[static_cast<size_t>(fromIdx)]->name = toNode;
            if (story.start_node_name == fromNode) {
                story.start_node_name = toNode;
            }
            retargetAllReferences(story, fromNode, toNode);
            continue;
        }

        if (opName == "delete_node") {
            std::string nodeName;
            std::string redirectTarget;
            if (!readRequiredString(opObj, "node", nodeName, opName, errorOut)) return false;
            if (!readRequiredString(opObj, "redirect_target", redirectTarget, opName, errorOut)) return false;
            if (nodeName == redirectTarget) {
                if (errorOut) *errorOut = "delete_node requires redirect_target different from node.";
                return false;
            }

            const int nodeIdx = findNodeIndex(story, nodeName);
            if (nodeIdx < 0) {
                if (errorOut) *errorOut = "delete_node target does not exist: " + nodeName;
                return false;
            }
            if (findNodeIndex(story, redirectTarget) < 0) {
                if (errorOut) *errorOut = "delete_node redirect_target does not exist: " + redirectTarget;
                return false;
            }

            if (story.start_node_name == nodeName && !sawSetStartNode) {
                if (errorOut) *errorOut = "Deleting the current start node requires prior set_start_node op.";
                return false;
            }

            retargetAllReferences(story, nodeName, redirectTarget);
            story.nodes.erase(story.nodes.begin() + nodeIdx);
            if (enableV2 || preserveLineIds) {
                instructionOrigins.erase(instructionOrigins.begin() + nodeIdx);
            }
            if (story.start_node_name == nodeName) {
                story.start_node_name = redirectTarget;
            }
            continue;
        }

        if (opName == "retarget_edge") {
            std::string edgeId;
            std::string toNode;
            if (!readRequiredString(opObj, "edge_id", edgeId, opName, errorOut)) return false;
            if (!readRequiredString(opObj, "to", toNode, opName, errorOut)) return false;
            if (findNodeIndex(story, toNode) < 0) {
                if (errorOut) *errorOut = "retarget_edge target node does not exist: " + toNode;
                return false;
            }
            if (!retargetEdgeById(story, edgeId, toNode, errorOut)) return false;
            continue;
        }

        if (opName == "set_start_node") {
            std::string nodeName;
            if (!readRequiredString(opObj, "node", nodeName, opName, errorOut)) return false;
            if (findNodeIndex(story, nodeName) < 0) {
                if (errorOut) *errorOut = "set_start_node target does not exist: " + nodeName;
                return false;
            }
            story.start_node_name = nodeName;
            sawSetStartNode = true;
            continue;
        }

        if (enableV2 && opName == "update_line_text") {
            std::string instructionId;
            std::string text;
            if (!readRequiredString(opObj, "instruction_id", instructionId, opName, errorOut)) return false;
            if (!readRequiredString(opObj, "text", text, opName, errorOut)) return false;

            InstructionPos pos = findInstructionByOriginId(instructionOrigins, instructionId);
            if (!isValidPos(pos)) {
                if (errorOut) *errorOut = "Unknown instruction_id: " + instructionId;
                return false;
            }
            auto& instr = story.nodes[static_cast<size_t>(pos.nodeIndex)]->lines[static_cast<size_t>(pos.instrIndex)];
            if (!instr || instr->data.type != OpData::Line) {
                if (errorOut) *errorOut = "update_line_text target is not a Line instruction.";
                return false;
            }
            auto* line = instr->data.AsLine();
            line->text_id = findOrAddString(story, text);
            continue;
        }

        if (enableV2 && opName == "update_choice_text") {
            std::string instructionId;
            std::string text;
            if (!readRequiredString(opObj, "instruction_id", instructionId, opName, errorOut)) return false;
            if (!readRequiredString(opObj, "text", text, opName, errorOut)) return false;

            InstructionPos pos = findInstructionByOriginId(instructionOrigins, instructionId);
            if (!isValidPos(pos)) {
                if (errorOut) *errorOut = "Unknown instruction_id: " + instructionId;
                return false;
            }
            auto& instr = story.nodes[static_cast<size_t>(pos.nodeIndex)]->lines[static_cast<size_t>(pos.instrIndex)];
            if (!instr || instr->data.type != OpData::Choice) {
                if (errorOut) *errorOut = "update_choice_text target is not a Choice instruction.";
                return false;
            }
            auto* choice = instr->data.AsChoice();
            choice->text_id = findOrAddString(story, text);
            continue;
        }

        if (enableV2 && opName == "update_command") {
            std::string instructionId;
            if (!readRequiredString(opObj, "instruction_id", instructionId, opName, errorOut)) return false;
            if (!opObj.contains("command_type") || !opObj["command_type"].is_string()) {
                if (errorOut) *errorOut = "update_command requires string field 'command_type'.";
                return false;
            }
            if (opObj.contains("params") && !opObj["params"].is_array()) {
                if (errorOut) *errorOut = "update_command params must be an array of strings.";
                return false;
            }
            InstructionPos pos = findInstructionByOriginId(instructionOrigins, instructionId);
            if (!isValidPos(pos)) {
                if (errorOut) *errorOut = "Unknown instruction_id: " + instructionId;
                return false;
            }
            auto& instr = story.nodes[static_cast<size_t>(pos.nodeIndex)]->lines[static_cast<size_t>(pos.instrIndex)];
            if (!instr || instr->data.type != OpData::Command) {
                if (errorOut) *errorOut = "update_command target is not a Command instruction.";
                return false;
            }
            auto* cmd = instr->data.AsCommand();
            cmd->type_id = findOrAddString(story, opObj["command_type"].get<std::string>());
            cmd->params.clear();
            if (opObj.contains("params")) {
                for (const auto& p : opObj["params"]) {
                    if (!p.is_string()) {
                        if (errorOut) *errorOut = "update_command params must contain strings only.";
                        return false;
                    }
                    cmd->params.push_back(findOrAddString(story, p.get<std::string>()));
                }
            }
            continue;
        }

        if (enableV2 && opName == "update_expression") {
            std::string instructionId;
            std::string exprText;
            if (!readRequiredString(opObj, "instruction_id", instructionId, opName, errorOut)) return false;
            if (!readRequiredString(opObj, "expr_text", exprText, opName, errorOut)) return false;
            InstructionPos pos = findInstructionByOriginId(instructionOrigins, instructionId);
            if (!isValidPos(pos)) {
                if (errorOut) *errorOut = "Unknown instruction_id: " + instructionId;
                return false;
            }
            auto& instr = story.nodes[static_cast<size_t>(pos.nodeIndex)]->lines[static_cast<size_t>(pos.instrIndex)];
            if (!instr) {
                if (errorOut) *errorOut = "update_expression target instruction is null.";
                return false;
            }

            if (instr->data.type == OpData::SetVar) {
                auto* sv = instr->data.AsSetVar();
                if (!parseSetVarExpr(exprText, story, sv->expr, sv->value, errorOut)) return false;
                continue;
            }
            if (instr->data.type == OpData::Return) {
                auto* ret = instr->data.AsReturn();
                if (!parseReturnExpr(exprText, story, ret->expr, ret->value, errorOut)) return false;
                continue;
            }
            if (instr->data.type == OpData::Condition) {
                auto* cond = instr->data.AsCondition();
                const int32_t trueTarget = cond->true_jump_node_id;
                const int32_t falseTarget = cond->false_jump_node_id;
                if (!parseConditionExpr(exprText, story, *cond, errorOut)) return false;
                cond->true_jump_node_id = trueTarget;
                cond->false_jump_node_id = falseTarget;
                continue;
            }

            if (errorOut) *errorOut = "update_expression supports SetVar/Return/Condition targets only.";
            return false;
        }

        if (enableV2 && opName == "insert_instruction") {
            if (!opObj.contains("instruction")) {
                if (errorOut) *errorOut = "insert_instruction requires field 'instruction'.";
                return false;
            }
            InstructionPos insertPos;
            if (!resolveInsertionTarget(story, instructionOrigins, opObj, insertPos, errorOut)) return false;
            if (insertPos.nodeIndex < 0 || static_cast<size_t>(insertPos.nodeIndex) >= story.nodes.size()) {
                if (errorOut) *errorOut = "insert_instruction target node is invalid.";
                return false;
            }

            std::unique_ptr<InstructionT> newInstr;
            if (!buildInstructionFromJson(opObj["instruction"], story, newInstr, errorOut)) return false;

            auto& nodeLines = story.nodes[static_cast<size_t>(insertPos.nodeIndex)]->lines;
            nodeLines.insert(nodeLines.begin() + insertPos.instrIndex, std::move(newInstr));
            instructionOrigins[static_cast<size_t>(insertPos.nodeIndex)]
                .insert(instructionOrigins[static_cast<size_t>(insertPos.nodeIndex)].begin() + insertPos.instrIndex, "");
            continue;
        }

        if (enableV2 && opName == "delete_instruction") {
            std::string instructionId;
            if (!readRequiredString(opObj, "instruction_id", instructionId, opName, errorOut)) return false;
            InstructionPos pos = findInstructionByOriginId(instructionOrigins, instructionId);
            if (!isValidPos(pos)) {
                if (errorOut) *errorOut = "Unknown instruction_id: " + instructionId;
                return false;
            }
            auto& nodeLines = story.nodes[static_cast<size_t>(pos.nodeIndex)]->lines;
            nodeLines.erase(nodeLines.begin() + pos.instrIndex);
            instructionOrigins[static_cast<size_t>(pos.nodeIndex)]
                .erase(instructionOrigins[static_cast<size_t>(pos.nodeIndex)].begin() + pos.instrIndex);
            continue;
        }

        if (enableV2 && opName == "move_instruction") {
            std::string instructionId;
            if (!readRequiredString(opObj, "instruction_id", instructionId, opName, errorOut)) return false;
            InstructionPos srcPos = findInstructionByOriginId(instructionOrigins, instructionId);
            if (!isValidPos(srcPos)) {
                if (errorOut) *errorOut = "Unknown instruction_id: " + instructionId;
                return false;
            }

            InstructionPos dstPos;
            if (!resolveInsertionTarget(story, instructionOrigins, opObj, dstPos, errorOut)) return false;
            if (srcPos.nodeIndex == dstPos.nodeIndex && srcPos.instrIndex < dstPos.instrIndex) {
                dstPos.instrIndex -= 1;
            }
            if (srcPos.nodeIndex == dstPos.nodeIndex && srcPos.instrIndex == dstPos.instrIndex) {
                continue;
            }

            auto movingInstr = std::move(story.nodes[static_cast<size_t>(srcPos.nodeIndex)]->lines[static_cast<size_t>(srcPos.instrIndex)]);
            auto movingOrigin = instructionOrigins[static_cast<size_t>(srcPos.nodeIndex)][static_cast<size_t>(srcPos.instrIndex)];

            auto& srcLines = story.nodes[static_cast<size_t>(srcPos.nodeIndex)]->lines;
            auto& srcOrigins = instructionOrigins[static_cast<size_t>(srcPos.nodeIndex)];
            srcLines.erase(srcLines.begin() + srcPos.instrIndex);
            srcOrigins.erase(srcOrigins.begin() + srcPos.instrIndex);

            auto& dstLines = story.nodes[static_cast<size_t>(dstPos.nodeIndex)]->lines;
            auto& dstOrigins = instructionOrigins[static_cast<size_t>(dstPos.nodeIndex)];
            dstLines.insert(dstLines.begin() + dstPos.instrIndex, std::move(movingInstr));
            dstOrigins.insert(dstOrigins.begin() + dstPos.instrIndex, std::move(movingOrigin));
            continue;
        }

        if (errorOut) *errorOut = "Unsupported op: " + opName;
        return false;
    }
    if (!validateGraphIntegrity(story, errorOut)) return false;

    if (preserveLineIds && outLineIdMap) {
        json mapJson;
        mapJson["format"] = kLineIdMapFormat;
        mapJson["version"] = kLineIdMapVersion;
        mapJson["entries"] = json::object();

        for (size_t ni = 0; ni < story.nodes.size(); ++ni) {
            const auto& node = story.nodes[ni];
            if (!node) continue;
            for (size_t ii = 0; ii < node->lines.size(); ++ii) {
                const auto& instr = node->lines[ii];
                if (!instr) continue;
                if (instr->data.type != OpData::Line && instr->data.type != OpData::Choice) continue;
                if (ni >= instructionOrigins.size() || ii >= instructionOrigins[ni].size()) continue;
                const std::string& originId = instructionOrigins[ni][ii];
                if (originId.empty()) continue; // inserted instruction -> new line_id
                auto it = snapshotLineIdByOrigin.find(originId);
                if (it == snapshotLineIdByOrigin.end()) continue;
                mapJson["entries"][makeInstructionId(ni, ii)] = it->second;
            }
        }
        *outLineIdMap = std::move(mapJson);
    }

    return true;
}

std::string escapeString(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (char c : value) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
        }
    }
    return out;
}

std::string quote(const std::string& value) {
    return "\"" + escapeString(value) + "\"";
}

bool renderValue(const ValueDataUnion& value, const StoryT& story, std::string& out, std::string* errorOut);

bool renderExpression(const ExpressionT* expr, const StoryT& story, std::string& out, std::string* errorOut) {
    if (!expr) {
        if (errorOut) *errorOut = "Encountered null expression.";
        return false;
    }

    std::vector<std::string> stack;
    for (const auto& tok : expr->tokens) {
        if (!tok) continue;
        switch (tok->op) {
        case ExprOp::PushLiteral: {
            std::string literal;
            if (!renderValue(tok->literal_value, story, literal, errorOut)) return false;
            stack.push_back(literal);
            break;
        }
        case ExprOp::PushVar:
            stack.push_back(poolStr(story, tok->var_name_id));
            break;
        case ExprOp::PushVisitCount:
            stack.push_back("visit_count(" + poolStr(story, tok->var_name_id) + ")");
            break;
        case ExprOp::PushVisited:
            stack.push_back("visited(" + poolStr(story, tok->var_name_id) + ")");
            break;
        case ExprOp::ListLength:
            stack.push_back("len(" + poolStr(story, tok->var_name_id) + ")");
            break;

        case ExprOp::Negate:
        case ExprOp::Not: {
            if (stack.empty()) {
                if (errorOut) *errorOut = "Malformed expression: unary operator stack underflow.";
                return false;
            }
            const std::string value = stack.back();
            stack.pop_back();
            if (tok->op == ExprOp::Negate) {
                stack.push_back("(-(" + value + "))");
            } else {
                stack.push_back("(not (" + value + "))");
            }
            break;
        }

        default: {
            if (stack.size() < 2) {
                if (errorOut) *errorOut = "Malformed expression: binary operator stack underflow.";
                return false;
            }
            const std::string rhs = stack.back();
            stack.pop_back();
            const std::string lhs = stack.back();
            stack.pop_back();

            const char* op = nullptr;
            switch (tok->op) {
            case ExprOp::Add: op = "+"; break;
            case ExprOp::Sub: op = "-"; break;
            case ExprOp::Mul: op = "*"; break;
            case ExprOp::Div: op = "/"; break;
            case ExprOp::Mod: op = "%"; break;
            case ExprOp::CmpEq: op = "=="; break;
            case ExprOp::CmpNe: op = "!="; break;
            case ExprOp::CmpGt: op = ">"; break;
            case ExprOp::CmpLt: op = "<"; break;
            case ExprOp::CmpGe: op = ">="; break;
            case ExprOp::CmpLe: op = "<="; break;
            case ExprOp::And: op = "and"; break;
            case ExprOp::Or: op = "or"; break;
            case ExprOp::ListContains: op = "in"; break;
            default: break;
            }
            if (!op) {
                if (errorOut) *errorOut = "Unsupported expression opcode in script emitter.";
                return false;
            }
            stack.push_back("(" + lhs + " " + op + " " + rhs + ")");
            break;
        }
        }
    }

    if (stack.size() != 1) {
        if (errorOut) *errorOut = "Malformed expression: final stack size is not 1.";
        return false;
    }
    out = stack.back();
    return true;
}

bool renderValue(const ValueDataUnion& value, const StoryT& story, std::string& out, std::string* errorOut) {
    switch (value.type) {
    case ValueData::BoolValue: {
        const auto* v = value.AsBoolValue();
        out = (v && v->val) ? "true" : "false";
        return true;
    }
    case ValueData::IntValue: {
        const auto* v = value.AsIntValue();
        out = std::to_string(v ? v->val : 0);
        return true;
    }
    case ValueData::FloatValue: {
        const auto* v = value.AsFloatValue();
        std::ostringstream oss;
        oss << (v ? v->val : 0.0f);
        out = oss.str();
        return true;
    }
    case ValueData::StringRef: {
        const auto* v = value.AsStringRef();
        out = quote(poolStr(story, v ? v->index : -1));
        return true;
    }
    case ValueData::ListValue: {
        const auto* v = value.AsListValue();
        std::string text = "[";
        if (v) {
            for (size_t i = 0; i < v->items.size(); ++i) {
                if (i > 0) text += ", ";
                text += quote(poolStr(story, v->items[i]));
            }
        }
        text += "]";
        out = text;
        return true;
    }
    default:
        if (errorOut) *errorOut = "ValueData::NONE cannot be rendered to script literal.";
        return false;
    }
}

bool renderExpressionList(const std::vector<std::unique_ptr<ExpressionT>>& exprs,
                          const StoryT& story,
                          std::string& out,
                          std::string* errorOut) {
    std::ostringstream oss;
    for (size_t i = 0; i < exprs.size(); ++i) {
        std::string e;
        if (!renderExpression(exprs[i].get(), story, e, errorOut)) return false;
        if (i > 0) oss << ", ";
        oss << e;
    }
    out = oss.str();
    return true;
}

const char* assignOpSymbol(AssignOp op) {
    switch (op) {
    case AssignOp::Assign: return "=";
    case AssignOp::Append: return "+=";
    case AssignOp::Remove: return "-=";
    default: return "=";
    }
}

const char* operatorSymbol(Operator op) {
    switch (op) {
    case Operator::Equal: return "==";
    case Operator::NotEqual: return "!=";
    case Operator::Greater: return ">";
    case Operator::Less: return "<";
    case Operator::GreaterOrEqual: return ">=";
    case Operator::LessOrEqual: return "<=";
    default: return "==";
    }
}

bool renderInstructionLine(const InstructionT& instr,
                           const StoryT& story,
                           std::string& out,
                           std::string* errorOut) {
    switch (instr.data.type) {
    case OpData::Line: {
        const auto* line = instr.data.AsLine();
        if (!line) return false;
        std::ostringstream oss;
        if (line->character_id >= 0) {
            oss << poolStr(story, line->character_id) << " ";
        }
        oss << quote(poolStr(story, line->text_id));

        bool hasVoiceTag = false;
        for (const auto& tag : line->tags) {
            if (!tag) continue;
            const std::string key = poolStr(story, tag->key_id);
            const std::string value = poolStr(story, tag->value_id);
            oss << " #" << key;
            if (!value.empty()) {
                oss << ":" << value;
            }
            if (key == "voice") hasVoiceTag = true;
        }
        if (!hasVoiceTag && line->voice_asset_id >= 0) {
            oss << " #voice:" << poolStr(story, line->voice_asset_id);
        }
        out = oss.str();
        return true;
    }
    case OpData::Choice: {
        const auto* choice = instr.data.AsChoice();
        if (!choice) return false;
        std::ostringstream oss;
        oss << quote(poolStr(story, choice->text_id))
            << " -> " << poolStr(story, choice->target_node_name_id);
        if (choice->condition_var_id >= 0) {
            oss << " if " << poolStr(story, choice->condition_var_id);
        }
        if (choice->choice_modifier == ChoiceModifier::Once) oss << " #once";
        else if (choice->choice_modifier == ChoiceModifier::Sticky) oss << " #sticky";
        else if (choice->choice_modifier == ChoiceModifier::Fallback) oss << " #fallback";
        out = oss.str();
        return true;
    }
    case OpData::Jump: {
        const auto* jump = instr.data.AsJump();
        if (!jump) return false;
        std::ostringstream oss;
        oss << (jump->is_call ? "call " : "jump ") << poolStr(story, jump->target_node_name_id);
        if (!jump->arg_exprs.empty()) {
            if (!jump->is_call) {
                if (errorOut) *errorOut = "Jump instruction with args is not representable in .gyeol syntax.";
                return false;
            }
            std::string args;
            if (!renderExpressionList(jump->arg_exprs, story, args, errorOut)) return false;
            oss << "(" << args << ")";
        }
        out = oss.str();
        return true;
    }
    case OpData::Command: {
        const auto* cmd = instr.data.AsCommand();
        if (!cmd) return false;
        std::ostringstream oss;
        oss << "@ " << poolStr(story, cmd->type_id);
        for (int32_t p : cmd->params) {
            oss << " " << quote(poolStr(story, p));
        }
        out = oss.str();
        return true;
    }
    case OpData::SetVar: {
        const auto* setVar = instr.data.AsSetVar();
        if (!setVar) return false;
        std::string rhs;
        if (setVar->expr) {
            if (!renderExpression(setVar->expr.get(), story, rhs, errorOut)) return false;
        } else {
            if (!renderValue(setVar->value, story, rhs, errorOut)) return false;
        }
        out = "$ " + poolStr(story, setVar->var_name_id) + " " + assignOpSymbol(setVar->assign_op) + " " + rhs;
        return true;
    }
    case OpData::Condition: {
        const auto* cond = instr.data.AsCondition();
        if (!cond) return false;
        if (cond->true_jump_node_id < 0) {
            if (errorOut) *errorOut = "Condition without true branch is not representable.";
            return false;
        }
        std::string exprText;
        if (cond->cond_expr) {
            if (!renderExpression(cond->cond_expr.get(), story, exprText, errorOut)) return false;
        } else {
            std::string lhs;
            std::string rhs;
            if (cond->lhs_expr) {
                if (!renderExpression(cond->lhs_expr.get(), story, lhs, errorOut)) return false;
            } else {
                lhs = poolStr(story, cond->var_name_id);
            }
            if (cond->rhs_expr) {
                if (!renderExpression(cond->rhs_expr.get(), story, rhs, errorOut)) return false;
            } else {
                if (!renderValue(cond->compare_value, story, rhs, errorOut)) return false;
            }
            exprText = lhs + " " + operatorSymbol(cond->op) + " " + rhs;
        }

        std::ostringstream oss;
        oss << "if " << exprText << " -> " << poolStr(story, cond->true_jump_node_id);
        if (cond->false_jump_node_id >= 0) {
            oss << " else " << poolStr(story, cond->false_jump_node_id);
        }
        out = oss.str();
        return true;
    }
    case OpData::Random:
        if (errorOut) *errorOut = "Random blocks are emitted by node-level formatter.";
        return false;
    case OpData::Return: {
        const auto* ret = instr.data.AsReturn();
        if (!ret) return false;
        if (ret->expr) {
            std::string exprText;
            if (!renderExpression(ret->expr.get(), story, exprText, errorOut)) return false;
            out = "return " + exprText;
            return true;
        }
        if (ret->value.type != ValueData::NONE) {
            std::string valueText;
            if (!renderValue(ret->value, story, valueText, errorOut)) return false;
            out = "return " + valueText;
            return true;
        }
        out = "return";
        return true;
    }
    case OpData::CallWithReturn: {
        const auto* cwr = instr.data.AsCallWithReturn();
        if (!cwr) return false;
        std::ostringstream oss;
        oss << "$ " << poolStr(story, cwr->return_var_name_id)
            << " = call " << poolStr(story, cwr->target_node_name_id);
        if (!cwr->arg_exprs.empty()) {
            std::string args;
            if (!renderExpressionList(cwr->arg_exprs, story, args, errorOut)) return false;
            oss << "(" << args << ")";
        }
        out = oss.str();
        return true;
    }
    default:
        if (errorOut) *errorOut = "Unsupported instruction while writing canonical script.";
        return false;
    }
}

void appendNodeHeader(std::ostringstream& oss, const NodeT& node, const StoryT& story) {
    oss << "label " << node.name;
    if (!node.param_ids.empty()) {
        oss << "(";
        for (size_t i = 0; i < node.param_ids.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << poolStr(story, node.param_ids[i]);
        }
        oss << ")";
    }
    for (const auto& t : node.tags) {
        if (!t) continue;
        const std::string key = poolStr(story, t->key_id);
        const std::string value = poolStr(story, t->value_id);
        oss << " #" << key;
        if (!value.empty()) {
            oss << "=" << value;
        }
    }
    oss << ":\n";
}

bool emitNodeBody(std::ostringstream& oss, const NodeT& node, const StoryT& story, std::string* errorOut) {
    size_t i = 0;
    while (i < node.lines.size()) {
        const auto& instr = node.lines[i];
        if (!instr) {
            ++i;
            continue;
        }

        if (instr->data.type == OpData::Choice) {
            oss << "    menu:\n";
            while (i < node.lines.size() && node.lines[i] && node.lines[i]->data.type == OpData::Choice) {
                std::string lineText;
                if (!renderInstructionLine(*node.lines[i], story, lineText, errorOut)) return false;
                oss << "        " << lineText << "\n";
                ++i;
            }
            continue;
        }

        if (instr->data.type == OpData::Random) {
            const auto* rnd = instr->data.AsRandom();
            if (!rnd) {
                if (errorOut) *errorOut = "Malformed random instruction.";
                return false;
            }
            oss << "    random:\n";
            for (const auto& branch : rnd->branches) {
                if (!branch) continue;
                oss << "        " << branch->weight << " -> "
                    << poolStr(story, branch->target_node_name_id) << "\n";
            }
            ++i;
            continue;
        }

        std::string lineText;
        if (!renderInstructionLine(*instr, story, lineText, errorOut)) return false;
        oss << "    " << lineText << "\n";
        ++i;
    }
    return true;
}

const char* choiceModifierName(ChoiceModifier modifier) {
    switch (modifier) {
    case ChoiceModifier::Default: return "Default";
    case ChoiceModifier::Once: return "Once";
    case ChoiceModifier::Sticky: return "Sticky";
    case ChoiceModifier::Fallback: return "Fallback";
    default: return "Default";
    }
}

const char* assignOpName(AssignOp op) {
    switch (op) {
    case AssignOp::Assign: return "Assign";
    case AssignOp::Append: return "Append";
    case AssignOp::Remove: return "Remove";
    default: return "Assign";
    }
}

json valueToJson(const ValueDataUnion& value, const StoryT& story) {
    switch (value.type) {
    case ValueData::BoolValue:
        return json{
            {"type", "Bool"},
            {"val", value.AsBoolValue() ? value.AsBoolValue()->val : false},
        };
    case ValueData::IntValue:
        return json{
            {"type", "Int"},
            {"val", value.AsIntValue() ? value.AsIntValue()->val : 0},
        };
    case ValueData::FloatValue:
        return json{
            {"type", "Float"},
            {"val", value.AsFloatValue() ? value.AsFloatValue()->val : 0.0f},
        };
    case ValueData::StringRef:
        return json{
            {"type", "String"},
            {"val", poolStr(story, value.AsStringRef() ? value.AsStringRef()->index : -1)},
        };
    case ValueData::ListValue: {
        json list = json::array();
        if (auto* lv = value.AsListValue()) {
            for (int32_t item : lv->items) {
                list.push_back(poolStr(story, item));
            }
        }
        return json{
            {"type", "List"},
            {"val", std::move(list)},
        };
    }
    default:
        return json{
            {"type", "None"},
        };
    }
}

std::string conditionExprText(const ConditionT& cond, const StoryT& story) {
    std::string out;
    std::string ignored;
    if (cond.cond_expr) {
        if (renderExpression(cond.cond_expr.get(), story, out, &ignored)) {
            return out;
        }
        return "";
    }

    std::string lhs;
    if (cond.lhs_expr) {
        if (!renderExpression(cond.lhs_expr.get(), story, lhs, &ignored)) return "";
    } else {
        lhs = poolStr(story, cond.var_name_id);
    }

    std::string rhs;
    if (cond.rhs_expr) {
        if (!renderExpression(cond.rhs_expr.get(), story, rhs, &ignored)) return "";
    } else {
        if (!renderValue(cond.compare_value, story, rhs, &ignored)) return "";
    }

    return lhs + " " + operatorSymbol(cond.op) + " " + rhs;
}

json buildInstructionDoc(const InstructionT& instr, const StoryT& story, size_t nodeIndex, size_t instrIndex) {
    json out;
    out["instruction_id"] = makeInstructionId(nodeIndex, instrIndex);

    switch (instr.data.type) {
    case OpData::Line: {
        out["type"] = "Line";
        const auto* line = instr.data.AsLine();
        if (!line) break;
        out["text"] = poolStr(story, line->text_id);
        if (line->character_id >= 0) {
            out["character"] = poolStr(story, line->character_id);
        }
        if (line->voice_asset_id >= 0) {
            out["voice_asset"] = poolStr(story, line->voice_asset_id);
        }
        json tags = json::array();
        for (const auto& tag : line->tags) {
            if (!tag) continue;
            tags.push_back({
                {"key", poolStr(story, tag->key_id)},
                {"value", poolStr(story, tag->value_id)},
            });
        }
        out["tags"] = std::move(tags);
        break;
    }
    case OpData::Choice: {
        out["type"] = "Choice";
        const auto* choice = instr.data.AsChoice();
        if (!choice) break;
        out["text"] = poolStr(story, choice->text_id);
        out["target_node"] = poolStr(story, choice->target_node_name_id);
        out["choice_modifier"] = choiceModifierName(choice->choice_modifier);
        if (choice->condition_var_id >= 0) {
            out["condition_var"] = poolStr(story, choice->condition_var_id);
        }
        break;
    }
    case OpData::Jump: {
        out["type"] = "Jump";
        const auto* jump = instr.data.AsJump();
        if (!jump) break;
        out["target_node"] = poolStr(story, jump->target_node_name_id);
        out["is_call"] = jump->is_call;
        break;
    }
    case OpData::Command: {
        out["type"] = "Command";
        const auto* command = instr.data.AsCommand();
        if (!command) break;
        out["command_type"] = poolStr(story, command->type_id);
        json params = json::array();
        for (int32_t p : command->params) {
            params.push_back(poolStr(story, p));
        }
        out["params"] = std::move(params);
        break;
    }
    case OpData::SetVar: {
        out["type"] = "SetVar";
        const auto* setVar = instr.data.AsSetVar();
        if (!setVar) break;
        out["var_name"] = poolStr(story, setVar->var_name_id);
        out["assign_op"] = assignOpName(setVar->assign_op);
        if (setVar->expr) {
            std::string exprText;
            std::string ignored;
            if (renderExpression(setVar->expr.get(), story, exprText, &ignored)) {
                out["expr_text"] = exprText;
            }
        } else {
            out["value"] = valueToJson(setVar->value, story);
        }
        break;
    }
    case OpData::Condition: {
        out["type"] = "Condition";
        const auto* cond = instr.data.AsCondition();
        if (!cond) break;
        out["expr_text"] = conditionExprText(*cond, story);
        if (cond->true_jump_node_id >= 0) {
            out["true_target"] = poolStr(story, cond->true_jump_node_id);
        }
        if (cond->false_jump_node_id >= 0) {
            out["false_target"] = poolStr(story, cond->false_jump_node_id);
        }
        break;
    }
    case OpData::Random: {
        out["type"] = "Random";
        const auto* rnd = instr.data.AsRandom();
        json branches = json::array();
        if (rnd) {
            for (const auto& branch : rnd->branches) {
                if (!branch) continue;
                branches.push_back({
                    {"weight", branch->weight},
                    {"target_node", poolStr(story, branch->target_node_name_id)},
                });
            }
        }
        out["branches"] = std::move(branches);
        break;
    }
    case OpData::Return: {
        out["type"] = "Return";
        const auto* ret = instr.data.AsReturn();
        if (!ret) break;
        if (ret->expr) {
            std::string exprText;
            std::string ignored;
            if (renderExpression(ret->expr.get(), story, exprText, &ignored)) {
                out["expr_text"] = exprText;
            }
        } else if (ret->value.type != ValueData::NONE) {
            out["value"] = valueToJson(ret->value, story);
        }
        break;
    }
    case OpData::CallWithReturn: {
        out["type"] = "CallWithReturn";
        const auto* cwr = instr.data.AsCallWithReturn();
        if (!cwr) break;
        out["target_node"] = poolStr(story, cwr->target_node_name_id);
        out["return_var"] = poolStr(story, cwr->return_var_name_id);
        json argExprs = json::array();
        for (const auto& arg : cwr->arg_exprs) {
            std::string exprText;
            std::string ignored;
            if (arg && renderExpression(arg.get(), story, exprText, &ignored)) {
                argExprs.push_back(exprText);
            } else {
                argExprs.push_back("");
            }
        }
        out["arg_exprs"] = std::move(argExprs);
        break;
    }
    default:
        out["type"] = "Unknown";
        break;
    }

    return out;
}

} // namespace

nlohmann::json buildGraphDoc(const StoryT& story) {
    json root;
    root["format"] = kGraphDocFormat;
    root["version"] = kGraphDocVersion;
    root["story_version"] = story.version;
    root["start_node"] = story.start_node_name;

    root["nodes"] = json::array();
    for (size_t nodeIndex = 0; nodeIndex < story.nodes.size(); ++nodeIndex) {
        const auto& nodePtr = story.nodes[nodeIndex];
        if (!nodePtr) continue;
        const auto& node = *nodePtr;
        json n;
        n["name"] = node.name;
        n["instruction_count"] = node.lines.size();
        n["params"] = json::array();
        for (int32_t pid : node.param_ids) {
            n["params"].push_back(poolStr(story, pid));
        }
        n["tags"] = json::array();
        for (const auto& tag : node.tags) {
            if (!tag) continue;
            n["tags"].push_back({
                {"key", poolStr(story, tag->key_id)},
                {"value", poolStr(story, tag->value_id)},
            });
        }
        n["instructions"] = json::array();
        for (size_t instrIndex = 0; instrIndex < node.lines.size(); ++instrIndex) {
            const auto& instrPtr = node.lines[instrIndex];
            if (!instrPtr) continue;
            n["instructions"].push_back(buildInstructionDoc(*instrPtr, story, nodeIndex, instrIndex));
        }
        root["nodes"].push_back(std::move(n));
    }

    root["edges"] = json::array();
    forEachEdgeConst(story, [&](const EdgeView& edge) {
        json e = {
            {"edge_id", edge.edgeId},
            {"from", edge.from},
            {"to", edge.to},
            {"type", edge.type},
        };
        if (!edge.label.empty()) {
            e["label"] = edge.label;
        }
        root["edges"].push_back(std::move(e));
    });

    return root;
}

bool exportGraphJson(const StoryT& story, const std::string& outputPath, std::string* errorOut) {
    std::ofstream ofs(outputPath);
    if (!ofs.is_open()) {
        if (errorOut) *errorOut = "Failed to write graph JSON: " + outputPath;
        return false;
    }
    ofs << buildGraphDoc(story).dump(2);
    return true;
}

bool writeLineIdMap(const nlohmann::json& lineIdMap, const std::string& outputPath, std::string* errorOut) {
    std::ofstream ofs(outputPath);
    if (!ofs.is_open()) {
        if (errorOut) *errorOut = "Failed to write line-id map JSON: " + outputPath;
        return false;
    }
    ofs << lineIdMap.dump(2);
    return true;
}

std::string toCanonicalScript(const StoryT& story, std::string* errorOut) {
    std::ostringstream oss;

    for (const auto& ch : story.characters) {
        if (!ch) continue;
        oss << "character " << poolStr(story, ch->name_id) << ":\n";
        for (const auto& prop : ch->properties) {
            if (!prop) continue;
            oss << "    " << poolStr(story, prop->key_id) << ": "
                << quote(poolStr(story, prop->value_id)) << "\n";
        }
        oss << "\n";
    }

    for (const auto& gv : story.global_vars) {
        if (!gv) continue;
        std::string rhs;
        if (!renderValue(gv->value, story, rhs, errorOut)) return "";
        oss << "$ " << poolStr(story, gv->var_name_id) << " = " << rhs << "\n";
    }
    if (!story.global_vars.empty()) {
        oss << "\n";
    }

    for (size_t ni = 0; ni < story.nodes.size(); ++ni) {
        const auto& node = story.nodes[ni];
        if (!node) continue;
        appendNodeHeader(oss, *node, story);
        if (!emitNodeBody(oss, *node, story, errorOut)) return "";
        if (ni + 1 < story.nodes.size()) {
            oss << "\n";
        }
    }

    return oss.str();
}

bool writeCanonicalScript(const StoryT& story, const std::string& outputPath, std::string* errorOut) {
    const std::string script = toCanonicalScript(story, errorOut);
    if (script.empty() && errorOut && !errorOut->empty()) {
        return false;
    }

    std::ofstream ofs(outputPath);
    if (!ofs.is_open()) {
        if (errorOut) *errorOut = "Failed to write canonical script: " + outputPath;
        return false;
    }
    ofs << script;
    return true;
}

bool validateGraphPatchJson(const StoryT& story, const json& patch, std::string* errorOut) {
    auto working = cloneStory(story);
    if (!working) {
        if (errorOut) *errorOut = "Failed to clone story for validation.";
        return false;
    }
    if (!applyPatchOps(*working, patch, false, nullptr, errorOut)) {
        return false;
    }

    std::string canonicalError;
    const std::string canonical = toCanonicalScript(*working, &canonicalError);
    if (canonical.empty() && !canonicalError.empty()) {
        if (errorOut) *errorOut = "Patched story cannot be emitted canonically: " + canonicalError;
        return false;
    }

    Parser parser;
    if (!parser.parseString(canonical, "<graph-patch-validation>")) {
        std::string parseError = parser.getError();
        if (parseError.empty() && !parser.getErrors().empty()) {
            parseError = parser.getErrors().front();
        }
        if (errorOut) *errorOut = "Canonical round-trip parse failed: " + parseError;
        return false;
    }
    return true;
}

bool validateGraphPatchFile(const StoryT& story, const std::string& patchPath, std::string* errorOut) {
    json patch;
    if (!loadJsonFile(patchPath, patch, errorOut)) return false;
    return validateGraphPatchJson(story, patch, errorOut);
}

bool applyGraphPatchJsonWithOptions(StoryT& story,
                                    const json& patch,
                                    bool preserveLineIds,
                                    nlohmann::json* outLineIdMap,
                                    std::string* errorOut) {
    auto working = cloneStory(story);
    if (!working) {
        if (errorOut) *errorOut = "Failed to clone story for patch apply.";
        return false;
    }

    json generatedMap;
    json* mapOut = nullptr;
    if (preserveLineIds) {
        mapOut = outLineIdMap ? outLineIdMap : &generatedMap;
    }

    if (!applyPatchOps(*working, patch, preserveLineIds, mapOut, errorOut)) {
        return false;
    }

    std::string canonicalError;
    const std::string canonical = toCanonicalScript(*working, &canonicalError);
    if (canonical.empty() && !canonicalError.empty()) {
        if (errorOut) *errorOut = "Patched story cannot be emitted canonically: " + canonicalError;
        return false;
    }

    Parser parser;
    if (!parser.parseString(canonical, "<graph-patch-apply>")) {
        std::string parseError = parser.getError();
        if (parseError.empty() && !parser.getErrors().empty()) {
            parseError = parser.getErrors().front();
        }
        if (errorOut) *errorOut = "Canonical round-trip parse failed: " + parseError;
        return false;
    }

    story = std::move(*working);
    return true;
}

bool applyGraphPatchJson(StoryT& story, const json& patch, std::string* errorOut) {
    return applyGraphPatchJsonWithOptions(story, patch, false, nullptr, errorOut);
}

bool applyGraphPatchFileWithOptions(StoryT& story,
                                    const std::string& patchPath,
                                    bool preserveLineIds,
                                    const std::string& lineIdMapPath,
                                    std::string* errorOut) {
    if (!preserveLineIds && !lineIdMapPath.empty()) {
        if (errorOut) *errorOut = "lineIdMapPath is only valid when preserveLineIds=true.";
        return false;
    }
    if (preserveLineIds && lineIdMapPath.empty()) {
        if (errorOut) *errorOut = "preserveLineIds requires a non-empty lineIdMapPath.";
        return false;
    }

    json patch;
    if (!loadJsonFile(patchPath, patch, errorOut)) return false;

    json lineIdMap;
    if (!applyGraphPatchJsonWithOptions(
            story,
            patch,
            preserveLineIds,
            preserveLineIds ? &lineIdMap : nullptr,
            errorOut)) {
        return false;
    }

    if (!preserveLineIds) return true;
    return writeLineIdMap(lineIdMap, lineIdMapPath, errorOut);
}

bool applyGraphPatchFile(StoryT& story, const std::string& patchPath, std::string* errorOut) {
    return applyGraphPatchFileWithOptions(story, patchPath, false, "", errorOut);
}

} // namespace Gyeol::GraphTools
