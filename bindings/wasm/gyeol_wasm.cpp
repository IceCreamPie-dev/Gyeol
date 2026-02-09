/**
 * Gyeol WASM Binding Layer
 *
 * Emscripten Embind를 사용하여 Runner + Parser를 JavaScript에 노출.
 * 브라우저에서 .gyeol 스크립트를 컴파일하고 .gyb 스토리를 실행할 수 있다.
 *
 * 핵심 설계: GyeolEngine이 Compiler + Runner를 통합.
 * compile() → 내부 버퍼에 바이너리 저장 → loadLast() → Runner에 직접 전달.
 * 바이너리가 JS를 경유하지 않으므로 null 바이트 문제 없음.
 */

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include "gyeol_runner.h"
#include "gyeol_parser.h"
#include "gyeol_json_export.h"

using namespace emscripten;
using namespace Gyeol;

// ============================================================
// GyeolEngine: Compiler + Runner 통합 (바이너리 직접 전달)
// ============================================================
class GyeolEngine {
public:
    // .gyeol 소스를 컴파일 → 결과 반환 (바이너리는 내부 보관)
    val compile(const std::string& source) {
        Parser parser;
        val result = val::object();
        compiledBuffer_.clear();

        if (!parser.parseString(source, "script.gyeol")) {
            result.set("success", false);
            val errors = val::array();
            for (const auto& err : parser.getErrors()) {
                errors.call<void>("push", err);
            }
            result.set("errors", errors);
            return result;
        }

        val warnings = val::array();
        for (const auto& warn : parser.getWarnings()) {
            warnings.call<void>("push", warn);
        }

        compiledBuffer_ = parser.compileToBuffer();
        if (compiledBuffer_.empty()) {
            result.set("success", false);
            val errors = val::array();
            errors.call<void>("push", std::string("Compilation failed"));
            result.set("errors", errors);
            return result;
        }

        result.set("success", true);
        result.set("errors", val::array());
        result.set("warnings", warnings);
        result.set("size", static_cast<int>(compiledBuffer_.size()));
        return result;
    }

    // 마지막 컴파일 결과를 Runner에 로드
    bool loadLast() {
        if (compiledBuffer_.empty()) return false;
        // Runner용 버퍼 복사 (Runner가 버퍼 수명 동안 유효해야 함)
        runnerBuffer_ = compiledBuffer_;
        return runner_.start(runnerBuffer_.data(), runnerBuffer_.size());
    }

    // compile + loadLast 한번에
    val compileAndLoad(const std::string& source) {
        val result = compile(source);
        if (!result["success"].as<bool>()) {
            return result;
        }
        if (!loadLast()) {
            result.set("success", false);
            val errors = val::array();
            errors.call<void>("push", std::string("Failed to load compiled story"));
            result.set("errors", errors);
        }
        return result;
    }

    // --- Runner API ---
    val step() {
        auto result = runner_.step();
        val obj = val::object();

        switch (result.type) {
        case StepType::LINE:
            obj.set("type", std::string("LINE"));
            if (result.line.character) {
                obj.set("character", std::string(result.line.character));
            } else {
                obj.set("character", val::null());
            }
            obj.set("text", result.line.text ? std::string(result.line.text) : std::string(""));
            {
                val tags = val::array();
                for (size_t i = 0; i < result.line.tags.size(); ++i) {
                    val tag = val::object();
                    tag.set("key", std::string(result.line.tags[i].first ? result.line.tags[i].first : ""));
                    tag.set("value", std::string(result.line.tags[i].second ? result.line.tags[i].second : ""));
                    tags.call<void>("push", tag);
                }
                obj.set("tags", tags);
            }
            break;

        case StepType::CHOICES:
            obj.set("type", std::string("CHOICES"));
            {
                val choices = val::array();
                for (size_t i = 0; i < result.choices.size(); ++i) {
                    val ch = val::object();
                    ch.set("text", result.choices[i].text ? std::string(result.choices[i].text) : std::string(""));
                    ch.set("index", result.choices[i].index);
                    choices.call<void>("push", ch);
                }
                obj.set("choices", choices);
            }
            break;

        case StepType::COMMAND:
            obj.set("type", std::string("COMMAND"));
            obj.set("commandType", result.command.type ? std::string(result.command.type) : std::string(""));
            {
                val params = val::array();
                for (auto* p : result.command.params) {
                    params.call<void>("push", p ? std::string(p) : std::string(""));
                }
                obj.set("params", params);
            }
            break;

        case StepType::END:
            obj.set("type", std::string("END"));
            break;
        }

        return obj;
    }

