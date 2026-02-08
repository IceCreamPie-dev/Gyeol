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

// --- 노드 검색 및 이동 ---
void Runner::jumpToNode(const char* name) {
    auto* story = asStory(story_);
    auto* nodes = story->nodes();
    if (!nodes) {
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

    std::cerr << "[Gyeol] Node not found: " << name << std::endl;
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
                if (stack.size() < 2) return Variant::Int(0);
                Variant rhs = stack.back(); stack.pop_back();
                Variant lhs = stack.back(); stack.pop_back();
                stack.push_back(applyBinaryOp(lhs, token->op(), rhs));
                break;
            }
            case ExprOp::Negate: {
                if (stack.empty()) return Variant::Int(0);
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
                if (stack.size() < 2) return Variant::Int(0);
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
                if (stack.size() < 2) return Variant::Int(0);
                Variant rhs = stack.back(); stack.pop_back();
                Variant lhs = stack.back(); stack.pop_back();
                stack.push_back(Variant::Bool(variantToBool(lhs) && variantToBool(rhs)));
                break;
            }
            case ExprOp::Or: {
                if (stack.size() < 2) return Variant::Int(0);
                Variant rhs = stack.back(); stack.pop_back();
                Variant lhs = stack.back(); stack.pop_back();
                stack.push_back(Variant::Bool(variantToBool(lhs) || variantToBool(rhs)));
                break;
            }
            case ExprOp::Not: {
                if (stack.empty()) return Variant::Int(0);
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
                if (stack.size() < 2) return Variant::Int(0);
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

std::string Runner::interpolateText(const char* text) const {
    if (!text) return "";

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
                std::string interp = interpolateText(chosen.c_str());
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
    flatbuffers::Verifier verifier(buffer, size);
    if (!VerifyStoryBuffer(verifier)) {
        std::cerr << "[Gyeol] Invalid buffer" << std::endl;
        return false;
    }

    story_ = GetStory(buffer);
    auto* story = asStory(story_);
    pool_ = story->string_pool();

    // 로케일 초기화
    localePool_.clear();
    currentLocale_.clear();

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

    // start_node로 이동
    callStack_.clear();
    pendingChoices_.clear();
    visitCounts_.clear();
    hasPendingReturn_ = false;
    rng_.seed(std::random_device{}());
    finished_ = false;

    if (story->start_node_name()) {
        jumpToNode(story->start_node_name()->c_str());
    } else {
        finished_ = true;
        return false;
    }

    return !finished_;
}

// --- step ---
StepResult Runner::step() {
    StepResult result;
    result.type = StepType::END;

    if (finished_) return result;

    auto* node = asNode(currentNode_);
    auto* pool = asPool(pool_);

    while (true) {
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
                return result;
            }

            case OpData::Choice: {
                // Choice를 연속으로 수집
                auto* choice = instr->data_as_Choice();
                pendingChoices_.clear();

                // condition_var_id 체크 (조건부 선택지)
                bool visible = true;
                if (choice->condition_var_id() >= 0) {
                    std::string condVar = poolStr(choice->condition_var_id());
                    auto it = variables_.find(condVar);
                    if (it != variables_.end()) {
                        visible = (it->second.type == Variant::BOOL) ? it->second.b : (it->second.i != 0);
                    } else {
                        visible = false;
                    }
                }
                if (visible) {
                    pendingChoices_.push_back({choice->text_id(), choice->target_node_name_id()});
                }

                // 다음 instruction도 Choice면 계속 수집
                while (node->lines() && pc_ < node->lines()->size()) {
                    auto* next = node->lines()->Get(pc_);
                    if (next->data_type() != OpData::Choice) break;
                    pc_++;

                    auto* nextChoice = next->data_as_Choice();
                    bool nextVisible = true;
                    if (nextChoice->condition_var_id() >= 0) {
                        std::string condVar = poolStr(nextChoice->condition_var_id());
                        auto it = variables_.find(condVar);
                        if (it != variables_.end()) {
                            nextVisible = (it->second.type == Variant::BOOL) ? it->second.b : (it->second.i != 0);
                        } else {
                            nextVisible = false;
                        }
                    }
                    if (nextVisible) {
                        pendingChoices_.push_back({nextChoice->text_id(), nextChoice->target_node_name_id()});
                    }
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
                return result;
            }

            case OpData::Jump: {
                auto* jump = instr->data_as_Jump();
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
                    // 3. 대상 노드로 이동
                    jumpToNodeById(jump->target_node_name_id());
                    // 4. 매개변수 바인딩
                    if (!finished_) {
                        bindParameters(currentNode_, argValues, callStack_.back());
                    }
                } else {
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
                continue;
            }

            case OpData::Condition: {
                auto* cond = instr->data_as_Condition();
                bool condResult;

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

                int totalWeight = 0;
                for (flatbuffers::uoffset_t k = 0; k < random->branches()->size(); ++k) {
                    int w = random->branches()->Get(k)->weight();
                    if (w > 0) totalWeight += w;
                }
                if (totalWeight <= 0) continue; // 모든 weight 0 → skip

                std::uniform_int_distribution<int> dist(0, totalWeight - 1);
                int roll = dist(rng_);

                int cumulative = 0;
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
                result.command.params.clear();
                auto* params = cmd->params();
                if (params) {
                    for (flatbuffers::uoffset_t k = 0; k < params->size(); ++k) {
                        result.command.params.push_back(poolStr(params->Get(k)));
                    }
                }
                return result;
            }

            case OpData::Return: {
                auto* ret = instr->data_as_Return();
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

// --- choose ---
void Runner::choose(int index) {
    if (index < 0 || index >= static_cast<int>(pendingChoices_.size())) {
        std::cerr << "[Gyeol] Invalid choice index: " << index << std::endl;
        return;
    }

    jumpToNodeById(pendingChoices_[index].target_node_name_id);
    pendingChoices_.clear();
}

// --- isFinished ---
bool Runner::isFinished() const {
    return finished_;
}

// --- setSeed ---
void Runner::setSeed(uint32_t seed) {
    rng_.seed(seed);
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
    auto it = characterProps_.find(characterId);
    if (it == characterProps_.end()) return "";
    for (const auto& prop : it->second) {
        if (prop.first == key) return prop.second;
    }
    return "";
}

std::vector<std::string> Runner::getCharacterNames() const {
    std::vector<std::string> names;
    names.reserve(characterProps_.size());
    for (const auto& pair : characterProps_) {
        names.push_back(pair.first);
    }
    return names;
}

std::string Runner::getCharacterDisplayName(const std::string& characterId) const {
    std::string name = getCharacterProperty(characterId, "name");
    return name.empty() ? characterId : name;
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

// --- saveState ---
bool Runner::saveState(const std::string& filepath) const {
    if (!story_) {
        std::cerr << "[Gyeol] No story loaded" << std::endl;
        return false;
    }

    auto* story = asStory(story_);

    // SaveStateT 객체 생성
    SaveStateT state;
    state.version = "1.0";
    state.story_version = story->version() ? story->version()->c_str() : "";
    state.current_node_name = currentNodeName();
    state.pc = pc_;
    state.finished = finished_;

    // 변수 저장
    for (auto& pair : variables_) {
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
                // STRING은 string_value 필드에 직접 저장 (pool index 불필요)
                sv->string_value = pair.second.s;
                // value union은 비워둠 (타입 구분용으로 StringRef 사용)
                auto sr = std::make_unique<StringRefT>();
                sr->index = -1; // 의미 없음, 타입 마커용
                sv->value.Set(std::move(*sr));
                break;
            }
            case Variant::LIST: {
                // LIST는 list_items 필드에 직접 저장
                for (const auto& item : pair.second.list) {
                    sv->list_items.push_back(item);
                }
                // value union은 ListValue 타입 마커용
                auto lv = std::make_unique<ListValueT>();
                sv->value.Set(std::move(*lv));
                break;
            }
        }
        state.variables.push_back(std::move(sv));
    }

    // Call stack 저장
    for (auto& frame : callStack_) {
        auto cf = std::make_unique<SavedCallFrameT>();
        cf->node_name = nodeNameFromPtr(frame.node);
        cf->pc = frame.pc;
        cf->return_var_name = frame.returnVarName;

        // 섀도된 변수 저장
        for (auto& sv : frame.shadowedVars) {
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

        // 매개변수 이름 저장
        cf->param_names = frame.paramNames;

        state.call_stack.push_back(std::move(cf));
    }

    // Pending choices 저장
    for (auto& pc : pendingChoices_) {
        auto spc = std::make_unique<SavedPendingChoiceT>();
        spc->text = poolStr(pc.text_id);
        spc->target_node_name = poolStr(pc.target_node_name_id);
        state.pending_choices.push_back(std::move(spc));
    }

    // Visit counts 저장
    for (auto& pair : visitCounts_) {
        auto vc = std::make_unique<SavedVisitCountT>();
        vc->node_name = pair.first;
        vc->count = pair.second;
        state.visit_counts.push_back(std::move(vc));
    }

    // FlatBuffers 직렬화
    flatbuffers::FlatBufferBuilder fbb;
    auto offset = SaveState::Pack(fbb, &state);
    fbb.Finish(offset);

    // 파일 쓰기
    std::ofstream ofs(filepath, std::ios::binary);
    if (!ofs) {
        std::cerr << "[Gyeol] Cannot open save file: " << filepath << std::endl;
        return false;
    }
    ofs.write(reinterpret_cast<const char*>(fbb.GetBufferPointer()), fbb.GetSize());
    return ofs.good();
}

// --- loadState ---
bool Runner::loadState(const std::string& filepath) {
    if (!story_) {
        std::cerr << "[Gyeol] No story loaded" << std::endl;
        return false;
    }

    // 파일 읽기
    std::ifstream ifs(filepath, std::ios::binary | std::ios::ate);
    if (!ifs) {
        std::cerr << "[Gyeol] Cannot open save file: " << filepath << std::endl;
        return false;
    }

    auto size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    if (!ifs.read(reinterpret_cast<char*>(buf.data()), size)) {
        std::cerr << "[Gyeol] Failed to read save file" << std::endl;
        return false;
    }

    // 검증
    flatbuffers::Verifier verifier(buf.data(), buf.size());
    auto* saveState = flatbuffers::GetRoot<SaveState>(buf.data());
    if (!saveState->Verify(verifier)) {
        std::cerr << "[Gyeol] Invalid save file" << std::endl;
        return false;
    }

    // 상태 복원
    finished_ = saveState->finished();
    pc_ = saveState->pc();

    // 현재 노드 복원
    if (saveState->current_node_name()) {
        currentNode_ = findNodeByName(saveState->current_node_name()->c_str());
        if (!currentNode_ && !finished_) {
            std::cerr << "[Gyeol] Save state node not found: "
                      << saveState->current_node_name()->c_str() << std::endl;
            finished_ = true;
            return false;
        }
    } else {
        currentNode_ = nullptr;
    }

    // 변수 복원
    variables_.clear();
    auto* vars = saveState->variables();
    if (vars) {
        auto* pool = asPool(pool_);
        for (flatbuffers::uoffset_t i = 0; i < vars->size(); ++i) {
            auto* sv = vars->Get(i);
            if (!sv->name()) continue;
            std::string varName = sv->name()->c_str();

            if (sv->value_type() == ValueData::StringRef) {
                // STRING 타입: string_value 필드에서 읽기
                if (sv->string_value()) {
                    variables_[varName] = Variant::String(sv->string_value()->c_str());
                } else {
                    variables_[varName] = Variant::String("");
                }
            } else if (sv->value_type() == ValueData::ListValue) {
                // LIST 타입: list_items 필드에서 읽기
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

    // Call stack 복원
    callStack_.clear();
    auto* stack = saveState->call_stack();
    if (stack) {
        for (flatbuffers::uoffset_t i = 0; i < stack->size(); ++i) {
            auto* frame = stack->Get(i);
            if (!frame->node_name()) continue;
            const void* nodePtr = findNodeByName(frame->node_name()->c_str());
            if (nodePtr) {
                std::string retVar = frame->return_var_name()
                    ? frame->return_var_name()->c_str() : "";

                CallFrame cf = {nodePtr, frame->pc(), retVar, {}, {}};

                // 섀도된 변수 복원 (하위 호환: nullptr 체크)
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

                // 매개변수 이름 복원 (하위 호환: nullptr 체크)
                if (frame->param_names()) {
                    for (flatbuffers::uoffset_t j = 0; j < frame->param_names()->size(); ++j) {
                        cf.paramNames.push_back(frame->param_names()->Get(j)->c_str());
                    }
                }

                callStack_.push_back(std::move(cf));
            }
        }
    }
    hasPendingReturn_ = false;

    // Pending choices 복원
    pendingChoices_.clear();
    auto* choices = saveState->pending_choices();
    if (choices) {
        for (flatbuffers::uoffset_t i = 0; i < choices->size(); ++i) {
            auto* pc = choices->Get(i);
            int32_t textId = pc->text() ? findStringInPool(pc->text()->c_str()) : -1;
            int32_t targetId = pc->target_node_name()
                ? findStringInPool(pc->target_node_name()->c_str()) : -1;
            if (textId >= 0 && targetId >= 0) {
                pendingChoices_.push_back({textId, targetId});
            }
        }
    }

    // Visit counts 복원
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

    return true;
}

// --- CSV 파싱 헬퍼 ---
static std::vector<std::string> parseCSVLine(const std::string& line) {
    std::vector<std::string> result;
    std::string cur;
    bool inQ = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            if (inQ && i + 1 < line.size() && line[i + 1] == '"') {
                cur += '"';
                i++;
            } else {
                inQ = !inQ;
            }
        } else if (c == ',' && !inQ) {
            result.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    result.push_back(cur);
    return result;
}

// --- Locale API ---
bool Runner::loadLocale(const std::string& csvPath) {
    auto* story = asStory(story_);
    auto* pool = asPool(pool_);
    auto* lineIds = story ? story->line_ids() : nullptr;
    if (!pool || !lineIds || lineIds->size() == 0) {
        std::cerr << "[Gyeol] No line_ids in story\n";
        return false;
    }

    std::ifstream ifs(csvPath);
    if (!ifs.is_open()) {
        std::cerr << "[Gyeol] Failed to open locale: " << csvPath << "\n";
        return false;
    }

    // line_id → pool index 매핑
    std::unordered_map<std::string, int32_t> idMap;
    for (flatbuffers::uoffset_t i = 0; i < lineIds->size(); ++i) {
        auto* s = lineIds->Get(i);
        if (s && s->size() > 0) {
            idMap[s->str()] = static_cast<int32_t>(i);
        }
    }

    localePool_.clear();
    localePool_.resize(pool->size());

    std::string line;
    std::getline(ifs, line); // 헤더 스킵
    int count = 0;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        // Windows CRLF 처리
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        auto cols = parseCSVLine(line);
        if (cols.size() < 5) continue;
        auto it = idMap.find(cols[0]);
        if (it != idMap.end()) {
            localePool_[static_cast<size_t>(it->second)] = cols[4];
            count++;
        }
    }

    // 로케일 이름 추출 (파일명에서)
    size_t slash = csvPath.find_last_of("/\\");
    size_t dot = csvPath.find_last_of('.');
    size_t nameStart = (slash == std::string::npos) ? 0 : slash + 1;
    currentLocale_ = csvPath.substr(nameStart,
        (dot == std::string::npos || dot < nameStart) ? std::string::npos : dot - nameStart);

    return true;
}

void Runner::clearLocale() {
    localePool_.clear();
    currentLocale_.clear();
}

std::string Runner::getLocale() const {
    return currentLocale_;
}

// ==========================================================================
// Debug API 구현
// ==========================================================================

// --- Breakpoint management ---
void Runner::addBreakpoint(const std::string& nodeName, uint32_t pc) {
    breakpoints_.insert({nodeName, pc});
}

void Runner::removeBreakpoint(const std::string& nodeName, uint32_t pc) {
    breakpoints_.erase({nodeName, pc});
}

void Runner::clearBreakpoints() {
    breakpoints_.clear();
}

bool Runner::hasBreakpoint(const std::string& nodeName, uint32_t pc) const {
    return breakpoints_.count({nodeName, pc}) > 0;
}

std::vector<std::pair<std::string, uint32_t>> Runner::getBreakpoints() const {
    std::vector<std::pair<std::string, uint32_t>> result;
    result.reserve(breakpoints_.size());
    for (const auto& bp : breakpoints_) {
        result.push_back(bp);
    }
    return result;
}

// --- Step control ---
void Runner::setStepMode(bool enabled) {
    stepMode_ = enabled;
}

bool Runner::isStepMode() const {
    return stepMode_;
}

// --- Debug info ---
Runner::DebugLocation Runner::getLocation() const {
    DebugLocation loc;
    if (!currentNode_) return loc;

    loc.nodeName = currentNodeName();
    loc.pc = pc_;

    auto* node = asNode(currentNode_);
    if (node && node->lines() && pc_ < node->lines()->size()) {
        auto* instr = node->lines()->Get(pc_);
        switch (instr->data_type()) {
            case OpData::Line:            loc.instructionType = "Line"; break;
            case OpData::Choice:          loc.instructionType = "Choice"; break;
            case OpData::Jump:            loc.instructionType = "Jump"; break;
            case OpData::Command:         loc.instructionType = "Command"; break;
            case OpData::SetVar:          loc.instructionType = "SetVar"; break;
            case OpData::Condition:       loc.instructionType = "Condition"; break;
            case OpData::Random:          loc.instructionType = "Random"; break;
            case OpData::Return:          loc.instructionType = "Return"; break;
            case OpData::CallWithReturn:  loc.instructionType = "CallWithReturn"; break;
            default:                      loc.instructionType = "Unknown"; break;
        }
    }

    return loc;
}

std::vector<Runner::CallFrameInfo> Runner::getCallStack() const {
    std::vector<CallFrameInfo> result;
    result.reserve(callStack_.size());
    for (const auto& frame : callStack_) {
        CallFrameInfo info;
        info.nodeName = nodeNameFromPtr(frame.node);
        info.pc = frame.pc;
        info.returnVarName = frame.returnVarName;
        info.paramNames = frame.paramNames;
        result.push_back(std::move(info));
    }
    return result;
}

std::string Runner::getCurrentNodeName() const {
    return currentNodeName();
}

uint32_t Runner::getCurrentPC() const {
    return pc_;
}

// --- Node inspection ---
std::vector<std::string> Runner::getNodeNames() const {
    std::vector<std::string> result;
    auto* story = asStory(story_);
    if (!story || !story->nodes()) return result;

    auto* nodes = story->nodes();
    result.reserve(nodes->size());
    for (flatbuffers::uoffset_t i = 0; i < nodes->size(); ++i) {
        auto* node = nodes->Get(i);
        if (node->name()) {
            result.push_back(node->name()->c_str());
        }
    }
    return result;
}

uint32_t Runner::getNodeInstructionCount(const std::string& nodeName) const {
    const void* nodePtr = findNodeByName(nodeName.c_str());
    if (!nodePtr) return 0;
    auto* node = asNode(nodePtr);
    if (!node->lines()) return 0;
    return node->lines()->size();
}

std::string Runner::getInstructionInfo(const std::string& nodeName, uint32_t pc) const {
    if (!story_) return "";
    const void* nodePtr = findNodeByName(nodeName.c_str());
    if (!nodePtr) return "";
    auto* node = asNode(nodePtr);
    if (!node->lines() || pc >= node->lines()->size()) return "";

    auto* instr = node->lines()->Get(pc);
    switch (instr->data_type()) {
        case OpData::Line: {
            auto* line = instr->data_as_Line();
            std::string chr = (line->character_id() >= 0)
                ? poolStr(line->character_id()) : "(narration)";
            std::string txt = poolStr(line->text_id());
            return "Line: " + chr + " \"" + txt + "\"";
        }
        case OpData::Choice: {
            auto* choice = instr->data_as_Choice();
            std::string txt = poolStr(choice->text_id());
            std::string target = poolStr(choice->target_node_name_id());
            return "Choice: \"" + txt + "\" -> " + target;
        }
        case OpData::Jump: {
            auto* jump = instr->data_as_Jump();
            std::string target = poolStr(jump->target_node_name_id());
            if (jump->is_call()) {
                return "Call: -> " + target;
            }
            return "Jump: -> " + target;
        }
        case OpData::Command: {
            auto* cmd = instr->data_as_Command();
            std::string info = "Command: @ " + std::string(poolStr(cmd->type_id()));
            auto* params = cmd->params();
            if (params) {
                for (flatbuffers::uoffset_t k = 0; k < params->size(); ++k) {
                    info += " " + std::string(poolStr(params->Get(k)));
                }
            }
            return info;
        }
        case OpData::SetVar: {
            auto* sv = instr->data_as_SetVar();
            return "SetVar: $ " + std::string(poolStr(sv->var_name_id())) + " = ...";
        }
        case OpData::Condition: {
            auto* cond = instr->data_as_Condition();
            std::string info = "Condition: if ...";
            if (cond->true_jump_node_id() >= 0) {
                info += " -> " + std::string(poolStr(cond->true_jump_node_id()));
            }
            if (cond->false_jump_node_id() >= 0) {
                info += " else -> " + std::string(poolStr(cond->false_jump_node_id()));
            }
            return info;
        }
        case OpData::Random: {
            auto* random = instr->data_as_Random();
            uint32_t count = (random->branches()) ? random->branches()->size() : 0;
            return "Random: " + std::to_string(count) + " branches";
        }
        case OpData::Return: {
            auto* ret = instr->data_as_Return();
            if (ret->expr() || (ret->value() && ret->value_type() != ValueData::NONE)) {
                return "Return: <expr>";
            }
            return "Return";
        }
        case OpData::CallWithReturn: {
            auto* cwr = instr->data_as_CallWithReturn();
            std::string var = poolStr(cwr->return_var_name_id());
            std::string target = poolStr(cwr->target_node_name_id());
            return "CallWithReturn: $ " + var + " = call " + target;
        }
        default:
            return "Unknown";
    }
}

} // namespace Gyeol
