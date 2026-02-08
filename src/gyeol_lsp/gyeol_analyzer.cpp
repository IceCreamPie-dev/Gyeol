#include "gyeol_analyzer.h"
#include "gyeol_parser.h"

#include <fstream>
#include <sstream>
#include <regex>
#include <filesystem>
#include <cstdio>
#include <algorithm>

namespace Gyeol {

// --- 유틸리티 ---

std::string Analyzer::trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

bool Analyzer::startsWith(const std::string& str, const std::string& prefix) {
    if (prefix.size() > str.size()) return false;
    return str.compare(0, prefix.size(), prefix) == 0;
}

// --- 전체 분석 ---

void Analyzer::analyze(const std::string& content, const std::string& uri) {
    labels_.clear();
    variables_.clear();
    diagnostics_.clear();
    jumpRefs_.clear();
    varRefs_.clear();

    scanSymbols(content);
    collectDiagnostics(content, uri);
}

// --- 심볼 스캔 (텍스트 기반, 빠름) ---

void Analyzer::scanSymbols(const std::string& content) {
    labels_.clear();
    variables_.clear();
    jumpRefs_.clear();
    varRefs_.clear();

    std::istringstream stream(content);
    std::string line;
    int lineNum = 0;
    bool seenFirstLabel = false;

    while (std::getline(stream, line)) {
        scanLine(line, lineNum, seenFirstLabel);
        lineNum++;
    }
}

void Analyzer::scanLine(const std::string& line, int lineNum, bool& seenFirstLabel) {
    std::string trimmed = trim(line);

    // 빈 줄이나 주석 건너뛰기
    if (trimmed.empty() || trimmed[0] == '#') return;

    // import 문 건너뛰기 (분석에서는 추출만)
    if (startsWith(trimmed, "import ")) return;

    // label 선언: "label name:" 또는 "label name(a, b):"
    if (startsWith(trimmed, "label ")) {
        std::string rest = trimmed.substr(6);
        LabelInfo info;
        info.line = lineNum;

        // 매개변수가 있는 경우: label name(a, b):
        size_t parenPos = rest.find('(');
        size_t colonPos = rest.find(':');

        if (parenPos != std::string::npos && (colonPos == std::string::npos || parenPos < colonPos)) {
            info.name = trim(rest.substr(0, parenPos));
            size_t closePos = rest.find(')', parenPos);
            if (closePos != std::string::npos) {
                std::string paramStr = rest.substr(parenPos + 1, closePos - parenPos - 1);
                // 매개변수 파싱
                std::istringstream pstream(paramStr);
                std::string param;
                while (std::getline(pstream, param, ',')) {
                    std::string p = trim(param);
                    if (!p.empty()) {
                        info.params.push_back(p);
                    }
                }
            }
        } else if (colonPos != std::string::npos) {
            info.name = trim(rest.substr(0, colonPos));
        } else {
            info.name = trim(rest);
        }

        // 노드 태그 (#key, #key=value) 제거: 이름에서 '#' 이후 부분 삭제
        size_t hashPos = info.name.find('#');
        if (hashPos != std::string::npos) {
            info.name = trim(info.name.substr(0, hashPos));
        }

        if (!info.name.empty()) {
            labels_.push_back(info);
            seenFirstLabel = true;
        }
        return;
    }

    // 변수 설정: "$ var = ..."
    if (startsWith(trimmed, "$ ")) {
        std::string rest = trimmed.substr(2);
        size_t eqPos = rest.find('=');
        if (eqPos != std::string::npos) {
            VarInfo info;
            info.name = trim(rest.substr(0, eqPos));
            info.line = lineNum;
            info.isGlobal = !seenFirstLabel;

            // "call" 반환값 변수인 경우에도 변수로 등록
            if (!info.name.empty()) {
                // 중복 체크: 같은 이름이 이미 있으면 추가하지 않음 (첫 선언만 유지)
                bool found = false;
                for (const auto& v : variables_) {
                    if (v.name == info.name) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    variables_.push_back(info);
                }
            }
        }
        return;
    }

    // jump 참조: "jump nodename"
    if (startsWith(trimmed, "jump ")) {
        std::string target = trim(trimmed.substr(5));
        if (!target.empty()) {
            JumpRef ref;
            ref.targetName = target;
            ref.line = lineNum;
            // 원본 줄에서 타겟 이름 위치 찾기
            size_t pos = line.find("jump ");
            if (pos != std::string::npos) {
                pos += 5;
                // 공백 건너뛰기
                while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) pos++;
                ref.col = static_cast<int>(pos);
            } else {
                ref.col = 0;
            }
            jumpRefs_.push_back(ref);
        }
        return;
    }

