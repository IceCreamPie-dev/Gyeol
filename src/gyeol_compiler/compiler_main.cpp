#include "gyeol_generated.h" // FlatBuffers Object API
#include <iostream>
#include <fstream>
#include <vector>

using namespace ICPDev::Gyeol::Schema;

int main(int argc, char* argv[]) {
    // 1. FlatBufferBuilder 생성
    flatbuffers::FlatBufferBuilder builder;

    // 2. Object API(T)를 사용하여 스토리 데이터 생성 (C++ 객체처럼 다룸)
    StoryT story;
    story.version = "0.1.0";
    story.start_node_name = "start";

    // --- String Pool 채우기 (0: hero, 1: 안녕?, 2: start, 3: next) ---
    story.string_pool = {"hero", "안녕? 나는 결(Gyeol)이야.", "start", "next"};

    // --- 노드 생성: "start" ---
    std::unique_ptr<NodeT> startNode = std::make_unique<NodeT>();
    startNode->name = "start";

    // 대사 (Line) 추가
    // hero(idx:0)가 "안녕..."(idx:1)이라고 말함
    std::unique_ptr<LineT> line = std::make_unique<LineT>();
    line->character_id = 0; 
    line->text_id = 1;

    // 명령(Instruction)으로 포장
    std::unique_ptr<InstructionT> instr = std::make_unique<InstructionT>();
    instr->data.Set(*line); // Union 설정

    startNode->lines.push_back(std::move(instr));
    story.nodes.push_back(std::move(startNode));

    // 3. 직렬화 (Pack)
    // Object API 객체(story)를 바이너리(FlatBuffer)로 변환
    auto rootOffset = Story::Pack(builder, &story);
    builder.Finish(rootOffset);

    // 4. 파일로 저장 (output.gyb)
    std::string filename = "story.gyb";
    std::ofstream ofs(filename, std::ios::binary);
    ofs.write((const char*)builder.GetBufferPointer(), builder.GetSize());
    ofs.close();

    std::cout << "Successfully compiled to " << filename << " (" << builder.GetSize() << " bytes)" << std::endl;

    return 0;
}
