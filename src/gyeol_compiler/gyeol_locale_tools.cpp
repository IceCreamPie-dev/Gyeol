#include "gyeol_parser.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_set>
#include <unordered_map>

using namespace ICPDev::Gyeol::Schema;
using json = nlohmann::json;

namespace Gyeol {

namespace {

std::string escapePoString(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string unescapePoString(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c != '\\' || i + 1 >= s.size()) {
            out.push_back(c);
            continue;
        }

        char e = s[++i];
        switch (e) {
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case '\\': out.push_back('\\'); break;
            case '"': out.push_back('"'); break;
            default: out.push_back(e); break;
        }
    }
    return out;
}

bool extractPoQuoted(const std::string& line, std::string& out) {
    size_t first = line.find('"');
    if (first == std::string::npos) return false;
    size_t last = line.find_last_of('"');
    if (last == std::string::npos || last <= first) return false;
    out = unescapePoString(line.substr(first + 1, last - first - 1));
    return true;
}

struct PoEntry {
    bool fuzzy = false;
    std::string msgctxt;
    std::string msgid;
    std::string msgstr;
};

enum class PoField {
    NONE,
    MSGCTXT,
    MSGID,
    MSGSTR
};

bool loadPoEntries(const std::string& poPath,
                   std::unordered_map<std::string, std::string>& outEntries,
                   std::string* errorOut) {
    std::ifstream ifs(poPath);
    if (!ifs.is_open()) {
        if (errorOut) *errorOut = "Failed to open PO file: " + poPath;
        return false;
    }

    PoEntry entry;
    PoField field = PoField::NONE;

    auto flushEntry = [&]() {
        if (!entry.fuzzy && !entry.msgctxt.empty() && !entry.msgstr.empty()) {
            outEntries[entry.msgctxt] = entry.msgstr;
        }
        entry = {};
        field = PoField::NONE;
    };

    std::string line;
    while (std::getline(ifs, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) {
            flushEntry();
            continue;
        }

        if (line.rfind("#,", 0) == 0) {
            if (line.find("fuzzy") != std::string::npos) {
                entry.fuzzy = true;
            }
            continue;
        }
        if (line[0] == '#') {
            continue;
        }

        std::string value;
        if (line.rfind("msgctxt", 0) == 0) {
            if (!extractPoQuoted(line, value)) {
                if (errorOut) *errorOut = "Invalid msgctxt in PO: " + line;
                return false;
            }
            entry.msgctxt = value;
            field = PoField::MSGCTXT;
            continue;
        }

        if (line.rfind("msgid", 0) == 0) {
            if (!extractPoQuoted(line, value)) {
                if (errorOut) *errorOut = "Invalid msgid in PO: " + line;
                return false;
            }
            entry.msgid = value;
            field = PoField::MSGID;
            continue;
        }

        if (line.rfind("msgstr", 0) == 0) {
            if (!extractPoQuoted(line, value)) {
                if (errorOut) *errorOut = "Invalid msgstr in PO: " + line;
                return false;
            }
            entry.msgstr = value;
            field = PoField::MSGSTR;
            continue;
        }

        if (!line.empty() && line[0] == '"') {
            if (!extractPoQuoted(line, value)) {
                if (errorOut) *errorOut = "Invalid multiline PO string: " + line;
                return false;
            }
            switch (field) {
                case PoField::MSGCTXT: entry.msgctxt += value; break;
                case PoField::MSGID: entry.msgid += value; break;
                case PoField::MSGSTR: entry.msgstr += value; break;
                default: break;
            }
            continue;
        }
    }

    flushEntry();
    return true;
}

std::string poolStr(const StoryT& story, int32_t index) {
    if (index < 0 || static_cast<size_t>(index) >= story.string_pool.size()) return "";
    return story.string_pool[static_cast<size_t>(index)];
}

std::string makeCharacterContext(const std::string& characterId, const std::string& propertyKey) {
    return "char:" + characterId + ":" + propertyKey;
}

bool parseCharacterContext(const std::string& context,
                           std::string& outCharacterId,
                           std::string& outPropertyKey) {
    static const std::string prefix = "char:";
    if (context.rfind(prefix, 0) != 0) return false;
    size_t sep = context.find(':', prefix.size());
    if (sep == std::string::npos || sep <= prefix.size() || sep + 1 >= context.size()) return false;
    outCharacterId = context.substr(prefix.size(), sep - prefix.size());
    outPropertyKey = context.substr(sep + 1);
    return !outCharacterId.empty() && !outPropertyKey.empty();
}

void collectValidLineIds(const StoryT& story, std::unordered_set<std::string>& outIds) {
    for (const auto& lineId : story.line_ids) {
        if (!lineId.empty()) outIds.insert(lineId);
    }
}

void collectValidCharacterPropertyKeys(
    const StoryT& story,
    std::unordered_map<std::string, std::unordered_set<std::string>>& outKeys) {
    for (const auto& charDef : story.characters) {
        if (!charDef) continue;
        const std::string characterId = poolStr(story, charDef->name_id);
        if (characterId.empty()) continue;
        auto& keySet = outKeys[characterId];
        for (const auto& prop : charDef->properties) {
            if (!prop) continue;
            const std::string key = poolStr(story, prop->key_id);
            if (!key.empty()) keySet.insert(key);
        }
    }
}

struct ParsedLocaleDoc {
    std::string locale;
    std::unordered_map<std::string, std::string> lineEntries;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> characterEntries;
};

bool parseLocaleV1(const json& j, const std::string& fallbackLocale, ParsedLocaleDoc& out, std::string* errorOut) {
    if (!j.contains("version") || !j["version"].is_number_integer() || j["version"].get<int>() != 1) {
        if (errorOut) *errorOut = "Locale v1 requires version=1.";
        return false;
    }
    if (!j.contains("entries") || !j["entries"].is_object()) {
        if (errorOut) *errorOut = "Locale v1 requires object field 'entries'.";
        return false;
    }
    out.locale = (j.contains("locale") && j["locale"].is_string()) ? j["locale"].get<std::string>() : fallbackLocale;
    for (auto it = j["entries"].begin(); it != j["entries"].end(); ++it) {
        if (!it.value().is_string()) {
            if (errorOut) *errorOut = "Locale v1 entries must map to strings.";
            return false;
        }
        out.lineEntries[it.key()] = it.value().get<std::string>();
    }
    return true;
}

bool parseLocaleV2(const json& j, const std::string& fallbackLocale, ParsedLocaleDoc& out, std::string* errorOut) {
    if (j.value("format", "") != "gyeol-locale") {
        if (errorOut) *errorOut = "Locale v2 requires format='gyeol-locale'.";
        return false;
    }
    if (!j.contains("version") || !j["version"].is_number_integer() || j["version"].get<int>() != 2) {
        if (errorOut) *errorOut = "Locale v2 requires version=2.";
        return false;
    }
    out.locale = (j.contains("locale") && j["locale"].is_string()) ? j["locale"].get<std::string>() : fallbackLocale;

    if (j.contains("line_entries")) {
        if (!j["line_entries"].is_object()) {
            if (errorOut) *errorOut = "Locale v2 line_entries must be an object.";
            return false;
        }
        for (auto it = j["line_entries"].begin(); it != j["line_entries"].end(); ++it) {
            if (!it.value().is_string()) {
                if (errorOut) *errorOut = "Locale v2 line_entries values must be strings.";
                return false;
            }
            out.lineEntries[it.key()] = it.value().get<std::string>();
        }
    }

    if (j.contains("character_entries")) {
        if (!j["character_entries"].is_object()) {
            if (errorOut) *errorOut = "Locale v2 character_entries must be an object.";
            return false;
        }
        for (auto charIt = j["character_entries"].begin(); charIt != j["character_entries"].end(); ++charIt) {
            if (!charIt.value().is_object()) {
                if (errorOut) *errorOut = "Locale v2 character_entries.<character> must be an object.";
                return false;
            }
            auto& dst = out.characterEntries[charIt.key()];
            for (auto propIt = charIt.value().begin(); propIt != charIt.value().end(); ++propIt) {
                if (!propIt.value().is_string()) {
                    if (errorOut) *errorOut = "Locale v2 character_entries values must be strings.";
                    return false;
                }
                dst[propIt.key()] = propIt.value().get<std::string>();
            }
        }
    }

    return true;
}

bool parseLocaleFile(const std::string& localePath, ParsedLocaleDoc& out, std::string* errorOut) {
    std::ifstream ifs(localePath);
    if (!ifs.is_open()) {
        if (errorOut) *errorOut = "Failed to open locale file: " + localePath;
        return false;
    }

    json j;
    try {
        ifs >> j;
    } catch (const std::exception& e) {
        if (errorOut) *errorOut = std::string("Invalid locale JSON: ") + e.what();
        return false;
    }

    const std::string fallbackLocale = std::filesystem::path(localePath).stem().string();
    if (j.contains("format")) {
        return parseLocaleV2(j, fallbackLocale, out, errorOut);
    }
    return parseLocaleV1(j, fallbackLocale, out, errorOut);
}

bool writePoHeader(std::ofstream& ofs, const std::string& outputPath, std::string* errorOut) {
    if (!ofs.is_open()) {
        if (errorOut) *errorOut = "Failed to write: " + outputPath;
        return false;
    }
    ofs << "msgid \"\"\n";
    ofs << "msgstr \"\"\n";
    ofs << "\"Content-Type: text/plain; charset=UTF-8\\n\"\n";
    ofs << "\"Content-Transfer-Encoding: 8bit\\n\"\n\n";
    return true;
}

void writePoEntry(std::ofstream& ofs,
                  const std::string& comment,
                  const std::string& context,
                  const std::string& sourceText) {
    if (!comment.empty()) {
        ofs << "#. " << comment << "\n";
    }
    ofs << "msgctxt \"" << escapePoString(context) << "\"\n";
    ofs << "msgid \"" << escapePoString(sourceText) << "\"\n";
    ofs << "msgstr \"\"\n\n";
}

bool exportStringsPOImpl(const StoryT& story, const std::string& outputPath, std::string* errorOut) {
    std::ofstream ofs(outputPath);
    if (!writePoHeader(ofs, outputPath, errorOut)) return false;

    std::unordered_set<std::string> emittedContexts;

    for (const auto& node : story.nodes) {
        if (!node) continue;
        for (const auto& instr : node->lines) {
            if (!instr) continue;
            int32_t idx = -1;
            std::string commentType;
            std::string characterId;

            if (instr->data.type == OpData::Line) {
                auto* line = instr->data.AsLine();
                if (!line) continue;
                idx = line->text_id;
                commentType = "type: LINE node: " + node->name;
                if (line->character_id >= 0) {
                    characterId = poolStr(story, line->character_id);
                    if (!characterId.empty()) {
                        commentType += " character: " + characterId;
                    }
                }
            } else if (instr->data.type == OpData::Choice) {
                auto* choice = instr->data.AsChoice();
                if (!choice) continue;
                idx = choice->text_id;
                commentType = "type: CHOICE node: " + node->name;
            } else {
                continue;
            }

            if (idx < 0 || static_cast<size_t>(idx) >= story.line_ids.size() ||
                static_cast<size_t>(idx) >= story.string_pool.size()) {
                continue;
            }

            const std::string& lineId = story.line_ids[static_cast<size_t>(idx)];
            if (lineId.empty() || emittedContexts.count(lineId) > 0) continue;
            emittedContexts.insert(lineId);
            writePoEntry(ofs, commentType, lineId, story.string_pool[static_cast<size_t>(idx)]);
        }
    }

    for (const auto& charDef : story.characters) {
        if (!charDef) continue;
        const std::string characterId = poolStr(story, charDef->name_id);
        if (characterId.empty()) continue;
        for (const auto& prop : charDef->properties) {
            if (!prop) continue;
            const std::string key = poolStr(story, prop->key_id);
            const std::string value = poolStr(story, prop->value_id);
            if (key.empty()) continue;
            const std::string ctx = makeCharacterContext(characterId, key);
            if (emittedContexts.count(ctx) > 0) continue;
            emittedContexts.insert(ctx);
            writePoEntry(
                ofs,
                "type: CHARACTER_PROPERTY character: " + characterId + " key: " + key,
                ctx,
                value);
        }
    }

    return true;
}

} // namespace

