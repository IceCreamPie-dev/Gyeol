#include "runtime_contract_harness.h"
#include "gyeol_story_player_adapter.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace RuntimeContract {
namespace {

nlohmann::json variantToJson(const Gyeol::Variant& v) {
    using Type = Gyeol::Variant::Type;
    switch (v.type) {
    case Type::BOOL:
        return nlohmann::json{{"type", "bool"}, {"value", v.b}};
    case Type::INT:
        return nlohmann::json{{"type", "int"}, {"value", v.i}};
    case Type::FLOAT:
        return nlohmann::json{{"type", "float"}, {"value", v.f}};
    case Type::STRING:
        return nlohmann::json{{"type", "string"}, {"value", v.s}};
    case Type::LIST:
        return nlohmann::json{{"type", "list"}, {"value", v.list}};
    }
    return nlohmann::json::object();
}

bool applyActionOp(Gyeol::Runner& runner,
                   const nlohmann::json& action,
                   const RunOptions& options,
                   const std::string& tempDir,
                   std::unordered_map<std::string, Gyeol::Runner::Snapshot>& snapshots,
                   nlohmann::json& outStepRecord,
                   std::string* errorOut) {
    if (!action.contains("op") || !action["op"].is_string()) {
        if (errorOut) *errorOut = "Action is missing string field 'op'.";
        return false;
    }

    const std::string op = action["op"].get<std::string>();
    outStepRecord = nlohmann::json::object();
    outStepRecord["action"] = op;

    if (op == "set_seed") {
        if (!action.contains("seed") || !action["seed"].is_number_integer()) {
            if (errorOut) *errorOut = "set_seed requires integer field 'seed'.";
            return false;
        }
        const auto seed = static_cast<uint32_t>(action["seed"].get<int>());
        runner.setSeed(seed);
        outStepRecord["seed"] = seed;
    } else if (op == "step") {
        const auto result = runner.step();
        outStepRecord["result"] = stepResultToJson(result);
    } else if (op == "choose") {
        if (!action.contains("index") || !action["index"].is_number_integer()) {
            if (errorOut) *errorOut = "choose requires integer field 'index'.";
            return false;
        }
        const int index = action["index"].get<int>();
        runner.choose(index);
        outStepRecord["index"] = index;
    } else if (op == "resume") {
        outStepRecord["ok"] = runner.resume();
    } else if (op == "save") {
        if (!action.contains("path") || !action["path"].is_string()) {
            if (errorOut) *errorOut = "save requires string field 'path'.";
            return false;
        }
        const std::string path = resolveActionPath(action["path"].get<std::string>(), tempDir);
        outStepRecord["ok"] = runner.saveState(path);
    } else if (op == "load") {
        if (!action.contains("path") || !action["path"].is_string()) {
            if (errorOut) *errorOut = "load requires string field 'path'.";
            return false;
        }
        const std::string path = resolveActionPath(action["path"].get<std::string>(), tempDir);
        outStepRecord["ok"] = runner.loadState(path);
    } else if (op == "snapshot") {
        if (!action.contains("name") || !action["name"].is_string()) {
            if (errorOut) *errorOut = "snapshot requires string field 'name'.";
            return false;
        }
        const std::string name = action["name"].get<std::string>();
        snapshots[name] = runner.snapshot();
        outStepRecord["ok"] = true;
        outStepRecord["name"] = name;
    } else if (op == "restore") {
        if (!action.contains("name") || !action["name"].is_string()) {
            if (errorOut) *errorOut = "restore requires string field 'name'.";
            return false;
        }
        const std::string name = action["name"].get<std::string>();
        auto it = snapshots.find(name);
        if (it == snapshots.end()) {
            if (errorOut) *errorOut = "restore target snapshot not found: " + name;
            return false;
        }
        outStepRecord["ok"] = runner.restore(it->second);
        outStepRecord["name"] = name;
    } else if (op == "load_locale") {
        if (!action.contains("path") || !action["path"].is_string()) {
            if (errorOut) *errorOut = "load_locale requires string field 'path'.";
            return false;
        }
        const std::string path = resolveActionPath(action["path"].get<std::string>(), tempDir);
        outStepRecord["ok"] = runner.loadLocale(path);
    } else if (op == "clear_locale") {
        runner.clearLocale();
        outStepRecord["ok"] = true;
    } else if (op == "checkpoint") {
        if (!action.contains("label") || !action["label"].is_string()) {
            if (errorOut) *errorOut = "checkpoint requires string field 'label'.";
            return false;
        }
        outStepRecord["label"] = action["label"];
    } else if (op == "clear_last_error") {
        runner.clearLastError();
        outStepRecord["ok"] = true;
    } else {
        if (errorOut) *errorOut = "Unsupported action op: " + op;
        return false;
    }

    outStepRecord["state"] = runnerStateToJson(runner, options);
    return true;
}

bool validateActionsSchema(const nlohmann::json& actionsDoc, std::string* errorOut) {
    const int version = actionsDoc.value("version", 0);
    if (!actionsDoc.is_object() ||
        actionsDoc.value("format", "") != "gyeol-runtime-actions" ||
        (version != 1 && version != 2) ||
        !actionsDoc.contains("actions") ||
        !actionsDoc["actions"].is_array()) {
        if (errorOut) *errorOut = "Invalid actions document schema.";
        return false;
    }
    return true;
}

nlohmann::json adapterSignalToResultJson(const GyeolGodotAdapter::SignalEvent& event) {
    nlohmann::json result = nlohmann::json::object();

    switch (event.type) {
    case GyeolGodotAdapter::SignalType::DialogueLine: {
        result["type"] = "LINE";
        nlohmann::json tags = nlohmann::json::object();
        for (const auto& tag : event.tags) {
            tags[tag.first] = tag.second;
        }
        result["line"] = {
            {"character", event.character},
            {"text", event.text},
            {"tags", std::move(tags)}
        };
        break;
    }
    case GyeolGodotAdapter::SignalType::ChoicesPresented: {
        result["type"] = "CHOICES";
        nlohmann::json choices = nlohmann::json::array();
        for (size_t i = 0; i < event.choices.size(); ++i) {
            choices.push_back({
                {"index", static_cast<int>(i)},
                {"text", event.choices[i]}
            });
        }
        result["choices"] = std::move(choices);
        break;
    }
    case GyeolGodotAdapter::SignalType::CommandReceived:
        result["type"] = "COMMAND";
        result["command"] = {
            {"type", event.commandType},
            {"params", event.commandParams}
        };
        break;
    case GyeolGodotAdapter::SignalType::WaitRequested:
        result["type"] = "WAIT";
        result["wait"] = {
            {"tag", event.waitTag}
        };
        break;
    case GyeolGodotAdapter::SignalType::YieldEmitted:
        result["type"] = "YIELD";
        break;
    case GyeolGodotAdapter::SignalType::StoryEnded:
        result["type"] = "END";
        break;
    }

    return result;
}

} // namespace

