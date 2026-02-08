/**
 * Gyeol WASM Binding Layer
 *
 * Emscripten Embind를 사용하여 Runner + Parser를 JavaScript에 노출.
 * 브라우저에서 .gyeol 스크립트를 컴파일하고 .gyb 스토리를 실행할 수 있다.
 */

#include <emscripten/bind.h>
#include <emscripten/val.h>
#include "gyeol_runner.h"
#include "gyeol_parser.h"

using namespace emscripten;
using namespace Gyeol;

// ============================================================
// Wrapper: GyeolRunner (Runner + 버퍼 소유권 관리)
// ============================================================
class GyeolRunner {
public:
    // .gyb 바이너리 버퍼로 스토리 로드
    bool loadFromBuffer(const std::string& data) {
        buffer_.assign(data.begin(), data.end());
        return runner_.start(buffer_.data(), buffer_.size());
    }

    // 한 스텝 진행 — JS 객체로 결과 반환
    val step() {
        auto result = runner_.step();
        val obj = val::object();

        switch (result.type) {
        case StepType::LINE:
            obj.set("type", std::string("LINE"));
            obj.set("character", result.line.character ? std::string(result.line.character) : val::null());
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

    void choose(int index) {
        runner_.choose(index);
    }

    bool isFinished() const {
        return runner_.isFinished();
    }

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
            for (const auto& item : v.list) {
                arr.call<void>("push", item);
            }
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

    bool hasVariable(const std::string& name) const {
        return runner_.hasVariable(name);
    }

    val getVariableNames() const {
        val arr = val::array();
        for (const auto& name : runner_.getVariableNames()) {
            arr.call<void>("push", name);
        }
        return arr;
    }

    // Visit count API
    int getVisitCount(const std::string& nodeName) const {
        return runner_.getVisitCount(nodeName);
    }

    bool hasVisited(const std::string& nodeName) const {
        return runner_.hasVisited(nodeName);
    }

    // Character API
    std::string getCharacterProperty(const std::string& id, const std::string& key) const {
        return runner_.getCharacterProperty(id, key);
    }

    std::string getCharacterDisplayName(const std::string& id) const {
        return runner_.getCharacterDisplayName(id);
    }

    val getCharacterNames() const {
        val arr = val::array();
        for (const auto& name : runner_.getCharacterNames()) {
            arr.call<void>("push", name);
        }
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
    void setSeed(unsigned int seed) {
        runner_.setSeed(seed);
    }

private:
    Runner runner_;
    std::vector<uint8_t> buffer_;
};

// ============================================================
// Wrapper: GyeolCompiler (Parser + 메모리 컴파일)
// ============================================================
class GyeolCompiler {
public:
    // .gyeol 소스를 파싱하여 .gyb 바이너리 반환
    val compile(const std::string& source) {
        Parser parser;
        val result = val::object();

        if (!parser.parseString(source, "script.gyeol")) {
            result.set("success", false);
            val errors = val::array();
            for (const auto& err : parser.getErrors()) {
                errors.call<void>("push", err);
            }
            result.set("errors", errors);
            result.set("data", val::null());
            return result;
        }

        // 경고 수집
        val warnings = val::array();
        for (const auto& warn : parser.getWarnings()) {
            warnings.call<void>("push", warn);
        }

        auto buffer = parser.compileToBuffer();
        if (buffer.empty()) {
            result.set("success", false);
            val errors = val::array();
            errors.call<void>("push", std::string("Compilation failed"));
            result.set("errors", errors);
            result.set("data", val::null());
            return result;
        }

        result.set("success", true);
        result.set("errors", val::array());
        result.set("warnings", warnings);
        // 바이너리 데이터를 문자열로 전달 (JS에서 Uint8Array로 변환)
        result.set("data", std::string(reinterpret_cast<const char*>(buffer.data()), buffer.size()));
        result.set("size", static_cast<int>(buffer.size()));

        return result;
    }
};

// ============================================================
// Embind 등록
// ============================================================
EMSCRIPTEN_BINDINGS(gyeol) {
    class_<GyeolRunner>("GyeolRunner")
        .constructor<>()
        .function("loadFromBuffer", &GyeolRunner::loadFromBuffer)
        .function("step", &GyeolRunner::step)
        .function("choose", &GyeolRunner::choose)
        .function("isFinished", &GyeolRunner::isFinished)
        .function("getVariable", &GyeolRunner::getVariable)
        .function("setVariableInt", &GyeolRunner::setVariableInt)
        .function("setVariableFloat", &GyeolRunner::setVariableFloat)
        .function("setVariableBool", &GyeolRunner::setVariableBool)
        .function("setVariableString", &GyeolRunner::setVariableString)
        .function("hasVariable", &GyeolRunner::hasVariable)
        .function("getVariableNames", &GyeolRunner::getVariableNames)
        .function("getVisitCount", &GyeolRunner::getVisitCount)
        .function("hasVisited", &GyeolRunner::hasVisited)
        .function("getCharacterProperty", &GyeolRunner::getCharacterProperty)
        .function("getCharacterDisplayName", &GyeolRunner::getCharacterDisplayName)
        .function("getCharacterNames", &GyeolRunner::getCharacterNames)
        .function("getNodeTag", &GyeolRunner::getNodeTag)
        .function("hasNodeTag", &GyeolRunner::hasNodeTag)
        .function("setSeed", &GyeolRunner::setSeed)
        ;

    class_<GyeolCompiler>("GyeolCompiler")
        .constructor<>()
        .function("compile", &GyeolCompiler::compile)
        ;
}
