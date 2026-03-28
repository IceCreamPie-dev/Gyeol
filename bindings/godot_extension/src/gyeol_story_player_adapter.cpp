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
        for (const auto& arg : result.command.args) {
            CommandArgEvent out;
            switch (arg.type) {
            case Gyeol::CommandArgType::STRING:
                out.kind = "String";
                out.stringValue = arg.text;
                break;
            case Gyeol::CommandArgType::IDENTIFIER:
                out.kind = "Identifier";
                out.stringValue = arg.text;
                break;
            case Gyeol::CommandArgType::INT:
                out.kind = "Int";
                out.intValue = arg.intValue;
                break;
            case Gyeol::CommandArgType::FLOAT:
                out.kind = "Float";
                out.floatValue = arg.floatValue;
                break;
            case Gyeol::CommandArgType::BOOL:
                out.kind = "Bool";
                out.boolValue = arg.boolValue;
                break;
            }
            event.commandArgs.push_back(std::move(out));
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
