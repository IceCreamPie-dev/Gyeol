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

    // 문자열에서 직접 파싱 (WASM 등 파일 시스템 없는 환경용)
    bool parseString(const std::string& source, const std::string& filename = "<string>");

    // 파싱된 결과를 .gyb 바이너리로 저장
    bool compile(const std::string& outputPath);

    // 파싱된 결과를 메모리 버퍼로 컴파일 (WASM 등 파일 시스템 없는 환경용)
    std::vector<uint8_t> compileToBuffer();

    // 첫 번째 에러 (하위 호환)
    const std::string& getError() const { return error_; }

    // 수집된 모든 에러
    const std::vector<std::string>& getErrors() const { return errors_; }
    bool hasErrors() const { return !errors_.empty(); }

    // 경고 (에러와 분리 — 컴파일은 성공하지만 잠재적 문제)
    const std::vector<std::string>& getWarnings() const { return warnings_; }
    bool hasWarnings() const { return !warnings_.empty(); }

    // 번역 대상 문자열 CSV 추출
    bool exportStrings(const std::string& outputPath) const;

    // StoryT 접근 (analyzer 등 외부 도구용)
    const ICPDev::Gyeol::Schema::StoryT& getStory() const { return story_; }
    ICPDev::Gyeol::Schema::StoryT& getStoryMutable() { return story_; }

private:
    ICPDev::Gyeol::Schema::StoryT story_;
    std::string error_;
    std::vector<std::string> errors_;
    std::vector<std::string> warnings_;
    std::string filename_;

    // String Pool 관리 (중복 제거)
    std::unordered_map<std::string, int32_t> stringMap_;
    int32_t addString(const std::string& str);
    int32_t addStringWithId(const std::string& str, const std::string& lineId);

    // Line ID 추적 (string_pool과 병렬)
    std::vector<std::string> lineIds_;
    std::string currentNodeName_;

    // 현재 파싱 상태
    ICPDev::Gyeol::Schema::NodeT* currentNode_ = nullptr;
    bool inMenu_ = false;
    bool inRandom_ = false;
    bool inCharacterBlock_ = false;
    bool seenFirstLabel_ = false; // global_vars 처리용

    // 캐릭터 정의 블록
    std::string currentCharacterId_;
    std::unique_ptr<ICPDev::Gyeol::Schema::CharacterDefT> currentCharacter_;
    std::unordered_set<std::string> definedCharacters_;
    std::unordered_set<std::string> usedCharacters_;

    // random: 블록 분기 수집
    std::vector<std::unique_ptr<ICPDev::Gyeol::Schema::RandomBranchT>> pendingRandomBranches_;

    // elif/else 체인 추적
    enum class PrevLineType { NONE, IF, ELIF };
    PrevLineType prevLineType_ = PrevLineType::NONE;

    // 에러 수집 (계속 파싱)
    void addError(int lineNum, const std::string& msg);
    // 치명적 에러 (파싱 중단)
    void setError(int lineNum, const std::string& msg);
    // 경고 수집 (컴파일은 진행)
    void addWarning(int lineNum, const std::string& msg);

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
    bool parseElifLine(const std::string& content, int lineNum);
    bool parseElseLine(const std::string& content, int lineNum);
    bool parseRandomBranchLine(const std::string& content, int lineNum);
    void flushRandomBlock(int lineNum);
    bool parseCommandLine(const std::string& content, int lineNum);
    bool parseReturnLine(const std::string& content, int lineNum);

    // 캐릭터 정의 블록
    bool parseCharacterLine(const std::string& content, int lineNum);
    bool parseCharacterProperty(const std::string& content, int lineNum);
    void flushCharacterBlock();
    void validateCharacters();

    // 매개변수/인자 파싱 헬퍼
    bool parseParamList(const std::string& content, size_t& pos,
                        std::vector<std::string>& outParamNames, int lineNum);
    bool parseArgList(const std::string& content, size_t& pos,
                      std::vector<std::unique_ptr<ICPDev::Gyeol::Schema::ExpressionT>>& outArgExprs,
                      int lineNum);

    // 값 파싱
    bool parseValue(const std::string& text, size_t& pos,
                    ICPDev::Gyeol::Schema::ValueDataUnion& outValue);

    // 표현식 파싱 (Shunting-yard → RPN)
    bool parseExpression(const std::string& text, size_t& pos,
                         std::unique_ptr<ICPDev::Gyeol::Schema::ExpressionT>& outExpr,
                         ICPDev::Gyeol::Schema::ValueDataUnion& outSimpleValue,
                         bool& isSimpleLiteral);

    // 조건 표현식 파싱 (산술 + 비교 + 논리 → 단일 RPN)
    bool parseFullConditionExpr(const std::string& text, size_t& pos,
                                std::unique_ptr<ICPDev::Gyeol::Schema::ExpressionT>& outExpr,
                                bool& hasLogicalOps);

    // Jump target 검증
    std::unordered_map<uint64_t, int> instrLineMap_;
    static uint64_t instrKey(size_t nodeIdx, size_t instrIdx) {
        return (static_cast<uint64_t>(nodeIdx) << 32) | static_cast<uint64_t>(instrIdx);
    }
    void validateJumpTargets();

    // Line ID 생성 헬퍼
    static std::string hashText(const std::string& text);

    // --- Import tracking ---
    std::unordered_set<std::string> importedFiles_;  // 절대 경로 (순환 감지)
    bool isMainFile_ = true;       // 메인 파일 여부 (start_node 결정용)
    bool startNodeSet_ = false;    // start_node_name 설정 완료 여부

    // 단일 파일 파싱 (state 리셋 없이, import 재귀 호출용)
    bool parseFile(const std::string& filepath);
};

} // namespace Gyeol
