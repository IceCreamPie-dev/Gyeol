#include "gyeol_runner.h"
#include "gyeol_generated.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <sstream>
#include <algorithm>

using namespace ICPDev::Gyeol::Schema;

namespace Gyeol {

// --- 타입 캐스팅 헬퍼 (void* → FlatBuffers 타입) ---
static const Story* asStory(const void* p) { return static_cast<const Story*>(p); }
static const Node* asNode(const void* p) { return static_cast<const Node*>(p); }

static const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>*
asPool(const void* p) {
    return static_cast<const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>*>(p);
}

namespace {

constexpr char kStateExtensionMagic[] = {'G', 'Y', 'E', 'X'};
constexpr uint32_t kStateExtensionVersion = 2;

struct RuntimeExtensionState {
    uint32_t seed = 0;
    bool hasExplicitSeed = false;
    std::string rngState;
    std::vector<std::string> pendingOnceKeys;
    std::string currentLocale;
    std::string resolvedLocale;
    std::vector<std::string> localePool;
};

void appendUint32(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

bool readUint32(const uint8_t* data, size_t size, size_t& offset, uint32_t& value) {
    if (!data || offset + 4 > size) return false;
    value = static_cast<uint32_t>(data[offset])
        | (static_cast<uint32_t>(data[offset + 1]) << 8)
        | (static_cast<uint32_t>(data[offset + 2]) << 16)
        | (static_cast<uint32_t>(data[offset + 3]) << 24);
    offset += 4;
    return true;
}

void appendBool(std::vector<uint8_t>& out, bool value) {
    out.push_back(value ? 1 : 0);
}

bool readBool(const uint8_t* data, size_t size, size_t& offset, bool& value) {
    if (!data || offset >= size) return false;
    value = data[offset++] != 0;
    return true;
}

void appendString(std::vector<uint8_t>& out, const std::string& value) {
    appendUint32(out, static_cast<uint32_t>(value.size()));
    out.insert(out.end(), value.begin(), value.end());
}

bool readString(const uint8_t* data, size_t size, size_t& offset, std::string& value) {
    uint32_t length = 0;
    if (!readUint32(data, size, offset, length) || offset + length > size) {
        return false;
    }
    value.assign(reinterpret_cast<const char*>(data + offset), length);
    offset += length;
    return true;
}

std::vector<uint8_t> serializeExtensionState(const RuntimeExtensionState& ext) {
    std::vector<uint8_t> payload;
    appendUint32(payload, kStateExtensionVersion);
    appendUint32(payload, ext.seed);
    appendBool(payload, ext.hasExplicitSeed);
    appendString(payload, ext.rngState);

    appendUint32(payload, static_cast<uint32_t>(ext.pendingOnceKeys.size()));
    for (const auto& key : ext.pendingOnceKeys) {
        appendString(payload, key);
    }

    appendString(payload, ext.currentLocale);
    appendString(payload, ext.resolvedLocale);
    appendUint32(payload, static_cast<uint32_t>(ext.localePool.size()));
    for (const auto& value : ext.localePool) {
        appendString(payload, value);
    }

    return payload;
}

bool deserializeExtensionState(const uint8_t* data, size_t size, RuntimeExtensionState& ext) {
    size_t offset = 0;
    uint32_t version = 0;
    if (!readUint32(data, size, offset, version) || (version != 1 && version != 2)) {
        return false;
    }
    if (!readUint32(data, size, offset, ext.seed)) return false;
    if (!readBool(data, size, offset, ext.hasExplicitSeed)) return false;
    if (!readString(data, size, offset, ext.rngState)) return false;

    uint32_t onceCount = 0;
    if (!readUint32(data, size, offset, onceCount)) return false;
    ext.pendingOnceKeys.clear();
    ext.pendingOnceKeys.reserve(onceCount);
    for (uint32_t i = 0; i < onceCount; ++i) {
        std::string key;
        if (!readString(data, size, offset, key)) return false;
        ext.pendingOnceKeys.push_back(std::move(key));
    }

    if (!readString(data, size, offset, ext.currentLocale)) return false;
    if (version >= 2) {
        if (!readString(data, size, offset, ext.resolvedLocale)) return false;
    } else {
        ext.resolvedLocale = ext.currentLocale;
    }

    uint32_t localeCount = 0;
    if (!readUint32(data, size, offset, localeCount)) return false;
    ext.localePool.clear();
    ext.localePool.reserve(localeCount);
    for (uint32_t i = 0; i < localeCount; ++i) {
        std::string value;
        if (!readString(data, size, offset, value)) return false;
        ext.localePool.push_back(std::move(value));
    }

    return offset == size;
}

std::vector<uint8_t> attachExtensionToState(
    const std::vector<uint8_t>& baseState,
    const RuntimeExtensionState& ext)
{
    std::vector<uint8_t> result = baseState;
    std::vector<uint8_t> payload = serializeExtensionState(ext);
    result.insert(result.end(), payload.begin(), payload.end());
    appendUint32(result, static_cast<uint32_t>(payload.size()));
    result.insert(result.end(), kStateExtensionMagic, kStateExtensionMagic + sizeof(kStateExtensionMagic));
    return result;
}

bool splitSerializedState(
    const uint8_t* data,
    size_t size,
    const uint8_t*& baseData,
    size_t& baseSize,
    RuntimeExtensionState* ext)
{
    baseData = data;
    baseSize = size;
    if (!data || size < 8) {
        return true;
    }

    if (std::memcmp(data + size - sizeof(kStateExtensionMagic), kStateExtensionMagic, sizeof(kStateExtensionMagic)) != 0) {
        return true;
    }

    size_t lengthOffset = size - sizeof(kStateExtensionMagic) - 4;
    size_t cursor = lengthOffset;
    uint32_t payloadSize = 0;
    if (!readUint32(data, size, cursor, payloadSize)) {
        return false;
    }

    if (lengthOffset < payloadSize) {
        return false;
    }

    size_t payloadOffset = lengthOffset - payloadSize;
    baseSize = payloadOffset;

    if (ext) {
        RuntimeExtensionState parsed;
        if (!deserializeExtensionState(data + payloadOffset, payloadSize, parsed)) {
            return false;
        }
        *ext = std::move(parsed);
    }

    return true;
}

} // namespace

// --- poolStr ---
const char* Runner::poolStr(int32_t index) const {
    auto* pool = asPool(pool_);
    if (!pool || index < 0 || index >= static_cast<int32_t>(pool->size())) {
        return "";
    }
    // 로케일 오버레이 우선
    if (!localePool_.empty() && index < static_cast<int32_t>(localePool_.size())
        && !localePool_[static_cast<size_t>(index)].empty()) {
        return localePool_[static_cast<size_t>(index)].c_str();
    }
    return pool->Get(static_cast<flatbuffers::uoffset_t>(index))->c_str();
}

void Runner::setError(const std::string& message) const {
    lastError_ = message;
    metrics_.errors++;
    std::cerr << "[Gyeol] " << message << std::endl;
    recordTrace("ERROR", message);
}

void Runner::clearErrorInternal() const {
    lastError_.clear();
}

void Runner::recordTrace(const std::string& kind, const std::string& detail) const {
    recordTrace(kind, currentNodeName(), pc_, detail);
}

void Runner::recordTrace(const std::string& kind, const std::string& nodeName, uint32_t pc, const std::string& detail) const {
    if (!traceEnabled_ || traceLimit_ == 0) return;
    if (trace_.size() >= traceLimit_) {
        trace_.erase(trace_.begin());
    }
    trace_.push_back({kind, nodeName, pc, detail});
    metrics_.traceEvents++;
}

void Runner::seedRngForStart() {
    if (!hasExplicitSeed_) {
        currentSeed_ = std::random_device{}();
    }
    rng_.seed(currentSeed_);
}

std::string Runner::exportRngState() const {
    std::ostringstream oss;
    oss << rng_;
    return oss.str();
}

void Runner::importRngState(const std::string& state) {
    std::istringstream iss(state);
    iss >> rng_;
}

// --- 노드 검색 및 이동 ---
void Runner::jumpToNode(const char* name) {
    auto* story = asStory(story_);
    auto* nodes = story->nodes();
    if (!nodes) {
        setError("Story has no nodes");
        finished_ = true;
        return;
    }

    for (flatbuffers::uoffset_t i = 0; i < nodes->size(); ++i) {
        auto* node = nodes->Get(i);
        if (node->name() && std::strcmp(node->name()->c_str(), name) == 0) {
            currentNode_ = node;
            pc_ = 0;
            visitCounts_[name]++;
            return;
        }
    }

    setError(std::string("Node not found: ") + (name ? name : "<null>"));
    finished_ = true;
}

void Runner::jumpToNodeById(int32_t nameId) {
    jumpToNode(poolStr(nameId));
}

// --- Variant로부터 ValueData 읽기 헬퍼 ---
static Variant readValueData(
    const void* valuePtr, ValueData valueType,
    const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>* pool)
{
    switch (valueType) {
        case ValueData::BoolValue:
            return Variant::Bool(static_cast<const BoolValue*>(valuePtr)->val());
        case ValueData::IntValue:
            return Variant::Int(static_cast<const IntValue*>(valuePtr)->val());
        case ValueData::FloatValue:
            return Variant::Float(static_cast<const FloatValue*>(valuePtr)->val());
        case ValueData::StringRef: {
            int32_t idx = static_cast<const StringRef*>(valuePtr)->index();
            if (pool && idx >= 0 && idx < static_cast<int32_t>(pool->size())) {
                return Variant::String(pool->Get(static_cast<flatbuffers::uoffset_t>(idx))->c_str());
            }
            return Variant::String("");
        }
        case ValueData::ListValue: {
            auto* lv = static_cast<const ListValue*>(valuePtr);
            std::vector<std::string> items;
            if (lv->items()) {
                for (flatbuffers::uoffset_t j = 0; j < lv->items()->size(); ++j) {
                    int32_t idx = lv->items()->Get(j);
                    if (pool && idx >= 0 && idx < static_cast<int32_t>(pool->size())) {
                        items.push_back(pool->Get(static_cast<flatbuffers::uoffset_t>(idx))->c_str());
                    }
                }
            }
            return Variant::List(std::move(items));
        }
        default:
            return Variant::Int(0);
    }
}

// --- 조건 비교 ---
static bool compareVariants(const Variant& lhs, Operator op, const Variant& rhs) {
    // 타입이 다르면 INT로 비교 시도
    if (lhs.type == Variant::BOOL || rhs.type == Variant::BOOL) {
        bool a = (lhs.type == Variant::BOOL) ? lhs.b : (lhs.i != 0);
        bool b = (rhs.type == Variant::BOOL) ? rhs.b : (rhs.i != 0);
        switch (op) {
            case Operator::Equal:          return a == b;
            case Operator::NotEqual:       return a != b;
            default:                       return false;
        }
    }

    if (lhs.type == Variant::STRING || rhs.type == Variant::STRING) {
        switch (op) {
            case Operator::Equal:          return lhs.s == rhs.s;
            case Operator::NotEqual:       return lhs.s != rhs.s;
            default:                       return false;
        }
    }

    if (lhs.type == Variant::FLOAT || rhs.type == Variant::FLOAT) {
        float a = (lhs.type == Variant::FLOAT) ? lhs.f : static_cast<float>(lhs.i);
        float b = (rhs.type == Variant::FLOAT) ? rhs.f : static_cast<float>(rhs.i);
        switch (op) {
            case Operator::Equal:          return a == b;
            case Operator::NotEqual:       return a != b;
            case Operator::Greater:        return a > b;
            case Operator::Less:           return a < b;
            case Operator::GreaterOrEqual: return a >= b;
            case Operator::LessOrEqual:    return a <= b;
        }
    }

    // INT 비교
    int32_t a = lhs.i;
    int32_t b = rhs.i;
    switch (op) {
        case Operator::Equal:          return a == b;
        case Operator::NotEqual:       return a != b;
        case Operator::Greater:        return a > b;
        case Operator::Less:           return a < b;
        case Operator::GreaterOrEqual: return a >= b;
        case Operator::LessOrEqual:    return a <= b;
    }
    return false;
}

// --- truthiness 변환 ---
static bool variantToBool(const Variant& v) {
    switch (v.type) {
        case Variant::BOOL:   return v.b;
        case Variant::INT:    return v.i != 0;
        case Variant::FLOAT:  return v.f != 0.0f;
        case Variant::STRING: return !v.s.empty();
        case Variant::LIST:   return !v.list.empty();
    }
    return false;
}

// --- 산술 연산 ---
static Variant applyBinaryOp(const Variant& lhs, ExprOp op, const Variant& rhs) {
    // Float 하나라도 있으면 float 연산
    if (lhs.type == Variant::FLOAT || rhs.type == Variant::FLOAT) {
        float a = (lhs.type == Variant::FLOAT) ? lhs.f :
                  (lhs.type == Variant::BOOL) ? (lhs.b ? 1.0f : 0.0f) :
                  static_cast<float>(lhs.i);
        float b = (rhs.type == Variant::FLOAT) ? rhs.f :
                  (rhs.type == Variant::BOOL) ? (rhs.b ? 1.0f : 0.0f) :
                  static_cast<float>(rhs.i);
        switch (op) {
            case ExprOp::Add: return Variant::Float(a + b);
            case ExprOp::Sub: return Variant::Float(a - b);
            case ExprOp::Mul: return Variant::Float(a * b);
            case ExprOp::Div: return (b != 0.0f) ? Variant::Float(a / b) : Variant::Float(0.0f);
            case ExprOp::Mod: {
                int32_t ai = static_cast<int32_t>(a);
                int32_t bi = static_cast<int32_t>(b);
                return (bi != 0) ? Variant::Int(ai % bi) : Variant::Int(0);
            }
            default: return Variant::Int(0);
        }
    }
    // INT (BOOL은 INT로 변환)
    int32_t a = (lhs.type == Variant::BOOL) ? (lhs.b ? 1 : 0) : lhs.i;
    int32_t b = (rhs.type == Variant::BOOL) ? (rhs.b ? 1 : 0) : rhs.i;
    switch (op) {
        case ExprOp::Add: return Variant::Int(a + b);
        case ExprOp::Sub: return Variant::Int(a - b);
        case ExprOp::Mul: return Variant::Int(a * b);
        case ExprOp::Div: return (b != 0) ? Variant::Int(a / b) : Variant::Int(0);
        case ExprOp::Mod: return (b != 0) ? Variant::Int(a % b) : Variant::Int(0);
        default: return Variant::Int(0);
    }
}

Variant Runner::evaluateExpression(const void* exprPtr) const {
    auto* expr = static_cast<const Expression*>(exprPtr);
    if (!expr || !expr->tokens()) return Variant::Int(0);

    auto* pool = asPool(pool_);
    std::vector<Variant> stack;

    for (flatbuffers::uoffset_t i = 0; i < expr->tokens()->size(); ++i) {
        auto* token = expr->tokens()->Get(i);

        switch (token->op()) {
            case ExprOp::PushLiteral: {
                if (token->literal_value() &&
                    token->literal_value_type() != ValueData::NONE) {
                    stack.push_back(readValueData(
                        token->literal_value(),
                        token->literal_value_type(), pool));
                } else {
                    stack.push_back(Variant::Int(0));
                }
                break;
            }
            case ExprOp::PushVar: {
                std::string varName = poolStr(token->var_name_id());
                auto it = variables_.find(varName);
                if (it != variables_.end()) {
                    stack.push_back(it->second);
                } else {
                    stack.push_back(Variant::Int(0));
                }
                break;
            }
            case ExprOp::Add:
            case ExprOp::Sub:
            case ExprOp::Mul:
            case ExprOp::Div:
            case ExprOp::Mod: {
                if (stack.size() < 2) {
                    std::cerr << "[Gyeol] Warning: expression stack underflow (arithmetic op)\n";
                    return Variant::Int(0);
                }
                Variant rhs = stack.back(); stack.pop_back();
                Variant lhs = stack.back(); stack.pop_back();
                stack.push_back(applyBinaryOp(lhs, token->op(), rhs));
                break;
            }
            case ExprOp::Negate: {
                if (stack.empty()) {
                    std::cerr << "[Gyeol] Warning: expression stack underflow (negate op)\n";
                    return Variant::Int(0);
                }
                Variant val = stack.back(); stack.pop_back();
                if (val.type == Variant::FLOAT) {
                    stack.push_back(Variant::Float(-val.f));
                } else {
                    int32_t v = (val.type == Variant::BOOL) ? (val.b ? 1 : 0) : val.i;
                    stack.push_back(Variant::Int(-v));
                }
                break;
            }
            // --- 비교 연산자 ---
            case ExprOp::CmpEq:
            case ExprOp::CmpNe:
            case ExprOp::CmpGt:
            case ExprOp::CmpLt:
            case ExprOp::CmpGe:
            case ExprOp::CmpLe: {
                if (stack.size() < 2) {
                    std::cerr << "[Gyeol] Warning: expression stack underflow (comparison op)\n";
                    return Variant::Int(0);
                }
                Variant rhs = stack.back(); stack.pop_back();
                Variant lhs = stack.back(); stack.pop_back();
                Operator cmpOp = Operator::Equal;
                switch (token->op()) {
                    case ExprOp::CmpEq: cmpOp = Operator::Equal; break;
                    case ExprOp::CmpNe: cmpOp = Operator::NotEqual; break;
                    case ExprOp::CmpGt: cmpOp = Operator::Greater; break;
                    case ExprOp::CmpLt: cmpOp = Operator::Less; break;
                    case ExprOp::CmpGe: cmpOp = Operator::GreaterOrEqual; break;
                    case ExprOp::CmpLe: cmpOp = Operator::LessOrEqual; break;
                    default: break;
                }
                stack.push_back(Variant::Bool(compareVariants(lhs, cmpOp, rhs)));
                break;
            }
            // --- 논리 연산자 ---
            case ExprOp::And: {
                if (stack.size() < 2) {
                    std::cerr << "[Gyeol] Warning: expression stack underflow (logical AND)\n";
                    return Variant::Int(0);
                }
                Variant rhs = stack.back(); stack.pop_back();
                Variant lhs = stack.back(); stack.pop_back();
                stack.push_back(Variant::Bool(variantToBool(lhs) && variantToBool(rhs)));
                break;
            }
            case ExprOp::Or: {
                if (stack.size() < 2) {
                    std::cerr << "[Gyeol] Warning: expression stack underflow (logical OR)\n";
                    return Variant::Int(0);
                }
                Variant rhs = stack.back(); stack.pop_back();
                Variant lhs = stack.back(); stack.pop_back();
                stack.push_back(Variant::Bool(variantToBool(lhs) || variantToBool(rhs)));
                break;
            }
            case ExprOp::Not: {
                if (stack.empty()) {
                    std::cerr << "[Gyeol] Warning: expression stack underflow (logical NOT)\n";
                    return Variant::Int(0);
                }
                Variant val = stack.back(); stack.pop_back();
                stack.push_back(Variant::Bool(!variantToBool(val)));
                break;
            }
            // --- 함수 연산자 ---
            case ExprOp::PushVisitCount: {
                std::string nodeName = poolStr(token->var_name_id());
                auto it = visitCounts_.find(nodeName);
                stack.push_back(Variant::Int(it != visitCounts_.end()
                    ? static_cast<int32_t>(it->second) : 0));
                break;
            }
            case ExprOp::PushVisited: {
                std::string nodeName = poolStr(token->var_name_id());
                auto it = visitCounts_.find(nodeName);
                stack.push_back(Variant::Bool(it != visitCounts_.end() && it->second > 0));
                break;
            }
            // --- 리스트 연산자 ---
            case ExprOp::ListContains: {
                if (stack.size() < 2) {
                    std::cerr << "[Gyeol] Warning: expression stack underflow (list contains)\n";
                    return Variant::Int(0);
                }
                Variant rhs = stack.back(); stack.pop_back(); // 검색할 문자열
                Variant lhs = stack.back(); stack.pop_back(); // 리스트
                if (lhs.type == Variant::LIST) {
                    std::string needle = (rhs.type == Variant::STRING) ? rhs.s : variantToString(rhs);
                    bool found = std::find(lhs.list.begin(), lhs.list.end(), needle) != lhs.list.end();
                    stack.push_back(Variant::Bool(found));
                } else {
                    stack.push_back(Variant::Bool(false));
                }
                break;
            }
            case ExprOp::ListLength: {
                std::string varName = poolStr(token->var_name_id());
                auto it = variables_.find(varName);
                if (it != variables_.end() && it->second.type == Variant::LIST) {
                    stack.push_back(Variant::Int(static_cast<int32_t>(it->second.list.size())));
                } else {
                    stack.push_back(Variant::Int(0));
                }
                break;
            }
        }
    }

    return stack.empty() ? Variant::Int(0) : stack.back();
}

// --- 문자열 보간 ---
std::string Runner::variantToString(const Variant& v) {
    switch (v.type) {
        case Variant::BOOL:   return v.b ? "true" : "false";
        case Variant::INT:    return std::to_string(v.i);
        case Variant::FLOAT: {
            std::ostringstream oss;
            oss << v.f;
            return oss.str();
        }
        case Variant::STRING: return v.s;
        case Variant::LIST: {
            std::string result;
            for (size_t i = 0; i < v.list.size(); ++i) {
                if (i > 0) result += ", ";
                result += v.list[i];
            }
            return result;
        }
    }
    return "";
}

std::string Runner::interpolateText(const char* text, int depth) const {
    if (!text) return "";

    // 재귀 깊이 제한 (악의적 입력에 의한 스택 오버플로 방지)
    if (depth > 32) {
        std::cerr << "[Gyeol] Warning: interpolation depth limit exceeded\n";
        return text;
    }

    // 빠른 경로: { 가 없으면 빈 문자열 반환 (호출측이 pool 포인터 유지)
    if (std::strchr(text, '{') == nullptr) return "";

    std::string result;
    const char* p = text;
    while (*p) {
        if (*p == '{') {
            // '}' 까지 태그 추출
            const char* start = p + 1;
            const char* end = start;
            while (*end && *end != '}') end++;
            std::string tag(start, end);
            if (*end == '}') end++;

            if (tag.size() > 3 && tag.substr(0, 3) == "if ") {
                // --- 인라인 조건 분기 처리 ---
                std::string condStr = tag.substr(3);
                bool condResult = evaluateInlineCondition(condStr);

                // {if}...{else}...{endif} 텍스트 수집
                p = end; // '{if ...}' 다음
                std::string trueBranch, falseBranch;
                bool inElse = false;
                int depth = 1;

                while (*p && depth > 0) {
                    if (*p == '{') {
                        const char* ts = p + 1;
                        const char* te = ts;
                        while (*te && *te != '}') te++;
                        std::string innerTag(ts, te);
                        if (*te == '}') te++;

                        if (innerTag.size() > 3 && innerTag.substr(0, 3) == "if ") {
                            depth++;
                            if (inElse) falseBranch += std::string(p, te);
                            else trueBranch += std::string(p, te);
                            p = te;
                        } else if (innerTag == "else" && depth == 1) {
                            inElse = true;
                            p = te;
                        } else if (innerTag == "endif") {
                            depth--;
                            if (depth == 0) {
                                p = te;
                            } else {
                                if (inElse) falseBranch += std::string(p, te);
                                else trueBranch += std::string(p, te);
                                p = te;
                            }
                        } else {
                            // 일반 {var} 태그 — 분기 텍스트에 포함
                            if (inElse) falseBranch += std::string(p, te);
                            else trueBranch += std::string(p, te);
                            p = te;
                        }
                    } else {
                        if (inElse) falseBranch += *p;
                        else trueBranch += *p;
                        p++;
                    }
                }

                // 선택된 분기를 재귀 보간
                const std::string& chosen = condResult ? trueBranch : falseBranch;
                std::string interp = interpolateText(chosen.c_str(), depth + 1);
                result += interp.empty() ? chosen : interp;
            } else {
                p = end;
                // --- 함수 호출 보간 ---
                if (tag.size() > 13 && tag.substr(0, 12) == "visit_count(" && tag.back() == ')') {
                    std::string nodeName = tag.substr(12, tag.size() - 13);
                    if (nodeName.size() >= 2 && nodeName.front() == '"' && nodeName.back() == '"')
                        nodeName = nodeName.substr(1, nodeName.size() - 2);
                    auto it = visitCounts_.find(nodeName);
                    int32_t count = (it != visitCounts_.end()) ? static_cast<int32_t>(it->second) : 0;
                    result += std::to_string(count);
                } else if (tag.size() > 9 && tag.substr(0, 8) == "visited(" && tag.back() == ')') {
                    std::string nodeName = tag.substr(8, tag.size() - 9);
                    if (nodeName.size() >= 2 && nodeName.front() == '"' && nodeName.back() == '"')
                        nodeName = nodeName.substr(1, nodeName.size() - 2);
                    auto it = visitCounts_.find(nodeName);
                    result += (it != visitCounts_.end() && it->second > 0) ? "true" : "false";
                } else if (tag.size() > 5 && tag.substr(0, 4) == "len(" && tag.back() == ')') {
                    // --- len() 함수 보간 ---
                    std::string listVarName = tag.substr(4, tag.size() - 5);
                    if (listVarName.size() >= 2 && listVarName.front() == '"' && listVarName.back() == '"')
                        listVarName = listVarName.substr(1, listVarName.size() - 2);
                    auto it = variables_.find(listVarName);
                    if (it != variables_.end() && it->second.type == Variant::LIST) {
                        result += std::to_string(it->second.list.size());
                    } else {
                        result += "0";
                    }
                } else {
                    // --- 기존 변수 보간 ---
                    auto it = variables_.find(tag);
                    if (it != variables_.end()) {
                        result += variantToString(it->second);
                    }
                    // 미정의 변수: 빈 문자열 (아무것도 추가 안함)
                }
            }
        } else {
            result += *p;
            p++;
        }
    }
    return result;
}

// --- 인라인 조건 평가 ---
bool Runner::evaluateInlineCondition(const std::string& condStr) const {
    // 공백으로 토큰 분리
    // 패턴 1: "varname" (truthiness)
    // 패턴 2: "var op literal" (비교)

    size_t pos = 0;
    // 앞뒤 공백 스킵
    while (pos < condStr.size() && condStr[pos] == ' ') pos++;

    // 첫 토큰 (변수명)
    std::string varName;
    while (pos < condStr.size() && condStr[pos] != ' ') {
        varName += condStr[pos]; pos++;
    }
    while (pos < condStr.size() && condStr[pos] == ' ') pos++;

    // visit_count(X) / visited(X) 함수 호출 감지
    bool isFuncCall = false;
    Variant lhs = Variant::Int(0);

    if (varName.size() > 13 && varName.substr(0, 12) == "visit_count(" && varName.back() == ')') {
        std::string nodeName = varName.substr(12, varName.size() - 13);
        if (nodeName.size() >= 2 && nodeName.front() == '"' && nodeName.back() == '"')
            nodeName = nodeName.substr(1, nodeName.size() - 2);
        auto it = visitCounts_.find(nodeName);
        lhs = Variant::Int(it != visitCounts_.end() ? static_cast<int32_t>(it->second) : 0);
        isFuncCall = true;
    } else if (varName.size() > 9 && varName.substr(0, 8) == "visited(" && varName.back() == ')') {
        std::string nodeName = varName.substr(8, varName.size() - 9);
        if (nodeName.size() >= 2 && nodeName.front() == '"' && nodeName.back() == '"')
            nodeName = nodeName.substr(1, nodeName.size() - 2);
        auto it = visitCounts_.find(nodeName);
        lhs = Variant::Bool(it != visitCounts_.end() && it->second > 0);
        isFuncCall = true;
    } else if (varName.size() > 5 && varName.substr(0, 4) == "len(" && varName.back() == ')') {
        std::string listVarName = varName.substr(4, varName.size() - 5);
        if (listVarName.size() >= 2 && listVarName.front() == '"' && listVarName.back() == '"')
            listVarName = listVarName.substr(1, listVarName.size() - 2);
        auto it = variables_.find(listVarName);
        if (it != variables_.end() && it->second.type == Variant::LIST) {
            lhs = Variant::Int(static_cast<int32_t>(it->second.list.size()));
        }
        isFuncCall = true;
    }

    // 연산자 없으면 truthiness 체크
    if (pos >= condStr.size()) {
        if (isFuncCall) return variantToBool(lhs);
        auto it = variables_.find(varName);
        if (it == variables_.end()) return false;
        return variantToBool(it->second);
    }

    // 연산자 추출
    std::string opStr;
    while (pos < condStr.size() && condStr[pos] != ' ') {
        opStr += condStr[pos]; pos++;
    }
    while (pos < condStr.size() && condStr[pos] == ' ') pos++;

    // 우변 리터럴 추출
    std::string rhs = condStr.substr(pos);
    // 앞뒤 공백 제거
    while (!rhs.empty() && rhs.back() == ' ') rhs.pop_back();

    // 좌변 변수 조회
    if (!isFuncCall) {
        auto it = variables_.find(varName);
        if (it != variables_.end()) lhs = it->second;
    }

    // 우변 리터럴 파싱
    Variant rhsVal = Variant::Int(0);
    if (rhs == "true") { rhsVal = Variant::Bool(true); }
    else if (rhs == "false") { rhsVal = Variant::Bool(false); }
    else if (rhs.size() >= 2 && rhs.front() == '"' && rhs.back() == '"') {
        rhsVal = Variant::String(rhs.substr(1, rhs.size() - 2));
    } else {
        // 숫자 파싱 시도
        bool hasDecimal = (rhs.find('.') != std::string::npos);
        if (hasDecimal) {
            rhsVal = Variant::Float(std::strtof(rhs.c_str(), nullptr));
        } else {
            rhsVal = Variant::Int(static_cast<int32_t>(std::strtol(rhs.c_str(), nullptr, 10)));
        }
    }

    // "in" 연산자 특별 처리 (좌변=검색값, 우변=리스트 변수명)
    if (opStr == "in") {
        std::string listVarName = rhs;
        auto it = variables_.find(listVarName);
        if (it != variables_.end() && it->second.type == Variant::LIST) {
            std::string needle;
            if (varName.size() >= 2 && varName.front() == '"' && varName.back() == '"') {
                needle = varName.substr(1, varName.size() - 2);
            } else if (isFuncCall) {
                needle = variantToString(lhs);
            } else if (lhs.type == Variant::STRING) {
                needle = lhs.s;
            } else {
                needle = variantToString(lhs);
            }
            return std::find(it->second.list.begin(), it->second.list.end(), needle) != it->second.list.end();
        }
        return false;
    }

    // 연산자 매핑 + 비교
    Operator op = Operator::Equal;
    if (opStr == "==") op = Operator::Equal;
    else if (opStr == "!=") op = Operator::NotEqual;
    else if (opStr == ">") op = Operator::Greater;
    else if (opStr == "<") op = Operator::Less;
    else if (opStr == ">=") op = Operator::GreaterOrEqual;
    else if (opStr == "<=") op = Operator::LessOrEqual;

    return compareVariants(lhs, op, rhsVal);
}

// --- 함수 매개변수 바인딩/복원 ---
void Runner::bindParameters(const void* targetNodePtr,
                            const std::vector<Variant>& argValues,
                            CallFrame& frame) {
    auto* targetNode = asNode(targetNodePtr);
    if (!targetNode || !targetNode->param_ids() || targetNode->param_ids()->size() == 0)
        return;

    auto paramCount = targetNode->param_ids()->size();
    for (flatbuffers::uoffset_t i = 0; i < paramCount; ++i) {
        std::string paramName = poolStr(targetNode->param_ids()->Get(i));
        frame.paramNames.push_back(paramName);

        // 기존 값 저장 (또는 존재하지 않았음을 기록)
        auto it = variables_.find(paramName);
        if (it != variables_.end()) {
            frame.shadowedVars.push_back({paramName, it->second, true});
        } else {
            frame.shadowedVars.push_back({paramName, Variant::Int(0), false});
        }

        // 새 값 바인딩
        if (i < static_cast<flatbuffers::uoffset_t>(argValues.size())) {
            variables_[paramName] = argValues[i];
        } else {
            variables_[paramName] = Variant::Int(0); // 부족한 인자 기본값
        }
    }
}

void Runner::restoreShadowedVars(const CallFrame& frame) {
    for (auto& sv : frame.shadowedVars) {
        if (sv.existed) {
            variables_[sv.name] = sv.value;
        } else {
            variables_.erase(sv.name);
        }
    }
}

// --- start ---
bool Runner::start(const uint8_t* buffer, size_t size) {
    clearErrorInternal();
    flatbuffers::Verifier verifier(buffer, size);
    if (!VerifyStoryBuffer(verifier)) {
        setError("Invalid buffer");
        return false;
    }

    story_ = GetStory(buffer);
    auto* story = asStory(story_);
    pool_ = story->string_pool();

    // 로케일 초기화
    localePool_.clear();
    currentLocale_.clear();
    resolvedLocale_.clear();
    localeCharacterProps_.clear();
    hasLocaleCatalog_ = false;
    catalogDefaultLocale_.clear();
    catalogLineEntriesByLocale_.clear();
    catalogCharacterEntriesByLocale_.clear();

    // global_vars 초기화
    variables_.clear();
    auto* globalVars = story->global_vars();
    if (globalVars) {
        auto* pool = asPool(pool_);
        for (flatbuffers::uoffset_t i = 0; i < globalVars->size(); ++i) {
            auto* sv = globalVars->Get(i);
            std::string varName = poolStr(sv->var_name_id());
            if (sv->expr()) {
                variables_[varName] = evaluateExpression(sv->expr());
            } else if (sv->value() && sv->value_type() != ValueData::NONE) {
                variables_[varName] = readValueData(sv->value(), sv->value_type(), pool);
            }
        }
    }

    // 캐릭터 정의 캐시
    characterProps_.clear();
    if (story->characters()) {
        auto* pool = asPool(pool_);
        for (flatbuffers::uoffset_t ci = 0; ci < story->characters()->size(); ++ci) {
            auto* charDef = story->characters()->Get(ci);
            std::string charId = poolStr(charDef->name_id());
            std::vector<std::pair<std::string, std::string>> props;
            if (charDef->properties()) {
                for (flatbuffers::uoffset_t pi = 0; pi < charDef->properties()->size(); ++pi) {
                    auto* tag = charDef->properties()->Get(pi);
                    props.emplace_back(poolStr(tag->key_id()), poolStr(tag->value_id()));
                }
            }
            characterProps_[charId] = std::move(props);
        }
    }

    // 노드 메타데이터 태그 캐시
    nodeTags_.clear();
    if (story->nodes()) {
        for (flatbuffers::uoffset_t ni = 0; ni < story->nodes()->size(); ++ni) {
            auto* node = story->nodes()->Get(ni);
            if (node->tags() && node->tags()->size() > 0) {
                std::string nodeName = node->name() ? node->name()->c_str() : "";
                std::vector<std::pair<std::string, std::string>> tags;
                for (flatbuffers::uoffset_t ti = 0; ti < node->tags()->size(); ++ti) {
                    auto* tag = node->tags()->Get(ti);
                    tags.emplace_back(poolStr(tag->key_id()), poolStr(tag->value_id()));
                }
                nodeTags_[nodeName] = std::move(tags);
            }
        }
    }

    // start_node로 이동
    callStack_.clear();
    pendingChoices_.clear();
    chosenOnceChoices_.clear();
    visitCounts_.clear();
    hasPendingReturn_ = false;
    waitBlocked_ = false;
    waitTag_.clear();
    hitBreakpoint_ = false;
    seedRngForStart();
    finished_ = false;
    recordTrace("START", std::string("seed=") + std::to_string(currentSeed_));

    if (story->start_node_name()) {
        jumpToNode(story->start_node_name()->c_str());
    } else {
        setError("Story has no start node");
        finished_ = true;
        return false;
    }

    return !finished_;
}

bool Runner::startAtNode(const uint8_t* buffer, size_t size, const std::string& nodeName) {
    if (!start(buffer, size)) return false;
    // start()가 이미 start_node로 이동 + visitCount++
    // 지정된 노드로 재점프 (visitCount 리셋 후 다시 증가)
    visitCounts_.clear();
    jumpToNode(nodeName.c_str());
    recordTrace("START_AT_NODE", nodeName, pc_, "");
    return !finished_;
}

// --- step ---
StepResult Runner::step() {
    StepResult result;
    result.type = StepType::END;
    metrics_.stepCalls++;

    if (finished_) {
        metrics_.endResults++;
        recordTrace("END", "already_finished");
        return result;
    }

    auto* node = asNode(currentNode_);
    auto* pool = asPool(pool_);

    while (true) {
        if (waitBlocked_) {
            result.type = StepType::WAIT;
            result.wait.tag = waitTag_.empty() ? nullptr : waitTag_.c_str();
            setError("Cannot step while waiting; call resume() first");
            recordTrace("WAIT_BLOCKED", nodeNameFromPtr(currentNode_), pc_, waitTag_);
            return result;
        }

        // 노드 끝 도달
        if (!node || !node->lines() || pc_ >= node->lines()->size()) {
            // call stack에서 복귀
            if (!callStack_.empty()) {
                auto frame = callStack_.back();
                callStack_.pop_back();

                // 섀도된 변수 먼저 복원
                restoreShadowedVars(frame);

                // 명시적 return이 있었으면 반환값 저장
                if (hasPendingReturn_ && !frame.returnVarName.empty()) {
                    variables_[frame.returnVarName] = pendingReturnValue_;
                }
                hasPendingReturn_ = false;

                currentNode_ = frame.node;
                pc_ = frame.pc;
                node = asNode(currentNode_);
                continue;
            }
            // 스토리 종료
            finished_ = true;
            result.type = StepType::END;
            metrics_.endResults++;
            recordTrace("END", "story_finished");
            return result;
        }

        // --- Debug: breakpoint/step mode check (zero-cost when not debugging) ---
        if (!breakpoints_.empty() || stepMode_) {
            if (hitBreakpoint_) {
                // 이전 호출에서 여기서 멈췄음 — 해제하고 계속 진행
                hitBreakpoint_ = false;
            } else if (stepMode_) {
                // Step mode: 매 instruction마다 정지
                hitBreakpoint_ = true;
                return result;
            } else {
                // Breakpoint만 체크 (stepMode_ == false)
                std::string curNode = nodeNameFromPtr(currentNode_);
                if (breakpoints_.count({curNode, pc_}) > 0) {
                    hitBreakpoint_ = true;
                    return result;
                }
            }
        }

        auto* instr = node->lines()->Get(pc_);
        pc_++;
        metrics_.instructionsExecuted++;

        switch (instr->data_type()) {
            case OpData::Line: {
                auto* line = instr->data_as_Line();
                result.type = StepType::LINE;
                result.line.character = (line->character_id() >= 0)
                    ? poolStr(line->character_id()) : nullptr;
                const char* rawText = poolStr(line->text_id());
                std::string interp = interpolateText(rawText);
                if (!interp.empty()) {
                    result.ownedStrings_.push_back(std::move(interp));
                    result.line.text = result.ownedStrings_.back().c_str();
                } else {
                    result.line.text = rawText;
                }
                // tags 채우기
                if (line->tags()) {
                    for (flatbuffers::uoffset_t t = 0; t < line->tags()->size(); ++t) {
                        auto* tag = line->tags()->Get(t);
                        result.line.tags.emplace_back(
                            poolStr(tag->key_id()),
                            poolStr(tag->value_id())
                        );
                    }
                }
                metrics_.lineResults++;
                recordTrace(
                    "LINE",
                    nodeNameFromPtr(currentNode_),
                    pc_ - 1,
                    result.line.text ? result.line.text : "");
                return result;
            }

            case OpData::Choice: {
                // Choice를 연속으로 수집 (수식어 + 조건 필터링)
                auto* choice = instr->data_as_Choice();
                pendingChoices_.clear();
                std::string curNodeName = node->name() ? node->name()->c_str() : "";

                // 모든 연속 Choice를 먼저 수집 (raw)
                struct RawChoice {
                    int32_t text_id;
                    int32_t target_node_name_id;
                    int32_t condition_var_id;
                    int8_t choice_modifier;
                    uint32_t instrPc; // instruction의 PC (once_key용)
                };
                std::vector<RawChoice> rawChoices;
                rawChoices.push_back({choice->text_id(), choice->target_node_name_id(),
                                      choice->condition_var_id(),
                                      static_cast<int8_t>(choice->choice_modifier()),
                                      pc_ - 1});

                while (node->lines() && pc_ < node->lines()->size()) {
                    auto* next = node->lines()->Get(pc_);
                    if (next->data_type() != OpData::Choice) break;
                    auto* nextChoice = next->data_as_Choice();
                    rawChoices.push_back({nextChoice->text_id(), nextChoice->target_node_name_id(),
                                          nextChoice->condition_var_id(),
                                          static_cast<int8_t>(nextChoice->choice_modifier()),
                                          pc_});
                    pc_++;
                }

                // 조건 + once + modifier 필터링
                std::vector<PendingChoice> normalChoices;  // Default/Sticky/Once (visible)
                std::vector<PendingChoice> fallbackChoices; // Fallback (visible)

                for (const auto& rc : rawChoices) {
                    // 1) condition_var_id 체크
                    bool condVisible = true;
                    if (rc.condition_var_id >= 0) {
                        std::string condVar = poolStr(rc.condition_var_id);
                        auto it = variables_.find(condVar);
                        if (it != variables_.end()) {
                            condVisible = (it->second.type == Variant::BOOL) ? it->second.b : (it->second.i != 0);
                        } else {
                            condVisible = false;
                        }
                    }
                    if (!condVisible) continue;

                    // 2) once 체크: 이미 선택한 once 선택지는 숨김
                    std::string onceKey = curNodeName + ":" + std::to_string(rc.instrPc);
                    if (rc.choice_modifier == 1 /* Once */) {
                        if (chosenOnceChoices_.count(onceKey) > 0) continue;
                    }

                    PendingChoice pc;
                    pc.text_id = rc.text_id;
                    pc.target_node_name_id = rc.target_node_name_id;
                    pc.choice_modifier = rc.choice_modifier;
                    pc.once_key = onceKey;

                    if (rc.choice_modifier == 3 /* Fallback */) {
                        fallbackChoices.push_back(std::move(pc));
                    } else {
                        normalChoices.push_back(std::move(pc));
                    }
                }

                // Fallback: normal이 모두 비었을 때만 fallback 사용
                if (normalChoices.empty()) {
                    pendingChoices_ = std::move(fallbackChoices);
                } else {
                    pendingChoices_ = std::move(normalChoices);
                }

                // 결과 반환
                result.type = StepType::CHOICES;
                for (int k = 0; k < static_cast<int>(pendingChoices_.size()); ++k) {
                    ChoiceData cd;
                    const char* rawText = poolStr(pendingChoices_[k].text_id);
                    std::string interp = interpolateText(rawText);
                    if (!interp.empty()) {
                        result.ownedStrings_.push_back(std::move(interp));
                        cd.text = result.ownedStrings_.back().c_str();
                    } else {
                        cd.text = rawText;
                    }
                    cd.index = k;
                    result.choices.push_back(cd);
                }
                metrics_.choiceResults++;
                recordTrace(
                    "CHOICES",
                    nodeNameFromPtr(currentNode_),
                    pc_ - 1,
                    std::to_string(result.choices.size()));
                return result;
            }

            case OpData::Jump: {
                auto* jump = instr->data_as_Jump();
                metrics_.jumps++;
                if (jump->is_call()) {
                    // 1. 호출자 컨텍스트에서 인자 평가
                    std::vector<Variant> argValues;
                    if (jump->arg_exprs()) {
                        for (flatbuffers::uoffset_t ai = 0; ai < jump->arg_exprs()->size(); ++ai) {
                            argValues.push_back(evaluateExpression(jump->arg_exprs()->Get(ai)));
                        }
                    }
                    // 2. call frame push
                    callStack_.push_back({currentNode_, pc_, "", {}, {}});
                    metrics_.calls++;
                    recordTrace("CALL", nodeNameFromPtr(currentNode_), pc_ - 1, poolStr(jump->target_node_name_id()));
                    // 3. 대상 노드로 이동
                    jumpToNodeById(jump->target_node_name_id());
                    // 4. 매개변수 바인딩
                    if (!finished_) {
                        bindParameters(currentNode_, argValues, callStack_.back());
                    }
                } else {
                    recordTrace("JUMP", nodeNameFromPtr(currentNode_), pc_ - 1, poolStr(jump->target_node_name_id()));
                    jumpToNodeById(jump->target_node_name_id());
                }
                node = asNode(currentNode_);
                if (finished_) {
                    result.type = StepType::END;
                    return result;
                }
                continue; // 다음 instruction 계속
            }

            case OpData::SetVar: {
                auto* setvar = instr->data_as_SetVar();
                std::string varName = poolStr(setvar->var_name_id());
                Variant newVal = Variant::Int(0);
                if (setvar->expr()) {
                    newVal = evaluateExpression(setvar->expr());
                } else if (setvar->value() && setvar->value_type() != ValueData::NONE) {
                    newVal = readValueData(setvar->value(), setvar->value_type(), pool);
                }

                switch (setvar->assign_op()) {
                    case AssignOp::Assign:
                        variables_[varName] = newVal;
                        break;
                    case AssignOp::Append: {
                        auto& existing = variables_[varName];
                        if (existing.type == Variant::LIST) {
                            std::string item = (newVal.type == Variant::STRING) ? newVal.s : variantToString(newVal);
                            if (std::find(existing.list.begin(), existing.list.end(), item) == existing.list.end()) {
                                existing.list.push_back(item);
                            }
                        } else {
                            variables_[varName] = newVal;
                        }
                        break;
                    }
                    case AssignOp::Remove: {
                        auto it = variables_.find(varName);
                        if (it != variables_.end() && it->second.type == Variant::LIST) {
                            std::string item = (newVal.type == Variant::STRING) ? newVal.s : variantToString(newVal);
                            auto& list = it->second.list;
                            list.erase(std::remove(list.begin(), list.end(), item), list.end());
                        }
                        break;
                    }
                }
                recordTrace("SET_VAR", nodeNameFromPtr(currentNode_), pc_ - 1, varName);
                continue;
            }

            case OpData::Condition: {
                auto* cond = instr->data_as_Condition();
                bool condResult;
                metrics_.conditionsEvaluated++;

                if (cond->cond_expr()) {
                    // 논리 연산자 경로: 전체 불리언 표현식 평가
                    Variant result = evaluateExpression(cond->cond_expr());
                    condResult = variantToBool(result);
                } else {
                    // 기존 경로: lhs_expr/op/rhs_expr 또는 var_name_id/compare_value
                    Variant lhs = Variant::Int(0);
                    if (cond->lhs_expr()) {
                        lhs = evaluateExpression(cond->lhs_expr());
                    } else {
                        std::string varName = poolStr(cond->var_name_id());
                        auto it = variables_.find(varName);
                        if (it != variables_.end()) {
                            lhs = it->second;
                        }
                    }

                    Variant rhs = Variant::Int(0);
                    if (cond->rhs_expr()) {
                        rhs = evaluateExpression(cond->rhs_expr());
                    } else if (cond->compare_value() && cond->compare_value_type() != ValueData::NONE) {
                        rhs = readValueData(cond->compare_value(), cond->compare_value_type(), pool);
                    }

                    condResult = compareVariants(lhs, cond->op(), rhs);
                }

                int32_t targetId = condResult ? cond->true_jump_node_id() : cond->false_jump_node_id();
                recordTrace(
                    "CONDITION",
                    nodeNameFromPtr(currentNode_),
                    pc_ - 1,
                    condResult ? "true" : "false");
                if (targetId >= 0) {
                    jumpToNodeById(targetId);
                    node = asNode(currentNode_);
                    if (finished_) {
                        result.type = StepType::END;
                        return result;
                    }
                }
                // targetId < 0이면 다음 줄로 계속
                continue;
            }

            case OpData::Random: {
                auto* random = instr->data_as_Random();
                if (!random || !random->branches() || random->branches()->size() == 0)
                    continue;

                int64_t totalWeight = 0;
                for (flatbuffers::uoffset_t k = 0; k < random->branches()->size(); ++k) {
                    int w = random->branches()->Get(k)->weight();
                    if (w > 0) {
                        totalWeight += w;
                        if (totalWeight > 0x7FFFFFFF) {
                            std::cerr << "[Gyeol] Warning: random weight sum overflow, capping\n";
                            totalWeight = 0x7FFFFFFF;
                            break;
                        }
                    }
                }
                if (totalWeight <= 0) continue; // 모든 weight 0 → skip

                std::uniform_int_distribution<int> dist(0, static_cast<int>(totalWeight) - 1);
                int roll = dist(rng_);
                metrics_.randomRolls++;
                recordTrace("RANDOM", nodeNameFromPtr(currentNode_), pc_ - 1, std::to_string(roll));

                int64_t cumulative = 0;
                for (flatbuffers::uoffset_t k = 0; k < random->branches()->size(); ++k) {
                    int w = random->branches()->Get(k)->weight();
                    if (w <= 0) continue;
                    cumulative += w;
                    if (roll < cumulative) {
                        jumpToNodeById(random->branches()->Get(k)->target_node_name_id());
                        node = asNode(currentNode_);
                        if (finished_) { result.type = StepType::END; return result; }
                        break;
                    }
                }
                continue;
            }

            case OpData::Command: {
                auto* cmd = instr->data_as_Command();
                result.type = StepType::COMMAND;
                result.command.type = poolStr(cmd->type_id());
                result.command.args.clear();
                auto* args = cmd->args();
                if (args) {
                    for (flatbuffers::uoffset_t k = 0; k < args->size(); ++k) {
                        const auto* arg = args->Get(k);
                        if (!arg) continue;
                        CommandArgData outArg;
                        switch (arg->kind()) {
                        case CommandArgKind::String:
                            outArg.type = CommandArgType::STRING;
                            outArg.text = poolStr(arg->string_id());
                            break;
                        case CommandArgKind::Identifier:
                            outArg.type = CommandArgType::IDENTIFIER;
                            outArg.text = poolStr(arg->string_id());
                            break;
                        case CommandArgKind::Int:
                            outArg.type = CommandArgType::INT;
                            outArg.intValue = arg->int_value();
                            break;
                        case CommandArgKind::Float:
                            outArg.type = CommandArgType::FLOAT;
                            outArg.floatValue = arg->float_value();
                            break;
                        case CommandArgKind::Bool:
                            outArg.type = CommandArgType::BOOL;
                            outArg.boolValue = arg->bool_value();
                            break;
                        default:
                            outArg.type = CommandArgType::STRING;
                            outArg.text.clear();
                            break;
                        }
                        result.command.args.push_back(std::move(outArg));
                    }
                }
                metrics_.commandResults++;
                recordTrace(
                    "COMMAND",
                    nodeNameFromPtr(currentNode_),
                    pc_ - 1,
                    result.command.type ? result.command.type : "");
                return result;
            }

            case OpData::Wait: {
                auto* wait = instr->data_as_Wait();
                waitBlocked_ = true;
                waitTag_.clear();
                if (wait && wait->tag_id() >= 0) {
                    waitTag_ = poolStr(wait->tag_id());
                }

                result.type = StepType::WAIT;
                result.wait.tag = waitTag_.empty() ? nullptr : waitTag_.c_str();
                recordTrace("WAIT", nodeNameFromPtr(currentNode_), pc_ - 1, waitTag_);
                return result;
            }

            case OpData::Yield: {
                result.type = StepType::YIELD;
                recordTrace("YIELD", nodeNameFromPtr(currentNode_), pc_ - 1, "");
                return result;
            }

            case OpData::Return: {
                auto* ret = instr->data_as_Return();
                metrics_.returns++;
                recordTrace("RETURN", nodeNameFromPtr(currentNode_), pc_ - 1, "");
                // 반환값 평가
                if (ret->expr()) {
                    pendingReturnValue_ = evaluateExpression(ret->expr());
                    hasPendingReturn_ = true;
                } else if (ret->value() && ret->value_type() != ValueData::NONE) {
                    pendingReturnValue_ = readValueData(ret->value(), ret->value_type(), pool);
                    hasPendingReturn_ = true;
                } else {
                    // bare "return" (값 없음)
                    hasPendingReturn_ = false;
                }

                // call stack pop
                if (!callStack_.empty()) {
                    auto frame = callStack_.back();
                    callStack_.pop_back();

                    // 섀도된 변수 먼저 복원
                    restoreShadowedVars(frame);

                    // 반환값 저장 (호출자 스코프)
                    if (hasPendingReturn_ && !frame.returnVarName.empty()) {
                        variables_[frame.returnVarName] = pendingReturnValue_;
                    }
                    hasPendingReturn_ = false;

                    currentNode_ = frame.node;
                    pc_ = frame.pc;
                    node = asNode(currentNode_);
                    continue;
                }

                // call stack 비어있으면 스토리 종료
                hasPendingReturn_ = false;
                finished_ = true;
                result.type = StepType::END;
                metrics_.endResults++;
                recordTrace("END", "return_without_call");
                return result;
            }

            case OpData::CallWithReturn: {
                auto* cwr = instr->data_as_CallWithReturn();
                std::string returnVarName = poolStr(cwr->return_var_name_id());

                // 1. 호출자 컨텍스트에서 인자 평가
                std::vector<Variant> argValues;
                if (cwr->arg_exprs()) {
                    for (flatbuffers::uoffset_t ai = 0; ai < cwr->arg_exprs()->size(); ++ai) {
                        argValues.push_back(evaluateExpression(cwr->arg_exprs()->Get(ai)));
                    }
                }

                // 2. call stack에 반환변수 이름 포함하여 push
                callStack_.push_back({currentNode_, pc_, returnVarName, {}, {}});
                metrics_.calls++;
                recordTrace("CALL_RETURN", nodeNameFromPtr(currentNode_), pc_ - 1, poolStr(cwr->target_node_name_id()));

                // 3. 대상 노드로 이동
                jumpToNodeById(cwr->target_node_name_id());

                // 4. 매개변수 바인딩
                if (!finished_) {
                    bindParameters(currentNode_, argValues, callStack_.back());
                }

                node = asNode(currentNode_);
                if (finished_) {
                    result.type = StepType::END;
                    return result;
                }
                continue;
            }

            default:
                continue;
        }
    }
}

bool Runner::resume() {
    if (!waitBlocked_) {
        setError("Cannot resume when runner is not waiting");
        return false;
    }

    waitBlocked_ = false;
    waitTag_.clear();
    recordTrace("RESUME", nodeNameFromPtr(currentNode_), pc_, "");
    return true;
}

// --- choose ---
void Runner::choose(int index) {
    if (waitBlocked_) {
        setError("Cannot choose while waiting; call resume() first");
        return;
    }

    if (index < 0 || index >= static_cast<int>(pendingChoices_.size())) {
        setError("Invalid choice index: " + std::to_string(index));
        return;
    }

    // Once 선택지 추적: 선택된 once 선택지의 키를 기록
    auto& chosen = pendingChoices_[index];
    metrics_.choicesMade++;
    recordTrace("CHOOSE", nodeNameFromPtr(currentNode_), pc_, poolStr(chosen.text_id));
    if (chosen.choice_modifier == 1 /* Once */ && !chosen.once_key.empty()) {
        chosenOnceChoices_.insert(chosen.once_key);
    }

    jumpToNodeById(chosen.target_node_name_id);
    pendingChoices_.clear();
}

// --- isFinished ---
bool Runner::isFinished() const {
    return finished_;
}

// --- setSeed ---
void Runner::setSeed(uint32_t seed) {
    hasExplicitSeed_ = true;
    currentSeed_ = seed;
    rng_.seed(seed);
}

uint32_t Runner::getSeed() const {
    return currentSeed_;
}

// --- Variable access API ---
Variant Runner::getVariable(const std::string& name) const {
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        return it->second;
    }
    return Variant::Int(0);
}

void Runner::setVariable(const std::string& name, const Variant& value) {
    variables_[name] = value;
}

bool Runner::hasVariable(const std::string& name) const {
    return variables_.find(name) != variables_.end();
}

std::vector<std::string> Runner::getVariableNames() const {
    std::vector<std::string> names;
    names.reserve(variables_.size());
    for (const auto& pair : variables_) {
        names.push_back(pair.first);
    }
    return names;
}

// --- Visit tracking API ---
int32_t Runner::getVisitCount(const std::string& nodeName) const {
    auto it = visitCounts_.find(nodeName);
    return (it != visitCounts_.end()) ? static_cast<int32_t>(it->second) : 0;
}

bool Runner::hasVisited(const std::string& nodeName) const {
    auto it = visitCounts_.find(nodeName);
    return (it != visitCounts_.end() && it->second > 0);
}

// --- Character API ---
std::string Runner::getCharacterProperty(const std::string& characterId, const std::string& key) const {
    auto overlayIt = localeCharacterProps_.find(characterId);
    if (overlayIt != localeCharacterProps_.end()) {
        auto propIt = overlayIt->second.find(key);
        if (propIt != overlayIt->second.end()) return propIt->second;
    }

    auto it = characterProps_.find(characterId);
    if (it == characterProps_.end()) return "";
    for (const auto& prop : it->second) {
        if (prop.first == key) return prop.second;
    }
    return "";
}

std::vector<std::string> Runner::getCharacterNames() const {
    std::vector<std::string> names;
    names.reserve(characterProps_.size() + localeCharacterProps_.size());
    for (const auto& pair : characterProps_) {
        names.push_back(pair.first);
    }
    for (const auto& pair : localeCharacterProps_) {
        if (std::find(names.begin(), names.end(), pair.first) == names.end()) {
            names.push_back(pair.first);
        }
    }
    return names;
}

std::string Runner::getCharacterDisplayName(const std::string& characterId) const {
    std::string displayName = getCharacterProperty(characterId, "displayName");
    if (!displayName.empty()) return displayName;
    std::string name = getCharacterProperty(characterId, "name");
    return name.empty() ? characterId : name;
}

// --- Node Tag API ---
std::string Runner::getNodeTag(const std::string& nodeName, const std::string& key) const {
    auto it = nodeTags_.find(nodeName);
    if (it == nodeTags_.end()) return "";
    for (const auto& tag : it->second) {
        if (tag.first == key) return tag.second;
    }
    return "";
}

std::vector<std::pair<std::string, std::string>> Runner::getNodeTags(const std::string& nodeName) const {
    auto it = nodeTags_.find(nodeName);
    if (it == nodeTags_.end()) return {};
    return it->second;
}

bool Runner::hasNodeTag(const std::string& nodeName, const std::string& key) const {
    auto it = nodeTags_.find(nodeName);
    if (it == nodeTags_.end()) return false;
    for (const auto& tag : it->second) {
        if (tag.first == key) return true;
    }
    return false;
}

// --- Save/Load 헬퍼 ---
std::string Runner::nodeNameFromPtr(const void* nodePtr) const {
    auto* node = asNode(nodePtr);
    if (node && node->name()) {
        return node->name()->c_str();
    }
    return "";
}

std::string Runner::currentNodeName() const {
    return nodeNameFromPtr(currentNode_);
}

const void* Runner::findNodeByName(const char* name) const {
    auto* story = asStory(story_);
    if (!story || !story->nodes()) return nullptr;
    auto* nodes = story->nodes();
    for (flatbuffers::uoffset_t i = 0; i < nodes->size(); ++i) {
        auto* node = nodes->Get(i);
        if (node->name() && std::strcmp(node->name()->c_str(), name) == 0) {
            return node;
        }
    }
    return nullptr;
}

int32_t Runner::findStringInPool(const char* str) const {
    auto* pool = asPool(pool_);
    if (!pool) return -1;
    for (flatbuffers::uoffset_t i = 0; i < pool->size(); ++i) {
        if (std::strcmp(pool->Get(i)->c_str(), str) == 0) {
            return static_cast<int32_t>(i);
        }
    }
    return -1;
}

std::vector<uint8_t> Runner::serializeStateBuffer() const {
    if (!story_) {
        setError("No story loaded");
        return {};
    }

    auto* story = asStory(story_);

    SaveStateT state;
    state.version = "1.0";
    state.story_version = story->version() ? story->version()->c_str() : "";
    state.current_node_name = currentNodeName();
    state.pc = pc_;
    state.finished = finished_;
    state.wait_blocked = waitBlocked_;
    state.wait_tag = waitTag_;

    for (const auto& pair : variables_) {
        auto sv = std::make_unique<SavedVarT>();
        sv->name = pair.first;
        switch (pair.second.type) {
            case Variant::BOOL: {
                auto bv = std::make_unique<BoolValueT>();
                bv->val = pair.second.b;
                sv->value.Set(std::move(*bv));
                break;
            }
            case Variant::INT: {
                auto iv = std::make_unique<IntValueT>();
                iv->val = pair.second.i;
                sv->value.Set(std::move(*iv));
                break;
            }
            case Variant::FLOAT: {
                auto fv = std::make_unique<FloatValueT>();
                fv->val = pair.second.f;
                sv->value.Set(std::move(*fv));
                break;
            }
            case Variant::STRING: {
                sv->string_value = pair.second.s;
                auto sr = std::make_unique<StringRefT>();
                sr->index = -1;
                sv->value.Set(std::move(*sr));
                break;
            }
            case Variant::LIST: {
                for (const auto& item : pair.second.list) {
                    sv->list_items.push_back(item);
                }
                auto lv = std::make_unique<ListValueT>();
                sv->value.Set(std::move(*lv));
                break;
            }
        }
        state.variables.push_back(std::move(sv));
    }

    for (const auto& frame : callStack_) {
        auto cf = std::make_unique<SavedCallFrameT>();
        cf->node_name = nodeNameFromPtr(frame.node);
        cf->pc = frame.pc;
        cf->return_var_name = frame.returnVarName;

        for (const auto& sv : frame.shadowedVars) {
            auto ssv = std::make_unique<SavedShadowedVarT>();
            ssv->name = sv.name;
            ssv->existed = sv.existed;
            switch (sv.value.type) {
                case Variant::BOOL: {
                    auto bv = std::make_unique<BoolValueT>();
                    bv->val = sv.value.b;
                    ssv->value.Set(std::move(*bv));
                    break;
                }
                case Variant::INT: {
                    auto iv = std::make_unique<IntValueT>();
                    iv->val = sv.value.i;
                    ssv->value.Set(std::move(*iv));
                    break;
                }
                case Variant::FLOAT: {
                    auto fv = std::make_unique<FloatValueT>();
                    fv->val = sv.value.f;
                    ssv->value.Set(std::move(*fv));
                    break;
                }
                case Variant::STRING: {
                    ssv->string_value = sv.value.s;
                    auto sr = std::make_unique<StringRefT>();
                    sr->index = -1;
                    ssv->value.Set(std::move(*sr));
                    break;
                }
                case Variant::LIST: {
                    for (const auto& item : sv.value.list) {
                        ssv->list_items.push_back(item);
                    }
                    auto lv = std::make_unique<ListValueT>();
                    ssv->value.Set(std::move(*lv));
                    break;
                }
            }
            cf->shadowed_vars.push_back(std::move(ssv));
        }

        cf->param_names = frame.paramNames;
        state.call_stack.push_back(std::move(cf));
    }

    for (const auto& pc : pendingChoices_) {
        auto spc = std::make_unique<SavedPendingChoiceT>();
        spc->text = poolStr(pc.text_id);
        spc->target_node_name = poolStr(pc.target_node_name_id);
        spc->choice_modifier = static_cast<ChoiceModifier>(pc.choice_modifier);
        state.pending_choices.push_back(std::move(spc));
    }

    for (const auto& key : chosenOnceChoices_) {
        state.chosen_once_choices.push_back(key);
    }

    for (const auto& pair : visitCounts_) {
        auto vc = std::make_unique<SavedVisitCountT>();
        vc->node_name = pair.first;
        vc->count = pair.second;
        state.visit_counts.push_back(std::move(vc));
    }

    flatbuffers::FlatBufferBuilder fbb;
    auto offset = SaveState::Pack(fbb, &state);
    fbb.Finish(offset);

    std::vector<uint8_t> baseState(
        fbb.GetBufferPointer(),
        fbb.GetBufferPointer() + fbb.GetSize());

    RuntimeExtensionState ext;
    ext.seed = currentSeed_;
    ext.hasExplicitSeed = hasExplicitSeed_;
    ext.rngState = exportRngState();
    ext.pendingOnceKeys.reserve(pendingChoices_.size());
    for (const auto& pc : pendingChoices_) {
        ext.pendingOnceKeys.push_back(pc.once_key);
    }
    ext.currentLocale = currentLocale_;
    ext.resolvedLocale = resolvedLocale_;
    ext.localePool = localePool_;

    return attachExtensionToState(baseState, ext);
}

bool Runner::deserializeStateBuffer(const uint8_t* data, size_t size) {
    if (!story_) {
        setError("No story loaded");
        return false;
    }

    clearErrorInternal();

    RuntimeExtensionState ext;
    const uint8_t* baseData = nullptr;
    size_t baseSize = 0;
    if (!splitSerializedState(data, size, baseData, baseSize, &ext)) {
        setError("Invalid save metadata");
        return false;
    }
    if (!baseData || baseSize == 0) {
        setError("Invalid save file");
        return false;
    }

    flatbuffers::Verifier verifier(baseData, baseSize);
    auto* saveState = flatbuffers::GetRoot<SaveState>(baseData);
    if (!saveState->Verify(verifier)) {
        setError("Invalid save file");
        return false;
    }

    finished_ = saveState->finished();
    pc_ = saveState->pc();
    waitBlocked_ = saveState->wait_blocked();
    waitTag_ = saveState->wait_tag() ? saveState->wait_tag()->c_str() : "";
    hitBreakpoint_ = false;

    if (saveState->current_node_name()) {
        currentNode_ = findNodeByName(saveState->current_node_name()->c_str());
        if (!currentNode_ && !finished_) {
            setError(std::string("Save state node not found: ") + saveState->current_node_name()->c_str());
            finished_ = true;
            return false;
        }
    } else {
        currentNode_ = nullptr;
    }

    variables_.clear();
    auto* vars = saveState->variables();
    if (vars) {
        auto* pool = asPool(pool_);
        for (flatbuffers::uoffset_t i = 0; i < vars->size(); ++i) {
            auto* sv = vars->Get(i);
            if (!sv->name()) continue;
            std::string varName = sv->name()->c_str();

            if (sv->value_type() == ValueData::StringRef) {
                variables_[varName] = sv->string_value()
                    ? Variant::String(sv->string_value()->c_str())
                    : Variant::String("");
            } else if (sv->value_type() == ValueData::ListValue) {
                std::vector<std::string> items;
                if (sv->list_items()) {
                    for (flatbuffers::uoffset_t j = 0; j < sv->list_items()->size(); ++j) {
                        items.push_back(sv->list_items()->Get(j)->c_str());
                    }
                }
                variables_[varName] = Variant::List(std::move(items));
            } else if (sv->value() && sv->value_type() != ValueData::NONE) {
                variables_[varName] = readValueData(sv->value(), sv->value_type(), pool);
            }
        }
    }

    callStack_.clear();
    auto* stack = saveState->call_stack();
    if (stack) {
        for (flatbuffers::uoffset_t i = 0; i < stack->size(); ++i) {
            auto* frame = stack->Get(i);
            if (!frame->node_name()) continue;
            const void* nodePtr = findNodeByName(frame->node_name()->c_str());
            if (!nodePtr) continue;

            std::string retVar = frame->return_var_name()
                ? frame->return_var_name()->c_str() : "";
            CallFrame cf = {nodePtr, frame->pc(), retVar, {}, {}};

            if (frame->shadowed_vars()) {
                for (flatbuffers::uoffset_t j = 0; j < frame->shadowed_vars()->size(); ++j) {
                    auto* ssv = frame->shadowed_vars()->Get(j);
                    ShadowedVar sv;
                    sv.name = ssv->name() ? ssv->name()->c_str() : "";
                    sv.existed = ssv->existed();
                    if (ssv->value_type() == ValueData::StringRef) {
                        sv.value = ssv->string_value()
                            ? Variant::String(ssv->string_value()->c_str())
                            : Variant::String("");
                    } else if (ssv->value_type() == ValueData::ListValue) {
                        std::vector<std::string> items;
                        if (ssv->list_items()) {
                            for (flatbuffers::uoffset_t k = 0; k < ssv->list_items()->size(); ++k) {
                                items.push_back(ssv->list_items()->Get(k)->c_str());
                            }
                        }
                        sv.value = Variant::List(std::move(items));
                    } else if (ssv->value() && ssv->value_type() != ValueData::NONE) {
                        sv.value = readValueData(ssv->value(), ssv->value_type(), asPool(pool_));
                    }
                    cf.shadowedVars.push_back(std::move(sv));
                }
            }

            if (frame->param_names()) {
                for (flatbuffers::uoffset_t j = 0; j < frame->param_names()->size(); ++j) {
                    cf.paramNames.push_back(frame->param_names()->Get(j)->c_str());
                }
            }

            callStack_.push_back(std::move(cf));
        }
    }
    hasPendingReturn_ = false;

    pendingChoices_.clear();
    auto* choices = saveState->pending_choices();
    if (choices) {
        for (flatbuffers::uoffset_t i = 0; i < choices->size(); ++i) {
            auto* pc = choices->Get(i);
            int32_t textId = pc->text() ? findStringInPool(pc->text()->c_str()) : -1;
            int32_t targetId = pc->target_node_name()
                ? findStringInPool(pc->target_node_name()->c_str()) : -1;
            if (textId >= 0 && targetId >= 0) {
                PendingChoice pending;
                pending.text_id = textId;
                pending.target_node_name_id = targetId;
                pending.choice_modifier = static_cast<int8_t>(pc->choice_modifier());
                pendingChoices_.push_back(std::move(pending));
            }
        }
    }

    if (ext.pendingOnceKeys.size() == pendingChoices_.size()) {
        for (size_t i = 0; i < pendingChoices_.size(); ++i) {
            pendingChoices_[i].once_key = ext.pendingOnceKeys[i];
        }
    }

    chosenOnceChoices_.clear();
    auto* onceKeys = saveState->chosen_once_choices();
    if (onceKeys) {
        for (flatbuffers::uoffset_t i = 0; i < onceKeys->size(); ++i) {
            if (onceKeys->Get(i)) {
                chosenOnceChoices_.insert(onceKeys->Get(i)->c_str());
            }
        }
    }

    visitCounts_.clear();
    auto* vcs = saveState->visit_counts();
    if (vcs) {
        for (flatbuffers::uoffset_t i = 0; i < vcs->size(); ++i) {
            auto* vc = vcs->Get(i);
            if (vc->node_name()) {
                visitCounts_[vc->node_name()->c_str()] = vc->count();
            }
        }
    }

    currentSeed_ = ext.seed;
    hasExplicitSeed_ = ext.hasExplicitSeed;
    if (!ext.rngState.empty()) {
        importRngState(ext.rngState);
    } else {
        rng_.seed(currentSeed_);
    }

    currentLocale_ = ext.currentLocale;
    resolvedLocale_ = ext.resolvedLocale;
    localePool_ = ext.localePool;
    localeCharacterProps_.clear();
    if (hasLocaleCatalog_ && !currentLocale_.empty()) {
        applyLocaleSelection(currentLocale_, false);
    }

    return true;
}

// --- saveState ---
bool Runner::saveState(const std::string& filepath) const {
    std::vector<uint8_t> data = serializeStateBuffer();
    if (data.empty()) {
        return false;
    }

    std::ofstream ofs(filepath, std::ios::binary);
    if (!ofs) {
        setError("Cannot open save file: " + filepath);
        return false;
    }
    ofs.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    if (!ofs.good()) {
        setError("Failed to write save file: " + filepath);
        return false;
    }

    metrics_.saveOperations++;
    recordTrace("SAVE", currentNodeName(), pc_, filepath);
    return true;
}

// --- loadState ---
bool Runner::loadState(const std::string& filepath) {
    std::ifstream ifs(filepath, std::ios::binary | std::ios::ate);
    if (!ifs) {
        setError("Cannot open save file: " + filepath);
        return false;
    }

    auto size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    if (!ifs.read(reinterpret_cast<char*>(buf.data()), size)) {
        setError("Failed to read save file");
        return false;
    }

    if (!deserializeStateBuffer(buf.data(), buf.size())) {
        return false;
    }

    metrics_.loadOperations++;
    recordTrace("LOAD", currentNodeName(), pc_, filepath);
    return true;
}

Runner::Snapshot Runner::snapshot() const {
    Snapshot snapshot;
    snapshot.bytes = serializeStateBuffer();
    if (!snapshot.bytes.empty()) {
        metrics_.snapshotsCreated++;
        recordTrace("SNAPSHOT", currentNodeName(), pc_, "create");
    }
    return snapshot;
}

bool Runner::restore(const Snapshot& snapshot) {
    if (snapshot.bytes.empty()) {
        setError("Snapshot is empty");
        return false;
    }
    if (!deserializeStateBuffer(snapshot.bytes.data(), snapshot.bytes.size())) {
        return false;
    }

    metrics_.snapshotsRestored++;
    recordTrace("RESTORE", currentNodeName(), pc_, "snapshot");
    return true;
}

} // namespace Gyeol
