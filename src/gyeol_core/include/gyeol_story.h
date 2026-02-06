#pragma once
#include <string>
#include <vector>
#include <cstdint>

namespace Gyeol {
    class Story {
    public:
        void printVersion();

        // .gyb 파일을 로드하고 검증한다. 성공 시 true 반환.
        bool loadFromFile(const std::string& filepath);

        // 로드된 스토리 데이터를 콘솔에 출력한다.
        void printStory() const;

        // Runner가 버퍼에 접근하기 위한 접근자
        const uint8_t* getBuffer() const { return buffer_.data(); }
        size_t getBufferSize() const { return buffer_.size(); }

    private:
        std::vector<uint8_t> buffer_;
    };
}
