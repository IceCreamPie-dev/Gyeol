#pragma once

#include "gyeol_parser.h"
#include "gyeol_runner.h"

#include <nlohmann/json.hpp>

#include <string>

namespace RuntimeContract {

struct RunOptions {
    std::string engine = "core";
    bool includeSeedInState = false;
    bool includeLastErrorInState = false;
    bool includeLocaleInState = false;
    bool includeVisitsInState = false;
    bool includeMetricsInState = false;
};

bool loadJsonFile(const std::string& path, nlohmann::json& out, std::string* errorOut = nullptr);
bool loadTextFile(const std::string& path, std::string& out, std::string* errorOut = nullptr);
bool writeJsonFile(const std::string& path, const nlohmann::json& jsonData, std::string* errorOut = nullptr);

bool compileStoryToBuffer(const std::string& storyPath,
                          std::vector<uint8_t>& outBuffer,
                          std::string* errorOut = nullptr);

nlohmann::json runnerStateToJson(const Gyeol::Runner& runner, const RunOptions& options);
nlohmann::json stepResultToJson(const Gyeol::StepResult& result);

bool runCoreActions(const std::vector<uint8_t>& storyBuffer,
                    const nlohmann::json& actionsDoc,
                    const RunOptions& options,
                    nlohmann::json& outTranscript,
                    std::string* errorOut = nullptr);

bool runGodotAdapterActions(const std::vector<uint8_t>& storyBuffer,
                            const nlohmann::json& actionsDoc,
                            nlohmann::json& outTranscript,
                            std::string* errorOut = nullptr);

bool jsonEquals(const nlohmann::json& expected,
                const nlohmann::json& actual,
                std::string* errorOut = nullptr);

std::string resolveActionPath(const std::string& rawPath, const std::string& tempDir);

} // namespace RuntimeContract
