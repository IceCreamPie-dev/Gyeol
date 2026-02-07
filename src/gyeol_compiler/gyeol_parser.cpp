#include "gyeol_parser.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstdlib>

using namespace ICPDev::Gyeol::Schema;

namespace Gyeol {

// --- String Pool ---
int32_t Parser::addString(const std::string& str) {
    auto it = stringMap_.find(str);
    if (it != stringMap_.end()) {
        return it->second;
    }
    int32_t idx = static_cast<int32_t>(story_.string_pool.size());
    story_.string_pool.push_back(str);
    stringMap_[str] = idx;
    return idx;
}

// --- 유틸리티 ---
void Parser::addError(int lineNum, const std::string& msg) {
    std::string formatted = filename_ + ":" + std::to_string(lineNum) + ": " + msg;
    errors_.push_back(formatted);
    if (error_.empty()) {
        error_ = formatted;
    }
}

void Parser::setError(int lineNum, const std::string& msg) {
    addError(lineNum, msg);
}

size_t Parser::countIndent(const std::string& line) {
    size_t count = 0;
    for (char c : line) {
        if (c == ' ') count++;
        else if (c == '\t') count += 4;
        else break;
    }
    return count;
}

std::string Parser::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

void Parser::skipSpaces(const std::string& text, size_t& pos) {
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) {
        pos++;
    }
}

std::string Parser::parseQuotedString(const std::string& text, size_t& pos) {
    if (pos >= text.size() || text[pos] != '"') return "";
    pos++; // skip opening quote
    std::string result;
    while (pos < text.size() && text[pos] != '"') {
        if (text[pos] == '\\' && pos + 1 < text.size()) {
            pos++;
            switch (text[pos]) {
                case 'n': result += '\n'; break;
                case 't': result += '\t'; break;
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                default: result += text[pos]; break;
            }
        } else {
            result += text[pos];
        }
        pos++;
    }
    if (pos < text.size()) pos++; // skip closing quote
    return result;
}

std::string Parser::parseWord(const std::string& text, size_t& pos) {
    skipSpaces(text, pos);
    std::string result;
    while (pos < text.size() && text[pos] != ' ' && text[pos] != '\t'
           && text[pos] != '\r' && text[pos] != '\n') {
        result += text[pos];
        pos++;
    }
    return result;
}

// --- 값(Value) 파싱 ---
bool Parser::parseValue(const std::string& text, size_t& pos,
                         ValueDataUnion& outValue) {
    skipSpaces(text, pos);
    if (pos >= text.size()) return false;

    // 문자열
    if (text[pos] == '"') {
        std::string str = parseQuotedString(text, pos);
        int32_t idx = addString(str);
        auto ref = std::make_unique<StringRefT>();
        ref->index = idx;
        outValue.Set(*ref);
        return true;
    }

    std::string word = parseWord(text, pos);
    if (word.empty()) return false;

    // bool
    if (word == "true") {
        auto val = std::make_unique<BoolValueT>();
        val->val = true;
        outValue.Set(*val);
        return true;
    }
    if (word == "false") {
        auto val = std::make_unique<BoolValueT>();
        val->val = false;
        outValue.Set(*val);
        return true;
    }

    // float (소수점 포함)
    if (word.find('.') != std::string::npos) {
        char* end = nullptr;
        float f = std::strtof(word.c_str(), &end);
        if (end != word.c_str()) {
            auto val = std::make_unique<FloatValueT>();
            val->val = f;
            outValue.Set(*val);
            return true;
        }
    }

    // int
    char* end = nullptr;
    long i = std::strtol(word.c_str(), &end, 10);
    if (end != word.c_str() && *end == '\0') {
        auto val = std::make_unique<IntValueT>();
        val->val = static_cast<int32_t>(i);
        outValue.Set(*val);
        return true;
    }

    return false;
}