    void choose(int index) { runner_.choose(index); }
    bool isFinished() const { return runner_.isFinished(); }

    // Variable API
    val getVariable(const std::string& name) const {
        if (!runner_.hasVariable(name)) return val::null();
        auto v = runner_.getVariable(name);
        switch (v.type) {
        case Variant::BOOL:   return val(v.b);
        case Variant::INT:    return val(v.i);
        case Variant::FLOAT:  return val(v.f);
        case Variant::STRING: return val(v.s);
        case Variant::LIST: {
            val arr = val::array();
            for (const auto& item : v.list) arr.call<void>("push", item);
            return arr;
        }
        }
        return val::null();
    }

    void setVariableInt(const std::string& name, int value) {
        runner_.setVariable(name, Variant::Int(value));
    }
    void setVariableFloat(const std::string& name, float value) {
        runner_.setVariable(name, Variant::Float(value));
    }
    void setVariableBool(const std::string& name, bool value) {
        runner_.setVariable(name, Variant::Bool(value));
    }
    void setVariableString(const std::string& name, const std::string& value) {
        runner_.setVariable(name, Variant::String(value));
    }
    bool hasVariable(const std::string& name) const { return runner_.hasVariable(name); }

    val getVariableNames() const {
        val arr = val::array();
        for (const auto& name : runner_.getVariableNames()) arr.call<void>("push", name);
        return arr;
    }

    // Visit count API
    int getVisitCount(const std::string& nodeName) const { return runner_.getVisitCount(nodeName); }
    bool hasVisited(const std::string& nodeName) const { return runner_.hasVisited(nodeName); }

    // Character API
    std::string getCharacterProperty(const std::string& id, const std::string& key) const {
        return runner_.getCharacterProperty(id, key);
    }
    std::string getCharacterDisplayName(const std::string& id) const {
        return runner_.getCharacterDisplayName(id);
    }
    val getCharacterNames() const {
        val arr = val::array();
        for (const auto& name : runner_.getCharacterNames()) arr.call<void>("push", name);
        return arr;
    }

    // Node tag API
    std::string getNodeTag(const std::string& nodeName, const std::string& key) const {
        return runner_.getNodeTag(nodeName, key);
    }
    bool hasNodeTag(const std::string& nodeName, const std::string& key) const {
        return runner_.hasNodeTag(nodeName, key);
    }

    // RNG seed
    void setSeed(unsigned int seed) { runner_.setSeed(seed); }

    // --- JSON IR API ---

    // .gyeol 소스를 컴파일하여 JSON IR 문자열로 반환
    val compileToJson(const std::string& source) {
        Parser parser;
        val result = val::object();

        if (!parser.parseString(source, "script.gyeol")) {
            result.set("success", false);
            val errors = val::array();
            for (const auto& err : parser.getErrors()) {
                errors.call<void>("push", err);
            }
            result.set("errors", errors);
            result.set("json", std::string(""));
            return result;
        }

        val warnings = val::array();
        for (const auto& warn : parser.getWarnings()) {
            warnings.call<void>("push", warn);
        }

        std::string jsonStr = JsonExport::toJsonString(parser.getStory());

        result.set("success", true);
        result.set("errors", val::array());
        result.set("warnings", warnings);
        result.set("json", jsonStr);
        return result;
    }

    // --- Graph Data API (비주얼 에디터용) ---

    // 현재 노드 이름 반환
    std::string getCurrentNodeName() const { return runner_.getCurrentNodeName(); }

    // 특정 노드에서 시작
    bool startFromNode(const std::string& nodeName) {
        if (compiledBuffer_.empty()) return false;
        runnerBuffer_ = compiledBuffer_;
        return runner_.startAtNode(runnerBuffer_.data(), runnerBuffer_.size(), nodeName);
    }

