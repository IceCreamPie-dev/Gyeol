#pragma once
#include "gyeol_generated.h"
#include <nlohmann/json.hpp>
#include <string>

namespace Gyeol {

/**
 * JSON IR export for Gyeol stories.
 *
 * Converts the parsed StoryT object to a human-readable JSON representation
 * where all string pool indices are resolved to actual strings.
 * Designed for web services, external tools, and Godot addons that need
 * to consume compiled story data without a FlatBuffers dependency.
 */
class JsonExport {
public:
    /// Convert a StoryT to a JSON object (resolved string pool references)
    static nlohmann::json toJson(const ICPDev::Gyeol::Schema::StoryT& story);

    /// Convert a StoryT to a pretty-printed JSON string
    static std::string toJsonString(const ICPDev::Gyeol::Schema::StoryT& story, int indent = 2);

private:
    // String pool resolver helper
    static const std::string& poolStr(const std::vector<std::string>& pool, int32_t index);

    // Serialization helpers
    static nlohmann::json serializeValueData(
        const ICPDev::Gyeol::Schema::ValueDataUnion& value,
        const std::vector<std::string>& pool);

    static nlohmann::json serializeExpression(
        const ICPDev::Gyeol::Schema::ExpressionT* expr,
        const std::vector<std::string>& pool);

    static nlohmann::json serializeExprToken(
        const ICPDev::Gyeol::Schema::ExprTokenT& token,
        const std::vector<std::string>& pool);

    static nlohmann::json serializeTag(
        const ICPDev::Gyeol::Schema::TagT& tag,
        const std::vector<std::string>& pool);

    static nlohmann::json serializeTags(
        const std::vector<std::unique_ptr<ICPDev::Gyeol::Schema::TagT>>& tags,
        const std::vector<std::string>& pool);

    static nlohmann::json serializeInstruction(
        const ICPDev::Gyeol::Schema::InstructionT& instr,
        const std::vector<std::string>& pool);

    static nlohmann::json serializeNode(
        const ICPDev::Gyeol::Schema::NodeT& node,
        const std::vector<std::string>& pool);

    static nlohmann::json serializeSetVar(
        const ICPDev::Gyeol::Schema::SetVarT& setVar,
        const std::vector<std::string>& pool);

    static nlohmann::json serializeCharacterDef(
        const ICPDev::Gyeol::Schema::CharacterDefT& charDef,
        const std::vector<std::string>& pool);
};

} // namespace Gyeol
