#pragma once

#include "gyeol_generated.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace Gyeol {

struct JsonIrDiagnostic {
    std::string code;
    std::string severity;
    std::string path;
    std::string node;
    int instruction_index = -1;
    std::string message;
    std::string hint;
};

class JsonIrTooling {
public:
    static bool writeInitTemplate(const std::string& outputPath,
                                  std::string* errorOut = nullptr);

    static bool formatFile(const std::string& inputPath,
                           const std::string& outputPath,
                           std::string* errorOut = nullptr);

    static bool lintFile(const std::string& storyPath,
                         std::vector<JsonIrDiagnostic>& outDiagnostics,
                         ICPDev::Gyeol::Schema::StoryT* outStory = nullptr,
                         std::string* errorOut = nullptr);

    static bool lintStory(const ICPDev::Gyeol::Schema::StoryT& story,
                          const std::string& storyPath,
                          std::vector<JsonIrDiagnostic>& outDiagnostics);

    static bool hasErrors(const std::vector<JsonIrDiagnostic>& diagnostics);

    static nlohmann::json diagnosticsToJson(const std::vector<JsonIrDiagnostic>& diagnostics);
    static std::string diagnosticsToText(const std::vector<JsonIrDiagnostic>& diagnostics);

    static bool loadJsonFile(const std::string& path,
                             nlohmann::json& outJson,
                             std::string* errorOut = nullptr);

    static bool previewGraphPatch(const ICPDev::Gyeol::Schema::StoryT& story,
                                  const std::string& storyPath,
                                  const nlohmann::json& patch,
                                  nlohmann::json& outPreview,
                                  std::vector<JsonIrDiagnostic>* outDiagnostics = nullptr,
                                  std::string* errorOut = nullptr);
};

} // namespace Gyeol