// --- 표현식 파싱 (Shunting-yard → RPN) ---
bool Parser::parseExpression(const std::string& text, size_t& pos,
                              std::unique_ptr<ExpressionT>& outExpr,
                              ValueDataUnion& outSimpleValue,
                              bool& isSimpleLiteral) {
    skipSpaces(text, pos);
    if (pos >= text.size()) return false;

    // 내부 토큰 타입
    enum class TokType { LITERAL, VARREF, OP, LPAREN, RPAREN, NEGATE };
    struct Token {
        TokType type;
        ValueDataUnion literal;  // LITERAL용
        int32_t varNameId = -1;  // VARREF용
        ExprOp op = ExprOp::Add; // OP/NEGATE용
    };

    std::vector<Token> tokens;
    bool expectOperand = true; // 다음에 피연산자가 와야 하는지

    while (pos < text.size()) {
        skipSpaces(text, pos);
        if (pos >= text.size()) break;

        char c = text[pos];

        // 괄호
        if (c == '(') {
            Token t; t.type = TokType::LPAREN;
            tokens.push_back(t);
            pos++;
            expectOperand = true;
            continue;
        }
        if (c == ')') {
            Token t; t.type = TokType::RPAREN;
            tokens.push_back(t);
            pos++;
            expectOperand = false;
            continue;
        }

        // 연산자
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%') {
            // '->' 화살표는 연산자가 아님 (조건문 분기용)
            if (c == '-' && pos + 1 < text.size() && text[pos + 1] == '>') break;

            if (c == '-' && expectOperand) {
                // 단항 마이너스
                Token t; t.type = TokType::NEGATE; t.op = ExprOp::Negate;
                tokens.push_back(t);
                pos++;
                expectOperand = true;
                continue;
            }
            if (expectOperand) break; // 잘못된 위치의 연산자

            Token t; t.type = TokType::OP;
            switch (c) {
                case '+': t.op = ExprOp::Add; break;
                case '-': t.op = ExprOp::Sub; break;
                case '*': t.op = ExprOp::Mul; break;
                case '/': t.op = ExprOp::Div; break;
                case '%': t.op = ExprOp::Mod; break;
            }
            tokens.push_back(t);
            pos++;
            expectOperand = true;
            continue;
        }

        if (!expectOperand) break; // 피연산자 위치가 아닌데 피연산자가 옴

        // 문자열 리터럴
        if (c == '"') {
            std::string str = parseQuotedString(text, pos);
            Token t; t.type = TokType::LITERAL;
            auto ref = std::make_unique<StringRefT>();
            ref->index = addString(str);
            t.literal.Set(*ref);
            tokens.push_back(std::move(t));
            expectOperand = false;
            continue;
        }

        // 숫자 또는 식별자
        // 숫자: 0-9 또는 소수점으로 시작하는 경우
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            // 숫자 파싱
            size_t start = pos;
            bool hasDot = false;
            while (pos < text.size()) {
                char ch = text[pos];
                if (ch == '.') {
                    if (hasDot) break;
                    hasDot = true;
                    pos++;
                } else if (std::isdigit(static_cast<unsigned char>(ch))) {
                    pos++;
                } else {
                    break;
                }
            }
            std::string numStr = text.substr(start, pos - start);
            Token t; t.type = TokType::LITERAL;
            if (hasDot) {
                auto val = std::make_unique<FloatValueT>();
                val->val = std::strtof(numStr.c_str(), nullptr);
                t.literal.Set(*val);
            } else {
                auto val = std::make_unique<IntValueT>();
                val->val = static_cast<int32_t>(std::strtol(numStr.c_str(), nullptr, 10));
                t.literal.Set(*val);
            }
            tokens.push_back(std::move(t));
            expectOperand = false;
            continue;
        }

        // 식별자 (변수명 또는 true/false)
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = pos;
            while (pos < text.size() &&
                   (std::isalnum(static_cast<unsigned char>(text[pos])) || text[pos] == '_')) {
                pos++;
            }
            std::string word = text.substr(start, pos - start);

            Token t;
            if (word == "true") {
                t.type = TokType::LITERAL;
                auto val = std::make_unique<BoolValueT>();
                val->val = true;
                t.literal.Set(*val);
            } else if (word == "false") {
                t.type = TokType::LITERAL;
                auto val = std::make_unique<BoolValueT>();
                val->val = false;
                t.literal.Set(*val);
            } else {
                t.type = TokType::VARREF;
                t.varNameId = addString(word);
            }
            tokens.push_back(std::move(t));
            expectOperand = false;
            continue;
        }

        break; // 알 수 없는 문자
    }

    if (tokens.empty()) return false;

    // 단일 리터럴 최적화 → 기존 value 필드 사용
    if (tokens.size() == 1 && tokens[0].type == TokType::LITERAL) {
        isSimpleLiteral = true;
        outSimpleValue = std::move(tokens[0].literal);
        return true;
    }

    // Shunting-yard: infix → RPN
    auto precedence = [](ExprOp op) -> int {
        switch (op) {
            case ExprOp::Add: case ExprOp::Sub: return 1;
            case ExprOp::Mul: case ExprOp::Div: case ExprOp::Mod: return 2;
            case ExprOp::Negate: return 3;
            default: return 0;
        }
    };

    std::vector<Token> output;
    std::vector<Token> opStack;

    for (auto& tok : tokens) {
        switch (tok.type) {
            case TokType::LITERAL:
            case TokType::VARREF:
                output.push_back(std::move(tok));
                break;

            case TokType::NEGATE:
                opStack.push_back(std::move(tok));
                break;

            case TokType::OP:
                while (!opStack.empty()) {
                    auto& top = opStack.back();
                    if (top.type == TokType::LPAREN) break;
                    if ((top.type == TokType::OP || top.type == TokType::NEGATE) &&
                        precedence(top.op) >= precedence(tok.op)) {
                        output.push_back(std::move(top));
                        opStack.pop_back();
                    } else {
                        break;
                    }
                }
                opStack.push_back(std::move(tok));
                break;

            case TokType::LPAREN:
                opStack.push_back(std::move(tok));
                break;

            case TokType::RPAREN:
                while (!opStack.empty() && opStack.back().type != TokType::LPAREN) {
                    output.push_back(std::move(opStack.back()));
                    opStack.pop_back();
                }
                if (!opStack.empty()) opStack.pop_back(); // LPAREN 제거
                break;
        }
    }

    while (!opStack.empty()) {
        output.push_back(std::move(opStack.back()));
        opStack.pop_back();
    }

    // RPN → ExprToken 리스트
    auto expr = std::make_unique<ExpressionT>();
    for (auto& tok : output) {
        auto et = std::make_unique<ExprTokenT>();
        switch (tok.type) {
            case TokType::LITERAL:
                et->op = ExprOp::PushLiteral;
                et->literal_value = std::move(tok.literal);
                break;
            case TokType::VARREF:
                et->op = ExprOp::PushVar;
                et->var_name_id = tok.varNameId;
                break;
            case TokType::OP:
            case TokType::NEGATE:
                et->op = tok.op;
                break;
            default:
                break;
        }
        expr->tokens.push_back(std::move(et));
    }

    isSimpleLiteral = false;
    outExpr = std::move(expr);
    return true;
}

// --- label ---
bool Parser::parseLabelLine(const std::string& content, int lineNum) {
    // "label name:"
    size_t pos = 5; // skip "label"
    skipSpaces(content, pos);
    std::string name = parseWord(content, pos);

    // 콜론 제거
    if (!name.empty() && name.back() == ':') {
        name.pop_back();
    }

    if (name.empty()) {
        addError(lineNum, "label name is empty");
        return false;
    }

    seenFirstLabel_ = true;

    auto node = std::make_unique<NodeT>();
    node->name = name;
    story_.nodes.push_back(std::move(node));
    currentNode_ = story_.nodes.back().get();
    inMenu_ = false;

    // 첫 번째 label을 start_node로 설정
    if (story_.nodes.size() == 1) {
        story_.start_node_name = name;
    }

    return true;
}

// --- 대사 ---
bool Parser::parseDialogueLine(const std::string& content, int lineNum) {
    if (!currentNode_) {
        addError(lineNum, "dialogue outside of label");
        return false;
    }

    size_t pos = 0;
    skipSpaces(content, pos);

    auto line = std::make_unique<LineT>();

    if (pos < content.size() && content[pos] == '"') {
        // 나레이션: "대사"
        line->character_id = -1;
        std::string text = parseQuotedString(content, pos);
        line->text_id = addString(text);
    } else {
        // 캐릭터 대사: character "대사"
        std::string character = parseWord(content, pos);
        line->character_id = addString(character);

        skipSpaces(content, pos);
        if (pos >= content.size() || content[pos] != '"') {
            addError(lineNum, "expected quoted string after character name");
            return false;
        }
        std::string text = parseQuotedString(content, pos);
        line->text_id = addString(text);
    }

    // voice_asset_id: #voice:파일명 태그
    line->voice_asset_id = -1;
    skipSpaces(content, pos);
    if (pos < content.size() && content[pos] == '#') {
        pos++; // skip '#'
        std::string tag = parseWord(content, pos);
        if (tag.substr(0, 6) == "voice:") {
            std::string voiceFile = tag.substr(6);
            if (!voiceFile.empty()) {
                line->voice_asset_id = addString(voiceFile);
            }
        }
    }

    auto instr = std::make_unique<InstructionT>();
    instr->data.Set(*line);
    currentNode_->lines.push_back(std::move(instr));
    return true;
}

