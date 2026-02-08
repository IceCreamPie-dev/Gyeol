#pragma once
#include "gyeol_generated.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <ostream>

namespace Gyeol {

struct AnalysisIssue {
    enum Level { WARNING, INFO };
    enum Kind {
        UNREACHABLE_NODE,
        UNUSED_VARIABLE,
        DEAD_INSTRUCTION,
        CONSTANT_FOLDABLE
    };
    Level level;
    Kind kind;
    std::string nodeName;
    std::string detail;
};

struct AnalysisReport {
    // 메트릭
    int totalNodes = 0;
    int reachableNodes = 0;
    int totalInstructions = 0;
    int stringPoolSize = 0;
    int globalVarCount = 0;
    int characterCount = 0;
    // 이슈
    std::vector<AnalysisIssue> issues;
};

class CompilerAnalyzer {
public:
    // StoryT 분석 (parse 후, compile 전)
    AnalysisReport analyze(const ICPDev::Gyeol::Schema::StoryT& story);

    // 최적화 적용 (-O 플래그), 반환값: 적용된 최적화 수
    int optimize(ICPDev::Gyeol::Schema::StoryT& story);

    // 리포트 출력
    static void printReport(const AnalysisReport& report, std::ostream& out);

private:
    // 분석 패스
    std::unordered_set<std::string> findReachableNodes(
        const ICPDev::Gyeol::Schema::StoryT& story);
    std::unordered_set<std::string> findUsedVariables(
        const ICPDev::Gyeol::Schema::StoryT& story);
    std::unordered_set<std::string> findWrittenVariables(
        const ICPDev::Gyeol::Schema::StoryT& story);
    std::vector<std::pair<std::string, uint32_t>> findDeadInstructions(
        const ICPDev::Gyeol::Schema::StoryT& story);

    // Expression 내 변수 참조 수집
    void collectExprVarRefs(const ICPDev::Gyeol::Schema::ExpressionT* expr,
                            std::unordered_set<std::string>& vars,
                            const ICPDev::Gyeol::Schema::StoryT& story);

    // 문자열 보간 내 변수 참조 수집
    void collectInterpolationVarRefs(const std::string& text,
                                     std::unordered_set<std::string>& vars);

    // 최적화 패스
    int foldConstants(ICPDev::Gyeol::Schema::StoryT& story);
    int removeDeadInstructions(ICPDev::Gyeol::Schema::StoryT& story);
};

} // namespace Gyeol
