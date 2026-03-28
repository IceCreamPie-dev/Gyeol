#include "gyeol_json_ir_tooling.h"

#include "gyeol_graph_tools.h"
#include "gyeol_json_export.h"
#include "gyeol_json_ir_reader.h"

#include <algorithm>
#include <flatbuffers/flatbuffers.h>
#include <fstream>
#include <set>
#include <sstream>
#include <unordered_set>

using json = nlohmann::json;
using namespace ICPDev::Gyeol::Schema;

namespace Gyeol {
namespace {

const std::string& poolStr(const StoryT& story, int32_t index) {
    static const std::string kEmpty;
    if (index < 0 || static_cast<size_t>(index) >= story.string_pool.size()) return kEmpty;
    return story.string_pool[static_cast<size_t>(index)];
}

bool hasPoolIndex(const StoryT& story, int32_t index) {
    return index >= 0 && static_cast<size_t>(index) < story.string_pool.size();
}

bool hasNodeName(const StoryT& story, const std::string& name) {
    for (const auto& node : story.nodes) {
        if (node && node->name == name) return true;
    }
    return false;
}

void addDiagnostic(std::vector<JsonIrDiagnostic>& out,
                   const std::string& code,
                   const std::string& severity,
                   const std::string& path,
                   const std::string& node,
                   int instructionIndex,
                   const std::string& message,
                   const std::string& hint) {
    JsonIrDiagnostic d;
    d.code = code;
    d.severity = severity;
    d.path = path;
    d.node = node;
    d.instruction_index = instructionIndex;
    d.message = message;
    d.hint = hint;
    out.push_back(std::move(d));
}

std::string renderLocation(const JsonIrDiagnostic& d) {
    std::ostringstream oss;
    if (!d.path.empty()) {
        oss << d.path;
    }
    if (!d.node.empty()) {
        if (!d.path.empty()) oss << " ";
        oss << "(node=" << d.node;
        if (d.instruction_index >= 0) {
            oss << ", instruction=" << d.instruction_index;
        }
        oss << ")";
    } else if (d.instruction_index >= 0) {
        if (!d.path.empty()) oss << " ";
        oss << "(instruction=" << d.instruction_index << ")";
    }
    return oss.str();
}

int countInstructions(const StoryT& story) {
    int count = 0;
    for (const auto& node : story.nodes) {
        if (!node) continue;
        count += static_cast<int>(node->lines.size());
    }
    return count;
}

json makeSummary(const std::vector<JsonIrDiagnostic>& diagnostics) {
    int errors = 0;
    int warnings = 0;
    for (const auto& d : diagnostics) {
        if (d.severity == "error") ++errors;
        if (d.severity == "warning") ++warnings;
    }
    return json{
        {"errors", errors},
        {"warnings", warnings},
        {"total", static_cast<int>(diagnostics.size())},
    };
}

std::unique_ptr<StoryT> cloneStory(const StoryT& story) {
    flatbuffers::FlatBufferBuilder builder;
    auto* mutableStory = const_cast<StoryT*>(&story);
    const auto rootOffset = Story::Pack(builder, mutableStory);
    builder.Finish(rootOffset);
    const Story* packed = GetStory(builder.GetBufferPointer());
    return std::unique_ptr<StoryT>(packed->UnPack());
}

void collectNodeMaps(const json& graphDoc, std::unordered_map<std::string, json>& outByName) {
    outByName.clear();
    if (!graphDoc.contains("nodes") || !graphDoc["nodes"].is_array()) return;
    for (const auto& node : graphDoc["nodes"]) {
        if (!node.is_object() || !node.contains("name") || !node["name"].is_string()) continue;
        outByName[node["name"].get<std::string>()] = node;
    }
}

} // namespace

bool JsonIrTooling::writeInitTemplate(const std::string& outputPath, std::string* errorOut) {
    const json doc = {
        {"format", "gyeol-json-ir"},
        {"format_version", 2},
        {"version", "0.2.0"},
        {"start_node_name", "start"},
        {"string_pool", json::array()},
        {"line_ids", json::array()},
        {"nodes", json::array({
            {
                {"name", "start"},
                {"instructions", json::array({
                    {
                        {"type", "Line"},
                        {"character", nullptr},
                        {"text", "Hello JSON IR"}
                    }
                })}
            }
        })}
    };

    std::ofstream ofs(outputPath);
    if (!ofs.is_open()) {
        if (errorOut) *errorOut = "Failed to create JSON IR template: " + outputPath;
        return false;
    }
    ofs << doc.dump(2);
    return true;
}

bool JsonIrTooling::formatFile(const std::string& inputPath,
                               const std::string& outputPath,
                               std::string* errorOut) {
    StoryT story;
    std::string loadError;
    if (!JsonIrReader::fromFile(inputPath, story, &loadError)) {
        if (errorOut) *errorOut = loadError;
        return false;
    }

    const std::string canonical = JsonExport::toJsonString(story);
    std::ofstream ofs(outputPath);
    if (!ofs.is_open()) {
        if (errorOut) *errorOut = "Failed to write formatted JSON IR: " + outputPath;
        return false;
    }
    ofs << canonical;
    return true;
}

bool JsonIrTooling::lintFile(const std::string& storyPath,
                             std::vector<JsonIrDiagnostic>& outDiagnostics,
                             StoryT* outStory,
                             std::string* errorOut) {
    outDiagnostics.clear();

    StoryT story;
    std::string loadError;
    if (!JsonIrReader::fromFile(storyPath, story, &loadError)) {
        addDiagnostic(
            outDiagnostics,
            "IR_PARSE_ERROR",
            "error",
            storyPath,
            "",
            -1,
            loadError,
            "JSON IR 명세를 확인하고 필수 필드(format, format_version, start_node_name, nodes, string_pool)를 점검하세요.");
        if (errorOut) *errorOut = loadError;
        return false;
    }

    lintStory(story, storyPath, outDiagnostics);
    if (outStory) *outStory = std::move(story);
    return true;
}

bool JsonIrTooling::lintStory(const StoryT& story,
                              const std::string& storyPath,
                              std::vector<JsonIrDiagnostic>& outDiagnostics) {
    std::unordered_set<std::string> nodeNames;
    bool startFound = false;

    if (story.nodes.empty()) {
        addDiagnostic(
            outDiagnostics,
            "IR_NODES_EMPTY",
            "error",
            storyPath,
            "",
            -1,
            "nodes가 비어 있습니다.",
            "최소 1개 이상의 노드를 추가하세요.");
        return true;
    }

    for (size_t ni = 0; ni < story.nodes.size(); ++ni) {
        const auto& nodePtr = story.nodes[ni];
        if (!nodePtr) {
            addDiagnostic(
                outDiagnostics,
                "IR_NODE_NULL",
                "error",
                storyPath,
                "",
                -1,
                "null 노드가 포함되어 있습니다.",
                "유효한 노드 객체만 포함되도록 JSON IR을 정리하세요.");
            continue;
        }

        const std::string& nodeName = nodePtr->name;
        if (nodeName.empty()) {
            addDiagnostic(
                outDiagnostics,
                "IR_NODE_NAME_EMPTY",
                "error",
                storyPath,
                "",
                -1,
                "노드 이름이 비어 있습니다.",
                "각 노드는 고유한 name을 가져야 합니다.");
        } else {
            if (!nodeNames.insert(nodeName).second) {
                addDiagnostic(
                    outDiagnostics,
                    "IR_NODE_DUPLICATE",
                    "error",
                    storyPath,
                    nodeName,
                    -1,
                    "중복된 노드 이름이 있습니다: " + nodeName,
                    "노드 이름은 전체 스토리에서 유일해야 합니다.");
            }
            if (nodeName == story.start_node_name) {
                startFound = true;
            }
        }

        if (nodePtr->lines.empty()) {
            addDiagnostic(
                outDiagnostics,
                "IR_NODE_EMPTY",
                "warning",
                storyPath,
                nodeName,
                -1,
                "노드에 인스트럭션이 없습니다.",
                "의도된 빈 노드가 아니라면 Line/Jump/Choice 등을 추가하세요.");
        }
    }

    if (!startFound) {
        addDiagnostic(
            outDiagnostics,
            "IR_START_NODE_MISSING",
            "error",
            storyPath,
            "",
            -1,
            "start_node_name이 nodes 목록에 존재하지 않습니다: " + story.start_node_name,
            "start_node_name을 기존 노드 이름으로 맞추세요.");
    }

    for (const auto& nodePtr : story.nodes) {
        if (!nodePtr) continue;
        const std::string nodeName = nodePtr->name;

        for (size_t ii = 0; ii < nodePtr->lines.size(); ++ii) {
            const auto& instr = nodePtr->lines[ii];
            if (!instr) {
                addDiagnostic(
                    outDiagnostics,
                    "IR_INSTRUCTION_NULL",
                    "error",
                    storyPath,
                    nodeName,
                    static_cast<int>(ii),
                    "null 인스트럭션이 포함되어 있습니다.",
                    "유효한 인스트럭션 객체만 포함되도록 정리하세요.");
                continue;
            }

            switch (instr->data.type) {
            case OpData::Line: {
                const auto* line = instr->data.AsLine();
                if (!line) break;
                const std::string text = poolStr(story, line->text_id);
                if (text.empty()) {
                    addDiagnostic(
                        outDiagnostics,
                        "IR_LINE_TEXT_EMPTY",
                        "warning",
                        storyPath,
                        nodeName,
                        static_cast<int>(ii),
                        "Line.text가 비어 있습니다.",
                        "빈 줄이 의도된 것이 아니라면 text를 채우세요.");
                }
                break;
            }
            case OpData::Choice: {
                const auto* choice = instr->data.AsChoice();
                if (!choice) break;
                const std::string text = poolStr(story, choice->text_id);
                const std::string target = poolStr(story, choice->target_node_name_id);
                if (text.empty()) {
                    addDiagnostic(
                        outDiagnostics,
                        "IR_CHOICE_TEXT_EMPTY",
                        "warning",
                        storyPath,
                        nodeName,
                        static_cast<int>(ii),
                        "Choice.text가 비어 있습니다.",
                        "선택지 라벨을 채우세요.");
                }
                if (target.empty() || !hasNodeName(story, target)) {
                    addDiagnostic(
                        outDiagnostics,
                        "IR_TARGET_MISSING",
                        "error",
                        storyPath,
                        nodeName,
                        static_cast<int>(ii),
                        "Choice의 target_node가 존재하지 않습니다.",
                        "target_node를 존재하는 노드 이름으로 수정하세요.");
                }
                break;
            }
            case OpData::Jump: {
                const auto* jump = instr->data.AsJump();
                if (!jump) break;
                const std::string target = poolStr(story, jump->target_node_name_id);
                if (target.empty() || !hasNodeName(story, target)) {
                    addDiagnostic(
                        outDiagnostics,
                        "IR_TARGET_MISSING",
                        "error",
                        storyPath,
                        nodeName,
                        static_cast<int>(ii),
                        "Jump의 target_node가 존재하지 않습니다.",
                        "target_node를 존재하는 노드 이름으로 수정하세요.");
                }
                break;
            }
            case OpData::Condition: {
                const auto* cond = instr->data.AsCondition();
                if (!cond) break;
                const std::string trueTarget = poolStr(story, cond->true_jump_node_id);
                if (trueTarget.empty() || !hasNodeName(story, trueTarget)) {
                    addDiagnostic(
                        outDiagnostics,
                        "IR_TARGET_MISSING",
                        "error",
                        storyPath,
                        nodeName,
                        static_cast<int>(ii),
                        "Condition.true_jump_node가 존재하지 않습니다.",
                        "true_jump_node를 존재하는 노드 이름으로 수정하세요.");
                }
                if (cond->false_jump_node_id >= 0) {
                    const std::string falseTarget = poolStr(story, cond->false_jump_node_id);
                    if (falseTarget.empty() || !hasNodeName(story, falseTarget)) {
                        addDiagnostic(
                            outDiagnostics,
                            "IR_TARGET_MISSING",
                            "error",
                            storyPath,
                            nodeName,
                            static_cast<int>(ii),
                            "Condition.false_jump_node가 존재하지 않습니다.",
                            "false_jump_node를 존재하는 노드 이름으로 수정하세요.");
                    }
                }
                break;
            }
            case OpData::Random: {
                const auto* rnd = instr->data.AsRandom();
                if (!rnd) break;
                if (rnd->branches.empty()) {
                    addDiagnostic(
                        outDiagnostics,
                        "IR_RANDOM_BRANCH_EMPTY",
                        "error",
                        storyPath,
                        nodeName,
                        static_cast<int>(ii),
                        "Random.branches가 비어 있습니다.",
                        "최소 1개 이상의 branch를 추가하세요.");
                }
                for (const auto& branch : rnd->branches) {
                    if (!branch) continue;
                    const std::string target = poolStr(story, branch->target_node_name_id);
                    if (target.empty() || !hasNodeName(story, target)) {
                        addDiagnostic(
                            outDiagnostics,
                            "IR_TARGET_MISSING",
                            "error",
                            storyPath,
                            nodeName,
                            static_cast<int>(ii),
                            "Random branch target_node가 존재하지 않습니다.",
                            "branch의 target_node를 존재하는 노드 이름으로 수정하세요.");
                    }
                    if (branch->weight <= 0) {
                        addDiagnostic(
                            outDiagnostics,
                            "IR_RANDOM_WEIGHT_NON_POSITIVE",
                            "warning",
                            storyPath,
                            nodeName,
                            static_cast<int>(ii),
                            "Random branch weight가 0 이하입니다.",
                            "weight는 일반적으로 1 이상의 값을 권장합니다.");
                    }
                }
                break;
            }
            case OpData::Command: {
                const auto* command = instr->data.AsCommand();
                if (!command) break;
                const std::string commandType = poolStr(story, command->type_id);
                if (commandType.empty()) {
                    addDiagnostic(
                        outDiagnostics,
                        "IR_COMMAND_TYPE_EMPTY",
                        "error",
                        storyPath,
                        nodeName,
                        static_cast<int>(ii),
                        "Command.command_type이 비어 있습니다.",
                        "command_type에 엔진 명령 타입을 지정하세요.");
                }
                for (const auto& arg : command->args) {
                    if (!arg) continue;
                    if (arg->kind == CommandArgKind::String || arg->kind == CommandArgKind::Identifier) {
                        if (!hasPoolIndex(story, arg->string_id) || poolStr(story, arg->string_id).empty()) {
                            addDiagnostic(
                                outDiagnostics,
                                "IR_COMMAND_ARG_EMPTY",
                                "warning",
                                storyPath,
                                nodeName,
                                static_cast<int>(ii),
                                "Command string/identifier 인자가 비어 있습니다.",
                                "의도된 빈 인자가 아니라면 값을 채우세요.");
                        }
                    }
                }
                break;
            }
            case OpData::CallWithReturn: {
                const auto* cwr = instr->data.AsCallWithReturn();
                if (!cwr) break;
                const std::string target = poolStr(story, cwr->target_node_name_id);
                if (target.empty() || !hasNodeName(story, target)) {
                    addDiagnostic(
                        outDiagnostics,
                        "IR_TARGET_MISSING",
                        "error",
                        storyPath,
                        nodeName,
                        static_cast<int>(ii),
                        "CallWithReturn.target_node가 존재하지 않습니다.",
                        "target_node를 존재하는 노드 이름으로 수정하세요.");
                }
                break;
            }
            default:
                break;
            }
        }
    }

    return true;
}

bool JsonIrTooling::hasErrors(const std::vector<JsonIrDiagnostic>& diagnostics) {
    for (const auto& d : diagnostics) {
        if (d.severity == "error") return true;
    }
    return false;
}

json JsonIrTooling::diagnosticsToJson(const std::vector<JsonIrDiagnostic>& diagnostics) {
    json arr = json::array();
    for (const auto& d : diagnostics) {
        json entry = {
            {"code", d.code},
            {"severity", d.severity},
            {"path", d.path},
            {"node", d.node.empty() ? json(nullptr) : json(d.node)},
            {"instruction_index", d.instruction_index >= 0 ? json(d.instruction_index) : json(nullptr)},
            {"message", d.message},
            {"hint", d.hint},
        };
        arr.push_back(std::move(entry));
    }
    return json{
        {"diagnostics", std::move(arr)},
        {"summary", makeSummary(diagnostics)},
    };
}

std::string JsonIrTooling::diagnosticsToText(const std::vector<JsonIrDiagnostic>& diagnostics) {
    if (diagnostics.empty()) {
        return "No diagnostics.";
    }
    std::ostringstream oss;
    for (size_t i = 0; i < diagnostics.size(); ++i) {
        const auto& d = diagnostics[i];
        oss << d.severity << " [" << d.code << "] " << d.message;
        const std::string loc = renderLocation(d);
        if (!loc.empty()) {
            oss << " @ " << loc;
        }
        if (!d.hint.empty()) {
            oss << "\n  hint: " << d.hint;
        }
        if (i + 1 < diagnostics.size()) {
            oss << "\n";
        }
    }
    return oss.str();
}

bool JsonIrTooling::loadJsonFile(const std::string& path, json& outJson, std::string* errorOut) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        if (errorOut) *errorOut = "Failed to open JSON file: " + path;
        return false;
    }
    try {
        ifs >> outJson;
    } catch (const std::exception& e) {
        if (errorOut) *errorOut = std::string("Invalid JSON file: ") + e.what();
        return false;
    }
    return true;
}

