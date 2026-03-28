#pragma once

#include "gyeol_runner.h"

#include <string>
#include <utility>
#include <vector>

namespace GyeolGodotAdapter {

enum class SignalType {
    DialogueLine,
    ChoicesPresented,
    CommandReceived,
    WaitRequested,
    YieldEmitted,
    StoryEnded,
};

struct CommandArgEvent {
    std::string kind;
    std::string stringValue;
    int32_t intValue = 0;
    float floatValue = 0.0f;
    bool boolValue = false;
};

struct SignalEvent {
    SignalType type = SignalType::StoryEnded;
    std::string character;
    std::string text;
    std::vector<std::pair<std::string, std::string>> tags;
    std::vector<std::string> choices;
    std::string commandType;
    std::vector<CommandArgEvent> commandArgs;
    std::string waitTag;
};

SignalEvent toSignalEvent(const Gyeol::StepResult& result);

} // namespace GyeolGodotAdapter
