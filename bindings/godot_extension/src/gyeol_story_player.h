#ifndef GYEOL_STORY_PLAYER_H
#define GYEOL_STORY_PLAYER_H

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/array.hpp>

#include "gyeol_story.h"
#include "gyeol_runner.h"

namespace godot {

class StoryPlayer : public Node {
    GDCLASS(StoryPlayer, Node)

private:
    Gyeol::Story story_;
    Gyeol::Runner runner_;
    std::vector<uint8_t> buffer_; // Godot FileAccess로 읽은 버퍼

protected:
    static void _bind_methods();

public:
    StoryPlayer();
    ~StoryPlayer();

    bool load_story(const String &path);
    void start();
    void advance();
    void choose(int index);
    bool is_finished() const;
};

} // namespace godot

#endif
