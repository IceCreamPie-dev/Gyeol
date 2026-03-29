#pragma once
#include "gyeol_parser.h"
#include "gyeol_story.h"
#include "gyeol_runner.h"
#include <string>
#include <vector>
#include <fstream>
#include <cstdio>

namespace GyeolTest {

// .gyeol 스크립트 문자열 -> 컴파일 -> 런타임 버퍼 반환
inline std::vector<uint8_t> compileScript(const std::string& script) {
    // 임시 파일에 스크립트 저장
    std::string inPath = "test_tmp_in.gyeol";

    {
        std::ofstream ofs(inPath);
        ofs << script;
    }

    Gyeol::Parser parser;
    if (!parser.parse(inPath)) {
        std::remove(inPath.c_str());
        return {};
    }
    std::vector<uint8_t> buf = parser.compileToBuffer();

    std::remove(inPath.c_str());
    return buf;
}

// Runner를 스크립트로부터 바로 시작
inline bool startRunner(Gyeol::Runner& runner, const std::vector<uint8_t>& buf) {
    if (buf.empty()) return false;
    return runner.start(buf.data(), buf.size());
}

// 멀티 파일 컴파일: 여러 파일 작성 후 메인 파일 파싱 → 런타임 버퍼 반환
inline std::vector<uint8_t> compileMultiFileScript(
    const std::vector<std::pair<std::string, std::string>>& files,
    const std::string& mainFile)
{
    // 모든 파일 작성
    for (const auto& file : files) {
        std::ofstream ofs(file.first);
        ofs << file.second;
    }

    Gyeol::Parser parser;
    std::vector<uint8_t> result;

    if (parser.parse(mainFile)) {
        result = parser.compileToBuffer();
    }

    // 정리
    for (const auto& file : files) {
        std::remove(file.first.c_str());
    }
    return result;
}

} // namespace GyeolTest
