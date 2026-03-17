#pragma once

#include "gyeol_generated.h"
#include <nlohmann/json.hpp>
#include <string>

namespace Gyeol::GraphTools {

nlohmann::json buildGraphDoc(const ICPDev::Gyeol::Schema::StoryT& story);

bool exportGraphJson(const ICPDev::Gyeol::Schema::StoryT& story,
                     const std::string& outputPath,
                     std::string* errorOut = nullptr);

bool validateGraphPatchJson(const ICPDev::Gyeol::Schema::StoryT& story,
                            const nlohmann::json& patch,
                            std::string* errorOut = nullptr);

bool validateGraphPatchFile(const ICPDev::Gyeol::Schema::StoryT& story,
                            const std::string& patchPath,
                            std::string* errorOut = nullptr);

bool applyGraphPatchJson(ICPDev::Gyeol::Schema::StoryT& story,
                         const nlohmann::json& patch,
                         std::string* errorOut = nullptr);

bool applyGraphPatchJsonWithOptions(ICPDev::Gyeol::Schema::StoryT& story,
                                    const nlohmann::json& patch,
                                    bool preserveLineIds,
                                    nlohmann::json* outLineIdMap,
                                    std::string* errorOut = nullptr);

bool applyGraphPatchFile(ICPDev::Gyeol::Schema::StoryT& story,
                         const std::string& patchPath,
                         std::string* errorOut = nullptr);

bool applyGraphPatchFileWithOptions(ICPDev::Gyeol::Schema::StoryT& story,
                                    const std::string& patchPath,
                                    bool preserveLineIds,
                                    const std::string& lineIdMapPath,
                                    std::string* errorOut = nullptr);

std::string toCanonicalScript(const ICPDev::Gyeol::Schema::StoryT& story,
                              std::string* errorOut = nullptr);

bool writeCanonicalScript(const ICPDev::Gyeol::Schema::StoryT& story,
                          const std::string& outputPath,
                          std::string* errorOut = nullptr);

bool writeLineIdMap(const nlohmann::json& lineIdMap,
                    const std::string& outputPath,
                    std::string* errorOut = nullptr);

} // namespace Gyeol::GraphTools
