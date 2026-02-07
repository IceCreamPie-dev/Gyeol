#include "lsp_server.h"

#include <sstream>
#include <algorithm>
#include <regex>
#include <iostream>

namespace Gyeol {

// --- 키워드 목록 ---
static const std::vector<std::string> KEYWORDS = {
    "label", "jump", "call", "return", "menu", "random",
    "import", "if", "elif", "else"
};

// --- 내장 함수 ---
static const std::vector<std::pair<std::string, std::string>> BUILTIN_FUNCTIONS = {
    {"visit_count", "visit_count(\"node_name\") - Returns the number of times a node has been visited"},
    {"visited",     "visited(\"node_name\") - Returns true if the node has been visited at least once"}
};

// --- 키워드 설명 (hover 용) ---
static const std::unordered_map<std::string, std::string> KEYWORD_DOCS = {
    {"label",  "label name:\n\nDeclares a story node (scene/knot). The first label in the main file becomes the start node."},
    {"jump",   "jump node_name\n\nJumps to the specified node. Control does not return."},
    {"call",   "call node_name\ncall func(arg1, arg2)\n\nCalls a node as a subroutine. Returns to the calling point when the called node ends or executes 'return'."},
    {"return", "return [expression]\n\nReturns from a subroutine call, optionally with a value."},
    {"menu",   "menu:\n    \"Choice text\" -> target_node\n    \"Choice\" -> target if condition\n\nPresents choices to the player."},
    {"random", "random:\n    50 -> nodeA\n    30 -> nodeB\n    -> nodeC\n\nRandom branch with weighted probabilities."},
    {"import", "import \"filename.gyeol\"\n\nImports another .gyeol file, merging its labels into the current story."},
    {"if",     "if condition -> target_node\nif condition -> target else fallback\n\nConditional branch. Supports comparison operators (==, !=, >, <, >=, <=) and logical operators (and, or, not)."},
    {"elif",   "elif condition -> target_node\n\nAdditional condition branch after 'if' or another 'elif'."},
    {"else",   "else -> target_node\n\nDefault branch when all preceding if/elif conditions are false."}
};

// ============================================================
// 생성자
// ============================================================

LspServer::LspServer() = default;

// ============================================================
// 메시지 디스패치
// ============================================================

json LspServer::handleMessage(const json& message) {
    // JSON-RPC 메시지 처리
    std::string method;
    if (message.contains("method")) {
        method = message["method"].get<std::string>();
    }

    bool isRequest = message.contains("id");
    json id = isRequest ? message["id"] : json(nullptr);
    json params = message.value("params", json::object());

    // --- Lifecycle ---
    if (method == "initialize") {
        json result = handleInitialize(params);
        return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
    }
    if (method == "initialized") {
        // 클라이언트가 초기화 완료 알림 — 무시
        return json(nullptr);
    }
    if (method == "shutdown") {
        json result = handleShutdown();
        return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
    }
    if (method == "exit") {
        handleExit();
        return json(nullptr);
    }

    // --- Document synchronization (알림, 응답 없음) ---
    if (method == "textDocument/didOpen") {
        handleDidOpen(params);
        return json(nullptr);
    }
    if (method == "textDocument/didChange") {
        handleDidChange(params);
        return json(nullptr);
    }
    if (method == "textDocument/didClose") {
        handleDidClose(params);
        return json(nullptr);
    }

    // --- Language features (요청, 응답 필요) ---
    if (method == "textDocument/completion") {
        json result = handleCompletion(params);
        return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
    }
    if (method == "textDocument/definition") {
        json result = handleDefinition(params);
        return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
    }
    if (method == "textDocument/hover") {
        json result = handleHover(params);
        return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
    }
    if (method == "textDocument/documentSymbol") {
        json result = handleDocumentSymbol(params);
        return {{"jsonrpc", "2.0"}, {"id", id}, {"result", result}};
    }

    // 알 수 없는 요청 → MethodNotFound 에러
    if (isRequest) {
        return {
            {"jsonrpc", "2.0"},
            {"id", id},
            {"error", {
                {"code", -32601},
                {"message", "Method not found: " + method}
            }}
        };
    }

    // 알 수 없는 알림 → 무시
    return json(nullptr);
}

std::vector<json> LspServer::takePendingNotifications() {
    std::vector<json> result;
    std::swap(result, pendingNotifications_);
    return result;
}

// ============================================================
// Lifecycle
// ============================================================

json LspServer::handleInitialize(const json& /*params*/) {
    initialized_ = true;

    // 서버 capabilities 반환
    json capabilities = {
        {"textDocumentSync", {
            {"openClose", true},
            {"change", 1},  // TextDocumentSyncKind::Full
        }},
        {"completionProvider", {
            {"triggerCharacters", json::array({"$", " ", ">"})},
        }},
        {"definitionProvider", true},
        {"hoverProvider", true},
        {"documentSymbolProvider", true}
    };

    return {
        {"capabilities", capabilities},
        {"serverInfo", {
            {"name", "GyeolLSP"},
            {"version", "0.1.0"}
        }}
    };
}

json LspServer::handleShutdown() {
    shutdown_ = true;
    return json(nullptr);
}

void LspServer::handleExit() {
    exit_ = true;
}

// ============================================================
// Document synchronization
// ============================================================

void LspServer::handleDidOpen(const json& params) {
    auto& textDoc = params["textDocument"];
    std::string uri = textDoc["uri"].get<std::string>();
    std::string content = textDoc["text"].get<std::string>();
    int version = textDoc.value("version", 0);

    DocumentState doc;
    doc.uri = uri;
    doc.content = content;
    doc.version = version;

    documents_[uri] = std::move(doc);
    analyzeDocument(uri);
}

void LspServer::handleDidChange(const json& params) {
    auto& textDoc = params["textDocument"];
    std::string uri = textDoc["uri"].get<std::string>();
    int version = textDoc.value("version", 0);

    auto it = documents_.find(uri);
    if (it == documents_.end()) return;

    // Full sync: contentChanges[0].text 가 전체 내용
    auto& changes = params["contentChanges"];
    if (!changes.empty()) {
        it->second.content = changes[0]["text"].get<std::string>();
        it->second.version = version;
        it->second.analyzed = false;
    }

    analyzeDocument(uri);
}

void LspServer::handleDidClose(const json& params) {
    std::string uri = params["textDocument"]["uri"].get<std::string>();

    // 진단 클리어 (빈 진단 게시)
    publishDiagnostics(uri, {});
    documents_.erase(uri);
}

// ============================================================
// Completion
// ============================================================

json LspServer::handleCompletion(const json& params) {
    std::string uri = params["textDocument"]["uri"].get<std::string>();
    int line = params["position"]["line"].get<int>();
    int character = params["position"]["character"].get<int>();

    json items = json::array();

    auto it = documents_.find(uri);
    if (it == documents_.end()) return items;

    auto& doc = it->second;
    if (!doc.analyzed) {
        doc.analyzer.scanSymbols(doc.content);
        doc.analyzed = true;
    }

    std::string currentLine = getLine(doc.content, line);
    std::string trimmedLine;
    {
        size_t start = currentLine.find_first_not_of(" \t");
        if (start != std::string::npos) {
            // 커서 위치까지의 내용만 추출
            size_t endPos = std::min(static_cast<size_t>(character), currentLine.size());
            if (start < endPos) {
                trimmedLine = currentLine.substr(start, endPos - start);
            }
        }
    }

    // 컨텍스트 판단

    // 1) "jump " / "call " / "-> " 뒤: 라벨 이름 완성
    bool wantsLabels = false;
    if (trimmedLine.find("jump ") != std::string::npos ||
        trimmedLine.find("call ") != std::string::npos ||
        trimmedLine.find("-> ") != std::string::npos) {
        wantsLabels = true;
    }

    // 2) "$ " 뒤: 변수 이름 완성
    bool wantsVars = false;
    if (trimmedLine.find("$ ") != std::string::npos) {
        wantsVars = true;
    }

    // 3) 줄 시작: 키워드 완성
    bool wantsKeywords = !wantsLabels && !wantsVars;

    if (wantsLabels) {
        for (const auto& label : doc.analyzer.getLabels()) {
            json item = {
                {"label", label.name},
                {"kind", 3},  // CompletionItemKind::Function
                {"detail", "label"}
            };
            if (!label.params.empty()) {
                std::string paramStr;
                for (size_t i = 0; i < label.params.size(); i++) {
                    if (i > 0) paramStr += ", ";
                    paramStr += label.params[i];
                }
                item["detail"] = "label " + label.name + "(" + paramStr + ")";
            }
            items.push_back(item);
        }
    }

    if (wantsVars) {
        for (const auto& var : doc.analyzer.getVariables()) {
            json item = {
                {"label", var.name},
                {"kind", 6},  // CompletionItemKind::Variable
                {"detail", var.isGlobal ? "global variable" : "variable"}
            };
            items.push_back(item);
        }
    }

    if (wantsKeywords) {
        for (const auto& kw : KEYWORDS) {
            json item = {
                {"label", kw},
                {"kind", 14},  // CompletionItemKind::Keyword
                {"detail", "keyword"}
            };
            items.push_back(item);
        }

        // 내장 함수
        for (const auto& fn : BUILTIN_FUNCTIONS) {
            json item = {
                {"label", fn.first},
                {"kind", 3},   // CompletionItemKind::Function
                {"detail", fn.second}
            };
            items.push_back(item);
        }

        // 라벨도 키워드 컨텍스트에서 제안 (jump/call 없이 직접 참조할 때)
        for (const auto& label : doc.analyzer.getLabels()) {
            json item = {
                {"label", label.name},
                {"kind", 3},
                {"detail", "label"}
            };
            items.push_back(item);
        }

        // 변수도 키워드 컨텍스트에서 제안
        for (const auto& var : doc.analyzer.getVariables()) {
            json item = {
                {"label", var.name},
                {"kind", 6},
                {"detail", var.isGlobal ? "global variable" : "variable"}
            };
            items.push_back(item);
        }
    }

    return items;
}

// ============================================================
// Go to Definition
// ============================================================

json LspServer::handleDefinition(const json& params) {
    std::string uri = params["textDocument"]["uri"].get<std::string>();
    int line = params["position"]["line"].get<int>();
    int character = params["position"]["character"].get<int>();

    auto it = documents_.find(uri);
    if (it == documents_.end()) return json(nullptr);

    auto& doc = it->second;
    if (!doc.analyzed) {
        doc.analyzer.scanSymbols(doc.content);
        doc.analyzed = true;
    }

    std::string word = getWordAtPosition(doc.content, line, character);
    if (word.empty()) return json(nullptr);

    // 1) 라벨 정의 찾기 (jump/call/-> 타겟)
    for (const auto& label : doc.analyzer.getLabels()) {
        if (label.name == word) {
            return {
                {"uri", uri},
                {"range", {
                    {"start", {{"line", label.line}, {"character", 0}}},
                    {"end",   {{"line", label.line}, {"character", 0}}}
                }}
            };
        }
    }

    // 2) 변수 정의 찾기 (첫 번째 할당)
    for (const auto& var : doc.analyzer.getVariables()) {
        if (var.name == word) {
            return {
                {"uri", uri},
                {"range", {
                    {"start", {{"line", var.line}, {"character", 0}}},
                    {"end",   {{"line", var.line}, {"character", 0}}}
                }}
            };
        }
    }

    return json(nullptr);
}

// ============================================================
// Hover
// ============================================================

json LspServer::handleHover(const json& params) {
    std::string uri = params["textDocument"]["uri"].get<std::string>();
    int line = params["position"]["line"].get<int>();
    int character = params["position"]["character"].get<int>();

    auto it = documents_.find(uri);
    if (it == documents_.end()) return json(nullptr);

    auto& doc = it->second;
    if (!doc.analyzed) {
        doc.analyzer.scanSymbols(doc.content);
        doc.analyzed = true;
    }

    std::string word = getWordAtPosition(doc.content, line, character);
    if (word.empty()) return json(nullptr);

    // 1) 키워드 hover
    auto kwIt = KEYWORD_DOCS.find(word);
    if (kwIt != KEYWORD_DOCS.end()) {
        return {
            {"contents", {
                {"kind", "markdown"},
                {"value", "```\n" + kwIt->second + "\n```"}
            }}
        };
    }

    // 2) 내장 함수 hover
    for (const auto& fn : BUILTIN_FUNCTIONS) {
        if (fn.first == word) {
            return {
                {"contents", {
                    {"kind", "markdown"},
                    {"value", "```\n" + fn.second + "\n```"}
                }}
            };
        }
    }

    // 3) 라벨 hover
    for (const auto& label : doc.analyzer.getLabels()) {
        if (label.name == word) {
            std::string hoverText = "label " + label.name;
            if (!label.params.empty()) {
                hoverText += "(";
                for (size_t i = 0; i < label.params.size(); i++) {
                    if (i > 0) hoverText += ", ";
                    hoverText += label.params[i];
                }
                hoverText += ")";
            }
            return {
                {"contents", {
                    {"kind", "markdown"},
                    {"value", "```gyeol\n" + hoverText + "\n```"}
                }}
            };
        }
    }

    // 4) 변수 hover
    for (const auto& var : doc.analyzer.getVariables()) {
        if (var.name == word) {
            std::string scope = var.isGlobal ? "global" : "local";
            return {
                {"contents", {
                    {"kind", "markdown"},
                    {"value", "```gyeol\n" + var.name + " (" + scope + " variable)\n```"}
                }}
            };
        }
    }

    return json(nullptr);
}

// ============================================================
// Document Symbols
// ============================================================

json LspServer::handleDocumentSymbol(const json& params) {
    std::string uri = params["textDocument"]["uri"].get<std::string>();

    json symbols = json::array();

    auto it = documents_.find(uri);
    if (it == documents_.end()) return symbols;

    auto& doc = it->second;
    if (!doc.analyzed) {
        doc.analyzer.scanSymbols(doc.content);
        doc.analyzed = true;
    }

    // 라벨 → Function 심볼
    for (const auto& label : doc.analyzer.getLabels()) {
        std::string detail;
        if (!label.params.empty()) {
            detail = "(";
            for (size_t i = 0; i < label.params.size(); i++) {
                if (i > 0) detail += ", ";
                detail += label.params[i];
            }
            detail += ")";
        }

        json sym = {
            {"name", label.name},
            {"kind", 12},  // SymbolKind::Function
            {"range", {
                {"start", {{"line", label.line}, {"character", 0}}},
                {"end",   {{"line", label.line}, {"character", 0}}}
            }},
            {"selectionRange", {
                {"start", {{"line", label.line}, {"character", 0}}},
                {"end",   {{"line", label.line}, {"character", 0}}}
            }}
        };
        if (!detail.empty()) {
            sym["detail"] = detail;
        }
        symbols.push_back(sym);
    }

    // 변수 → Variable 심볼
    for (const auto& var : doc.analyzer.getVariables()) {
        json sym = {
            {"name", var.name},
            {"kind", 13},  // SymbolKind::Variable
            {"detail", var.isGlobal ? "global" : "local"},
            {"range", {
                {"start", {{"line", var.line}, {"character", 0}}},
                {"end",   {{"line", var.line}, {"character", 0}}}
            }},
            {"selectionRange", {
                {"start", {{"line", var.line}, {"character", 0}}},
                {"end",   {{"line", var.line}, {"character", 0}}}
            }}
        };
        symbols.push_back(sym);
    }

    return symbols;
}

// ============================================================
// 내부 헬퍼
// ============================================================

void LspServer::analyzeDocument(const std::string& uri) {
    auto it = documents_.find(uri);
    if (it == documents_.end()) return;

    auto& doc = it->second;

    // 전체 분석 (심볼 추출 + 진단)
    doc.analyzer.analyze(doc.content, uri);
    doc.analyzed = true;

    // 진단 게시
    publishDiagnostics(uri, doc.analyzer.getDiagnostics());
}

void LspServer::publishDiagnostics(const std::string& uri,
                                    const std::vector<DiagInfo>& diagnostics) {
    json diagArray = json::array();

    for (const auto& diag : diagnostics) {
        json d = {
            {"range", {
                {"start", {{"line", diag.line}, {"character", diag.col}}},
                {"end",   {{"line", diag.line}, {"character", diag.col}}}
            }},
            {"severity", diag.severity},
            {"source", "gyeol"},
            {"message", diag.message}
        };
        diagArray.push_back(d);
    }

    json notification = {
        {"jsonrpc", "2.0"},
        {"method", "textDocument/publishDiagnostics"},
        {"params", {
            {"uri", uri},
            {"diagnostics", diagArray}
        }}
    };

    pendingNotifications_.push_back(notification);
}

std::string LspServer::getWordAtPosition(const std::string& content,
                                          int line, int character) {
    std::string lineStr = getLine(content, line);
    if (lineStr.empty() || character < 0 ||
        static_cast<size_t>(character) >= lineStr.size()) {
        return "";
    }

    // 단어 경계 찾기 (식별자: 알파벳, 숫자, 밑줄)
    auto isWordChar = [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    };

    int pos = character;

    // 현재 위치가 단어 문자가 아니면 빈 문자열
    if (!isWordChar(lineStr[pos])) return "";

    // 단어 시작 찾기
    int start = pos;
    while (start > 0 && isWordChar(lineStr[start - 1])) {
        start--;
    }

    // 단어 끝 찾기
    int end = pos;
    while (static_cast<size_t>(end) < lineStr.size() - 1 && isWordChar(lineStr[end + 1])) {
        end++;
    }

    return lineStr.substr(start, end - start + 1);
}

std::string LspServer::getLine(const std::string& content, int line) {
    std::istringstream stream(content);
    std::string result;
    int current = 0;

    while (std::getline(stream, result)) {
        if (current == line) {
            // \r 제거
            if (!result.empty() && result.back() == '\r') {
                result.pop_back();
            }
            return result;
        }
        current++;
    }

    return "";
}

std::string LspServer::uriToPath(const std::string& uri) {
    // "file:///path" → "/path" (Linux/Mac)
    // "file:///C:/path" → "C:/path" (Windows)
    std::string path = uri;

    if (path.find("file:///") == 0) {
        path = path.substr(7);  // "file:///" → 7문자 제거

        // Windows 경로 처리: "/C:/..." → "C:/..."
        if (path.size() >= 3 && path[0] == '/' &&
            std::isalpha(static_cast<unsigned char>(path[1])) && path[2] == ':') {
            path = path.substr(1);
        }
    } else if (path.find("file://") == 0) {
        path = path.substr(7);
    }

    // URL 디코딩 (%20 → 공백 등)
    std::string decoded;
    decoded.reserve(path.size());
    for (size_t i = 0; i < path.size(); i++) {
        if (path[i] == '%' && i + 2 < path.size()) {
            std::string hex = path.substr(i + 1, 2);
            try {
                char c = static_cast<char>(std::stoi(hex, nullptr, 16));
                decoded += c;
                i += 2;
            } catch (...) {
                decoded += path[i];
            }
        } else {
            decoded += path[i];
        }
    }

    return decoded;
}

std::string LspServer::pathToUri(const std::string& path) {
    std::string uri = "file://";

    // Windows 경로 처리
    if (path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':') {
        uri += "/";
    }

    uri += path;
    return uri;
}

} // namespace Gyeol