bool loadJsonFile(const std::string& path, nlohmann::json& out, std::string* errorOut) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        if (errorOut) *errorOut = "Failed to open JSON file: " + path;
        return false;
    }

    try {
        ifs >> out;
    } catch (const std::exception& e) {
        if (errorOut) *errorOut = std::string("Failed to parse JSON file: ") + e.what();
        return false;
    }
    return true;
}

bool loadTextFile(const std::string& path, std::string& out, std::string* errorOut) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        if (errorOut) *errorOut = "Failed to open text file: " + path;
        return false;
    }

    std::ostringstream oss;
    oss << ifs.rdbuf();
    out = oss.str();
    return true;
}

bool writeJsonFile(const std::string& path, const nlohmann::json& jsonData, std::string* errorOut) {
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
        if (errorOut) *errorOut = "Failed to write JSON file: " + path;
        return false;
    }
    ofs << jsonData.dump(2);
    return true;
}

bool compileStoryToBuffer(const std::string& storyPath,
                          std::vector<uint8_t>& outBuffer,
                          std::string* errorOut) {
    Gyeol::Parser parser;
    if (!parser.parse(storyPath)) {
        std::string mergedErrors;
        for (const auto& err : parser.getErrors()) {
            if (!mergedErrors.empty()) mergedErrors += " | ";
            mergedErrors += err;
        }
        if (errorOut) *errorOut = "Failed to parse story: " + mergedErrors;
        return false;
    }
    outBuffer = parser.compileToBuffer();
    if (outBuffer.empty()) {
        if (errorOut) *errorOut = "compileToBuffer returned empty output.";
        return false;
    }
    return true;
}

