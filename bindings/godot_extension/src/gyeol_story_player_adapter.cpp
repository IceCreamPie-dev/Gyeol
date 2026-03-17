#include "gyeol_story_player_adapter.h"

namespace GyeolGodotAdapter {

namespace {

std::string safeStr(const char* text) {
    return text ? std::string(text) : std::string();
}

} // namespace

SignalEvent toSignalEvent(const Gyeol::StepResult& result) {
    SignalEvent event;

    switch (result.type) {
    case Gyeol::StepType::LINE:
        event.type = SignalType::DialogueLine;
        event.character = safeStr(result.line.character);
        event.text = safeStr(result.line.text);
        for (const auto& tag : result.line.tags) {
            event.tags.emplace_back(safeStr(tag.first), safeStr(tag.second));
        }
        break;

    case Gyeol::StepType::CHOICES:
        event.type = SignalType::ChoicesPresented;
        for (const auto& choice : result.choices) {
            event.choices.push_back(safeStr(choice.text));
        }
        break;

    case Gyeol::StepType::COMMAND:
        event.type = SignalType::CommandReceived;
        event.commandType = safeStr(result.command.type);
        for (const auto* param : result.command.params) {
            event.commandParams.push_back(safeStr(param));
        }
        break;

    case Gyeol::StepType::WAIT:
        event.type = SignalType::WaitRequested;
        event.waitTag = safeStr(result.wait.tag);
        break;

    case Gyeol::StepType::YIELD:
        event.type = SignalType::YieldEmitted;
        break;

    case Gyeol::StepType::END:
        event.type = SignalType::StoryEnded;
        break;
    }

    return event;
}

} // namespace GyeolGodotAdapter
