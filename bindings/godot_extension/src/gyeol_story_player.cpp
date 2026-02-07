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
    ClassDB::bind_method(D_METHOD("get_variable", "name"), &StoryPlayer::get_variable);
    ClassDB::bind_method(D_METHOD("set_variable", "name", "value"), &StoryPlayer::set_variable);
    ClassDB::bind_method(D_METHOD("has_variable", "name"), &StoryPlayer::has_variable);
    ClassDB::bind_method(D_METHOD("load_locale", "path"), &StoryPlayer::load_locale);
    ClassDB::bind_method(D_METHOD("clear_locale"), &StoryPlayer::clear_locale);
    ClassDB::bind_method(D_METHOD("get_locale"), &StoryPlayer::get_locale);
    ClassDB::bind_method(D_METHOD("get_visit_count", "node_name"), &StoryPlayer::get_visit_count);
    ClassDB::bind_method(D_METHOD("has_visited", "node_name"), &StoryPlayer::has_visited);
    ClassDB::bind_method(D_METHOD("get_variable_names"), &StoryPlayer::get_variable_names);
    ClassDB::bind_method(D_METHOD("set_seed", "seed"), &StoryPlayer::set_seed);

    // Signals
    ADD_SIGNAL(MethodInfo("dialogue_line",
        PropertyInfo(Variant::STRING, "character"),
        PropertyInfo(Variant::STRING, "text"),
        PropertyInfo(Variant::DICTIONARY, "tags")));

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
            Dictionary tags;
            for (const auto& tag : result.line.tags) {
                String key = tag.first ? String::utf8(tag.first) : String("");
                String value = tag.second ? String::utf8(tag.second) : String("");
                tags[key] = value;
            }
            emit_signal("dialogue_line", character, text, tags);
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

godot::Variant StoryPlayer::get_variable(const String &name) const {
    std::string n = name.utf8().get_data();
    if (!runner_.hasVariable(n)) {
        return godot::Variant(); // nil
    }
    Gyeol::Variant v = runner_.getVariable(n);
    switch (v.type) {
        case Gyeol::Variant::BOOL:   return godot::Variant(v.b);
        case Gyeol::Variant::INT:    return godot::Variant(v.i);
        case Gyeol::Variant::FLOAT:  return godot::Variant(static_cast<double>(v.f));
        case Gyeol::Variant::STRING: return godot::Variant(String::utf8(v.s.c_str()));
        case Gyeol::Variant::LIST: {
            Array arr;
            for (const auto& item : v.list) {
                arr.append(String::utf8(item.c_str()));
            }
            return godot::Variant(arr);
        }
    }
    return godot::Variant();
}

void StoryPlayer::set_variable(const String &name, const godot::Variant &value) {
    std::string n = name.utf8().get_data();
    switch (value.get_type()) {
        case godot::Variant::BOOL:
            runner_.setVariable(n, Gyeol::Variant::Bool(static_cast<bool>(value)));
            break;
        case godot::Variant::INT:
            runner_.setVariable(n, Gyeol::Variant::Int(static_cast<int32_t>(static_cast<int64_t>(value))));
            break;
        case godot::Variant::FLOAT:
            runner_.setVariable(n, Gyeol::Variant::Float(static_cast<float>(static_cast<double>(value))));
            break;
        case godot::Variant::STRING: {
            String s = value;
            runner_.setVariable(n, Gyeol::Variant::String(s.utf8().get_data()));
            break;
        }
        case godot::Variant::ARRAY: {
            Array arr = value;
            std::vector<std::string> items;
            for (int j = 0; j < arr.size(); ++j) {
                String s = arr[j];
                items.push_back(s.utf8().get_data());
            }
            runner_.setVariable(n, Gyeol::Variant::List(std::move(items)));
            break;
        }
        default:
            UtilityFunctions::printerr("[Gyeol] Unsupported variant type for set_variable");
            break;
    }
}

bool StoryPlayer::has_variable(const String &name) const {
    return runner_.hasVariable(name.utf8().get_data());
}

bool StoryPlayer::load_locale(const String &path) {
    if (!runner_.hasStory()) {
        UtilityFunctions::printerr("[Gyeol] No story loaded for locale.");
        return false;
    }

    // Godot 경로 → 시스템 경로 변환
    String global_path = path;
    if (path.begins_with("res://") || path.begins_with("user://")) {
        if (!FileAccess::file_exists(path)) {
            UtilityFunctions::printerr("[Gyeol] Locale file not found: ", path);
            return false;
        }
        Ref<FileAccess> probe = FileAccess::open(path, FileAccess::READ);
        if (probe.is_null()) {
            UtilityFunctions::printerr("[Gyeol] Cannot open locale file: ", path);
            return false;
        }
        global_path = probe->get_path_absolute();
        probe->close();
    }

    bool ok = runner_.loadLocale(global_path.utf8().get_data());
    if (ok) {
        UtilityFunctions::print("[Gyeol] Locale loaded: ", path);
    } else {
        UtilityFunctions::printerr("[Gyeol] Failed to load locale: ", path);
    }
    return ok;
}

void StoryPlayer::clear_locale() {
    runner_.clearLocale();
}

String StoryPlayer::get_locale() const {
    return String::utf8(runner_.getLocale().c_str());
}

int StoryPlayer::get_visit_count(const String &node_name) const {
    return runner_.getVisitCount(node_name.utf8().get_data());
}

bool StoryPlayer::has_visited(const String &node_name) const {
    return runner_.hasVisited(node_name.utf8().get_data());
}

PackedStringArray StoryPlayer::get_variable_names() const {
    PackedStringArray result;
    for (const auto &name : runner_.getVariableNames()) {
        result.append(String::utf8(name.c_str()));
    }
    return result;
}

void StoryPlayer::set_seed(int seed) {
    runner_.setSeed(static_cast<uint32_t>(seed));
}
