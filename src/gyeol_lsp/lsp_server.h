#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>
#include <functional>

#include "gyeol_analyzer.h"

namespace Gyeol {

using json = nlohmann::json;

// 열린 문서 정보
struct DocumentState {
    std::string uri;
    std::string content;
    int version = 0;
    // 캐시된 분석 결과
    Analyzer analyzer;
    bool analyzed = false;
};

class LspServer {
public:
    LspServer();

    // JSON-RPC 메시지 처리 (요청/알림 디스패치)
    // 응답이 필요하면 json 반환, 알림이면 std::nullopt
    json handleMessage(const json& message);

    // 서버 상태
    bool isShutdown() const { return shutdown_; }
    bool shouldExit() const { return exit_; }

    // 보류 중인 알림 (diagnostics 등) 가져오기
    std::vector<json> takePendingNotifications();

private:
    // --- LSP 프로토콜 핸들러 ---

    // Lifecycle
    json handleInitialize(const json& params);
    json handleShutdown();
    void handleExit();

    // Document synchronization
    void handleDidOpen(const json& params);
    void handleDidChange(const json& params);
    void handleDidClose(const json& params);

    // Language features
    json handleCompletion(const json& params);
    json handleDefinition(const json& params);
    json handleHover(const json& params);
    json handleDocumentSymbol(const json& params);

    // --- 내부 헬퍼 ---

    // 문서 분석 + 진단 게시
    void analyzeDocument(const std::string& uri);

    // 진단 알림 생성
    void publishDiagnostics(const std::string& uri,
                            const std::vector<DiagInfo>& diagnostics);

    // 문서의 특정 위치에서 단어 추출
    std::string getWordAtPosition(const std::string& content, int line, int character);

    // 문서의 특정 줄 가져오기
    std::string getLine(const std::string& content, int line);

    // URI ↔ 파일 경로 변환
    static std::string uriToPath(const std::string& uri);
    static std::string pathToUri(const std::string& path);

    // --- 상태 ---
    bool initialized_ = false;
    bool shutdown_ = false;
    bool exit_ = false;

    // 열린 문서들 (URI → DocumentState)
    std::unordered_map<std::string, DocumentState> documents_;

    // 보류 중인 알림 (diagnostics 등)
    std::vector<json> pendingNotifications_;
};

} // namespace Gyeol
