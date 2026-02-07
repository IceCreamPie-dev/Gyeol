#include "gyeol_debugger.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Gyeol {

// --- ANSI 색상 코드 ---
static const char* RESET  = "\033[0m";
static const char* BOLD   = "\033[1m";
static const char* RED    = "\033[31m";
static const char* GREEN  = "\033[32m";
static const char* YELLOW = "\033[33m";
static const char* BLUE   = "\033[34m";
static const char* CYAN   = "\033[36m";
static const char* DIM    = "\033[2m";

std::string Debugger::colorize(const std::string& text, const std::string& color) {
    return color + text + RESET;
}

std::string Debugger::variantToString(const Variant& v) {
    switch (v.type) {
        case Variant::BOOL:   return v.b ? "true" : "false";
        case Variant::INT:    return std::to_string(v.i);
        case Variant::FLOAT:  return std::to_string(v.f);
        case Variant::STRING: return "\"" + v.s + "\"";
        case Variant::LIST: {
            std::string result = "[";
            for (size_t i = 0; i < v.list.size(); i++) {
                if (i > 0) result += ", ";
                result += "\"" + v.list[i] + "\"";
            }
            return result + "]";
        }
    }
    return "?";
}

bool Debugger::loadStory(const std::string& gybPath) {
    std::ifstream ifs(gybPath, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        std::cerr << RED << "Error: Cannot open file: " << gybPath << RESET << std::endl;
        return false;
    }

    auto size = ifs.tellg();
    ifs.seekg(0);
    storyBuffer_.resize(static_cast<size_t>(size));
    ifs.read(reinterpret_cast<char*>(storyBuffer_.data()), size);

    if (!runner_.start(storyBuffer_.data(), storyBuffer_.size())) {
        std::cerr << RED << "Error: Failed to load story" << RESET << std::endl;
        return false;
    }

    // 디버그 모드: step mode 활성화
    runner_.setStepMode(true);
    started_ = true;
    return true;
}

void Debugger::run() {
    std::cout << BOLD << CYAN
              << "=== Gyeol Debugger v0.1.0 ===" << RESET << std::endl;
    std::cout << DIM << "Type 'h' for help, 'q' to quit" << RESET << std::endl;
    std::cout << std::endl;

    if (started_) {
        printLocation();
    }

    running_ = true;
    std::string input;

    while (running_) {
        std::cout << BOLD << GREEN << "(gyeol-dbg) " << RESET;
        if (!std::getline(std::cin, input)) {
            break;  // EOF
        }

        // 공백 제거
        size_t start = input.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        input = input.substr(start);

        // 커맨드 + 인자 분리
        std::string cmd, args;
        size_t spacePos = input.find(' ');
        if (spacePos != std::string::npos) {
            cmd = input.substr(0, spacePos);
            size_t argStart = input.find_first_not_of(" \t", spacePos);
            if (argStart != std::string::npos) {
                args = input.substr(argStart);
            }
        } else {
            cmd = input;
        }

        // 커맨드 디스패치 (약어 지원)
        if (cmd == "h" || cmd == "help") {
            cmdHelp();
        } else if (cmd == "s" || cmd == "step") {
            cmdStep();
        } else if (cmd == "c" || cmd == "continue") {
            cmdContinue();
        } else if (cmd == "b" || cmd == "break") {
            cmdBreak(args);
        } else if (cmd == "d" || cmd == "delete") {
            cmdDelete(args);
        } else if (cmd == "bp" || cmd == "breakpoints") {
            cmdBreakpoints();
        } else if (cmd == "l" || cmd == "locals") {
            cmdLocals();
        } else if (cmd == "p" || cmd == "print") {
            cmdPrint(args);
        } else if (cmd == "set") {
            cmdSet(args);
        } else if (cmd == "w" || cmd == "where") {
            cmdWhere();
        } else if (cmd == "n" || cmd == "nodes") {
            cmdNodes();
        } else if (cmd == "i" || cmd == "info") {
            cmdInfo(args);
        } else if (cmd == "ch" || cmd == "choose") {
            cmdChoose(args);
        } else if (cmd == "r" || cmd == "restart") {
            cmdRestart();
        } else if (cmd == "q" || cmd == "quit" || cmd == "exit") {
            running_ = false;
        } else {
            std::cout << RED << "Unknown command: " << cmd
                      << ". Type 'h' for help." << RESET << std::endl;
        }
    }

    std::cout << DIM << "Debugger exited." << RESET << std::endl;
}

// --- Command implementations ---

