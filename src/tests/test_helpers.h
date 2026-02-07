#pragma once
#include "gyeol_parser.h"
#include "gyeol_story.h"
#include "gyeol_runner.h"
#include <string>
#include <vector>
#include <fstream>
#include <cstdio>

namespace GyeolTest {

// .gyeol 스크립트 문자열 -> 컴파일 -> .gyb 바이너리 벡터 반환
inline std::vector<uint8_t> compileScript(const std::string& script) {
    // 임시 파일에 스크립트 저장
    std::string inPath = "test_tmp_in.gyeol";
    std::string outPath = "test_tmp_out.gyb";

    {
        std::ofstream ofs(inPath);
        ofs << script;
    }

    Gyeol::Parser parser;
    if (!parser.parse(inPath)) {
        std::remove(inPath.c_str());
        return {};
    }
    if (!parser.compile(outPath)) {
        std::remove(inPath.c_str());
        return {};
    }

    // .gyb 읽기
    std::ifstream ifs(outPath, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        std::remove(inPath.c_str());
        std::remove(outPath.c_str());
        return {};
    }
    auto size = ifs.tellg();
    ifs.seekg(0);
    std::vector<uint8_t> buf(static_cast<size_t>(size));
    ifs.read(reinterpret_cast<char*>(buf.data()), size);

    std::remove(inPath.c_str());
    std::remove(outPath.c_str());
    return buf;
}

// Runner를 스크립트로부터 바로 시작
inline bool startRunner(Gyeol::Runner& runner, const std::vector<uint8_t>& buf) {
    if (buf.empty()) return false;
    return runner.start(buf.data(), buf.size());
}

// 멀티 파일 컴파일: 여러 파일 작성 후 메인 파일 파싱 → .gyb 반환
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
    std::string outPath = "test_tmp_multi_out.gyb";
    std::vector<uint8_t> result;

    if (parser.parse(mainFile) && parser.compile(outPath)) {
        std::ifstream ifs(outPath, std::ios::binary | std::ios::ate);
        if (ifs.is_open()) {
            auto size = ifs.tellg();
            ifs.seekg(0);
            result.resize(static_cast<size_t>(size));
            ifs.read(reinterpret_cast<char*>(result.data()), size);
        }
    }

    // 정리
    for (const auto& file : files) {
        std::remove(file.first.c_str());
    }
    std::remove(outPath.c_str());
    return result;
}

} // namespace GyeolTest
