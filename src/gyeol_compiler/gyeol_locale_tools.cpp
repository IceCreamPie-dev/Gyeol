#include "gyeol_parser.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unordered_map>

using namespace ICPDev::Gyeol::Schema;

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

} // namespace

bool Parser::exportStringsPO(const std::string& outputPath) const {
    std::ofstream ofs(outputPath);
    if (!ofs.is_open()) {
        std::cerr << "Failed to write: " << outputPath << std::endl;
        return false;
    }

    ofs << "msgid \"\"\n";
    ofs << "msgstr \"\"\n";
    ofs << "\"Content-Type: text/plain; charset=UTF-8\\n\"\n";
    ofs << "\"Content-Transfer-Encoding: 8bit\\n\"\n\n";

    for (const auto& node : story_.nodes) {
        for (size_t i = 0; i < node->lines.size(); ++i) {
            const auto& instr = node->lines[i];
            int32_t idx = -1;
            std::string type;
            std::string character;

            if (instr->data.type == OpData::Line) {
                auto* line = instr->data.AsLine();
                idx = line->text_id;
                type = "LINE";
                if (line->character_id >= 0) {
                    character = story_.string_pool[static_cast<size_t>(line->character_id)];
                }
            } else if (instr->data.type == OpData::Choice) {
                auto* choice = instr->data.AsChoice();
                idx = choice->text_id;
                type = "CHOICE";
            } else {
                continue;
            }

            if (idx < 0 || idx >= static_cast<int32_t>(lineIds_.size()) || lineIds_[idx].empty()) {
                continue;
            }

            const std::string& lineId = lineIds_[idx];
            const std::string& srcText = story_.string_pool[static_cast<size_t>(idx)];

            ofs << "#. type: " << type << " node: " << node->name;
            if (!character.empty()) {
                ofs << " character: " << character;
            }
            ofs << "\n";
            ofs << "msgctxt \"" << escapePoString(lineId) << "\"\n";
            ofs << "msgid \"" << escapePoString(srcText) << "\"\n";
            ofs << "msgstr \"\"\n\n";
        }
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

    nlohmann::json j;
    j["version"] = 1;
    j["locale"] = locale;
    j["entries"] = nlohmann::json::object();
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

} // namespace LocaleTools

} // namespace Gyeol
