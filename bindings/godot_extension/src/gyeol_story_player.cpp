#include "gyeol_story_player.h"

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

using namespace godot;

StoryPlayer::StoryPlayer() {
}

StoryPlayer::~StoryPlayer() {
}

void StoryPlayer::_bind_methods() {
    // Methods
    ClassDB::bind_method(D_METHOD("load_story", "path"), &StoryPlayer::load_story);
    ClassDB::bind_method(D_METHOD("start"), &StoryPlayer::start);
    ClassDB::bind_method(D_METHOD("advance"), &StoryPlayer::advance);
    ClassDB::bind_method(D_METHOD("choose", "index"), &StoryPlayer::choose);
    ClassDB::bind_method(D_METHOD("is_finished"), &StoryPlayer::is_finished);
    ClassDB::bind_method(D_METHOD("save_state", "path"), &StoryPlayer::save_state);
    ClassDB::bind_method(D_METHOD("load_state", "path"), &StoryPlayer::load_state);

    // Signals
    ADD_SIGNAL(MethodInfo("dialogue_line",
        PropertyInfo(Variant::STRING, "character"),
        PropertyInfo(Variant::STRING, "text")));

    ADD_SIGNAL(MethodInfo("choices_presented",
        PropertyInfo(Variant::ARRAY, "choices")));

    ADD_SIGNAL(MethodInfo("command_received",
        PropertyInfo(Variant::STRING, "type"),
        PropertyInfo(Variant::ARRAY, "params")));

    ADD_SIGNAL(MethodInfo("story_ended"));
}

bool StoryPlayer::load_story(const String &path) {
    Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);
    if (file.is_null()) {
        UtilityFunctions::printerr("[Gyeol] Failed to open: ", path);
        return false;
    }

    int64_t length = file->get_length();
    buffer_.resize(static_cast<size_t>(length));

    PackedByteArray data = file->get_buffer(length);
    std::memcpy(buffer_.data(), data.ptr(), static_cast<size_t>(length));

    UtilityFunctions::print("[Gyeol] Loaded: ", path, " (", length, " bytes)");
    return true;
}

void StoryPlayer::start() {
    if (buffer_.empty()) {
        UtilityFunctions::printerr("[Gyeol] No story loaded.");
        return;
    }

    if (!runner_.start(buffer_.data(), buffer_.size())) {
        UtilityFunctions::printerr("[Gyeol] Failed to start runner.");
        return;
    }

    UtilityFunctions::print("[Gyeol] Story started.");
}

void StoryPlayer::advance() {
    if (runner_.isFinished()) {
        emit_signal("story_ended");
        return;
    }

    Gyeol::StepResult result = runner_.step();

    switch (result.type) {
        case Gyeol::StepType::LINE: {
            String character = result.line.character ? String::utf8(result.line.character) : String("");
            String text = result.line.text ? String::utf8(result.line.text) : String("");
            emit_signal("dialogue_line", character, text);
            break;
        }

        case Gyeol::StepType::CHOICES: {
            Array choices;
            for (const auto &choice : result.choices) {
                choices.append(String::utf8(choice.text ? choice.text : ""));
            }
            emit_signal("choices_presented", choices);
            break;
        }

        case Gyeol::StepType::COMMAND: {
            String type = result.command.type ? String::utf8(result.command.type) : String("");
            Array params;
            for (const auto &param : result.command.params) {
                params.append(String::utf8(param ? param : ""));
            }
            emit_signal("command_received", type, params);
            break;
        }

        case Gyeol::StepType::END: {
            emit_signal("story_ended");
            break;
        }
    }
}

void StoryPlayer::choose(int index) {
    runner_.choose(index);
    advance();
}

bool StoryPlayer::is_finished() const {
    return runner_.isFinished();
}

bool StoryPlayer::save_state(const String &path) {
    if (!runner_.hasStory()) {
        UtilityFunctions::printerr("[Gyeol] No story loaded for saving.");
        return false;
    }

    // Godot 경로 → 시스템 경로 변환
    String global_path = path;
    if (path.begins_with("res://") || path.begins_with("user://")) {
        Ref<FileAccess> probe = FileAccess::open(path, FileAccess::WRITE);
        if (probe.is_null()) {
            UtilityFunctions::printerr("[Gyeol] Cannot open save path: ", path);
            return false;
        }
        global_path = probe->get_path_absolute();
        probe->close();
    }

    bool ok = runner_.saveState(global_path.utf8().get_data());
    if (ok) {
        UtilityFunctions::print("[Gyeol] State saved: ", path);
    } else {
        UtilityFunctions::printerr("[Gyeol] Failed to save state: ", path);
    }
    return ok;
}

bool StoryPlayer::load_state(const String &path) {
    if (!runner_.hasStory()) {
        UtilityFunctions::printerr("[Gyeol] No story loaded for restoring.");
        return false;
    }

    // Godot 경로 → 시스템 경로 변환
    String global_path = path;
    if (path.begins_with("res://") || path.begins_with("user://")) {
        if (!FileAccess::file_exists(path)) {
            UtilityFunctions::printerr("[Gyeol] Save file not found: ", path);
            return false;
        }
        Ref<FileAccess> probe = FileAccess::open(path, FileAccess::READ);
        if (probe.is_null()) {
            UtilityFunctions::printerr("[Gyeol] Cannot open save file: ", path);
            return false;
        }
        global_path = probe->get_path_absolute();
        probe->close();
    }

    bool ok = runner_.loadState(global_path.utf8().get_data());
    if (ok) {
        UtilityFunctions::print("[Gyeol] State loaded: ", path);
    } else {
        UtilityFunctions::printerr("[Gyeol] Failed to load state: ", path);
    }
    return ok;
}
