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

struct SignalEvent {
    SignalType type = SignalType::StoryEnded;
    std::string character;
    std::string text;
    std::vector<std::pair<std::string, std::string>> tags;
    std::vector<std::string> choices;
    std::string commandType;
    std::vector<std::string> commandParams;
    std::string waitTag;
};

SignalEvent toSignalEvent(const Gyeol::StepResult& result);

} // namespace GyeolGodotAdapter