// --- menu 선택지 ---
bool Parser::parseMenuChoiceLine(const std::string& content, int lineNum) {
    if (!currentNode_) {
        addError(lineNum, "choice outside of label");
        return false;
    }

    size_t pos = 0;
    skipSpaces(content, pos);

    if (pos >= content.size() || content[pos] != '"') {
        addError(lineNum, "expected quoted string for choice text");
        return false;
    }

    std::string text = parseQuotedString(content, pos);
    skipSpaces(content, pos);

    // -> target
    if (pos + 1 >= content.size() || content[pos] != '-' || content[pos + 1] != '>') {
        addError(lineNum, "expected '->' after choice text");
        return false;
    }
    pos += 2; // skip ->

    std::string target = parseWord(content, pos);
    if (target.empty()) {
        addError(lineNum, "expected target node name after '->'");
        return false;
    }

    auto choice = std::make_unique<ChoiceT>();
    choice->text_id = addString(text);
    choice->target_node_name_id = addString(target);

    // 선택적: "if 변수명"
    skipSpaces(content, pos);
    if (pos < content.size()) {
        std::string keyword = parseWord(content, pos);
        if (keyword == "if") {
            std::string varName = parseWord(content, pos);
            if (!varName.empty()) {
                choice->condition_var_id = addString(varName);
            }
        }
    }

    auto instr = std::make_unique<InstructionT>();
    instr->data.Set(*choice);
    currentNode_->lines.push_back(std::move(instr));

    // 라인번호 기록 (jump target 검증용)
    size_t nodeIdx = story_.nodes.size() - 1;
    size_t instrIdx = currentNode_->lines.size() - 1;
    instrLineMap_[instrKey(nodeIdx, instrIdx)] = lineNum;

    return true;
}

// --- jump / call ---
bool Parser::parseJumpLine(const std::string& content, int lineNum, bool isCall) {
    if (!currentNode_) {
        addError(lineNum, "jump/call outside of label");
        return false;
    }

    size_t pos = isCall ? 4 : 4; // skip "jump" or "call"
    std::string target = parseWord(content, pos);
    if (target.empty()) {
        addError(lineNum, "expected target node name");
        return false;
    }

    auto jump = std::make_unique<JumpT>();
    jump->target_node_name_id = addString(target);
    jump->is_call = isCall;

    auto instr = std::make_unique<InstructionT>();
    instr->data.Set(*jump);
    currentNode_->lines.push_back(std::move(instr));

    // 라인번호 기록 (jump target 검증용)
    size_t nodeIdx = story_.nodes.size() - 1;
    size_t instrIdx = currentNode_->lines.size() - 1;
    instrLineMap_[instrKey(nodeIdx, instrIdx)] = lineNum;

    return true;
}

// --- $ 변수 = 값 (label 내부) ---
bool Parser::parseSetVarLine(const std::string& content, int lineNum) {
    if (!currentNode_) {
        addError(lineNum, "variable set outside of label");
        return false;
    }

    size_t pos = 1; // skip "$"
    std::string varName = parseWord(content, pos);
    if (varName.empty()) {
        addError(lineNum, "expected variable name after '$'");
        return false;
    }

    skipSpaces(content, pos);
    if (pos >= content.size() || content[pos] != '=') {
        addError(lineNum, "expected '=' after variable name");
        return false;
    }
    pos++; // skip '='

    auto setvar = std::make_unique<SetVarT>();
    setvar->var_name_id = addString(varName);

    bool isSimple = false;
    std::unique_ptr<ExpressionT> expr;
    if (!parseExpression(content, pos, expr, setvar->value, isSimple)) {
        addError(lineNum, "invalid expression");
        return false;
    }
    if (!isSimple) {
        setvar->expr = std::move(expr);
    }

    auto instr = std::make_unique<InstructionT>();
    instr->data.Set(*setvar);
    currentNode_->lines.push_back(std::move(instr));
    return true;
}

// --- $ 변수 = 값 (global, label 앞) ---
bool Parser::parseGlobalVarLine(const std::string& content, int lineNum) {
    size_t pos = 1; // skip "$"
    std::string varName = parseWord(content, pos);
    if (varName.empty()) {
        addError(lineNum, "expected variable name after '$'");
        return false;
    }

    skipSpaces(content, pos);
    if (pos >= content.size() || content[pos] != '=') {
        addError(lineNum, "expected '=' after variable name");
        return false;
    }
    pos++; // skip '='

    auto setvar = std::make_unique<SetVarT>();
    setvar->var_name_id = addString(varName);

    if (!parseValue(content, pos, setvar->value)) {
        addError(lineNum, "invalid value for global variable");
        return false;
    }

    story_.global_vars.push_back(std::move(setvar));
    return true;
}

// --- 비교 연산자 파싱 헬퍼 ---
static bool parseComparisonOp(const std::string& text, size_t& pos, Operator& op) {
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) pos++;
    if (pos >= text.size()) return false;

    char c = text[pos];
    if (c == '=' && pos + 1 < text.size() && text[pos + 1] == '=') {
        op = Operator::Equal; pos += 2; return true;
    }
    if (c == '!' && pos + 1 < text.size() && text[pos + 1] == '=') {
        op = Operator::NotEqual; pos += 2; return true;
    }
    if (c == '>' && pos + 1 < text.size() && text[pos + 1] == '=') {
        op = Operator::GreaterOrEqual; pos += 2; return true;
    }
    if (c == '<' && pos + 1 < text.size() && text[pos + 1] == '=') {
        op = Operator::LessOrEqual; pos += 2; return true;
    }
    if (c == '>') {
        op = Operator::Greater; pos += 1; return true;
    }
    if (c == '<') {
        op = Operator::Less; pos += 1; return true;
    }
    return false;
}

