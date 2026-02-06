#include <gtest/gtest.h>
#include "test_helpers.h"
#include "gyeol_story.h"
#include <fstream>
#include <cstdio>

using namespace Gyeol;

TEST(StoryTest, LoadValidFile) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    hero \"hello\"\n"
    );
    ASSERT_FALSE(buf.empty());

    // 임시 .gyb 파일 저장
    std::string path = "test_story_load.gyb";
    {
        std::ofstream ofs(path, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(buf.data()), buf.size());
    }

    Story story;
    EXPECT_TRUE(story.loadFromFile(path));
    EXPECT_NE(story.getBuffer(), nullptr);
    EXPECT_GT(story.getBufferSize(), 0u);

    std::remove(path.c_str());
}

TEST(StoryTest, LoadNonexistentFile) {
    Story story;
    EXPECT_FALSE(story.loadFromFile("does_not_exist.gyb"));
}

TEST(StoryTest, LoadInvalidFile) {
    std::string path = "test_invalid.gyb";
    {
        std::ofstream ofs(path, std::ios::binary);
        ofs << "this is not a valid gyb file";
    }

    Story story;
    EXPECT_FALSE(story.loadFromFile(path));

    std::remove(path.c_str());
}

TEST(StoryTest, BufferAccessAfterLoad) {
    auto buf = GyeolTest::compileScript(
        "label start:\n"
        "    \"test\"\n"
    );
    ASSERT_FALSE(buf.empty());

    std::string path = "test_buf_access.gyb";
    {
        std::ofstream ofs(path, std::ios::binary);
        ofs.write(reinterpret_cast<const char*>(buf.data()), buf.size());
    }

    Story story;
    ASSERT_TRUE(story.loadFromFile(path));

    // Runner와 연동 가능한지 확인
    Runner runner;
    EXPECT_TRUE(runner.start(story.getBuffer(), story.getBufferSize()));

    std::remove(path.c_str());
}
