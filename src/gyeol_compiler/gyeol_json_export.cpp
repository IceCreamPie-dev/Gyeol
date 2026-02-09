#include "gyeol_json_export.h"

using json = nlohmann::json;
using namespace ICPDev::Gyeol::Schema;

namespace Gyeol {

// Empty string for out-of-range pool access
static const std::string EMPTY_STRING;

const std::string& JsonExport::poolStr(const std::vector<std::string>& pool, int32_t index) {
    if (index < 0 || static_cast<size_t>(index) >= pool.size()) {
        return EMPTY_STRING;
    }
    return pool[static_cast<size_t>(index)];
}

// ============================================================
// Value Data serialization
// ============================================================

json JsonExport::serializeValueData(const ValueDataUnion& value,
                                    const std::vector<std::string>& pool) {
    switch (value.type) {
    case ValueData::BoolValue: {
        auto* v = value.AsBoolValue();
        return json{{"type", "Bool"}, {"val", v ? v->val : false}};
    }
    case ValueData::IntValue: {
        auto* v = value.AsIntValue();
        return json{{"type", "Int"}, {"val", v ? v->val : 0}};
    }
    case ValueData::FloatValue: {
        auto* v = value.AsFloatValue();
        return json{{"type", "Float"}, {"val", v ? v->val : 0.0f}};
    }
    case ValueData::StringRef: {
        auto* v = value.AsStringRef();
        int32_t idx = v ? v->index : 0;
        return json{{"type", "String"}, {"val", poolStr(pool, idx)}};
    }
    case ValueData::ListValue: {
        auto* v = value.AsListValue();
        json items = json::array();
        if (v) {
            for (auto idx : v->items) {
                items.push_back(poolStr(pool, idx));
            }
        }
        return json{{"type", "List"}, {"val", items}};
    }
    default:
        return nullptr;
    }
}

// ============================================================
// Expression serialization
// ============================================================

json JsonExport::serializeExprToken(const ExprTokenT& token,
                                    const std::vector<std::string>& pool) {
    json obj;
    obj["op"] = EnumNameExprOp(token.op);

    if (token.op == ExprOp::PushLiteral && token.literal_value.type != ValueData::NONE) {
        obj["value"] = serializeValueData(token.literal_value, pool);
    }

    if (token.var_name_id >= 0) {
        obj["var_name"] = poolStr(pool, token.var_name_id);
    }

    return obj;
}

json JsonExport::serializeExpression(const ExpressionT* expr,
                                     const std::vector<std::string>& pool) {
    if (!expr) return nullptr;

    json tokens = json::array();
    for (const auto& token : expr->tokens) {
        if (token) {
            tokens.push_back(serializeExprToken(*token, pool));
        }
    }

    return json{{"tokens", tokens}};
}

// ============================================================
// Tag serialization
// ============================================================

json JsonExport::serializeTag(const TagT& tag,
                              const std::vector<std::string>& pool) {
    return json{
        {"key", poolStr(pool, tag.key_id)},
        {"value", poolStr(pool, tag.value_id)}
    };
}

json JsonExport::serializeTags(
        const std::vector<std::unique_ptr<TagT>>& tags,
        const std::vector<std::string>& pool) {
    json arr = json::array();
    for (const auto& tag : tags) {
        if (tag) {
            arr.push_back(serializeTag(*tag, pool));
        }
    }
    return arr;
}

// ============================================================
// SetVar serialization
// ============================================================

json JsonExport::serializeSetVar(const SetVarT& setVar,
                                 const std::vector<std::string>& pool) {
    json obj;
    obj["type"] = "SetVar";
    obj["var_name"] = poolStr(pool, setVar.var_name_id);
    obj["assign_op"] = EnumNameAssignOp(setVar.assign_op);

    if (setVar.value.type != ValueData::NONE) {
        obj["value"] = serializeValueData(setVar.value, pool);
    } else {
        obj["value"] = nullptr;
    }

    obj["expr"] = serializeExpression(setVar.expr.get(), pool);

    return obj;
}

// ============================================================
// CharacterDef serialization
// ============================================================

json JsonExport::serializeCharacterDef(const CharacterDefT& charDef,
                                       const std::vector<std::string>& pool) {
    json obj;
    obj["name"] = poolStr(pool, charDef.name_id);
    obj["properties"] = serializeTags(charDef.properties, pool);
    return obj;
}

// ============================================================
// Instruction serialization
// ============================================================

json JsonExport::serializeInstruction(const InstructionT& instr,
                                      const std::vector<std::string>& pool) {
    const auto& data = instr.data;
    json obj;

    switch (data.type) {
    case OpData::Line: {
        auto* line = data.AsLine();
        if (!line) break;
        obj["type"] = "Line";
        obj["character"] = line->character_id >= 0
            ? json(poolStr(pool, line->character_id))
            : json(nullptr);
        obj["text"] = poolStr(pool, line->text_id);
        if (line->voice_asset_id >= 0) {
            obj["voice_asset"] = poolStr(pool, line->voice_asset_id);
        }
        if (!line->tags.empty()) {
            obj["tags"] = serializeTags(line->tags, pool);
        }
        break;
    }

    case OpData::Choice: {
        auto* choice = data.AsChoice();
        if (!choice) break;
        obj["type"] = "Choice";
        obj["text"] = poolStr(pool, choice->text_id);
        obj["target_node"] = poolStr(pool, choice->target_node_name_id);
        if (choice->condition_var_id >= 0) {
            obj["condition_var"] = poolStr(pool, choice->condition_var_id);
        }
        if (choice->choice_modifier != ChoiceModifier::Default) {
            obj["choice_modifier"] = EnumNameChoiceModifier(choice->choice_modifier);
        }
        break;
    }

    case OpData::Jump: {
        auto* jump = data.AsJump();
        if (!jump) break;
        obj["type"] = "Jump";
        obj["target_node"] = poolStr(pool, jump->target_node_name_id);
        obj["is_call"] = jump->is_call;
        if (!jump->arg_exprs.empty()) {
            json args = json::array();
            for (const auto& argExpr : jump->arg_exprs) {
                args.push_back(serializeExpression(argExpr.get(), pool));
            }
            obj["arg_exprs"] = args;
        }
        break;
    }

    case OpData::Command: {
        auto* cmd = data.AsCommand();
        if (!cmd) break;
        obj["type"] = "Command";
        obj["command_type"] = poolStr(pool, cmd->type_id);
        json params = json::array();
        for (auto paramId : cmd->params) {
            params.push_back(poolStr(pool, paramId));
        }
        obj["params"] = params;
        break;
    }

    case OpData::SetVar: {
        auto* sv = data.AsSetVar();
        if (!sv) break;
        obj = serializeSetVar(*sv, pool);
        break;
    }

    case OpData::Condition: {
        auto* cond = data.AsCondition();
        if (!cond) break;
        obj["type"] = "Condition";
        obj["op"] = EnumNameOperator(cond->op);
        obj["true_jump_node"] = poolStr(pool, cond->true_jump_node_id);
        obj["false_jump_node"] = poolStr(pool, cond->false_jump_node_id);

        // Full boolean expression (logical ops)
        if (cond->cond_expr) {
            obj["cond_expr"] = serializeExpression(cond->cond_expr.get(), pool);
        } else {
            // Expression-based LHS/RHS
            if (cond->lhs_expr) {
                obj["lhs_expr"] = serializeExpression(cond->lhs_expr.get(), pool);
            } else {
                obj["var_name"] = poolStr(pool, cond->var_name_id);
            }

            if (cond->rhs_expr) {
                obj["rhs_expr"] = serializeExpression(cond->rhs_expr.get(), pool);
            } else if (cond->compare_value.type != ValueData::NONE) {
                obj["compare_value"] = serializeValueData(cond->compare_value, pool);
            }
        }
        break;
    }

    case OpData::Random: {
        auto* rnd = data.AsRandom();
        if (!rnd) break;
        obj["type"] = "Random";
        json branches = json::array();
        for (const auto& branch : rnd->branches) {
            if (branch) {
                branches.push_back(json{
                    {"target_node", poolStr(pool, branch->target_node_name_id)},
                    {"weight", branch->weight}
                });
            }
        }
        obj["branches"] = branches;
        break;
    }

    case OpData::Return: {
        auto* ret = data.AsReturn();
        if (!ret) break;
        obj["type"] = "Return";
        obj["expr"] = serializeExpression(ret->expr.get(), pool);
        if (ret->value.type != ValueData::NONE) {
            obj["value"] = serializeValueData(ret->value, pool);
        }
        break;
    }

    case OpData::CallWithReturn: {
        auto* cwr = data.AsCallWithReturn();
        if (!cwr) break;
        obj["type"] = "CallWithReturn";
        obj["target_node"] = poolStr(pool, cwr->target_node_name_id);
        obj["return_var"] = poolStr(pool, cwr->return_var_name_id);
        if (!cwr->arg_exprs.empty()) {
            json args = json::array();
            for (const auto& argExpr : cwr->arg_exprs) {
                args.push_back(serializeExpression(argExpr.get(), pool));
            }
            obj["arg_exprs"] = args;
        }
        break;
    }

    default:
        obj["type"] = "Unknown";
        break;
    }

    return obj;
}

// ============================================================
// Node serialization
// ============================================================

json JsonExport::serializeNode(const NodeT& node,
                               const std::vector<std::string>& pool) {
    json obj;
    obj["name"] = node.name;

    // Parameters
    if (!node.param_ids.empty()) {
        json params = json::array();
        for (auto paramId : node.param_ids) {
            params.push_back(poolStr(pool, paramId));
        }
        obj["params"] = params;
    }

    // Node tags
    if (!node.tags.empty()) {
        obj["tags"] = serializeTags(node.tags, pool);
    }

    // Instructions
    json instructions = json::array();
    for (const auto& instr : node.lines) {
        if (instr) {
            instructions.push_back(serializeInstruction(*instr, pool));
        }
    }
    obj["instructions"] = instructions;

    return obj;
}

// ============================================================
// Top-level Story serialization
// ============================================================

json JsonExport::toJson(const StoryT& story) {
    json root;

    // Metadata
    root["format"] = "gyeol-json-ir";
    root["format_version"] = 1;
    root["version"] = story.version;
    root["start_node_name"] = story.start_node_name;

    // String pool (included for tools that need index-based access)
    root["string_pool"] = story.string_pool;

    // Line IDs (for localization)
    if (!story.line_ids.empty()) {
        root["line_ids"] = story.line_ids;
    }

    // Characters
    if (!story.characters.empty()) {
        json chars = json::array();
        for (const auto& charDef : story.characters) {
            if (charDef) {
                chars.push_back(serializeCharacterDef(*charDef, story.string_pool));
            }
        }
        root["characters"] = chars;
    }

    // Global variables
    if (!story.global_vars.empty()) {
        json vars = json::array();
        for (const auto& gv : story.global_vars) {
            if (gv) {
                vars.push_back(serializeSetVar(*gv, story.string_pool));
            }
        }
        root["global_vars"] = vars;
    }

    // Nodes
    json nodes = json::array();
    for (const auto& node : story.nodes) {
        if (node) {
            nodes.push_back(serializeNode(*node, story.string_pool));
        }
    }
    root["nodes"] = nodes;

    return root;
}

std::string JsonExport::toJsonString(const StoryT& story, int indent) {
    return toJson(story).dump(indent);
}

} // namespace Gyeol
