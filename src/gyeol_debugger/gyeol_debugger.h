#pragma once

#include "gyeol_runner.h"
#include <string>
#include <vector>
#include <cstdint>

namespace Gyeol {

class Debugger {
public:
    bool loadStory(const std::string& gybPath);
    void run();  // Main REPL loop

private:
    Runner runner_;
    std::vector<uint8_t> storyBuffer_;
    bool running_ = false;
    bool started_ = false;

    // Command handlers
    void cmdHelp();
    void cmdStep();
    void cmdContinue();
    void cmdBreak(const std::string& args);
    void cmdDelete(const std::string& args);
    void cmdBreakpoints();
    void cmdLocals();
    void cmdPrint(const std::string& varName);
    void cmdSet(const std::string& args);
    void cmdWhere();
    void cmdNodes();
    void cmdInfo(const std::string& args);
    void cmdChoose(const std::string& args);
    void cmdRestart();

    // Helpers
    void printStepResult(const StepResult& result);
    void printLocation();
    static std::string variantToString(const Variant& v);
    static std::string colorize(const std::string& text, const std::string& color);
};

} // namespace Gyeol