// --- 조건 표현식 파서 (산술 + 비교 + 논리 → 단일 RPN) ---
bool Parser::parseFullConditionExpr(const std::string& text, size_t& pos,
                                     std::unique_ptr<ExpressionT>& outExpr,
                                     bool& hasLogicalOps) {
    hasLogicalOps = false;
    skipSpaces(text, pos);
    if (pos >= text.size()) return false;

    // 내부 토큰 타입
    enum class TokType { LITERAL, VARREF, OP, LPAREN, RPAREN, NEGATE, NOTOP };
    struct Token {
        TokType type;
        ValueDataUnion literal;
        int32_t varNameId = -1;
        ExprOp op = ExprOp::Add;
    };

    std::vector<Token> tokens;
    bool expectOperand = true;

    while (pos < text.size()) {
        skipSpaces(text, pos);
        if (pos >= text.size()) break;

        char c = text[pos];

        // 괄호
        if (c == '(') {
            Token t; t.type = TokType::LPAREN;
            tokens.push_back(t);
            pos++;
            expectOperand = true;
            continue;
        }
        if (c == ')') {
            Token t; t.type = TokType::RPAREN;
            tokens.push_back(t);
            pos++;
            expectOperand = false;
            continue;
        }

        // '->' 화살표 → 종료
        if (c == '-' && pos + 1 < text.size() && text[pos + 1] == '>') break;

        // 비교 연산자 (피연산자 뒤에서만)
        if (!expectOperand) {
            if (c == '=' && pos + 1 < text.size() && text[pos + 1] == '=') {
                Token t; t.type = TokType::OP; t.op = ExprOp::CmpEq;
                tokens.push_back(t); pos += 2; expectOperand = true; continue;
            }
            if (c == '!' && pos + 1 < text.size() && text[pos + 1] == '=') {
                Token t; t.type = TokType::OP; t.op = ExprOp::CmpNe;
                tokens.push_back(t); pos += 2; expectOperand = true; continue;
            }
            if (c == '>' && pos + 1 < text.size() && text[pos + 1] == '=') {
                Token t; t.type = TokType::OP; t.op = ExprOp::CmpGe;
                tokens.push_back(t); pos += 2; expectOperand = true; continue;
            }
            if (c == '<' && pos + 1 < text.size() && text[pos + 1] == '=') {
                Token t; t.type = TokType::OP; t.op = ExprOp::CmpLe;
                tokens.push_back(t); pos += 2; expectOperand = true; continue;
            }
            if (c == '>') {
                Token t; t.type = TokType::OP; t.op = ExprOp::CmpGt;
                tokens.push_back(t); pos += 1; expectOperand = true; continue;
            }
            if (c == '<') {
                Token t; t.type = TokType::OP; t.op = ExprOp::CmpLt;
                tokens.push_back(t); pos += 1; expectOperand = true; continue;
            }
        }

        // 산술 연산자
        if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%') {
            if (c == '-' && expectOperand) {
                Token t; t.type = TokType::NEGATE; t.op = ExprOp::Negate;
                tokens.push_back(t);
                pos++;
                expectOperand = true;
                continue;
            }
            if (expectOperand) break;

            Token t; t.type = TokType::OP;
            switch (c) {
                case '+': t.op = ExprOp::Add; break;
                case '-': t.op = ExprOp::Sub; break;
                case '*': t.op = ExprOp::Mul; break;
                case '/': t.op = ExprOp::Div; break;
                case '%': t.op = ExprOp::Mod; break;
            }
            tokens.push_back(t);
            pos++;
            expectOperand = true;
            continue;
        }

        // 식별자가 아닌 문자가 피연산자 위치가 아닌 곳에 오면 종료
        // (식별자는 and/or 키워드일 수 있으므로 아래에서 처리)
        if (!expectOperand && !(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) break;

        // 문자열 리터럴
        if (c == '"') {
            std::string str = parseQuotedString(text, pos);
            Token t; t.type = TokType::LITERAL;
            auto ref = std::make_unique<StringRefT>();
            ref->index = addString(str);
            t.literal.Set(*ref);
            tokens.push_back(std::move(t));
            expectOperand = false;
            continue;
        }

        // 숫자
        if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
            size_t start = pos;
            bool hasDot = false;
            while (pos < text.size()) {
                char ch = text[pos];
                if (ch == '.') {
                    if (hasDot) break;
                    hasDot = true;
                    pos++;
                } else if (std::isdigit(static_cast<unsigned char>(ch))) {
                    pos++;
                } else {
                    break;
                }
            }
            std::string numStr = text.substr(start, pos - start);
            Token t; t.type = TokType::LITERAL;
            if (hasDot) {
                auto val = std::make_unique<FloatValueT>();
                val->val = std::strtof(numStr.c_str(), nullptr);
                t.literal.Set(*val);
            } else {
                auto val = std::make_unique<IntValueT>();
                val->val = static_cast<int32_t>(std::strtol(numStr.c_str(), nullptr, 10));
                t.literal.Set(*val);
            }
            tokens.push_back(std::move(t));
            expectOperand = false;
            continue;
        }

        // 식별자 (변수명, true/false, and/or/not)
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = pos;
            while (pos < text.size() &&
                   (std::isalnum(static_cast<unsigned char>(text[pos])) || text[pos] == '_')) {
                pos++;
            }
            std::string word = text.substr(start, pos - start);

            // 논리 연산자 키워드
            if (word == "and" && !expectOperand) {
                Token t; t.type = TokType::OP; t.op = ExprOp::And;
                tokens.push_back(t);
                expectOperand = true;
                hasLogicalOps = true;
                continue;
            }
            if (word == "or" && !expectOperand) {
                Token t; t.type = TokType::OP; t.op = ExprOp::Or;
                tokens.push_back(t);
                expectOperand = true;
                hasLogicalOps = true;
                continue;
            }
            if (word == "not" && expectOperand) {
                Token t; t.type = TokType::NOTOP; t.op = ExprOp::Not;
                tokens.push_back(t);
                expectOperand = true;
                hasLogicalOps = true;
                continue;
            }

            // 피연산자 위치가 아닌데 키워드가 아닌 식별자 → 종료
            if (!expectOperand) {
                pos = start; // 되감기
                break;
            }

            Token t;
            if (word == "true") {
                t.type = TokType::LITERAL;
                auto val = std::make_unique<BoolValueT>();
                val->val = true;
                t.literal.Set(*val);
            } else if (word == "false") {
                t.type = TokType::LITERAL;
                auto val = std::make_unique<BoolValueT>();
                val->val = false;
                t.literal.Set(*val);
            } else {
                t.type = TokType::VARREF;
                t.varNameId = addString(word);
            }
            tokens.push_back(std::move(t));
            expectOperand = false;
            continue;
        }

        break;
    }

    if (tokens.empty()) return false;

    // Shunting-yard: infix → RPN (확장된 우선순위)
    auto precedence = [](ExprOp op) -> int {
        switch (op) {
            case ExprOp::Or:     return 1;
            case ExprOp::And:    return 2;
            case ExprOp::Not:    return 3;
            case ExprOp::CmpEq: case ExprOp::CmpNe:
            case ExprOp::CmpGt: case ExprOp::CmpLt:
            case ExprOp::CmpGe: case ExprOp::CmpLe:
                                 return 4;
            case ExprOp::Add: case ExprOp::Sub: return 5;
            case ExprOp::Mul: case ExprOp::Div: case ExprOp::Mod: return 6;
            case ExprOp::Negate: return 7;
            default: return 0;
        }
    };

    std::vector<Token> output;
    std::vector<Token> opStack;

    for (auto& tok : tokens) {
        switch (tok.type) {
            case TokType::LITERAL:
            case TokType::VARREF:
                output.push_back(std::move(tok));
                break;

            case TokType::NEGATE:
            case TokType::NOTOP:
                opStack.push_back(std::move(tok));
                break;

            case TokType::OP:
                while (!opStack.empty()) {
                    auto& top = opStack.back();
                    if (top.type == TokType::LPAREN) break;
                    if ((top.type == TokType::OP || top.type == TokType::NEGATE ||
                         top.type == TokType::NOTOP) &&
                        precedence(top.op) >= precedence(tok.op)) {
                        output.push_back(std::move(top));
                        opStack.pop_back();
                    } else {
                        break;
                    }
                }
                opStack.push_back(std::move(tok));
                break;

            case TokType::LPAREN:
                opStack.push_back(std::move(tok));
                break;

            case TokType::RPAREN:
                while (!opStack.empty() && opStack.back().type != TokType::LPAREN) {
                    output.push_back(std::move(opStack.back()));
                    opStack.pop_back();
                }
                if (!opStack.empty()) opStack.pop_back();
                break;
        }
    }

    while (!opStack.empty()) {
        output.push_back(std::move(opStack.back()));
        opStack.pop_back();
    }

    // RPN → ExprToken 리스트
    auto expr = std::make_unique<ExpressionT>();
    for (auto& tok : output) {
        auto et = std::make_unique<ExprTokenT>();
        switch (tok.type) {
            case TokType::LITERAL:
                et->op = ExprOp::PushLiteral;
                et->literal_value = std::move(tok.literal);
                break;
            case TokType::VARREF:
                et->op = ExprOp::PushVar;
                et->var_name_id = tok.varNameId;
                break;
            case TokType::OP:
            case TokType::NEGATE:
            case TokType::NOTOP:
                et->op = tok.op;
                break;
            default:
                break;
        }
        expr->tokens.push_back(std::move(et));
    }

    outExpr = std::move(expr);
    return true;
}

