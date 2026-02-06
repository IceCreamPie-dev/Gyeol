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

    if (!parseValue(content, pos, setvar->value)) {
        addError(lineNum, "invalid value");
        return false;
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

// --- if 변수 op 값 -> 참 else 거짓 ---
bool Parser::parseConditionLine(const std::string& content, int lineNum) {
    if (!currentNode_) {
        addError(lineNum, "condition outside of label");
        return false;
    }

    size_t pos = 2; // skip "if"
    std::string varName = parseWord(content, pos);
    if (varName.empty()) {
        addError(lineNum, "expected variable name after 'if'");
        return false;
    }

    // 연산자
    std::string opStr = parseWord(content, pos);
    Operator op = Operator::Equal;
    if (opStr == "==") op = Operator::Equal;
    else if (opStr == "!=") op = Operator::NotEqual;
    else if (opStr == ">") op = Operator::Greater;
    else if (opStr == "<") op = Operator::Less;
    else if (opStr == ">=") op = Operator::GreaterOrEqual;
    else if (opStr == "<=") op = Operator::LessOrEqual;
    else {
        addError(lineNum, "unknown operator: " + opStr);
        return false;
    }

    auto cond = std::make_unique<ConditionT>();
    cond->var_name_id = addString(varName);
    cond->op = op;

    // 비교 값
    if (!parseValue(content, pos, cond->compare_value)) {
        addError(lineNum, "invalid compare value");
        return false;
    }

    // -> true_node
    skipSpaces(content, pos);
    if (pos + 1 >= content.size() || content[pos] != '-' || content[pos + 1] != '>') {
        addError(lineNum, "expected '->' after value");
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
    seenFirstLabel_ = false;
    error_.clear();
    errors_.clear();
    instrLineMap_.clear();

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
                inMenu_ = false;
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
            inMenu_ = false; // menu 블록 종료

            if (trimmed == "menu:") {
                inMenu_ = true;
                continue;
            }

            // jump
            if (trimmed.substr(0, 4) == "jump" && (trimmed.size() == 4 || trimmed[4] == ' ')) {
                parseJumpLine(trimmed, lineNum, false);
                continue;
            }

            // call
            if (trimmed.substr(0, 4) == "call" && (trimmed.size() == 4 || trimmed[4] == ' ')) {
                parseJumpLine(trimmed, lineNum, true);
                continue;
            }

            // $ 변수 설정
            if (trimmed[0] == '$') {
                parseSetVarLine(trimmed, lineNum);
                continue;
            }

            // if 조건문
            if (trimmed.substr(0, 2) == "if" && (trimmed.size() == 2 || trimmed[2] == ' ')) {
                parseConditionLine(trimmed, lineNum);
                continue;
            }

            // @ 엔진 명령
            if (trimmed[0] == '@') {
                parseCommandLine(trimmed, lineNum);
                continue;
            }

            // 대사 (캐릭터 "대사" 또는 "나레이션")
            parseDialogueLine(trimmed, lineNum);
            continue;
        }

        // --- 들여쓰기 레벨 8+: menu 선택지 ---
        if (indent >= 8) {
            if (!inMenu_) {
                addError(lineNum, "unexpected deep indentation (not inside menu:)");
                continue; // 에러 복구
            }

            parseMenuChoiceLine(trimmed, lineNum);
            continue;
        }
    }

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
