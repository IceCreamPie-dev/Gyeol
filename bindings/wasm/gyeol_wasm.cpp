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
    // .gyb 바이너리 버퍼로 스토리 로드 (Uint8Array → C++ vector)
    bool loadFromBuffer(const val& jsData) {
        // JS Uint8Array/Array에서 바이트 복사
        unsigned int length = jsData["length"].as<unsigned int>();
        buffer_.resize(length);
        for (unsigned int i = 0; i < length; ++i) {
            buffer_[i] = jsData[i].as<uint8_t>();
        }
        return runner_.start(buffer_.data(), buffer_.size());
    }

    // 내부 버퍼에서 직접 로드 (compileAndLoad용)
    bool loadFromVector(const std::vector<uint8_t>& data) {
        buffer_ = data;
        return runner_.start(buffer_.data(), buffer_.size());
    }

    // 한 스텝 진행 — JS 객체로 결과 반환
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
    // .gyeol 소스를 파싱하여 결과 반환 (바이너리는 내부 보관)
    val compile(const std::string& source) {
        Parser parser;
        val result = val::object();
        lastBuffer_.clear();

        if (!parser.parseString(source, "script.gyeol")) {
            result.set("success", false);
            val errors = val::array();
            for (const auto& err : parser.getErrors()) {
                errors.call<void>("push", err);
            }
            result.set("errors", errors);
            return result;
        }

        // 경고 수집
        val warnings = val::array();
        for (const auto& warn : parser.getWarnings()) {
            warnings.call<void>("push", warn);
        }

        lastBuffer_ = parser.compileToBuffer();
        if (lastBuffer_.empty()) {
            result.set("success", false);
            val errors = val::array();
            errors.call<void>("push", std::string("Compilation failed"));
            result.set("errors", errors);
            return result;
        }

        result.set("success", true);
        result.set("errors", val::array());
        result.set("warnings", warnings);
        result.set("size", static_cast<int>(lastBuffer_.size()));

        return result;
    }

    // 마지막 컴파일 결과의 바이너리 데이터 접근
    const std::vector<uint8_t>& getLastBuffer() const {
        return lastBuffer_;
    }

private:
    std::vector<uint8_t> lastBuffer_;
};

// ============================================================
// 편의 함수: 컴파일 후 바로 Runner에 로드
// ============================================================
static GyeolCompiler* g_compiler = nullptr;

val compileAndLoad(GyeolRunner& runner, GyeolCompiler& compiler, const std::string& source) {
    val result = compiler.compile(source);
    if (!result["success"].as<bool>()) {
        return result;
    }

    if (!runner.loadFromVector(compiler.getLastBuffer())) {
        result.set("success", false);
        val errors = val::array();
        errors.call<void>("push", std::string("Failed to load compiled story"));
        result.set("errors", errors);
    }

    return result;
}

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

    function("compileAndLoad", &compileAndLoad);
}