// --- if 표현식 op 표현식 -> 참 else 거짓 ---
bool Parser::parseConditionLine(const std::string& content, int lineNum) {
    if (!currentNode_) {
        addError(lineNum, "condition outside of label");
        return false;
    }

    size_t pos = 2; // skip "if"
    auto cond = std::make_unique<ConditionT>();

    // 전체 조건식 파싱 (산술 + 비교 + 논리 → 단일 RPN)
    bool hasLogicalOps = false;
    std::unique_ptr<ExpressionT> fullExpr;
    if (!parseFullConditionExpr(content, pos, fullExpr, hasLogicalOps)) {
        addError(lineNum, "expected expression after 'if'");
        return false;
    }

    if (hasLogicalOps) {
        // 논리 연산자 사용 → cond_expr에 전체 Expression 저장
        cond->cond_expr = std::move(fullExpr);
    } else {
        // 논리 연산자 없음 → 기존 lhs/op/rhs 방식으로 분해 (하위 호환)
        // RPN에서 비교 연산자를 찾아 분리
        auto& tokens = fullExpr->tokens;

        // 비교 연산자 찾기 (마지막 토큰이 비교 연산자)
        ExprOp lastOp = tokens.back()->op;
        if (lastOp >= ExprOp::CmpEq && lastOp <= ExprOp::CmpLe) {
            // Operator 매핑
            switch (lastOp) {
                case ExprOp::CmpEq: cond->op = Operator::Equal; break;
                case ExprOp::CmpNe: cond->op = Operator::NotEqual; break;
                case ExprOp::CmpGt: cond->op = Operator::Greater; break;
                case ExprOp::CmpLt: cond->op = Operator::Less; break;
                case ExprOp::CmpGe: cond->op = Operator::GreaterOrEqual; break;
                case ExprOp::CmpLe: cond->op = Operator::LessOrEqual; break;
                default: break;
            }
            tokens.pop_back(); // 비교 연산자 제거

            // RPN에서 LHS/RHS 분리: 스택 시뮬레이션으로 RHS 시작 지점 찾기
            // 스택 깊이가 마지막으로 1이 되는 지점 = LHS 끝 (그 이후가 RHS)
            size_t splitIdx = 0;
            int stackDepth = 0;
            for (size_t i = 0; i < tokens.size(); ++i) {
                ExprOp op = tokens[i]->op;
                if (op == ExprOp::PushLiteral || op == ExprOp::PushVar) {
                    stackDepth++;
                } else if (op == ExprOp::Negate || op == ExprOp::Not) {
                    // 단항: pop 1 push 1 → 변화 없음
                } else {
                    // 이항: pop 2 push 1 → -1
                    stackDepth--;
                }
                if (stackDepth == 1) {
                    splitIdx = i + 1; // 매번 갱신 → 마지막 분할점
                }
            }

            // LHS 토큰 (0..splitIdx-1)
            if (splitIdx == 1 && tokens[0]->op == ExprOp::PushVar) {
                // 단일 변수 → var_name_id (하위 호환)
                cond->var_name_id = tokens[0]->var_name_id;
            } else if (splitIdx == 1 && tokens[0]->op == ExprOp::PushLiteral) {
                // 단일 리터럴 LHS → lhs_expr
                auto lhsExpr = std::make_unique<ExpressionT>();
                lhsExpr->tokens.push_back(std::move(tokens[0]));
                cond->lhs_expr = std::move(lhsExpr);
            } else {
                // 복합 LHS → lhs_expr
                auto lhsExpr = std::make_unique<ExpressionT>();
                for (size_t i = 0; i < splitIdx; ++i) {
                    lhsExpr->tokens.push_back(std::move(tokens[i]));
                }
                cond->lhs_expr = std::move(lhsExpr);
            }

            // RHS 토큰 (splitIdx..end)
            size_t rhsCount = tokens.size() - splitIdx;
            if (rhsCount == 1 && tokens[splitIdx]->op == ExprOp::PushLiteral) {
                // 단일 리터럴 → compare_value (하위 호환)
                cond->compare_value = std::move(tokens[splitIdx]->literal_value);
            } else if (rhsCount > 0) {
                // 복합 RHS → rhs_expr
                auto rhsExpr = std::make_unique<ExpressionT>();
                for (size_t i = splitIdx; i < tokens.size(); ++i) {
                    rhsExpr->tokens.push_back(std::move(tokens[i]));
                }
                cond->rhs_expr = std::move(rhsExpr);
            }
        } else {
            // 비교 연산자 없음 → 에러
            addError(lineNum, "expected comparison operator (==, !=, >, <, >=, <=)");
            return false;
        }
    }

    // -> true_node
    skipSpaces(content, pos);
    if (pos + 1 >= content.size() || content[pos] != '-' || content[pos + 1] != '>') {
        addError(lineNum, "expected '->' after condition");
        return false;
    }
    pos += 2;

    std::string trueTarget = parseWord(content, pos);
    cond->true_jump_node_id = addString(trueTarget);

    // else false_node (선택적)
    cond->false_jump_node_id = -1;
    skipSpaces(content, pos);
    if (pos < content.size()) {
        std::string elseKw = parseWord(content, pos);
        if (elseKw == "else") {
            std::string falseTarget = parseWord(content, pos);
            if (!falseTarget.empty()) {
                cond->false_jump_node_id = addString(falseTarget);
            }
        }
    }

    auto instr = std::make_unique<InstructionT>();
    instr->data.Set(*cond);
    currentNode_->lines.push_back(std::move(instr));

    // 라인번호 기록 (jump target 검증용)
    size_t nodeIdx = story_.nodes.size() - 1;
    size_t instrIdx = currentNode_->lines.size() - 1;
    instrLineMap_[instrKey(nodeIdx, instrIdx)] = lineNum;

    return true;
}

