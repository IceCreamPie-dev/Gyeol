#include "lsp_server.h"

#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>

// JSON-RPC over stdin/stdout 프로토콜 구현
// 메시지 형식: "Content-Length: N\r\n\r\n{json...}"

namespace {

// stderr 로깅 (stdout은 LSP 프로토콜 전용)
void logMessage(const std::string& msg) {
    std::cerr << "[GyeolLSP] " << msg << std::endl;
}

// Content-Length 헤더 읽기
// 반환: body 크기, 실패 시 -1
int readContentLength() {
    std::string headerLine;

    while (std::getline(std::cin, headerLine)) {
        // \r 제거
        if (!headerLine.empty() && headerLine.back() == '\r') {
            headerLine.pop_back();
        }

        // 빈 줄 = 헤더 끝
        if (headerLine.empty()) {
            break;
        }

        // Content-Length 파싱
        const std::string prefix = "Content-Length: ";
        if (headerLine.compare(0, prefix.size(), prefix) == 0) {
            try {
                return std::stoi(headerLine.substr(prefix.size()));
            } catch (...) {
                logMessage("Failed to parse Content-Length: " + headerLine);
                return -1;
            }
        }
        // Content-Type 등 다른 헤더는 무시
    }

    // stdin EOF
    if (std::cin.eof()) {
        return -1;
    }

    return -1;
}

// JSON body 읽기
std::string readBody(int contentLength) {
    std::string body(contentLength, '\0');
    std::cin.read(&body[0], contentLength);

    if (std::cin.gcount() != contentLength) {
        logMessage("Failed to read full body: expected " +
                   std::to_string(contentLength) + ", got " +
                   std::to_string(std::cin.gcount()));
        return "";
    }

    return body;
}

// JSON-RPC 메시지 전송
void sendMessage(const nlohmann::json& message) {
    std::string body = message.dump();
    std::string header = "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n";

    std::cout << header << body;
    std::cout.flush();
}

} // anonymous namespace

int main(int argc, char* argv[]) {
    // --version / --help 처리
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--version") == 0) {
            std::cerr << "GyeolLSP 0.1.0" << std::endl;
            return 0;
        }
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            std::cerr << "GyeolLSP - Language Server for .gyeol files\n"
                      << "Usage: GyeolLSP [options]\n"
                      << "\n"
                      << "Options:\n"
                      << "  --version  Show version\n"
                      << "  -h, --help Show this help\n"
                      << "\n"
                      << "Communicates via JSON-RPC over stdin/stdout.\n";
            return 0;
        }
    }

    // stdout 버퍼링 비활성화
    std::ios_base::sync_with_stdio(false);
    std::cout << std::unitbuf;  // 즉시 flush

    logMessage("Starting GyeolLSP server...");

    Gyeol::LspServer server;

    // 메인 이벤트 루프
    while (!server.shouldExit()) {
        // 1) Content-Length 헤더 읽기
        int contentLength = readContentLength();
        if (contentLength < 0) {
            // stdin EOF 또는 파싱 에러
            if (std::cin.eof()) {
                logMessage("stdin EOF, exiting.");
                break;
            }
            logMessage("Invalid Content-Length, skipping.");
            continue;
        }

        // 2) JSON body 읽기
        std::string body = readBody(contentLength);
        if (body.empty() && contentLength > 0) {
            logMessage("Failed to read body, skipping.");
            continue;
        }

        // 3) JSON 파싱
        nlohmann::json message;
        try {
            message = nlohmann::json::parse(body);
        } catch (const nlohmann::json::parse_error& e) {
            logMessage(std::string("JSON parse error: ") + e.what());
            continue;
        }

        // 4) 메시지 처리
        nlohmann::json response = server.handleMessage(message);

        // 5) 보류 중인 알림 전송 (diagnostics 등)
        auto notifications = server.takePendingNotifications();
        for (const auto& notif : notifications) {
            sendMessage(notif);
        }

        // 6) 응답 전송 (null이 아닌 경우)
        if (!response.is_null()) {
            sendMessage(response);
        }
    }

    logMessage("GyeolLSP server stopped.");

    // exit 코드: shutdown 후 exit이면 0, 아니면 1
    return server.isShutdown() ? 0 : 1;
}