nlohmann::json runnerStateToJson(const Gyeol::Runner& runner, const RunOptions& options) {
    nlohmann::json state = nlohmann::json::object();
    state["finished"] = runner.isFinished();
    state["current_node"] = runner.getCurrentNodeName();

    nlohmann::json vars = nlohmann::json::object();
    auto names = runner.getVariableNames();
    std::sort(names.begin(), names.end());
    for (const auto& name : names) {
        vars[name] = variantToJson(runner.getVariable(name));
    }
    state["variables"] = std::move(vars);

    if (options.includeSeedInState) {
        state["seed"] = runner.getSeed();
    }
    if (options.includeLastErrorInState) {
        state["last_error"] = runner.getLastError();
    }
    if (options.includeLocaleInState) {
        state["locale"] = runner.getLocale();
    }
    if (options.includeVisitsInState) {
        nlohmann::json visits = nlohmann::json::object();
        auto nodeNames = runner.getNodeNames();
        std::sort(nodeNames.begin(), nodeNames.end());
        for (const auto& nodeName : nodeNames) {
            const auto count = runner.getVisitCount(nodeName);
            if (count > 0) visits[nodeName] = count;
        }
        state["visits"] = std::move(visits);
    }
    if (options.includeMetricsInState) {
        const auto& m = runner.getMetrics();
        state["metrics"] = {
            {"step_calls", m.stepCalls},
            {"instructions_executed", m.instructionsExecuted},
            {"line_results", m.lineResults},
            {"choice_results", m.choiceResults},
            {"command_results", m.commandResults},
            {"end_results", m.endResults},
            {"choices_made", m.choicesMade},
            {"save_ops", m.saveOperations},
            {"load_ops", m.loadOperations},
            {"snapshots_created", m.snapshotsCreated},
            {"snapshots_restored", m.snapshotsRestored},
            {"errors", m.errors}
        };
    }

    return state;
}

nlohmann::json stepResultToJson(const Gyeol::StepResult& result) {
    nlohmann::json j = nlohmann::json::object();

    switch (result.type) {
    case Gyeol::StepType::LINE: {
        j["type"] = "LINE";
        j["line"] = {
            {"character", result.line.character ? result.line.character : ""},
            {"text", result.line.text ? result.line.text : ""}
        };
        nlohmann::json tags = nlohmann::json::object();
        for (const auto& tag : result.line.tags) {
            const std::string key = tag.first ? tag.first : "";
            const std::string value = tag.second ? tag.second : "";
            tags[key] = value;
        }
        j["line"]["tags"] = std::move(tags);
        break;
    }
    case Gyeol::StepType::CHOICES: {
        j["type"] = "CHOICES";
        nlohmann::json choices = nlohmann::json::array();
        for (const auto& choice : result.choices) {
            choices.push_back({
                {"index", choice.index},
                {"text", choice.text ? choice.text : ""}
            });
        }
        j["choices"] = std::move(choices);
        break;
    }
    case Gyeol::StepType::COMMAND: {
        j["type"] = "COMMAND";
        nlohmann::json params = nlohmann::json::array();
        for (const auto* p : result.command.params) {
            params.push_back(p ? p : "");
        }
        j["command"] = {
            {"type", result.command.type ? result.command.type : ""},
            {"params", std::move(params)}
        };
        break;
    }
    case Gyeol::StepType::WAIT:
        j["type"] = "WAIT";
        j["wait"] = {
            {"tag", result.wait.tag ? result.wait.tag : ""}
        };
        break;
    case Gyeol::StepType::YIELD:
        j["type"] = "YIELD";
        break;
    case Gyeol::StepType::END:
        j["type"] = "END";
        break;
    }

    return j;
}