// --- elif 조건식 -> 참 ---
bool Parser::parseElifLine(const std::string& content, int lineNum) {
    if (!currentNode_) {
        addError(lineNum, "elif outside of label");
        return false;
    }

    // 이전 Condition에 이미 inline else가 있는지 체크
    if (!currentNode_->lines.empty()) {
        auto& prev = currentNode_->lines.back();
        if (prev->data.type == OpData::Condition) {
            auto* prevCond = prev->data.AsCondition();
            if (prevCond->false_jump_node_id >= 0) {
                addError(lineNum, "'elif' cannot follow a condition that already has 'else'");
                return false;
            }
        }
    }

    size_t pos = 4; // skip "elif"
    auto cond = std::make_unique<ConditionT>();

    // 전체 조건식 파싱 (parseConditionLine과 동일한 로직)
    bool hasLogicalOps = false;
    std::unique_ptr<ExpressionT> fullExpr;
    if (!parseFullConditionExpr(content, pos, fullExpr, hasLogicalOps)) {
        addError(lineNum, "expected expression after 'elif'");
        return false;
    }

    if (hasLogicalOps) {
        cond->cond_expr = std::move(fullExpr);
    } else {
        auto& tokens = fullExpr->tokens;
        ExprOp lastOp = tokens.back()->op;
        if (lastOp >= ExprOp::CmpEq && lastOp <= ExprOp::CmpLe) {
            switch (lastOp) {
                case ExprOp::CmpEq: cond->op = Operator::Equal; break;
                case ExprOp::CmpNe: cond->op = Operator::NotEqual; break;
                case ExprOp::CmpGt: cond->op = Operator::Greater; break;
                case ExprOp::CmpLt: cond->op = Operator::Less; break;
                case ExprOp::CmpGe: cond->op = Operator::GreaterOrEqual; break;
                case ExprOp::CmpLe: cond->op = Operator::LessOrEqual; break;
                default: break;
            }
            tokens.pop_back();

            size_t splitIdx = 0;
            int stackDepth = 0;
            for (size_t i = 0; i < tokens.size(); ++i) {
                ExprOp op = tokens[i]->op;
                if (op == ExprOp::PushLiteral || op == ExprOp::PushVar) {
                    stackDepth++;
                } else if (op == ExprOp::Negate || op == ExprOp::Not) {
                    // 단항: 변화 없음
                } else {
                    stackDepth--;
                }
                if (stackDepth == 1) {
                    splitIdx = i + 1;
                }
            }

            // LHS
            if (splitIdx == 1 && tokens[0]->op == ExprOp::PushVar) {
                cond->var_name_id = tokens[0]->var_name_id;
            } else if (splitIdx == 1 && tokens[0]->op == ExprOp::PushLiteral) {
                auto lhsExpr = std::make_unique<ExpressionT>();
                lhsExpr->tokens.push_back(std::move(tokens[0]));
                cond->lhs_expr = std::move(lhsExpr);
            } else {
                auto lhsExpr = std::make_unique<ExpressionT>();
                for (size_t i = 0; i < splitIdx; ++i) {
                    lhsExpr->tokens.push_back(std::move(tokens[i]));
                }
                cond->lhs_expr = std::move(lhsExpr);
            }

            // RHS
            size_t rhsCount = tokens.size() - splitIdx;
            if (rhsCount == 1 && tokens[splitIdx]->op == ExprOp::PushLiteral) {
                cond->compare_value = std::move(tokens[splitIdx]->literal_value);
            } else if (rhsCount > 0) {
                auto rhsExpr = std::make_unique<ExpressionT>();
                for (size_t i = splitIdx; i < tokens.size(); ++i) {
                    rhsExpr->tokens.push_back(std::move(tokens[i]));
                }
                cond->rhs_expr = std::move(rhsExpr);
            }
        } else {
            addError(lineNum, "expected comparison operator (==, !=, >, <, >=, <=)");
            return false;
        }
    }

    // -> true_node (elif은 항상 false_jump=-1)
    skipSpaces(content, pos);
    if (pos + 1 >= content.size() || content[pos] != '-' || content[pos + 1] != '>') {
        addError(lineNum, "expected '->' after elif condition");
        return false;
    }
    pos += 2;

    std::string trueTarget = parseWord(content, pos);
    if (trueTarget.empty()) {
        addError(lineNum, "expected target node name after '->'");
        return false;
    }
    cond->true_jump_node_id = addString(trueTarget);
    cond->false_jump_node_id = -1; // 항상 fall-through

    auto instr = std::make_unique<InstructionT>();
    instr->data.Set(*cond);
    currentNode_->lines.push_back(std::move(instr));

    size_t nodeIdx = story_.nodes.size() - 1;
    size_t instrIdx = currentNode_->lines.size() - 1;
    instrLineMap_[instrKey(nodeIdx, instrIdx)] = lineNum;

    return true;
}

// --- else -> 타겟 ---
bool Parser::parseElseLine(const std::string& content, int lineNum) {
    if (!currentNode_) {
        addError(lineNum, "else outside of label");
        return false;
    }

    // 이전 Condition에 이미 inline else가 있는지 체크
    if (!currentNode_->lines.empty()) {
        auto& prev = currentNode_->lines.back();
        if (prev->data.type == OpData::Condition) {
            auto* prevCond = prev->data.AsCondition();
            if (prevCond->false_jump_node_id >= 0) {
                addError(lineNum, "'else' cannot follow a condition that already has 'else'");
                return false;
            }
        }
    }

    size_t pos = 4; // skip "else"
    skipSpaces(content, pos);

    // -> 필수
    if (pos + 1 >= content.size() || content[pos] != '-' || content[pos + 1] != '>') {
        addError(lineNum, "expected '->' after 'else'");
        return false;
    }
    pos += 2;

    std::string target = parseWord(content, pos);
    if (target.empty()) {
        addError(lineNum, "expected target node name after 'else ->'");
        return false;
    }

    // Jump 명령어 생성
    auto jump = std::make_unique<JumpT>();
    jump->target_node_name_id = addString(target);
    jump->is_call = false;

    auto instr = std::make_unique<InstructionT>();
    instr->data.Set(*jump);
    currentNode_->lines.push_back(std::move(instr));

    size_t nodeIdx = story_.nodes.size() - 1;
    size_t instrIdx = currentNode_->lines.size() - 1;
    instrLineMap_[instrKey(nodeIdx, instrIdx)] = lineNum;

    return true;
}

