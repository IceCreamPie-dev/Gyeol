#include "gyeol_story.h"
#include "gyeol_runner.h"
#include <iostream>

int main(int argc, char* argv[]) {
    // .gyb 파일 로드
    std::string filepath = (argc > 1) ? argv[1] : "story.gyb";

    Gyeol::Story story;
    if (!story.loadFromFile(filepath)) {
        return 1;
    }

    // Runner 시작
    Gyeol::Runner runner;
    if (!runner.start(story.getBuffer(), story.getBufferSize())) {
        std::cerr << "Failed to start runner." << std::endl;
        return 1;
    }

    std::cout << "\n=== Gyeol Story Player ===" << std::endl;
    std::cout << std::endl;

    // 콘솔 플레이 루프
    while (!runner.isFinished()) {
        auto result = runner.step();

        switch (result.type) {
            case Gyeol::StepType::LINE: {
                if (result.line.character) {
                    std::cout << result.line.character << ": "
                              << result.line.text << std::endl;
                } else {
                    std::cout << result.line.text << std::endl;
                }
                std::cout << std::endl;
                break;
            }

            case Gyeol::StepType::CHOICES: {
                for (const auto& choice : result.choices) {
                    std::cout << "  [" << (choice.index + 1) << "] "
                              << choice.text << std::endl;
                }
                std::cout << std::endl;

                // 사용자 입력
                int input = 0;
                while (true) {
                    std::cout << "> ";
                    std::cin >> input;
                    if (input >= 1 && input <= static_cast<int>(result.choices.size())) {
                        break;
                    }
                    std::cout << "1~" << result.choices.size()
                              << " 사이의 번호를 입력하세요." << std::endl;
                }
                std::cout << std::endl;
                runner.choose(input - 1);
                break;
            }

            case Gyeol::StepType::COMMAND: {
                std::cout << "[CMD] " << result.command.type << "(";
                for (size_t i = 0; i < result.command.params.size(); ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << result.command.params[i];
                }
                std::cout << ")" << std::endl;
                std::cout << std::endl;
                break;
            }

            case Gyeol::StepType::END:
                break;
        }
    }

    std::cout << "=== END ===" << std::endl;
    return 0;
}
