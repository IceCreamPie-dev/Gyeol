#include "gyeol_story.h"
#include "gyeol_generated.h"
#include <iostream>
#include <fstream>

using namespace ICPDev::Gyeol::Schema;

namespace Gyeol {

void Story::printVersion() {
    std::cout << "Gyeol Engine Core Initialized." << std::endl;
    std::cout << "FlatBuffers Schema Loaded." << std::endl;
}

bool Story::loadFromFile(const std::string& filepath) {
    std::ifstream ifs(filepath, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        std::cerr << "[Gyeol] Failed to open file: " << filepath << std::endl;
        return false;
    }

    auto size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    buffer_.resize(static_cast<size_t>(size));
    ifs.read(reinterpret_cast<char*>(buffer_.data()), size);
    ifs.close();

    // FlatBuffers 버퍼 검증
    flatbuffers::Verifier verifier(buffer_.data(), buffer_.size());
    if (!VerifyStoryBuffer(verifier)) {
        std::cerr << "[Gyeol] Invalid .gyb file: " << filepath << std::endl;
        buffer_.clear();
        return false;
    }

    std::cout << "[Gyeol] Loaded: " << filepath
              << " (" << buffer_.size() << " bytes)" << std::endl;
    return true;
}

// string_pool에서 안전하게 문자열을 가져오는 헬퍼
static const char* poolStr(
    const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>* pool,
    int32_t index)
{
    if (!pool || index < 0 || index >= static_cast<int32_t>(pool->size())) {
        return "(none)";
    }
    return pool->Get(static_cast<flatbuffers::uoffset_t>(index))->c_str();
}

void Story::printStory() const {
    if (buffer_.empty()) {
        std::cerr << "[Gyeol] No story loaded." << std::endl;
        return;
    }

    const auto* story = GetStory(buffer_.data());
    const auto* pool = story->string_pool();

    // --- 기본 정보 ---
    std::cout << "=== Gyeol Story ===" << std::endl;
    std::cout << "Version: " << (story->version() ? story->version()->c_str() : "?")
              << std::endl;
    std::cout << "Start Node: " << (story->start_node_name() ? story->start_node_name()->c_str() : "?")
              << std::endl;

    // --- String Pool ---
    if (pool) {
        std::cout << "\n--- String Pool (" << pool->size() << " entries) ---" << std::endl;
        for (flatbuffers::uoffset_t i = 0; i < pool->size(); ++i) {
            std::cout << "  [" << i << "] " << pool->Get(i)->c_str() << std::endl;
        }
    }

    // --- Nodes ---
    const auto* nodes = story->nodes();
    if (!nodes) return;

    std::cout << "\n--- Nodes (" << nodes->size() << ") ---" << std::endl;

    for (flatbuffers::uoffset_t i = 0; i < nodes->size(); ++i) {
        const auto* node = nodes->Get(i);
        std::cout << "\n[Node] \"" << (node->name() ? node->name()->c_str() : "?")
                  << "\"" << std::endl;

        const auto* lines = node->lines();
        if (!lines) continue;

        for (flatbuffers::uoffset_t j = 0; j < lines->size(); ++j) {
            const auto* instr = lines->Get(j);
            std::cout << "  " << j << ": ";

            switch (instr->data_type()) {
                case OpData::Line: {
                    const auto* line = instr->data_as_Line();
                    const char* character = (line->character_id() >= 0)
                        ? poolStr(pool, line->character_id())
                        : "(narration)";
                    std::cout << "[Line] " << character
                              << ": \"" << poolStr(pool, line->text_id()) << "\"";
                    if (line->voice_asset_id() >= 0) {
                        std::cout << " [voice:" << poolStr(pool, line->voice_asset_id()) << "]";
                    }
                    std::cout << std::endl;
                    break;
                }
                case OpData::Choice: {
                    const auto* choice = instr->data_as_Choice();
                    std::cout << "[Choice] > " << poolStr(pool, choice->text_id())
                              << " -> " << poolStr(pool, choice->target_node_name_id());
                    if (choice->condition_var_id() >= 0) {
                        std::cout << " (if " << poolStr(pool, choice->condition_var_id()) << ")";
                    }
                    std::cout << std::endl;
                    break;
                }
                case OpData::Jump: {
                    const auto* jump = instr->data_as_Jump();
                    std::cout << "[Jump] -> " << poolStr(pool, jump->target_node_name_id())
                              << (jump->is_call() ? " (call)" : "") << std::endl;
                    break;
                }
                case OpData::Command: {
                    const auto* cmd = instr->data_as_Command();
                    std::cout << "[Command] " << poolStr(pool, cmd->type_id()) << "(";
                    const auto* params = cmd->params();
                    if (params) {
                        for (flatbuffers::uoffset_t k = 0; k < params->size(); ++k) {
                            if (k > 0) std::cout << ", ";
                            std::cout << poolStr(pool, params->Get(k));
                        }
                    }
                    std::cout << ")" << std::endl;
                    break;
                }
                case OpData::SetVar: {
                    const auto* setvar = instr->data_as_SetVar();
                    std::cout << "[SetVar] " << poolStr(pool, setvar->var_name_id()) << " = ";
                    switch (setvar->value_type()) {
                        case ValueData::BoolValue:
                            std::cout << (setvar->value_as_BoolValue()->val() ? "true" : "false");
                            break;
                        case ValueData::IntValue:
                            std::cout << setvar->value_as_IntValue()->val();
                            break;
                        case ValueData::FloatValue:
                            std::cout << setvar->value_as_FloatValue()->val();
                            break;
                        case ValueData::StringRef:
                            std::cout << "\"" << poolStr(pool, setvar->value_as_StringRef()->index()) << "\"";
                            break;
                        default:
                            std::cout << "(unknown)";
                            break;
                    }
                    std::cout << std::endl;
                    break;
                }
                case OpData::Condition: {
                    const auto* cond = instr->data_as_Condition();
                    std::cout << "[Condition] IF " << poolStr(pool, cond->var_name_id())
                              << " " << EnumNameOperator(cond->op()) << " ";
                    switch (cond->compare_value_type()) {
                        case ValueData::BoolValue:
                            std::cout << (cond->compare_value_as_BoolValue()->val() ? "true" : "false");
                            break;
                        case ValueData::IntValue:
                            std::cout << cond->compare_value_as_IntValue()->val();
                            break;
                        case ValueData::FloatValue:
                            std::cout << cond->compare_value_as_FloatValue()->val();
                            break;
                        case ValueData::StringRef:
                            std::cout << "\"" << poolStr(pool, cond->compare_value_as_StringRef()->index()) << "\"";
                            break;
                        default:
                            std::cout << "?";
                            break;
                    }
                    std::cout << " THEN -> " << poolStr(pool, cond->true_jump_node_id())
                              << " ELSE -> " << poolStr(pool, cond->false_jump_node_id())
                              << std::endl;
                    break;
                }
                default:
                    std::cout << "[Unknown OpData]" << std::endl;
                    break;
            }
        }
    }

    std::cout << "\n=== End ===" << std::endl;
}

} // namespace Gyeol