// --- random: 블록 내 분기 라인 ---
bool Parser::parseRandomBranchLine(const std::string& content, int lineNum) {
    if (!currentNode_) {
        addError(lineNum, "random branch outside of label");
        return false;
    }

    size_t pos = 0;
    skipSpaces(content, pos);

    int weight = 1; // 기본 가중치

    // 숫자(가중치)인지 '->'인지 판별
    if (pos < content.size() && content[pos] != '-') {
        // 가중치 숫자 파싱
        size_t start = pos;
        std::string weightStr = parseWord(content, pos);
        char* end = nullptr;
        long w = std::strtol(weightStr.c_str(), &end, 10);
        if (end == weightStr.c_str() || *end != '\0' || w < 0) {
            addError(lineNum, "expected weight (non-negative integer) or '->' in random branch");
            return false;
        }
        weight = static_cast<int>(w);
        skipSpaces(content, pos);
    }

    // -> 필수
    if (pos + 1 >= content.size() || content[pos] != '-' || content[pos + 1] != '>') {
        addError(lineNum, "expected '->' in random branch");
        return false;
    }
    pos += 2;

    std::string target = parseWord(content, pos);
    if (target.empty()) {
        addError(lineNum, "expected target node name after '->'");
        return false;
    }

    auto branch = std::make_unique<RandomBranchT>();
    branch->target_node_name_id = addString(target);
    branch->weight = weight;
    pendingRandomBranches_.push_back(std::move(branch));

    return true;
}

// --- random 블록 flush ---
void Parser::flushRandomBlock(int lineNum) {
    if (pendingRandomBranches_.empty()) return;
    if (!currentNode_) {
        pendingRandomBranches_.clear();
        return;
    }

    auto random = std::make_unique<RandomT>();
    random->branches = std::move(pendingRandomBranches_);
    pendingRandomBranches_.clear();

    auto instr = std::make_unique<InstructionT>();
    instr->data.Set(*random);
    currentNode_->lines.push_back(std::move(instr));

    size_t nodeIdx = story_.nodes.size() - 1;
    size_t instrIdx = currentNode_->lines.size() - 1;
    instrLineMap_[instrKey(nodeIdx, instrIdx)] = lineNum;
}

// --- @ 명령 파라미터... ---
bool Parser::parseCommandLine(const std::string& content, int lineNum) {
    if (!currentNode_) {
        addError(lineNum, "command outside of label");
        return false;
    }

    size_t pos = 1; // skip "@"
    std::string cmdType = parseWord(content, pos);
    if (cmdType.empty()) {
        addError(lineNum, "expected command type after '@'");
        return false;
    }

    auto cmd = std::make_unique<CommandT>();
    cmd->type_id = addString(cmdType);

    // 파라미터들
    while (true) {
        skipSpaces(content, pos);
        if (pos >= content.size()) break;

        if (content[pos] == '"') {
            std::string param = parseQuotedString(content, pos);
            cmd->params.push_back(addString(param));
        } else {
            std::string param = parseWord(content, pos);
            if (param.empty()) break;
            cmd->params.push_back(addString(param));
        }
    }

    auto instr = std::make_unique<InstructionT>();
    instr->data.Set(*cmd);
    currentNode_->lines.push_back(std::move(instr));
    return true;
}