bool runCoreActions(const std::vector<uint8_t>& storyBuffer,
                    const nlohmann::json& actionsDoc,
                    const RunOptions& options,
                    nlohmann::json& outTranscript,
                    std::string* errorOut) {
    if (!validateActionsSchema(actionsDoc, errorOut)) return false;

    Gyeol::Runner runner;
    if (!runner.start(storyBuffer.data(), storyBuffer.size())) {
        if (errorOut) *errorOut = "Runner.start failed.";
        return false;
    }

    const std::string tempDir = std::filesystem::temp_directory_path().string();
    std::unordered_map<std::string, Gyeol::Runner::Snapshot> snapshots;

    outTranscript = nlohmann::json::object();
    outTranscript["format"] = "gyeol-runtime-transcript";
    outTranscript["version"] = 2;
    outTranscript["engine"] = options.engine;
    outTranscript["steps"] = nlohmann::json::array();
    outTranscript["checkpoints"] = nlohmann::json::array();

    for (const auto& action : actionsDoc["actions"]) {
        nlohmann::json record;
        if (!applyActionOp(runner, action, options, tempDir, snapshots, record, errorOut)) {
            return false;
        }

        if (record.value("action", "") == "checkpoint") {
            outTranscript["checkpoints"].push_back({
                {"label", record["label"]},
                {"state", record["state"]}
            });
        } else {
            outTranscript["steps"].push_back(std::move(record));
        }
    }

    return true;
}

bool runGodotAdapterActions(const std::vector<uint8_t>& storyBuffer,
                            const nlohmann::json& actionsDoc,
                            nlohmann::json& outTranscript,
                            std::string* errorOut) {
    if (!validateActionsSchema(actionsDoc, errorOut)) return false;

    Gyeol::Runner runner;
    if (!runner.start(storyBuffer.data(), storyBuffer.size())) {
        if (errorOut) *errorOut = "Runner.start failed.";
        return false;
    }

    RunOptions options;
    options.engine = "godot_adapter";

    outTranscript = {
        {"format", "gyeol-runtime-transcript"},
        {"version", 2},
        {"engine", options.engine},
        {"steps", nlohmann::json::array()},
        {"checkpoints", nlohmann::json::array()}
    };

    for (const auto& action : actionsDoc["actions"]) {
        if (!action.contains("op") || !action["op"].is_string()) {
            if (errorOut) *errorOut = "Action is missing string field 'op'.";
            return false;
        }

        const std::string op = action["op"].get<std::string>();
        nlohmann::json record = {{"action", op}};

        if (op == "set_seed") {
            if (!action.contains("seed") || !action["seed"].is_number_integer()) {
                if (errorOut) *errorOut = "set_seed requires integer field 'seed'.";
                return false;
            }
            const auto seed = static_cast<uint32_t>(action["seed"].get<int>());
            runner.setSeed(seed);
            record["seed"] = seed;
        } else if (op == "step") {
            const auto result = runner.step();
            const auto signalEvent = GyeolGodotAdapter::toSignalEvent(result);
            record["result"] = adapterSignalToResultJson(signalEvent);
        } else if (op == "choose") {
            if (!action.contains("index") || !action["index"].is_number_integer()) {
                if (errorOut) *errorOut = "choose requires integer field 'index'.";
                return false;
            }
            const int index = action["index"].get<int>();
            runner.choose(index);
            record["index"] = index;
        } else if (op == "resume") {
            record["ok"] = runner.resume();
        } else if (op == "checkpoint") {
            if (!action.contains("label") || !action["label"].is_string()) {
                if (errorOut) *errorOut = "checkpoint requires string field 'label'.";
                return false;
            }
            record["label"] = action["label"];
        } else {
            if (errorOut) *errorOut = "Unsupported adapter action op: " + op;
            return false;
        }

        record["state"] = runnerStateToJson(runner, options);
        if (op == "checkpoint") {
            outTranscript["checkpoints"].push_back({
                {"label", record["label"]},
                {"state", record["state"]}
            });
        } else {
            outTranscript["steps"].push_back(std::move(record));
        }
    }

    return true;
}

bool jsonEquals(const nlohmann::json& expected,
                const nlohmann::json& actual,
                std::string* errorOut) {
    if (expected == actual) return true;
    if (errorOut) {
        *errorOut = "JSON mismatch.\nEXPECTED:\n" + expected.dump(2) +
                    "\nACTUAL:\n" + actual.dump(2);
    }
    return false;
}

std::string resolveActionPath(const std::string& rawPath, const std::string& tempDir) {
    const std::string token = "{temp_dir}";
    const auto pos = rawPath.find(token);
    if (pos == std::string::npos) return rawPath;
    std::string resolved = rawPath;
    resolved.replace(pos, token.size(), tempDir);
    return resolved;
}

} // namespace RuntimeContract