    // 전체 그래프 데이터 반환
    val getGraphData() {
        auto data = runner_.getGraphData();
        val result = val::object();
        result.set("startNode", data.startNode);

        // nodes
        val nodes = val::array();
        for (const auto& gn : data.nodes) {
            val node = val::object();
            node.set("name", gn.name);
            node.set("instructionCount", gn.instructionCount);

            val params = val::array();
            for (const auto& p : gn.params) params.call<void>("push", p);
            node.set("params", params);

            val tags = val::array();
            for (const auto& t : gn.tags) {
                val tag = val::object();
                tag.set("key", t.first);
                tag.set("value", t.second);
                tags.call<void>("push", tag);
            }
            node.set("tags", tags);

            // summary
            val summary = val::object();
            summary.set("lineCount", gn.summary.lineCount);
            summary.set("choiceCount", gn.summary.choiceCount);
            summary.set("hasJump", gn.summary.hasJump);
            summary.set("hasCondition", gn.summary.hasCondition);
            summary.set("hasRandom", gn.summary.hasRandom);
            summary.set("hasCommand", gn.summary.hasCommand);
            summary.set("firstLine", gn.summary.firstLine);

            val chars = val::array();
            for (const auto& c : gn.summary.characters) chars.call<void>("push", c);
            summary.set("characters", chars);

            node.set("summary", summary);
            nodes.call<void>("push", node);
        }
        result.set("nodes", nodes);

        // edges
        val edges = val::array();
        for (const auto& ge : data.edges) {
            val edge = val::object();
            edge.set("from", ge.from);
            edge.set("to", ge.to);
            edge.set("type", ge.type);
            edge.set("label", ge.label);
            edges.call<void>("push", edge);
        }
        result.set("edges", edges);

        return result;
    }

private:
    Runner runner_;
    std::vector<uint8_t> compiledBuffer_;  // 마지막 컴파일 결과
    std::vector<uint8_t> runnerBuffer_;    // Runner가 참조하는 버퍼
};

// ============================================================
// Embind 등록
// ============================================================
EMSCRIPTEN_BINDINGS(gyeol) {
    class_<GyeolEngine>("GyeolEngine")
        .constructor<>()
        // Compiler
        .function("compile", &GyeolEngine::compile)
        .function("compileToJson", &GyeolEngine::compileToJson)
        .function("loadLast", &GyeolEngine::loadLast)
        .function("compileAndLoad", &GyeolEngine::compileAndLoad)
        // Runner
        .function("step", &GyeolEngine::step)
        .function("choose", &GyeolEngine::choose)
        .function("isFinished", &GyeolEngine::isFinished)
        // Variables
        .function("getVariable", &GyeolEngine::getVariable)
        .function("setVariableInt", &GyeolEngine::setVariableInt)
        .function("setVariableFloat", &GyeolEngine::setVariableFloat)
        .function("setVariableBool", &GyeolEngine::setVariableBool)
        .function("setVariableString", &GyeolEngine::setVariableString)
        .function("hasVariable", &GyeolEngine::hasVariable)
        .function("getVariableNames", &GyeolEngine::getVariableNames)
        // Visit
        .function("getVisitCount", &GyeolEngine::getVisitCount)
        .function("hasVisited", &GyeolEngine::hasVisited)
        // Character
        .function("getCharacterProperty", &GyeolEngine::getCharacterProperty)
        .function("getCharacterDisplayName", &GyeolEngine::getCharacterDisplayName)
        .function("getCharacterNames", &GyeolEngine::getCharacterNames)
        // Node tags
        .function("getNodeTag", &GyeolEngine::getNodeTag)
        .function("hasNodeTag", &GyeolEngine::hasNodeTag)
        // RNG
        .function("setSeed", &GyeolEngine::setSeed)
        // Graph (비주얼 에디터)
        .function("getCurrentNodeName", &GyeolEngine::getCurrentNodeName)
        .function("startFromNode", &GyeolEngine::startFromNode)
        .function("getGraphData", &GyeolEngine::getGraphData)
        ;
}