// =================================================================
// 메인 파서
// =================================================================
bool Parser::parse(const std::string& filepath) {
    filename_ = filepath;
    story_ = StoryT();
    story_.version = "0.1.0";
    stringMap_.clear();
    currentNode_ = nullptr;
    inMenu_ = false;
    inRandom_ = false;
    seenFirstLabel_ = false;
    prevLineType_ = PrevLineType::NONE;
    error_.clear();
    errors_.clear();
    instrLineMap_.clear();
    pendingRandomBranches_.clear();

    std::ifstream ifs(filepath);
    if (!ifs.is_open()) {
        error_ = "Failed to open file: " + filepath;
        errors_.push_back(error_);
        return false;
    }

    std::string rawLine;
    int lineNum = 0;

    while (std::getline(ifs, rawLine)) {
        lineNum++;

        // BOM 제거 (UTF-8 BOM)
        if (lineNum == 1 && rawLine.size() >= 3 &&
            rawLine[0] == '\xEF' && rawLine[1] == '\xBB' && rawLine[2] == '\xBF') {
            rawLine = rawLine.substr(3);
        }

        // CR 제거
        if (!rawLine.empty() && rawLine.back() == '\r') {
            rawLine.pop_back();
        }

        // 빈 줄
        std::string trimmed = trim(rawLine);
        if (trimmed.empty()) continue;

        // 주석
        if (trimmed[0] == '#') continue;

        size_t indent = countIndent(rawLine);

        // --- 들여쓰기 레벨 0: label 또는 global var ---
        if (indent == 0) {
            if (trimmed.substr(0, 5) == "label") {
                if (inRandom_) { flushRandomBlock(lineNum); inRandom_ = false; }
                inMenu_ = false;
                prevLineType_ = PrevLineType::NONE;
                if (!parseLabelLine(trimmed, lineNum)) continue; // 에러 복구
                continue;
            }

            // global variable: label 앞에 $ 변수 = 값
            if (!seenFirstLabel_ && trimmed[0] == '$') {
                parseGlobalVarLine(trimmed, lineNum);
                continue;
            }

            // 들여쓰기 0에서 다른 것은 에러
            addError(lineNum, "unexpected content at column 0 (expected 'label' or global '$')");
            continue; // 에러 복구
        }

        // --- 들여쓰기 레벨 4+: label 내부 ---
        if (indent >= 4 && indent < 8) {
            if (inRandom_) { flushRandomBlock(lineNum); }
            inRandom_ = false;
            inMenu_ = false; // menu 블록 종료

            if (trimmed == "menu:") {
                inMenu_ = true;
                prevLineType_ = PrevLineType::NONE;
                continue;
            }

            if (trimmed == "random:") {
                inRandom_ = true;
                prevLineType_ = PrevLineType::NONE;
                continue;
            }

            // jump
            if (trimmed.substr(0, 4) == "jump" && (trimmed.size() == 4 || trimmed[4] == ' ')) {
                parseJumpLine(trimmed, lineNum, false);
                prevLineType_ = PrevLineType::NONE;
                continue;
            }

            // call
            if (trimmed.substr(0, 4) == "call" && (trimmed.size() == 4 || trimmed[4] == ' ')) {
                parseJumpLine(trimmed, lineNum, true);
                prevLineType_ = PrevLineType::NONE;
                continue;
            }

            // $ 변수 설정
            if (trimmed[0] == '$') {
                parseSetVarLine(trimmed, lineNum);
                prevLineType_ = PrevLineType::NONE;
                continue;
            }

            // if 조건문
            if (trimmed.substr(0, 2) == "if" && (trimmed.size() == 2 || trimmed[2] == ' ')) {
                parseConditionLine(trimmed, lineNum);
                prevLineType_ = PrevLineType::IF;
                continue;
            }

            // elif 조건문
            if (trimmed.size() >= 4 && trimmed.substr(0, 4) == "elif" && (trimmed.size() == 4 || trimmed[4] == ' ')) {
                if (prevLineType_ == PrevLineType::IF || prevLineType_ == PrevLineType::ELIF) {
                    parseElifLine(trimmed, lineNum);
                    prevLineType_ = PrevLineType::ELIF;
                } else {
                    addError(lineNum, "'elif' must follow 'if' or 'elif'");
                }
                continue;
            }

            // standalone else
            if (trimmed.size() >= 4 && trimmed.substr(0, 4) == "else" && (trimmed.size() == 4 || trimmed[4] == ' ')) {
                if (prevLineType_ == PrevLineType::IF || prevLineType_ == PrevLineType::ELIF) {
                    parseElseLine(trimmed, lineNum);
                } else {
                    // 체인 밖의 else → dialogue로 처리 (캐릭터 이름)
                    parseDialogueLine(trimmed, lineNum);
                }
                prevLineType_ = PrevLineType::NONE;
                continue;
            }

            // @ 엔진 명령
            if (trimmed[0] == '@') {
                parseCommandLine(trimmed, lineNum);
                prevLineType_ = PrevLineType::NONE;
                continue;
            }

            // 대사 (캐릭터 "대사" 또는 "나레이션")
            parseDialogueLine(trimmed, lineNum);
            prevLineType_ = PrevLineType::NONE;
            continue;
        }

        // --- 들여쓰기 레벨 8+: menu 선택지 / random 분기 ---
        if (indent >= 8) {
            if (inMenu_) {
                parseMenuChoiceLine(trimmed, lineNum);
                continue;
            }
            if (inRandom_) {
                parseRandomBranchLine(trimmed, lineNum);
                continue;
            }
            addError(lineNum, "unexpected deep indentation (not inside menu: or random:)");
            continue; // 에러 복구
        }
    }

    // 파일 끝에서 pending random 블록 flush
    if (inRandom_) { flushRandomBlock(lineNum); inRandom_ = false; }

    if (story_.nodes.empty()) {
        std::string msg = "No labels found in " + filepath;
        if (error_.empty()) error_ = msg;
        errors_.push_back(msg);
        return false;
    }

    // Jump target 검증 패스
    validateJumpTargets();

    return errors_.empty();
}

// =================================================================
// Jump target 검증
// =================================================================
void Parser::validateJumpTargets() {
    // 모든 노드 이름 수집
    std::unordered_set<std::string> nodeNames;
    for (const auto& node : story_.nodes) {
        nodeNames.insert(node->name);
    }

    // 모든 instruction 순회하며 타겟 검증
    for (size_t ni = 0; ni < story_.nodes.size(); ++ni) {
        const auto& node = story_.nodes[ni];
        for (size_t ii = 0; ii < node->lines.size(); ++ii) {
            const auto& instr = node->lines[ii];

            int lineNum = 0;
            auto it = instrLineMap_.find(instrKey(ni, ii));
            if (it != instrLineMap_.end()) {
                lineNum = it->second;
            }

            switch (instr->data.type) {
                case OpData::Jump: {
                    auto* jump = instr->data.AsJump();
                    const std::string& target = story_.string_pool[jump->target_node_name_id];
                    if (nodeNames.find(target) == nodeNames.end()) {
                        addError(lineNum, "jump target '" + target + "' does not exist");
                    }
                    break;
                }
                case OpData::Choice: {
                    auto* choice = instr->data.AsChoice();
                    const std::string& target = story_.string_pool[choice->target_node_name_id];
                    if (nodeNames.find(target) == nodeNames.end()) {
                        addError(lineNum, "choice target '" + target + "' does not exist");
                    }
                    break;
                }
                case OpData::Condition: {
                    auto* cond = instr->data.AsCondition();
                    if (cond->true_jump_node_id >= 0) {
                        const std::string& target = story_.string_pool[cond->true_jump_node_id];
                        if (nodeNames.find(target) == nodeNames.end()) {
                            addError(lineNum, "condition true target '" + target + "' does not exist");
                        }
                    }
                    if (cond->false_jump_node_id >= 0) {
                        const std::string& target = story_.string_pool[cond->false_jump_node_id];
                        if (nodeNames.find(target) == nodeNames.end()) {
                            addError(lineNum, "condition false target '" + target + "' does not exist");
                        }
                    }
                    break;
                }
                case OpData::Random: {
                    auto* random = instr->data.AsRandom();
                    for (const auto& branch : random->branches) {
                        const std::string& target = story_.string_pool[branch->target_node_name_id];
                        if (nodeNames.find(target) == nodeNames.end()) {
                            addError(lineNum, "random target '" + target + "' does not exist");
                        }
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }
}

// =================================================================
// 컴파일 (.gyb 출력)
// =================================================================
bool Parser::compile(const std::string& outputPath) {
    if (hasErrors()) {
        if (error_.empty()) error_ = "Cannot compile: parse errors exist";
        return false;
    }

    flatbuffers::FlatBufferBuilder builder;

    auto rootOffset = Story::Pack(builder, &story_);
    builder.Finish(rootOffset);

    std::ofstream ofs(outputPath, std::ios::binary);
    if (!ofs.is_open()) {
        error_ = "Failed to write: " + outputPath;
        errors_.push_back(error_);
        return false;
    }

    ofs.write(reinterpret_cast<const char*>(builder.GetBufferPointer()),
              builder.GetSize());
    ofs.close();

    std::cout << "Compiled: " << outputPath
              << " (" << builder.GetSize() << " bytes, "
              << story_.nodes.size() << " nodes, "
              << story_.string_pool.size() << " strings)" << std::endl;
    return true;
}

} // namespace Gyeol