    // call 참조: "call nodename" 또는 "call func(args)"
    if (startsWith(trimmed, "call ")) {
        std::string rest = trimmed.substr(5);
        // 괄호 전까지가 노드/함수 이름
        size_t parenPos = rest.find('(');
        std::string target;
        if (parenPos != std::string::npos) {
            target = trim(rest.substr(0, parenPos));
        } else {
            target = trim(rest);
        }
        if (!target.empty()) {
            JumpRef ref;
            ref.targetName = target;
            ref.line = lineNum;
            size_t pos = line.find("call ");
            if (pos != std::string::npos) {
                pos += 5;
                while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) pos++;
                ref.col = static_cast<int>(pos);
            } else {
                ref.col = 0;
            }
            jumpRefs_.push_back(ref);
        }
        return;
    }

    // 선택지의 -> 참조: "text" -> node 또는 weight -> node
    {
        size_t arrowPos = trimmed.find("->");
        if (arrowPos != std::string::npos) {
            std::string afterArrow = trim(trimmed.substr(arrowPos + 2));
            // "if" 조건 제거
            size_t ifPos = afterArrow.find(" if ");
            if (ifPos != std::string::npos) {
                afterArrow = trim(afterArrow.substr(0, ifPos));
            }
            if (!afterArrow.empty()) {
                JumpRef ref;
                ref.targetName = afterArrow;
                ref.line = lineNum;
                size_t pos = line.find("->");
                if (pos != std::string::npos) {
                    pos += 2;
                    while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) pos++;
                    ref.col = static_cast<int>(pos);
                } else {
                    ref.col = 0;
                }
                jumpRefs_.push_back(ref);
            }
        }
    }
}

// --- Parser 기반 진단 수집 ---

void Analyzer::collectDiagnostics(const std::string& content, const std::string& /*uri*/) {
    diagnostics_.clear();

    // temp 파일에 내용 기록
    std::string tempPath;
    {
        // 플랫폼 독립적 temp 파일 생성
        auto tempDir = std::filesystem::temp_directory_path();
        tempPath = (tempDir / "gyeol_lsp_temp.gyeol").string();
    }

    {
        std::ofstream ofs(tempPath);
        if (!ofs.is_open()) {
            DiagInfo diag;
            diag.line = 0;
            diag.col = 0;
            diag.message = "LSP: Failed to create temp file for diagnostics";
            diag.severity = 1;
            diagnostics_.push_back(diag);
            return;
        }
        ofs << content;
        ofs.close();
    }

    // Parser로 파싱
    Parser parser;
    parser.parse(tempPath);

    // 에러 수집
    if (parser.hasErrors()) {
        for (const auto& err : parser.getErrors()) {
            diagnostics_.push_back(parseErrorString(err));
        }
    }

    // temp 파일 정리
    std::filesystem::remove(tempPath);
}

// Parser 에러 문자열 파싱
// 포맷: "filename:lineNum: message"
DiagInfo Analyzer::parseErrorString(const std::string& errorStr) {
    DiagInfo diag;
    diag.line = 0;
    diag.col = 0;
    diag.severity = 1;  // Error

    // "filename:lineNum: message" 형식 파싱
    // 파일명에 ':'가 포함될 수 있으므로 (Windows 경로) 마지막 두 번째 ':' 찾기
    // 패턴: 숫자 앞의 ':'를 찾아 라인 번호 추출
    std::regex re(R"(.*?:(\d+):\s*(.*))");
    std::smatch match;

    if (std::regex_match(errorStr, match, re)) {
        try {
            int parsedLine = std::stoi(match[1].str());
            // Parser 라인 번호는 1-based, LSP는 0-based
            diag.line = (parsedLine > 0) ? parsedLine - 1 : 0;
        } catch (...) {
            diag.line = 0;
        }
        diag.message = match[2].str();
    } else {
        // 파싱 실패 시 원본 메시지 그대로 사용
        diag.message = errorStr;
    }

    return diag;
}

} // namespace Gyeol