bool JsonIrTooling::previewGraphPatch(const StoryT& story,
                                      const std::string& storyPath,
                                      const json& patch,
                                      json& outPreview,
                                      std::vector<JsonIrDiagnostic>* outDiagnostics,
                                      std::string* errorOut) {
    if (outDiagnostics) outDiagnostics->clear();

    std::unique_ptr<StoryT> working = cloneStory(story);
    if (!working) {
        if (errorOut) *errorOut = "Failed to clone story for patch preview.";
        if (outDiagnostics) {
            addDiagnostic(
                *outDiagnostics,
                "IR_PATCH_PREVIEW_ERROR",
                "error",
                storyPath,
                "",
                -1,
                "그래프 패치 프리뷰를 위한 스토리 복제에 실패했습니다.",
                "스토리 데이터 무결성을 점검하고 다시 시도하세요.");
        }
        return false;
    }

    std::string applyError;
    if (!GraphTools::applyGraphPatchJson(*working, patch, &applyError)) {
        if (errorOut) *errorOut = applyError;
        if (outDiagnostics) {
            addDiagnostic(
                *outDiagnostics,
                "IR_PATCH_PREVIEW_APPLY_ERROR",
                "error",
                storyPath,
                "",
                -1,
                applyError,
                "패치 파일 형식/참조 대상/instruction_id를 확인하세요.");
        }
        return false;
    }

    const json beforeDoc = GraphTools::buildGraphDoc(story);
    const json afterDoc = GraphTools::buildGraphDoc(*working);

    std::unordered_map<std::string, json> beforeByName;
    std::unordered_map<std::string, json> afterByName;
    collectNodeMaps(beforeDoc, beforeByName);
    collectNodeMaps(afterDoc, afterByName);

    std::vector<std::string> addedNodes;
    std::vector<std::string> removedNodes;
    std::vector<std::string> changedNodes;

    for (const auto& [name, beforeNode] : beforeByName) {
        auto it = afterByName.find(name);
        if (it == afterByName.end()) {
            removedNodes.push_back(name);
            continue;
        }
        if (beforeNode.dump() != it->second.dump()) {
            changedNodes.push_back(name);
        }
    }
    for (const auto& [name, _] : afterByName) {
        if (beforeByName.find(name) == beforeByName.end()) {
            addedNodes.push_back(name);
        }
    }

    std::sort(addedNodes.begin(), addedNodes.end());
    std::sort(removedNodes.begin(), removedNodes.end());
    std::sort(changedNodes.begin(), changedNodes.end());

    const int nodesBefore = static_cast<int>(story.nodes.size());
    const int nodesAfter = static_cast<int>(working->nodes.size());
    const int instructionsBefore = countInstructions(story);
    const int instructionsAfter = countInstructions(*working);
    const int edgesBefore = (beforeDoc.contains("edges") && beforeDoc["edges"].is_array())
        ? static_cast<int>(beforeDoc["edges"].size())
        : 0;
    const int edgesAfter = (afterDoc.contains("edges") && afterDoc["edges"].is_array())
        ? static_cast<int>(afterDoc["edges"].size())
        : 0;

    int opCount = 0;
    if (patch.contains("ops") && patch["ops"].is_array()) {
        opCount = static_cast<int>(patch["ops"].size());
    }

    outPreview = {
        {"format", "gyeol-graph-patch-preview"},
        {"version", 1},
        {"story_path", storyPath},
        {"patch_op_count", opCount},
        {"summary", {
            {"nodes_before", nodesBefore},
            {"nodes_after", nodesAfter},
            {"nodes_delta", nodesAfter - nodesBefore},
            {"instructions_before", instructionsBefore},
            {"instructions_after", instructionsAfter},
            {"instructions_delta", instructionsAfter - instructionsBefore},
            {"edges_before", edgesBefore},
            {"edges_after", edgesAfter},
            {"edges_delta", edgesAfter - edgesBefore},
        }},
        {"added_nodes", addedNodes},
        {"removed_nodes", removedNodes},
        {"changed_nodes", changedNodes},
    };

    return true;
}

} // namespace Gyeol
