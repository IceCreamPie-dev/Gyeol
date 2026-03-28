#pragma once

#include "gyeol_generated.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace Gyeol {

class JsonIrReader {
public:
    static bool fromJson(const nlohmann::json& doc,
                         ICPDev::Gyeol::Schema::StoryT& outStory,
                         std::string* errorOut = nullptr);

    static bool fromJsonString(const std::string& jsonText,
                               ICPDev::Gyeol::Schema::StoryT& outStory,
                               std::string* errorOut = nullptr);

    static bool fromFile(const std::string& path,
                         ICPDev::Gyeol::Schema::StoryT& outStory,
                         std::string* errorOut = nullptr);

    static std::vector<uint8_t> compileToBuffer(const ICPDev::Gyeol::Schema::StoryT& story);
};

} // namespace Gyeol
