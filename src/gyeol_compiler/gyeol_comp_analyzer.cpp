#include "gyeol_comp_analyzer.h"
#include <queue>
#include <regex>
#include <algorithm>
#include <sstream>

using namespace ICPDev::Gyeol::Schema;

namespace Gyeol {

// =================================================================
// 도달 가능 노드 (BFS)
// =================================================================
std::unordered_set<std::string> CompilerAnalyzer::findReachableNodes(
    const StoryT& story) {

    std::unordered_set<std::string> reachable;
    std::queue<std::string> queue;

    // 시작 노드
    if (!story.start_node_name.empty()) {
        queue.push(story.start_node_name);
        reachable.insert(story.start_node_name);
    }

    // 노드 이름 → 인덱스 맵
    std::unordered_map<std::string, size_t> nodeIndex;
    for (size_t i = 0; i < story.nodes.size(); ++i) {
        nodeIndex[story.nodes[i]->name] = i;
    }

    while (!queue.empty()) {
        std::string current = queue.front();
        queue.pop();

        auto it = nodeIndex.find(current);
        if (it == nodeIndex.end()) continue;

        const auto& node = story.nodes[it->second];
        for (const auto& instr : node->lines) {
            std::vector<int32_t> targetIds;

            switch (instr->data.type) {
                case OpData::Jump: {
                    auto* jump = instr->data.AsJump();
                    targetIds.push_back(jump->target_node_name_id);
                    break;
                }
                case OpData::Choice: {
                    auto* choice = instr->data.AsChoice();
                    targetIds.push_back(choice->target_node_name_id);
                    break;
                }
                case OpData::Condition: {
                    auto* cond = instr->data.AsCondition();
                    if (cond->true_jump_node_id >= 0)
                        targetIds.push_back(cond->true_jump_node_id);
                    if (cond->false_jump_node_id >= 0)
                        targetIds.push_back(cond->false_jump_node_id);
                    break;
                }
                case OpData::Random: {
                    auto* random = instr->data.AsRandom();
                    for (const auto& branch : random->branches) {
                        targetIds.push_back(branch->target_node_name_id);
                    }
                    break;
                }
                case OpData::CallWithReturn: {
                    auto* cwr = instr->data.AsCallWithReturn();
                    targetIds.push_back(cwr->target_node_name_id);
                    break;
                }
                default:
                    break;
            }

            for (int32_t id : targetIds) {
                if (id >= 0 && id < static_cast<int32_t>(story.string_pool.size())) {
                    const std::string& name = story.string_pool[id];
                    if (reachable.find(name) == reachable.end()) {
                        reachable.insert(name);
                        queue.push(name);
                    }
                }
            }
        }
    }

    return reachable;
}

// =================================================================
// Expression 내 변수 참조 수집
// =================================================================
void CompilerAnalyzer::collectExprVarRefs(
    const ExpressionT* expr,
    std::unordered_set<std::string>& vars,
    const StoryT& story) {

    if (!expr) return;
    for (const auto& token : expr->tokens) {
        if (token->op == ExprOp::PushVar && token->var_name_id >= 0 &&
            token->var_name_id < static_cast<int32_t>(story.string_pool.size())) {
            vars.insert(story.string_pool[token->var_name_id]);
        }
    }
}

// =================================================================
// 문자열 보간 내 변수 참조 수집
// =================================================================
void CompilerAnalyzer::collectInterpolationVarRefs(
    const std::string& text,
    std::unordered_set<std::string>& vars) {

    // {varname} 또는 {if varname ...} 패턴에서 변수 추출
    size_t pos = 0;
    while (pos < text.size()) {
        size_t start = text.find('{', pos);
        if (start == std::string::npos) break;

        size_t end = text.find('}', start);
        if (end == std::string::npos) break;

        std::string inner = text.substr(start + 1, end - start - 1);

        // {if ...} / {else} / {endif} 스킵하되, 조건 내 변수 추출
        if (inner.size() >= 3 && inner.substr(0, 3) == "if ") {
            // 조건 내 단어들 중 키워드가 아닌 것을 변수로 추정
            std::istringstream iss(inner.substr(3));
            std::string word;
            while (iss >> word) {
                if (word != ">" && word != "<" && word != ">=" && word != "<=" &&
                    word != "==" && word != "!=" && word != "and" && word != "or" &&
                    word != "not" && word != "true" && word != "false") {
                    // 숫자가 아니면 변수
                    bool isNum = !word.empty();
                    for (char c : word) {
                        if (!std::isdigit(c) && c != '.' && c != '-') { isNum = false; break; }
                    }
                    // 따옴표 문자열 스킵
                    if (!word.empty() && (word[0] == '"' || word[0] == '\'')) continue;
                    // 함수 호출 스킵
                    if (word.find('(') != std::string::npos) continue;
                    if (!isNum) vars.insert(word);
                }
            }
        } else if (inner != "else" && inner != "endif") {
            // 단순 {varname}
            if (!inner.empty() && inner[0] != '/') {
                vars.insert(inner);
            }
        }

        pos = end + 1;
    }
}

// =================================================================
// 미사용 변수 — Write/Read 추적
// =================================================================
std::unordered_set<std::string> CompilerAnalyzer::findWrittenVariables(
    const StoryT& story) {

    std::unordered_set<std::string> written;

    // global_vars
    for (const auto& gv : story.global_vars) {
        if (gv->var_name_id >= 0 &&
            gv->var_name_id < static_cast<int32_t>(story.string_pool.size())) {
            written.insert(story.string_pool[gv->var_name_id]);
        }
    }

    // 노드 내 SetVar + Function params
    for (const auto& node : story.nodes) {
        // 함수 매개변수
        for (int32_t pid : node->param_ids) {
            if (pid >= 0 && pid < static_cast<int32_t>(story.string_pool.size())) {
                written.insert(story.string_pool[pid]);
            }
        }

        for (const auto& instr : node->lines) {
            if (instr->data.type == OpData::SetVar) {
                auto* sv = instr->data.AsSetVar();
                if (sv->var_name_id >= 0 &&
                    sv->var_name_id < static_cast<int32_t>(story.string_pool.size())) {
                    written.insert(story.string_pool[sv->var_name_id]);
                }
            }
            else if (instr->data.type == OpData::CallWithReturn) {
                auto* cwr = instr->data.AsCallWithReturn();
                if (cwr->return_var_name_id >= 0 &&
                    cwr->return_var_name_id < static_cast<int32_t>(story.string_pool.size())) {
                    written.insert(story.string_pool[cwr->return_var_name_id]);
                }
            }
        }
    }

    return written;
}

std::unordered_set<std::string> CompilerAnalyzer::findUsedVariables(
    const StoryT& story) {

    std::unordered_set<std::string> used;

    for (const auto& node : story.nodes) {
        for (const auto& instr : node->lines) {
            switch (instr->data.type) {
                case OpData::Condition: {
                    auto* cond = instr->data.AsCondition();
                    // 단순 변수 참조
                    if (cond->var_name_id >= 0 &&
                        cond->var_name_id < static_cast<int32_t>(story.string_pool.size()) &&
                        !cond->cond_expr && !cond->lhs_expr) {
                        used.insert(story.string_pool[cond->var_name_id]);
                    }
                    collectExprVarRefs(cond->lhs_expr.get(), used, story);
                    collectExprVarRefs(cond->rhs_expr.get(), used, story);
                    collectExprVarRefs(cond->cond_expr.get(), used, story);
                    break;
                }
                case OpData::SetVar: {
                    auto* sv = instr->data.AsSetVar();
                    // expression 내 참조 (자기 자신 참조 포함: x = x + 1)
                    collectExprVarRefs(sv->expr.get(), used, story);
                    break;
                }
                case OpData::Line: {
                    auto* line = instr->data.AsLine();
                    if (line->text_id >= 0 &&
                        line->text_id < static_cast<int32_t>(story.string_pool.size())) {
                        collectInterpolationVarRefs(story.string_pool[line->text_id], used);
                    }
                    break;
                }
                case OpData::Choice: {
                    auto* choice = instr->data.AsChoice();
                    if (choice->text_id >= 0 &&
                        choice->text_id < static_cast<int32_t>(story.string_pool.size())) {
                        collectInterpolationVarRefs(story.string_pool[choice->text_id], used);
                    }
                    // 조건부 선택지
                    if (choice->condition_var_id >= 0 &&
                        choice->condition_var_id < static_cast<int32_t>(story.string_pool.size())) {
                        used.insert(story.string_pool[choice->condition_var_id]);
                    }
                    break;
                }
                case OpData::Return: {
                    auto* ret = instr->data.AsReturn();
                    collectExprVarRefs(ret->expr.get(), used, story);
                    break;
                }
                case OpData::CallWithReturn: {
                    auto* cwr = instr->data.AsCallWithReturn();
                    for (const auto& argExpr : cwr->arg_exprs) {
                        collectExprVarRefs(argExpr.get(), used, story);
                    }
                    break;
                }
                case OpData::Jump: {
                    auto* jump = instr->data.AsJump();
                    for (const auto& argExpr : jump->arg_exprs) {
                        collectExprVarRefs(argExpr.get(), used, story);
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }

    return used;
}

// =================================================================
// 데드 인스트럭션 (jump/return 뒤 코드)
// =================================================================
std::vector<std::pair<std::string, uint32_t>> CompilerAnalyzer::findDeadInstructions(
    const StoryT& story) {

    std::vector<std::pair<std::string, uint32_t>> dead;

    for (const auto& node : story.nodes) {
        bool afterTerminator = false;
        for (uint32_t i = 0; i < node->lines.size(); ++i) {
            if (afterTerminator) {
                dead.emplace_back(node->name, i);
                continue;
            }

            const auto& instr = node->lines[i];
            if (instr->data.type == OpData::Jump) {
                auto* jump = instr->data.AsJump();
                if (!jump->is_call) { // unconditional jump (not call)
                    afterTerminator = true;
                }
            }
            else if (instr->data.type == OpData::Return) {
                afterTerminator = true;
            }
        }
    }

    return dead;
}

// =================================================================
// 상수 폴딩
// =================================================================
int CompilerAnalyzer::foldConstants(StoryT& story) {
    int count = 0;

    for (auto& node : story.nodes) {
        for (auto& instr : node->lines) {
            if (instr->data.type != OpData::SetVar) continue;
            auto* sv = instr->data.AsSetVar();
            if (!sv->expr) continue;

            // Expression이 PushLiteral + 연산자로만 구성되어 있는지 확인
            bool allLiterals = true;
            for (const auto& token : sv->expr->tokens) {
                if (token->op == ExprOp::PushVar ||
                    token->op == ExprOp::PushVisitCount ||
                    token->op == ExprOp::PushVisited ||
                    token->op == ExprOp::ListContains ||
                    token->op == ExprOp::ListLength) {
                    allLiterals = false;
                    break;
                }
            }
            if (!allLiterals || sv->expr->tokens.empty()) continue;

            // 간단한 RPN 스택 머신으로 상수 평가
            std::vector<int32_t> stack;
            bool canFold = true;

            for (const auto& token : sv->expr->tokens) {
                switch (token->op) {
                    case ExprOp::PushLiteral: {
                        if (token->literal_value.type == ValueData::IntValue) {
                            stack.push_back(token->literal_value.AsIntValue()->val);
                        } else {
                            canFold = false; // float/bool/string 폴딩은 미지원
                        }
                        break;
                    }
                    case ExprOp::Add:
                        if (stack.size() >= 2) {
                            auto b = stack.back(); stack.pop_back();
                            auto a = stack.back(); stack.pop_back();
                            stack.push_back(a + b);
                        } else { canFold = false; }
                        break;
                    case ExprOp::Sub:
                        if (stack.size() >= 2) {
                            auto b = stack.back(); stack.pop_back();
                            auto a = stack.back(); stack.pop_back();
                            stack.push_back(a - b);
                        } else { canFold = false; }
                        break;
                    case ExprOp::Mul:
                        if (stack.size() >= 2) {
                            auto b = stack.back(); stack.pop_back();
                            auto a = stack.back(); stack.pop_back();
                            stack.push_back(a * b);
                        } else { canFold = false; }
                        break;
                    case ExprOp::Div:
                        if (stack.size() >= 2 && stack.back() != 0) {
                            auto b = stack.back(); stack.pop_back();
                            auto a = stack.back(); stack.pop_back();
                            stack.push_back(a / b);
                        } else { canFold = false; }
                        break;
                    case ExprOp::Mod:
                        if (stack.size() >= 2 && stack.back() != 0) {
                            auto b = stack.back(); stack.pop_back();
                            auto a = stack.back(); stack.pop_back();
                            stack.push_back(a % b);
                        } else { canFold = false; }
                        break;
                    case ExprOp::Negate:
                        if (!stack.empty()) {
                            stack.back() = -stack.back();
                        } else { canFold = false; }
                        break;
                    default:
                        canFold = false;
                        break;
                }
                if (!canFold) break;
            }

            if (canFold && stack.size() == 1) {
                // Expression을 리터럴 값으로 대체
                sv->expr.reset();
                sv->value.Set(IntValueT());
                sv->value.AsIntValue()->val = stack[0];
                count++;
            }
        }
    }

    return count;
}

// =================================================================
// 데드 인스트럭션 제거
// =================================================================
int CompilerAnalyzer::removeDeadInstructions(StoryT& story) {
    int count = 0;

    for (auto& node : story.nodes) {
        bool afterTerminator = false;
        auto it = node->lines.begin();
        while (it != node->lines.end()) {
            if (afterTerminator) {
                it = node->lines.erase(it);
                count++;
                continue;
            }

            if ((*it)->data.type == OpData::Jump) {
                auto* jump = (*it)->data.AsJump();
                if (!jump->is_call) {
                    afterTerminator = true;
                }
            }
            else if ((*it)->data.type == OpData::Return) {
                afterTerminator = true;
            }
            ++it;
        }
    }

    return count;
}

// =================================================================
// 분석
// =================================================================
AnalysisReport CompilerAnalyzer::analyze(const StoryT& story) {
    AnalysisReport report;

    report.totalNodes = static_cast<int>(story.nodes.size());
    report.stringPoolSize = static_cast<int>(story.string_pool.size());
    report.globalVarCount = static_cast<int>(story.global_vars.size());
    report.characterCount = static_cast<int>(story.characters.size());

    // 총 instruction 수
    for (const auto& node : story.nodes) {
        report.totalInstructions += static_cast<int>(node->lines.size());
    }

    // 도달 가능 노드
    auto reachable = findReachableNodes(story);
    report.reachableNodes = static_cast<int>(reachable.size());

    for (const auto& node : story.nodes) {
        if (reachable.find(node->name) == reachable.end()) {
            AnalysisIssue issue;
            issue.level = AnalysisIssue::WARNING;
            issue.kind = AnalysisIssue::UNREACHABLE_NODE;
            issue.nodeName = node->name;
            issue.detail = "unreachable node '" + node->name + "'";
            report.issues.push_back(issue);
        }
    }

    // 미사용 변수
    auto written = findWrittenVariables(story);
    auto used = findUsedVariables(story);

    for (const auto& var : written) {
        if (used.find(var) == used.end()) {
            AnalysisIssue issue;
            issue.level = AnalysisIssue::WARNING;
            issue.kind = AnalysisIssue::UNUSED_VARIABLE;
            issue.detail = "variable '" + var + "' is set but never read";
            report.issues.push_back(issue);
        }
    }

    // 데드 인스트럭션
    auto dead = findDeadInstructions(story);
    for (const auto& [nodeName, pc] : dead) {
        AnalysisIssue issue;
        issue.level = AnalysisIssue::WARNING;
        issue.kind = AnalysisIssue::DEAD_INSTRUCTION;
        issue.nodeName = nodeName;
        issue.detail = "dead instruction in '" + nodeName + "' at PC " + std::to_string(pc) + " (after unconditional jump/return)";
        report.issues.push_back(issue);
    }

    // 상수 폴딩 가능 여부 (info)
    for (const auto& node : story.nodes) {
        for (uint32_t i = 0; i < node->lines.size(); ++i) {
            const auto& instr = node->lines[i];
            if (instr->data.type != OpData::SetVar) continue;
            auto* sv = instr->data.AsSetVar();
            if (!sv->expr) continue;

            bool allLiterals = true;
            bool hasOps = false;
            for (const auto& token : sv->expr->tokens) {
                if (token->op == ExprOp::PushVar ||
                    token->op == ExprOp::PushVisitCount ||
                    token->op == ExprOp::PushVisited ||
                    token->op == ExprOp::ListContains ||
                    token->op == ExprOp::ListLength) {
                    allLiterals = false;
                    break;
                }
                if (token->op != ExprOp::PushLiteral) hasOps = true;
            }
            if (allLiterals && hasOps && !sv->expr->tokens.empty()) {
                AnalysisIssue issue;
                issue.level = AnalysisIssue::INFO;
                issue.kind = AnalysisIssue::CONSTANT_FOLDABLE;
                issue.nodeName = node->name;
                issue.detail = "constant expression in '" + node->name + "' at PC " + std::to_string(i) + " can be folded";
                report.issues.push_back(issue);
            }
        }
    }

    return report;
}

// =================================================================
// 최적화 적용
// =================================================================
int CompilerAnalyzer::optimize(StoryT& story) {
    int total = 0;
    total += foldConstants(story);
    total += removeDeadInstructions(story);
    return total;
}

// =================================================================
// 리포트 출력
// =================================================================
void CompilerAnalyzer::printReport(const AnalysisReport& report, std::ostream& out) {
    out << "=== Gyeol Analysis Report ===\n\n";

    out << "[Summary]\n";
    out << "  Nodes: " << report.totalNodes
        << " (" << report.reachableNodes << " reachable)\n";
    out << "  Instructions: " << report.totalInstructions << "\n";
    out << "  String pool: " << report.stringPoolSize << " entries\n";
    out << "  Global variables: " << report.globalVarCount << "\n";
    out << "  Characters: " << report.characterCount << "\n";
    out << "\n";

    int warnings = 0, infos = 0;
    for (const auto& issue : report.issues) {
        if (issue.level == AnalysisIssue::WARNING) warnings++;
        else infos++;
    }

    if (warnings > 0) {
        out << "[Warnings]\n";
        for (const auto& issue : report.issues) {
            if (issue.level == AnalysisIssue::WARNING) {
                out << "  W: " << issue.detail << "\n";
            }
        }
        out << "\n";
    }

    if (infos > 0) {
        out << "[Optimizations Available]\n";
        for (const auto& issue : report.issues) {
            if (issue.level == AnalysisIssue::INFO) {
                out << "  O: " << issue.detail << "\n";
            }
        }
        out << "\n";
    }

    if (warnings == 0 && infos == 0) {
        out << "No issues found.\n";
    }
}

} // namespace Gyeol