bool Parser::exportStringsPO(const std::string& outputPath) const {
    StoryT storyCopy = story_;
    storyCopy.line_ids = lineIds_;
    std::string error;
    if (!exportStringsPOImpl(storyCopy, outputPath, &error)) {
        std::cerr << error << std::endl;
        return false;
    }
    std::cout << "Exported: " << outputPath << std::endl;
    return true;
}

namespace LocaleTools {

bool convertPoToJson(const std::string& poPath,
                     const std::string& outputPath,
                     const std::string& localeHint,
                     std::string* errorOut) {
    std::unordered_map<std::string, std::string> entries;
    if (!loadPoEntries(poPath, entries, errorOut)) {
        return false;
    }

    std::string locale = localeHint;
    if (locale.empty()) {
        locale = std::filesystem::path(poPath).stem().string();
    }

    json j;
    j["version"] = 1;
    j["locale"] = locale;
    j["entries"] = json::object();
    for (const auto& kv : entries) {
        j["entries"][kv.first] = kv.second;
    }

    std::ofstream ofs(outputPath);
    if (!ofs.is_open()) {
        if (errorOut) *errorOut = "Failed to write JSON locale: " + outputPath;
        return false;
    }
    ofs << j.dump(2);
    return true;
}

bool exportStringsPOFromStory(const StoryT& story,
                              const std::string& outputPath,
                              std::string* errorOut) {
    return exportStringsPOImpl(story, outputPath, errorOut);
}

bool exportLocaleTemplateFromStory(const StoryT& story,
                                   const std::string& outputPath,
                                   std::string* errorOut) {
    json j;
    j["format"] = "gyeol-locale";
    j["version"] = 2;
    j["locale"] = "";
    j["line_entries"] = json::object();
    j["character_entries"] = json::object();

    for (size_t i = 0; i < story.line_ids.size(); ++i) {
        const std::string& lineId = story.line_ids[i];
        if (lineId.empty()) continue;
        j["line_entries"][lineId] = "";
    }

    for (const auto& charDef : story.characters) {
        if (!charDef) continue;
        const std::string characterId = poolStr(story, charDef->name_id);
        if (characterId.empty()) continue;
        json props = json::object();
        for (const auto& prop : charDef->properties) {
            if (!prop) continue;
            const std::string key = poolStr(story, prop->key_id);
            if (!key.empty()) props[key] = "";
        }
        if (!props.empty()) {
            j["character_entries"][characterId] = props;
        }
    }

    std::ofstream ofs(outputPath);
    if (!ofs.is_open()) {
        if (errorOut) *errorOut = "Failed to write locale template: " + outputPath;
        return false;
    }
    ofs << j.dump(2);
    return true;
}

bool convertPoToLocaleJson(const std::string& poPath,
                           const StoryT& story,
                           const std::string& outputPath,
                           const std::string& localeHint,
                           std::string* errorOut) {
    std::unordered_map<std::string, std::string> poEntries;
    if (!loadPoEntries(poPath, poEntries, errorOut)) {
        return false;
    }

    std::unordered_set<std::string> validLineIds;
    std::unordered_map<std::string, std::unordered_set<std::string>> validCharacterKeys;
    collectValidLineIds(story, validLineIds);
    collectValidCharacterPropertyKeys(story, validCharacterKeys);

    std::string locale = localeHint.empty()
        ? std::filesystem::path(poPath).stem().string()
        : localeHint;

    json j;
    j["format"] = "gyeol-locale";
    j["version"] = 2;
    j["locale"] = locale;
    j["line_entries"] = json::object();
    j["character_entries"] = json::object();

    size_t ignored = 0;
    for (const auto& kv : poEntries) {
        const std::string& context = kv.first;
        const std::string& translated = kv.second;
        if (translated.empty()) continue;

        if (validLineIds.count(context) > 0) {
            j["line_entries"][context] = translated;
            continue;
        }

        std::string characterId;
        std::string propertyKey;
        if (parseCharacterContext(context, characterId, propertyKey)) {
            auto charIt = validCharacterKeys.find(characterId);
            if (charIt != validCharacterKeys.end() && charIt->second.count(propertyKey) > 0) {
                j["character_entries"][characterId][propertyKey] = translated;
                continue;
            }
        }

        ignored++;
    }

    std::ofstream ofs(outputPath);
    if (!ofs.is_open()) {
        if (errorOut) *errorOut = "Failed to write locale JSON: " + outputPath;
        return false;
    }
    ofs << j.dump(2);

    if (ignored > 0) {
        std::cerr << "[GyeolCompiler] Ignored " << ignored << " unmatched PO context(s)." << std::endl;
    }

    return true;
}

bool validateLocaleJsonFile(const std::string& localePath,
                            const StoryT& story,
                            std::string* errorOut) {
    ParsedLocaleDoc localeDoc;
    if (!parseLocaleFile(localePath, localeDoc, errorOut)) return false;

    std::unordered_set<std::string> validLineIds;
    std::unordered_map<std::string, std::unordered_set<std::string>> validCharacterKeys;
    collectValidLineIds(story, validLineIds);
    collectValidCharacterPropertyKeys(story, validCharacterKeys);

    for (const auto& kv : localeDoc.lineEntries) {
        if (validLineIds.count(kv.first) == 0) {
            if (errorOut) *errorOut = "Unknown line_id in locale JSON: " + kv.first;
            return false;
        }
    }

    for (const auto& charEntry : localeDoc.characterEntries) {
        auto charIt = validCharacterKeys.find(charEntry.first);
        if (charIt == validCharacterKeys.end()) {
            if (errorOut) *errorOut = "Unknown character_id in locale JSON: " + charEntry.first;
            return false;
        }
        for (const auto& propEntry : charEntry.second) {
            if (charIt->second.count(propEntry.first) == 0) {
                if (errorOut) {
                    *errorOut = "Unknown character property key in locale JSON: " + charEntry.first + "." + propEntry.first;
                }
                return false;
            }
        }
    }

    return true;
}

bool buildLocaleCatalog(const std::vector<std::string>& localePaths,
                        const std::string& outputPath,
                        const std::string& defaultLocale,
                        std::string* errorOut) {
    if (localePaths.empty()) {
        if (errorOut) *errorOut = "buildLocaleCatalog requires at least one locale JSON path.";
        return false;
    }

    json catalog;
    catalog["format"] = "gyeol-locale-catalog";
    catalog["version"] = 2;
    catalog["default_locale"] = defaultLocale;
    catalog["locales"] = json::object();

    std::string firstLocale;
    for (const auto& localePath : localePaths) {
        ParsedLocaleDoc localeDoc;
        if (!parseLocaleFile(localePath, localeDoc, errorOut)) return false;
        if (localeDoc.locale.empty()) {
            if (errorOut) *errorOut = "Locale code is empty in file: " + localePath;
            return false;
        }
        if (firstLocale.empty()) firstLocale = localeDoc.locale;

        json localeObj;
        localeObj["line_entries"] = json::object();
        localeObj["character_entries"] = json::object();
        for (const auto& kv : localeDoc.lineEntries) {
            localeObj["line_entries"][kv.first] = kv.second;
        }
        for (const auto& charEntry : localeDoc.characterEntries) {
            for (const auto& propEntry : charEntry.second) {
                localeObj["character_entries"][charEntry.first][propEntry.first] = propEntry.second;
            }
        }

        catalog["locales"][localeDoc.locale] = std::move(localeObj);
    }

    if (catalog["default_locale"].get<std::string>().empty()) {
        catalog["default_locale"] = firstLocale;
    }

    std::ofstream ofs(outputPath);
    if (!ofs.is_open()) {
        if (errorOut) *errorOut = "Failed to write locale catalog: " + outputPath;
        return false;
    }
    ofs << catalog.dump(2);
    return true;
}

} // namespace LocaleTools

} // namespace Gyeol
