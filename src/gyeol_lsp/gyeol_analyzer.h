#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace Gyeol {

// 라벨(노드) 정보
struct LabelInfo {
    std::string name;
    int line;  // 0-based line number
    std::vector<std::string> params;
};

// 변수 정보
struct VarInfo {
    std::string name;
    int line;  // 0-based line number
    bool isGlobal;  // label 앞에 선언된 전역 변수 여부
};

// 진단 정보 (에러/경고)
struct DiagInfo {
    int line;       // 0-based line number
    int col;        // 0-based column
    std::string message;
    int severity;   // 1=Error, 2=Warning, 3=Info, 4=Hint
};

// Jump/Call 참조 정보 (go-to-definition 용)
struct JumpRef {
    std::string targetName;
    int line;   // 0-based
    int col;    // 0-based (타겟 이름 시작 위치)
};

// 변수 참조 정보 (go-to-definition 용)
struct VarRef {
    std::string name;
    int line;   // 0-based
    int col;    // 0-based
};

// 경량 텍스트 기반 분석기 (Parser 없이 빠른 심볼 추출)
// 진단은 Parser를 사용하여 정확한 에러를 수집
class Analyzer {
public:
    // 전체 분석 실행 (심볼 추출 + 진단)
    void analyze(const std::string& content, const std::string& uri = "");

    // 심볼만 추출 (빠른 스캔, temp 파일 불필요)
    void scanSymbols(const std::string& content);

    // Parser 기반 진단 수집 (temp 파일 생성 → Parser::parse → 에러 수집)
    void collectDiagnostics(const std::string& content, const std::string& uri);

    // 결과 접근
    const std::vector<LabelInfo>& getLabels() const { return labels_; }
    const std::vector<VarInfo>& getVariables() const { return variables_; }
    const std::vector<DiagInfo>& getDiagnostics() const { return diagnostics_; }
    const std::vector<JumpRef>& getJumpRefs() const { return jumpRefs_; }
    const std::vector<VarRef>& getVarRefs() const { return varRefs_; }

private:
    std::vector<LabelInfo> labels_;
    std::vector<VarInfo> variables_;
    std::vector<DiagInfo> diagnostics_;
    std::vector<JumpRef> jumpRefs_;
    std::vector<VarRef> varRefs_;

    // 텍스트 기반 라인 스캔 헬퍼
    void scanLine(const std::string& line, int lineNum, bool& seenFirstLabel);

    // 문자열 유틸리티
    static std::string trim(const std::string& str);
    static bool startsWith(const std::string& str, const std::string& prefix);

    // Parser 에러 문자열 → DiagInfo 변환
    // 포맷: "filename:lineNum: message"
    DiagInfo parseErrorString(const std::string& errorStr);
};

} // namespace Gyeol