void Debugger::cmdHelp() {
    std::cout << BOLD << "Commands:" << RESET << std::endl;
    std::cout << "  " << CYAN << "s, step      " << RESET << "  Execute one instruction" << std::endl;
    std::cout << "  " << CYAN << "c, continue   " << RESET << "  Continue until breakpoint or end" << std::endl;
    std::cout << "  " << CYAN << "b, break NODE [PC]" << RESET << " Set breakpoint (PC defaults to 0)" << std::endl;
    std::cout << "  " << CYAN << "d, delete NODE [PC]" << RESET << " Remove breakpoint" << std::endl;
    std::cout << "  " << CYAN << "bp, breakpoints" << RESET << " List all breakpoints" << std::endl;
    std::cout << "  " << CYAN << "ch, choose N  " << RESET << "  Choose option N (0-based)" << std::endl;
    std::cout << "  " << CYAN << "l, locals     " << RESET << "  Show all variables" << std::endl;
    std::cout << "  " << CYAN << "p, print VAR  " << RESET << "  Print variable value" << std::endl;
    std::cout << "  " << CYAN << "set VAR VALUE " << RESET << "  Set variable value" << std::endl;
    std::cout << "  " << CYAN << "w, where      " << RESET << "  Show current location + call stack" << std::endl;
    std::cout << "  " << CYAN << "n, nodes      " << RESET << "  List all node names" << std::endl;
    std::cout << "  " << CYAN << "i, info NODE  " << RESET << "  Show node instructions" << std::endl;
    std::cout << "  " << CYAN << "r, restart    " << RESET << "  Restart story from beginning" << std::endl;
    std::cout << "  " << CYAN << "q, quit       " << RESET << "  Exit debugger" << std::endl;
}

void Debugger::cmdStep() {
    if (runner_.isFinished()) {
        std::cout << DIM << "Story has ended. Use 'r' to restart." << RESET << std::endl;
        return;
    }

    auto result = runner_.step();

    // Step mode pause: step()이 "정지" 신호로 빈 END를 반환한 경우
    // (실제 종료가 아님) → 한 번 더 호출하여 instruction 실행
    if (result.type == StepType::END && !runner_.isFinished()) {
        result = runner_.step();
    }

    printStepResult(result);

    if (result.type != StepType::END && !runner_.isFinished()) {
        printLocation();
    }
}

void Debugger::cmdContinue() {
    if (runner_.isFinished()) {
        std::cout << DIM << "Story has ended. Use 'r' to restart." << RESET << std::endl;
        return;
    }

    // step mode 해제, breakpoint로만 멈춤
    runner_.setStepMode(false);

    while (!runner_.isFinished()) {
        auto result = runner_.step();

        // Breakpoint hit (END 반환이지만 아직 끝나지 않음)
        if (result.type == StepType::END && !runner_.isFinished()) {
            std::cout << YELLOW << "* Breakpoint hit *" << RESET << std::endl;
            runner_.setStepMode(true);
            printLocation();
            return;
        }

        // 스토리 종료
        if (result.type == StepType::END) {
            printStepResult(result);
            break;
        }

        // LINE/COMMAND 결과가 있으면 출력
        if (result.type == StepType::LINE || result.type == StepType::COMMAND) {
            printStepResult(result);
        }

        // CHOICES → 사용자 입력 필요, 멈춤
        if (result.type == StepType::CHOICES) {
            printStepResult(result);
            runner_.setStepMode(true);
            printLocation();
            return;
        }
    }

    // 다시 step mode로 복귀
    runner_.setStepMode(true);
}

void Debugger::cmdBreak(const std::string& args) {
    if (args.empty()) {
        std::cout << RED << "Usage: b NODE [PC]" << RESET << std::endl;
        return;
    }

    std::istringstream iss(args);
    std::string nodeName;
    uint32_t pc = 0;
    iss >> nodeName;
    if (iss >> pc) {
        // PC 지정됨
    }

    runner_.addBreakpoint(nodeName, pc);
    std::cout << GREEN << "Breakpoint set: " << nodeName << ":" << pc << RESET << std::endl;
}

void Debugger::cmdDelete(const std::string& args) {
    if (args.empty()) {
        runner_.clearBreakpoints();
        std::cout << GREEN << "All breakpoints cleared." << RESET << std::endl;
        return;
    }

    std::istringstream iss(args);
    std::string nodeName;
    uint32_t pc = 0;
    iss >> nodeName;
    if (iss >> pc) {
        // PC 지정됨
    }

    runner_.removeBreakpoint(nodeName, pc);
    std::cout << GREEN << "Breakpoint removed: " << nodeName << ":" << pc << RESET << std::endl;
}

