#include "gyeol_json_ir_reader.h"

#include <flatbuffers/flatbuffers.h>

#include <fstream>
#include <memory>
#include <unordered_map>

using json = nlohmann::json;
using namespace ICPDev::Gyeol::Schema;

namespace Gyeol {
namespace {

struct BuildContext {
    StoryT& story;
    std::unordered_map<std::string, int32_t> indexByString;
    std::vector<std::string> lineIds;

    explicit BuildContext(StoryT& s) : story(s) {}

    int32_t addString(const std::string& value, const std::string& lineId = "") {
        auto it = indexByString.find(value);
        if (it != indexByString.end()) {
            const size_t idx = static_cast<size_t>(it->second);
            if (!lineId.empty() && idx < lineIds.size() && lineIds[idx].empty()) {
                lineIds[idx] = lineId;
            }
            return it->second;
        }
        const int32_t idx = static_cast<int32_t>(story.string_pool.size());
        story.string_pool.push_back(value);
        lineIds.push_back(lineId);
        indexByString[value] = idx;
        return idx;
    }
};

bool setError(std::string* errorOut, const std::string& message) {
    if (errorOut) *errorOut = message;
    return false;
}

template <typename EnumType, typename NameFn, typename ValuesContainer>
bool parseEnumByName(const std::string& text, ValuesContainer&& values, NameFn nameFn, EnumType& out) {
    for (const auto& value : values) {
        const char* name = nameFn(value);
        if (name && text == name) {
            out = value;
            return true;
        }
    }
    return false;
}

bool parseTag(const json& spec, BuildContext& ctx, std::unique_ptr<TagT>& outTag, std::string* errorOut) {
    if (!spec.is_object()) {
        return setError(errorOut, "Tag must be an object.");
    }
    if (!spec.contains("key") || !spec["key"].is_string()) {
        return setError(errorOut, "Tag.key must be a string.");
    }
    const std::string key = spec["key"].get<std::string>();
    const std::string value = (spec.contains("value") && spec["value"].is_string())
        ? spec["value"].get<std::string>()
        : "";

    auto tag = std::make_unique<TagT>();
    tag->key_id = ctx.addString(key);
    tag->value_id = ctx.addString(value);
    outTag = std::move(tag);
    return true;
}

bool parseValueData(const json& spec, BuildContext& ctx, ValueDataUnion& outValue, std::string* errorOut);
bool parseExpression(const json& spec,
                     BuildContext& ctx,
                     std::unique_ptr<ExpressionT>& outExpr,
                     std::string* errorOut);

bool parseValueData(const json& spec, BuildContext& ctx, ValueDataUnion& outValue, std::string* errorOut) {
    if (spec.is_null()) {
        outValue.type = ValueData::NONE;
        outValue.value = nullptr;
        return true;
    }
    if (!spec.is_object() || !spec.contains("type") || !spec["type"].is_string()) {
        return setError(errorOut, "Value must be an object with string field 'type'.");
    }

    const std::string type = spec["type"].get<std::string>();
    if (!spec.contains("val")) {
        return setError(errorOut, "Value object must contain field 'val'.");
    }

    if (type == "Bool") {
        if (!spec["val"].is_boolean()) return setError(errorOut, "Bool value must be boolean.");
        BoolValueT value;
        value.val = spec["val"].get<bool>();
        outValue.Set(value);
        return true;
    }
    if (type == "Int") {
        if (!spec["val"].is_number_integer()) return setError(errorOut, "Int value must be integer.");
        IntValueT value;
        value.val = spec["val"].get<int32_t>();
        outValue.Set(value);
        return true;
    }
    if (type == "Float") {
        if (!spec["val"].is_number()) return setError(errorOut, "Float value must be number.");
        FloatValueT value;
        value.val = spec["val"].get<float>();
        outValue.Set(value);
        return true;
    }
    if (type == "String") {
        if (!spec["val"].is_string()) return setError(errorOut, "String value must be string.");
        StringRefT ref;
        ref.index = ctx.addString(spec["val"].get<std::string>());
        outValue.Set(ref);
        return true;
    }
    if (type == "List") {
        if (!spec["val"].is_array()) return setError(errorOut, "List value must be array.");
        ListValueT listValue;
        for (const auto& item : spec["val"]) {
            if (!item.is_string()) return setError(errorOut, "List items must be strings.");
            listValue.items.push_back(ctx.addString(item.get<std::string>()));
        }
        outValue.Set(listValue);
        return true;
    }

    return setError(errorOut, "Unsupported Value.type: " + type);
}

bool parseExpression(const json& spec,
                     BuildContext& ctx,
                     std::unique_ptr<ExpressionT>& outExpr,
                     std::string* errorOut) {
    if (spec.is_null()) {
        outExpr.reset();
        return true;
    }
    if (!spec.is_object() || !spec.contains("tokens") || !spec["tokens"].is_array()) {
        return setError(errorOut, "Expression must be an object with array field 'tokens'.");
    }

    auto expr = std::make_unique<ExpressionT>();
    for (const auto& tok : spec["tokens"]) {
        if (!tok.is_object() || !tok.contains("op") || !tok["op"].is_string()) {
            return setError(errorOut, "ExprToken must contain string field 'op'.");
        }
        auto token = std::make_unique<ExprTokenT>();
        const std::string opText = tok["op"].get<std::string>();
        if (!parseEnumByName(opText, EnumValuesExprOp(), EnumNameExprOp, token->op)) {
            return setError(errorOut, "Unknown ExprOp: " + opText);
        }

        token->var_name_id = -1;
        if (tok.contains("var_name")) {
            if (!tok["var_name"].is_string()) {
                return setError(errorOut, "ExprToken.var_name must be string.");
            }
            token->var_name_id = ctx.addString(tok["var_name"].get<std::string>());
        }

        token->literal_value.type = ValueData::NONE;
        token->literal_value.value = nullptr;
        if (tok.contains("value")) {
            if (!parseValueData(tok["value"], ctx, token->literal_value, errorOut)) return false;
        }

        expr->tokens.push_back(std::move(token));
    }

    outExpr = std::move(expr);
    return true;
}

bool parseCommandArg(const json& spec,
                     BuildContext& ctx,
                     std::unique_ptr<CommandArgT>& outArg,
                     std::string* errorOut) {
    if (!spec.is_object()) return setError(errorOut, "CommandArg must be an object.");
    if (!spec.contains("kind") || !spec["kind"].is_string()) {
        return setError(errorOut, "CommandArg.kind must be a string.");
    }
    if (!spec.contains("value")) {
        return setError(errorOut, "CommandArg.value is required.");
    }

    auto arg = std::make_unique<CommandArgT>();
    const std::string kindText = spec["kind"].get<std::string>();
    if (!parseEnumByName(kindText, EnumValuesCommandArgKind(), EnumNameCommandArgKind, arg->kind)) {
        return setError(errorOut, "Unknown CommandArg.kind: " + kindText);
    }

    arg->string_id = -1;
    arg->int_value = 0;
    arg->float_value = 0.0f;
    arg->bool_value = false;

    switch (arg->kind) {
    case CommandArgKind::String:
    case CommandArgKind::Identifier:
        if (!spec["value"].is_string()) return setError(errorOut, "CommandArg string value must be string.");
        arg->string_id = ctx.addString(spec["value"].get<std::string>());
        break;
    case CommandArgKind::Int:
        if (!spec["value"].is_number_integer()) return setError(errorOut, "CommandArg int value must be integer.");
        arg->int_value = spec["value"].get<int32_t>();
        break;
    case CommandArgKind::Float:
        if (!spec["value"].is_number()) return setError(errorOut, "CommandArg float value must be number.");
        arg->float_value = spec["value"].get<float>();
        break;
    case CommandArgKind::Bool:
        if (!spec["value"].is_boolean()) return setError(errorOut, "CommandArg bool value must be boolean.");
        arg->bool_value = spec["value"].get<bool>();
        break;
    default:
        return setError(errorOut, "Unsupported CommandArg.kind.");
    }

    outArg = std::move(arg);
    return true;
}

bool parseInstruction(const json& spec,
                      BuildContext& ctx,
                      std::unique_ptr<InstructionT>& outInstr,
                      std::string* errorOut) {
    if (!spec.is_object() || !spec.contains("type") || !spec["type"].is_string()) {
        return setError(errorOut, "Instruction must contain string field 'type'.");
    }
    const std::string type = spec["type"].get<std::string>();
    auto instr = std::make_unique<InstructionT>();

    if (type == "Line") {
        if (!spec.contains("text") || !spec["text"].is_string()) {
            return setError(errorOut, "Line.text must be a string.");
        }
        LineT line;
        line.character_id = -1;
        if (spec.contains("character")) {
            if (spec["character"].is_string()) {
                line.character_id = ctx.addString(spec["character"].get<std::string>());
            } else if (!spec["character"].is_null()) {
                return setError(errorOut, "Line.character must be string or null.");
            }
        }
        line.text_id = ctx.addString(spec["text"].get<std::string>());
        line.voice_asset_id = -1;
        if (spec.contains("voice_asset")) {
            if (!spec["voice_asset"].is_string()) return setError(errorOut, "Line.voice_asset must be string.");
            line.voice_asset_id = ctx.addString(spec["voice_asset"].get<std::string>());
        }
        if (spec.contains("tags")) {
            if (!spec["tags"].is_array()) return setError(errorOut, "Line.tags must be an array.");
            for (const auto& tagSpec : spec["tags"]) {
                std::unique_ptr<TagT> tag;
                if (!parseTag(tagSpec, ctx, tag, errorOut)) return false;
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
            return setError(errorOut, "Choice requires string fields 'text' and 'target_node'.");
        }
        ChoiceT choice;
        choice.text_id = ctx.addString(spec["text"].get<std::string>());
        choice.target_node_name_id = ctx.addString(spec["target_node"].get<std::string>());
        choice.condition_var_id = -1;
        choice.choice_modifier = ChoiceModifier::Default;
        if (spec.contains("condition_var")) {
            if (!spec["condition_var"].is_string()) return setError(errorOut, "Choice.condition_var must be string.");
            choice.condition_var_id = ctx.addString(spec["condition_var"].get<std::string>());
        }
        if (spec.contains("choice_modifier")) {
            if (!spec["choice_modifier"].is_string()) return setError(errorOut, "Choice.choice_modifier must be string.");
            const std::string modText = spec["choice_modifier"].get<std::string>();
            if (!parseEnumByName(modText, EnumValuesChoiceModifier(), EnumNameChoiceModifier, choice.choice_modifier)) {
                return setError(errorOut, "Unknown ChoiceModifier: " + modText);
            }
        }
        instr->data.Set(choice);
        outInstr = std::move(instr);
        return true;
    }

    if (type == "Jump") {
        if (!spec.contains("target_node") || !spec["target_node"].is_string()) {
            return setError(errorOut, "Jump.target_node must be a string.");
        }
        JumpT jump;
        jump.target_node_name_id = ctx.addString(spec["target_node"].get<std::string>());
        jump.is_call = spec.value("is_call", false);
        if (spec.contains("arg_exprs")) {
            if (!spec["arg_exprs"].is_array()) return setError(errorOut, "Jump.arg_exprs must be an array.");
            for (const auto& argExprSpec : spec["arg_exprs"]) {
                std::unique_ptr<ExpressionT> argExpr;
                if (!parseExpression(argExprSpec, ctx, argExpr, errorOut)) return false;
                jump.arg_exprs.push_back(std::move(argExpr));
            }
        }
        instr->data.Set(jump);
        outInstr = std::move(instr);
        return true;
    }

    if (type == "Command") {
        if (!spec.contains("command_type") || !spec["command_type"].is_string()) {
            return setError(errorOut, "Command.command_type must be a string.");
        }
        CommandT cmd;
        cmd.type_id = ctx.addString(spec["command_type"].get<std::string>());
        if (spec.contains("args")) {
            if (!spec["args"].is_array()) return setError(errorOut, "Command.args must be an array.");
            for (const auto& argSpec : spec["args"]) {
                std::unique_ptr<CommandArgT> arg;
                if (!parseCommandArg(argSpec, ctx, arg, errorOut)) return false;
                cmd.args.push_back(std::move(arg));
            }
        }
        instr->data.Set(cmd);
        outInstr = std::move(instr);
        return true;
    }

    if (type == "Wait") {
        WaitT wait;
        wait.tag_id = -1;
        if (spec.contains("tag")) {
            if (!spec["tag"].is_string()) return setError(errorOut, "Wait.tag must be string.");
            wait.tag_id = ctx.addString(spec["tag"].get<std::string>());
        }
        instr->data.Set(wait);
        outInstr = std::move(instr);
        return true;
    }

    if (type == "Yield") {
        YieldT y;
        instr->data.Set(y);
        outInstr = std::move(instr);
        return true;
    }

    if (type == "SetVar") {
        if (!spec.contains("var_name") || !spec["var_name"].is_string()) {
            return setError(errorOut, "SetVar.var_name must be a string.");
        }
        SetVarT setVar;
        setVar.var_name_id = ctx.addString(spec["var_name"].get<std::string>());
        setVar.assign_op = AssignOp::Assign;
        if (spec.contains("assign_op")) {
            if (!spec["assign_op"].is_string()) return setError(errorOut, "SetVar.assign_op must be string.");
            const std::string opText = spec["assign_op"].get<std::string>();
            if (!parseEnumByName(opText, EnumValuesAssignOp(), EnumNameAssignOp, setVar.assign_op)) {
                return setError(errorOut, "Unknown AssignOp: " + opText);
            }
        }

        setVar.value.type = ValueData::NONE;
        setVar.value.value = nullptr;
        if (spec.contains("value")) {
            if (!parseValueData(spec["value"], ctx, setVar.value, errorOut)) return false;
        }
        if (spec.contains("expr")) {
            if (!parseExpression(spec["expr"], ctx, setVar.expr, errorOut)) return false;
        }

        instr->data.Set(setVar);
        outInstr = std::move(instr);
        return true;
    }

    if (type == "Condition") {
        ConditionT cond;
        cond.var_name_id = -1;
        cond.true_jump_node_id = -1;
        cond.false_jump_node_id = -1;
        cond.op = Operator::Equal;
        if (spec.contains("op")) {
            if (!spec["op"].is_string()) return setError(errorOut, "Condition.op must be string.");
            const std::string opText = spec["op"].get<std::string>();
            if (!parseEnumByName(opText, EnumValuesOperator(), EnumNameOperator, cond.op)) {
                return setError(errorOut, "Unknown Operator: " + opText);
            }
        }

        if (!spec.contains("true_jump_node") || !spec["true_jump_node"].is_string()) {
            return setError(errorOut, "Condition.true_jump_node must be string.");
        }
        cond.true_jump_node_id = ctx.addString(spec["true_jump_node"].get<std::string>());
        if (spec.contains("false_jump_node") && spec["false_jump_node"].is_string() &&
            !spec["false_jump_node"].get<std::string>().empty()) {
            cond.false_jump_node_id = ctx.addString(spec["false_jump_node"].get<std::string>());
        }

        cond.compare_value.type = ValueData::NONE;
        cond.compare_value.value = nullptr;
        if (spec.contains("var_name")) {
            if (!spec["var_name"].is_string()) return setError(errorOut, "Condition.var_name must be string.");
            cond.var_name_id = ctx.addString(spec["var_name"].get<std::string>());
        }
        if (spec.contains("compare_value")) {
            if (!parseValueData(spec["compare_value"], ctx, cond.compare_value, errorOut)) return false;
        }
        if (spec.contains("lhs_expr")) {
            if (!parseExpression(spec["lhs_expr"], ctx, cond.lhs_expr, errorOut)) return false;
        }
        if (spec.contains("rhs_expr")) {
            if (!parseExpression(spec["rhs_expr"], ctx, cond.rhs_expr, errorOut)) return false;
        }
        if (spec.contains("cond_expr")) {
            if (!parseExpression(spec["cond_expr"], ctx, cond.cond_expr, errorOut)) return false;
        }

        instr->data.Set(cond);
        outInstr = std::move(instr);
        return true;
    }

    if (type == "Random") {
        if (!spec.contains("branches") || !spec["branches"].is_array()) {
            return setError(errorOut, "Random.branches must be an array.");
        }
        RandomT randomOp;
        for (const auto& branchSpec : spec["branches"]) {
            if (!branchSpec.is_object() ||
                !branchSpec.contains("target_node") ||
                !branchSpec["target_node"].is_string()) {
                return setError(errorOut, "Random branch requires string field 'target_node'.");
            }
            auto branch = std::make_unique<RandomBranchT>();
            branch->target_node_name_id = ctx.addString(branchSpec["target_node"].get<std::string>());
            branch->weight = branchSpec.value("weight", 1);
            randomOp.branches.push_back(std::move(branch));
        }
        instr->data.Set(randomOp);
        outInstr = std::move(instr);
        return true;
    }

    if (type == "Return") {
        ReturnT ret;
        ret.value.type = ValueData::NONE;
        ret.value.value = nullptr;
        if (spec.contains("value")) {
            if (!parseValueData(spec["value"], ctx, ret.value, errorOut)) return false;
        }
        if (spec.contains("expr")) {
            if (!parseExpression(spec["expr"], ctx, ret.expr, errorOut)) return false;
        }
        instr->data.Set(ret);
        outInstr = std::move(instr);
        return true;
    }

    if (type == "CallWithReturn") {
        if (!spec.contains("target_node") || !spec["target_node"].is_string()) {
            return setError(errorOut, "CallWithReturn.target_node must be string.");
        }
        CallWithReturnT cwr;
        cwr.target_node_name_id = ctx.addString(spec["target_node"].get<std::string>());
        cwr.return_var_name_id = ctx.addString(
            spec.contains("return_var") && spec["return_var"].is_string()
                ? spec["return_var"].get<std::string>()
                : "");
        if (spec.contains("arg_exprs")) {
            if (!spec["arg_exprs"].is_array()) return setError(errorOut, "CallWithReturn.arg_exprs must be an array.");
            for (const auto& argExprSpec : spec["arg_exprs"]) {
                std::unique_ptr<ExpressionT> argExpr;
                if (!parseExpression(argExprSpec, ctx, argExpr, errorOut)) return false;
                cwr.arg_exprs.push_back(std::move(argExpr));
            }
        }
        instr->data.Set(cwr);
        outInstr = std::move(instr);
        return true;
    }

    return setError(errorOut, "Unsupported Instruction.type: " + type);
}

bool parseNode(const json& spec, BuildContext& ctx, std::unique_ptr<NodeT>& outNode, std::string* errorOut) {
    if (!spec.is_object() || !spec.contains("name") || !spec["name"].is_string()) {
        return setError(errorOut, "Node must contain string field 'name'.");
    }
    if (!spec.contains("instructions") || !spec["instructions"].is_array()) {
        return setError(errorOut, "Node must contain array field 'instructions'.");
    }

    auto node = std::make_unique<NodeT>();
    node->name = spec["name"].get<std::string>();

    if (spec.contains("params")) {
        if (!spec["params"].is_array()) return setError(errorOut, "Node.params must be an array.");
        for (const auto& paramSpec : spec["params"]) {
            if (!paramSpec.is_string()) return setError(errorOut, "Node.params items must be strings.");
            node->param_ids.push_back(ctx.addString(paramSpec.get<std::string>()));
        }
    }

    if (spec.contains("tags")) {
        if (!spec["tags"].is_array()) return setError(errorOut, "Node.tags must be an array.");
        for (const auto& tagSpec : spec["tags"]) {
            std::unique_ptr<TagT> tag;
            if (!parseTag(tagSpec, ctx, tag, errorOut)) return false;
            node->tags.push_back(std::move(tag));
        }
    }

    for (const auto& instrSpec : spec["instructions"]) {
        std::unique_ptr<InstructionT> instr;
        if (!parseInstruction(instrSpec, ctx, instr, errorOut)) return false;
        node->lines.push_back(std::move(instr));
    }

    outNode = std::move(node);
    return true;
}

bool parseCharacterDef(const json& spec,
                       BuildContext& ctx,
                       std::unique_ptr<CharacterDefT>& outDef,
                       std::string* errorOut) {
    if (!spec.is_object() || !spec.contains("name") || !spec["name"].is_string()) {
        return setError(errorOut, "CharacterDef.name must be a string.");
    }
    auto charDef = std::make_unique<CharacterDefT>();
    charDef->name_id = ctx.addString(spec["name"].get<std::string>());

    if (spec.contains("properties")) {
        if (!spec["properties"].is_array()) return setError(errorOut, "CharacterDef.properties must be an array.");
        for (const auto& propSpec : spec["properties"]) {
            std::unique_ptr<TagT> tag;
            if (!parseTag(propSpec, ctx, tag, errorOut)) return false;
            charDef->properties.push_back(std::move(tag));
        }
    }

    outDef = std::move(charDef);
    return true;
}

bool parseSetVar(const json& spec,
                 BuildContext& ctx,
                 std::unique_ptr<SetVarT>& outVar,
                 std::string* errorOut) {
    if (!spec.is_object() || spec.value("type", "") != "SetVar") {
        return setError(errorOut, "Global variable entries must be SetVar instructions.");
    }
    if (!spec.contains("var_name") || !spec["var_name"].is_string()) {
        return setError(errorOut, "SetVar.var_name must be a string.");
    }

    auto setVar = std::make_unique<SetVarT>();
    setVar->var_name_id = ctx.addString(spec["var_name"].get<std::string>());
    setVar->assign_op = AssignOp::Assign;
    if (spec.contains("assign_op")) {
        if (!spec["assign_op"].is_string()) return setError(errorOut, "SetVar.assign_op must be string.");
        const std::string opText = spec["assign_op"].get<std::string>();
        if (!parseEnumByName(opText, EnumValuesAssignOp(), EnumNameAssignOp, setVar->assign_op)) {
            return setError(errorOut, "Unknown AssignOp: " + opText);
        }
    }
    setVar->value.type = ValueData::NONE;
    setVar->value.value = nullptr;
    if (spec.contains("value")) {
        if (!parseValueData(spec["value"], ctx, setVar->value, errorOut)) return false;
    }
    if (spec.contains("expr")) {
        if (!parseExpression(spec["expr"], ctx, setVar->expr, errorOut)) return false;
    }
    outVar = std::move(setVar);
    return true;
}

} // namespace

bool JsonIrReader::fromJson(const json& doc, StoryT& outStory, std::string* errorOut) {
    outStory = StoryT{};

    if (!doc.is_object()) {
        return setError(errorOut, "JSON IR root must be an object.");
    }
    if (doc.value("format", "") != "gyeol-json-ir") {
        return setError(errorOut, "Invalid JSON IR format. Expected 'gyeol-json-ir'.");
    }
    if (!doc.contains("format_version") || !doc["format_version"].is_number_integer()) {
        return setError(errorOut, "JSON IR requires integer 'format_version'.");
    }
    if (doc["format_version"].get<int>() != 2) {
        return setError(errorOut, "Unsupported JSON IR format_version. Expected 2.");
    }
    if (!doc.contains("start_node_name") || !doc["start_node_name"].is_string()) {
        return setError(errorOut, "JSON IR requires string 'start_node_name'.");
    }
    if (!doc.contains("nodes") || !doc["nodes"].is_array()) {
        return setError(errorOut, "JSON IR requires array 'nodes'.");
    }
    if (!doc.contains("string_pool") || !doc["string_pool"].is_array()) {
        return setError(errorOut, "JSON IR requires array 'string_pool'.");
    }

    outStory.version = doc.value("version", "");
    outStory.start_node_name = doc["start_node_name"].get<std::string>();

    BuildContext ctx(outStory);

    // Seed string pool + line_ids for stable translation keys.
    for (const auto& strVal : doc["string_pool"]) {
        if (!strVal.is_string()) return setError(errorOut, "string_pool must contain only strings.");
        ctx.addString(strVal.get<std::string>());
    }
    if (doc.contains("line_ids")) {
        if (!doc["line_ids"].is_array()) return setError(errorOut, "line_ids must be an array.");
        if (doc["line_ids"].size() != outStory.string_pool.size()) {
            return setError(errorOut, "line_ids size must match string_pool size.");
        }
        for (size_t i = 0; i < doc["line_ids"].size(); ++i) {
            if (!doc["line_ids"][i].is_string()) return setError(errorOut, "line_ids must contain only strings.");
            ctx.lineIds[i] = doc["line_ids"][i].get<std::string>();
        }
    }

    if (doc.contains("characters")) {
        if (!doc["characters"].is_array()) return setError(errorOut, "characters must be an array.");
        for (const auto& charSpec : doc["characters"]) {
            std::unique_ptr<CharacterDefT> def;
            if (!parseCharacterDef(charSpec, ctx, def, errorOut)) return false;
            outStory.characters.push_back(std::move(def));
        }
    }

    if (doc.contains("global_vars")) {
        if (!doc["global_vars"].is_array()) return setError(errorOut, "global_vars must be an array.");
        for (const auto& varSpec : doc["global_vars"]) {
            std::unique_ptr<SetVarT> setVar;
            if (!parseSetVar(varSpec, ctx, setVar, errorOut)) return false;
            outStory.global_vars.push_back(std::move(setVar));
        }
    }

    for (const auto& nodeSpec : doc["nodes"]) {
        std::unique_ptr<NodeT> node;
        if (!parseNode(nodeSpec, ctx, node, errorOut)) return false;
        outStory.nodes.push_back(std::move(node));
    }

    bool foundStartNode = false;
    for (const auto& node : outStory.nodes) {
        if (node && node->name == outStory.start_node_name) {
            foundStartNode = true;
            break;
        }
    }
    if (!foundStartNode) {
        return setError(errorOut, "start_node_name does not exist in nodes.");
    }

    outStory.line_ids = std::move(ctx.lineIds);
    if (outStory.line_ids.size() < outStory.string_pool.size()) {
        outStory.line_ids.resize(outStory.string_pool.size(), "");
    }
    return true;
}

bool JsonIrReader::fromJsonString(const std::string& jsonText, StoryT& outStory, std::string* errorOut) {
    json doc;
    try {
        doc = json::parse(jsonText);
    } catch (const std::exception& e) {
        return setError(errorOut, std::string("Invalid JSON: ") + e.what());
    }
    return fromJson(doc, outStory, errorOut);
}

bool JsonIrReader::fromFile(const std::string& path, StoryT& outStory, std::string* errorOut) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        return setError(errorOut, "Failed to open JSON IR file: " + path);
    }

    json doc;
    try {
        ifs >> doc;
    } catch (const std::exception& e) {
        return setError(errorOut, std::string("Invalid JSON IR file: ") + e.what());
    }
    return fromJson(doc, outStory, errorOut);
}

std::vector<uint8_t> JsonIrReader::compileToBuffer(const StoryT& story) {
    flatbuffers::FlatBufferBuilder builder;
    auto* mutableStory = const_cast<StoryT*>(&story);
    const auto rootOffset = Story::Pack(builder, mutableStory);
    builder.Finish(rootOffset);
    return std::vector<uint8_t>(builder.GetBufferPointer(),
                                builder.GetBufferPointer() + builder.GetSize());
}

} // namespace Gyeol
