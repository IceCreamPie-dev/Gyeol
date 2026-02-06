#pragma once
#include "gyeol_generated.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>

namespace Gyeol {

class Parser {
public:
    // .gyeol 파일을 파싱하여 내부 StoryT 객체 생성
    bool parse(const std::string& filepath);

    // 파싱된 결과를 .gyb 바이너리로 저장
    bool compile(const std::string& outputPath);

    // 첫 번째 에러 (하위 호환)
    const std::string& getError() const { return error_; }

    // 수집된 모든 에러
    const std::vector<std::string>& getErrors() const { return errors_; }
    bool hasErrors() const { return !errors_.empty(); }

private:
    ICPDev::Gyeol::Schema::StoryT story_;
    std::string error_;
    std::vector<std::string> errors_;
    std::string filename_;

    // String Pool 관리 (중복 제거)
    std::unordered_map<std::string, int32_t> stringMap_;
    int32_t addString(const std::string& str);

    // 현재 파싱 상태
    ICPDev::Gyeol::Schema::NodeT* currentNode_ = nullptr;
    bool inMenu_ = false;
    bool seenFirstLabel_ = false; // global_vars 처리용

    // 에러 수집 (계속 파싱)
    void addError(int lineNum, const std::string& msg);
    // 치명적 에러 (파싱 중단)
    void setError(int lineNum, const std::string& msg);

    // 파싱 헬퍼
    static size_t countIndent(const std::string& line);
    static std::string trim(const std::string& str);
    std::string parseQuotedString(const std::string& text, size_t& pos);
    static std::string parseWord(const std::string& text, size_t& pos);
    static void skipSpaces(const std::string& text, size_t& pos);

    // 라인별 파서
    bool parseLabelLine(const std::string& content, int lineNum);
    bool parseDialogueLine(const std::string& content, int lineNum);
    bool parseMenuChoiceLine(const std::string& content, int lineNum);
    bool parseJumpLine(const std::string& content, int lineNum, bool isCall);
    bool parseSetVarLine(const std::string& content, int lineNum);
    bool parseGlobalVarLine(const std::string& content, int lineNum);
    bool parseConditionLine(const std::string& content, int lineNum);
    bool parseCommandLine(const std::string& content, int lineNum);

    // 값 파싱
    bool parseValue(const std::string& text, size_t& pos,
                    ICPDev::Gyeol::Schema::ValueDataUnion& outValue);

    // Jump target 검증
    std::unordered_map<uint64_t, int> instrLineMap_;
    static uint64_t instrKey(size_t nodeIdx, size_t instrIdx) {
        return (static_cast<uint64_t>(nodeIdx) << 32) | static_cast<uint64_t>(instrIdx);
    }
    void validateJumpTargets();
};

} // namespace Gyeol