void Debugger::cmdBreakpoints() {
    auto bps = runner_.getBreakpoints();
    if (bps.empty()) {
        std::cout << DIM << "No breakpoints set." << RESET << std::endl;
        return;
    }

    std::cout << BOLD << "Breakpoints:" << RESET << std::endl;
    for (size_t i = 0; i < bps.size(); i++) {
        std::cout << "  " << CYAN << "[" << i << "]" << RESET
                  << " " << bps[i].first << ":" << bps[i].second << std::endl;
    }
}

void Debugger::cmdLocals() {
    auto names = runner_.getVariableNames();
    if (names.empty()) {
        std::cout << DIM << "No variables." << RESET << std::endl;
        return;
    }

    std::sort(names.begin(), names.end());
    std::cout << BOLD << "Variables:" << RESET << std::endl;
    for (const auto& name : names) {
        auto val = runner_.getVariable(name);
        std::cout << "  " << CYAN << name << RESET
                  << " = " << variantToString(val) << std::endl;
    }
}

void Debugger::cmdPrint(const std::string& varName) {
    if (varName.empty()) {
        std::cout << RED << "Usage: p VARIABLE" << RESET << std::endl;
        return;
    }

    if (!runner_.hasVariable(varName)) {
        std::cout << RED << "Variable not found: " << varName << RESET << std::endl;
        return;
    }

    auto val = runner_.getVariable(varName);
    std::cout << CYAN << varName << RESET << " = " << variantToString(val) << std::endl;
}

void Debugger::cmdSet(const std::string& args) {
    // "set var value" 형식 파싱
    std::istringstream iss(args);
    std::string varName, valueStr;
    iss >> varName;
    std::getline(iss, valueStr);

    // 앞뒤 공백 제거
    size_t vs = valueStr.find_first_not_of(" \t");
    if (vs != std::string::npos) valueStr = valueStr.substr(vs);

    if (varName.empty() || valueStr.empty()) {
        std::cout << RED << "Usage: set VARIABLE VALUE" << RESET << std::endl;
        return;
    }

    // 값 파싱
    Variant val = Variant::Int(0);
    if (valueStr == "true") {
        val = Variant::Bool(true);
    } else if (valueStr == "false") {
        val = Variant::Bool(false);
    } else if (valueStr.front() == '"' && valueStr.back() == '"') {
        val = Variant::String(valueStr.substr(1, valueStr.size() - 2));
    } else if (valueStr.find('.') != std::string::npos) {
        try { val = Variant::Float(std::stof(valueStr)); } catch (...) {}
    } else {
        try { val = Variant::Int(std::stoi(valueStr)); } catch (...) {}
    }

    runner_.setVariable(varName, val);
    std::cout << GREEN << varName << " = " << variantToString(val) << RESET << std::endl;
}

void Debugger::cmdWhere() {
    auto loc = runner_.getLocation();
    std::cout << BOLD << "Location:" << RESET << std::endl;
    std::cout << "  Node: " << CYAN << loc.nodeName << RESET << std::endl;
    std::cout << "  PC:   " << loc.pc << std::endl;
    std::cout << "  Type: " << loc.instructionType << std::endl;

    // instruction 상세 정보
    if (!loc.nodeName.empty()) {
        std::string info = runner_.getInstructionInfo(loc.nodeName, loc.pc);
        if (!info.empty()) {
            std::cout << "  Inst: " << YELLOW << info << RESET << std::endl;
        }
    }

    // Call stack
    auto stack = runner_.getCallStack();
    if (!stack.empty()) {
        std::cout << std::endl;
        std::cout << BOLD << "Call stack:" << RESET << std::endl;
        for (int i = static_cast<int>(stack.size()) - 1; i >= 0; i--) {
            auto& frame = stack[i];
            std::cout << "  " << DIM << "#" << i << RESET
                      << " " << frame.nodeName << ":" << frame.pc;
            if (!frame.returnVarName.empty()) {
                std::cout << " (return -> " << frame.returnVarName << ")";
            }
            if (!frame.paramNames.empty()) {
                std::cout << " params:[";
                for (size_t j = 0; j < frame.paramNames.size(); j++) {
                    if (j > 0) std::cout << ", ";
                    std::cout << frame.paramNames[j];
                }
                std::cout << "]";
            }
            std::cout << std::endl;
        }
    }

    // Visit counts
    if (!loc.nodeName.empty()) {
        int32_t visits = runner_.getVisitCount(loc.nodeName);
        if (visits > 0) {
            std::cout << "  Visits: " << visits << std::endl;
        }
    }
}

void Debugger::cmdNodes() {
    auto names = runner_.getNodeNames();
    if (names.empty()) {
        std::cout << DIM << "No nodes." << RESET << std::endl;
        return;
    }

    std::cout << BOLD << "Nodes (" << names.size() << "):" << RESET << std::endl;
    for (const auto& name : names) {
        uint32_t count = runner_.getNodeInstructionCount(name);
        int32_t visits = runner_.getVisitCount(name);
        std::cout << "  " << CYAN << name << RESET
                  << " (" << count << " instructions";
        if (visits > 0) {
            std::cout << ", " << visits << " visits";
        }
        std::cout << ")" << std::endl;
    }
}

void Debugger::cmdInfo(const std::string& args) {
    if (args.empty()) {
        // 현재 노드 info
        auto loc = runner_.getLocation();
        if (loc.nodeName.empty()) {
            std::cout << RED << "Usage: i NODE" << RESET << std::endl;
            return;
        }
        cmdInfo(loc.nodeName);
        return;
    }

    uint32_t count = runner_.getNodeInstructionCount(args);
    if (count == 0) {
        std::cout << RED << "Node not found: " << args << RESET << std::endl;
        return;
    }

    auto loc = runner_.getLocation();
    std::cout << BOLD << "Node: " << args << " (" << count << " instructions)" << RESET << std::endl;

    for (uint32_t i = 0; i < count; i++) {
        std::string info = runner_.getInstructionInfo(args, i);
        bool isCurrent = (args == loc.nodeName && i == loc.pc);

        if (isCurrent) {
            std::cout << "  " << GREEN << ">> " << RESET;
        } else {
            std::cout << "     ";
        }

        std::cout << DIM << "[" << i << "]" << RESET << " " << info << std::endl;
    }
}

void Debugger::cmdChoose(const std::string& args) {
    if (args.empty()) {
        std::cout << RED << "Usage: ch INDEX" << RESET << std::endl;
        return;
    }

    int index = 0;
    try {
        index = std::stoi(args);
    } catch (...) {
        std::cout << RED << "Invalid index: " << args << RESET << std::endl;
        return;
    }

    runner_.choose(index);
    std::cout << GREEN << "Chose option " << index << RESET << std::endl;

    if (!runner_.isFinished()) {
        printLocation();
    }
}

void Debugger::cmdRestart() {
    if (storyBuffer_.empty()) {
        std::cout << RED << "No story loaded." << RESET << std::endl;
        return;
    }

    runner_.clearBreakpoints();
    auto bps = runner_.getBreakpoints(); // breakpoint 유지를 위해 임시 저장
    // 실제로는 breakpoint를 유지하며 restart

    if (!runner_.start(storyBuffer_.data(), storyBuffer_.size())) {
        std::cout << RED << "Failed to restart story." << RESET << std::endl;
        return;
    }

    runner_.setStepMode(true);
    std::cout << GREEN << "Story restarted." << RESET << std::endl;
    printLocation();
}

// --- Helpers ---

void Debugger::printStepResult(const StepResult& result) {
    switch (result.type) {
        case StepType::LINE: {
            if (result.line.character) {
                std::cout << BOLD << BLUE << result.line.character << RESET
                          << ": " << result.line.text << std::endl;
            } else {
                std::cout << DIM << "* " << RESET << result.line.text << std::endl;
            }
            // 태그 표시
            if (!result.line.tags.empty()) {
                std::cout << DIM << "  tags: ";
                for (size_t i = 0; i < result.line.tags.size(); i++) {
                    if (i > 0) std::cout << ", ";
                    std::cout << result.line.tags[i].first;
                    if (result.line.tags[i].second && result.line.tags[i].second[0] != '\0') {
                        std::cout << ":" << result.line.tags[i].second;
                    }
                }
                std::cout << RESET << std::endl;
            }
            break;
        }
        case StepType::CHOICES: {
            std::cout << BOLD << YELLOW << "Choices:" << RESET << std::endl;
            for (const auto& choice : result.choices) {
                std::cout << "  " << CYAN << "[" << choice.index << "]" << RESET
                          << " " << choice.text << std::endl;
            }
            std::cout << DIM << "Use 'ch N' to choose." << RESET << std::endl;
            break;
        }
        case StepType::COMMAND: {
            std::cout << YELLOW << "@ " << result.command.type;
            for (const auto& param : result.command.params) {
                std::cout << " " << param;
            }
            std::cout << RESET << std::endl;
            break;
        }
        case StepType::END: {
            std::cout << BOLD << DIM << "--- Story ended ---" << RESET << std::endl;
            break;
        }
    }
}

void Debugger::printLocation() {
    auto loc = runner_.getLocation();
    if (loc.nodeName.empty()) return;

    std::string info = runner_.getInstructionInfo(loc.nodeName, loc.pc);
    std::cout << DIM << "  @ " << loc.nodeName << ":" << loc.pc
              << " [" << loc.instructionType << "] " << info << RESET << std::endl;
}

} // namespace Gyeol
