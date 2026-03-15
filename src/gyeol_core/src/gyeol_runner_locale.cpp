#include "gyeol_runner.h"
#include "gyeol_generated.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>

using namespace ICPDev::Gyeol::Schema;

namespace Gyeol {

namespace {

static const Story* asStory(const void* p) { return static_cast<const Story*>(p); }
static const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>*
asPool(const void* p) {
    return static_cast<const flatbuffers::Vector<flatbuffers::Offset<flatbuffers::String>>*>(p);
}

std::string toLowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

std::vector<std::string> parseCSVLine(const std::string& line) {
    std::vector<std::string> result;
    std::string cur;
    bool inQ = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (c == '"') {
            if (inQ && i + 1 < line.size() && line[i + 1] == '"') {
                cur += '"';
                i++;
            } else {
                inQ = !inQ;
            }
        } else if (c == ',' && !inQ) {
            result.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    result.push_back(cur);
    return result;
}

std::string fileStem(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    size_t dot = path.find_last_of('.');
    size_t nameStart = (slash == std::string::npos) ? 0 : slash + 1;
    return path.substr(nameStart,
        (dot == std::string::npos || dot < nameStart) ? std::string::npos : dot - nameStart);
}

struct JsonCursor {
    const std::string& text;
    size_t pos = 0;
};

void skipWs(JsonCursor& c) {
    while (c.pos < c.text.size() &&
           (c.text[c.pos] == ' ' || c.text[c.pos] == '\t' ||
            c.text[c.pos] == '\r' || c.text[c.pos] == '\n')) {
        c.pos++;
    }
}

bool consume(JsonCursor& c, char ch) {
    skipWs(c);
    if (c.pos >= c.text.size() || c.text[c.pos] != ch) return false;
    c.pos++;
    return true;
}

bool parseJsonString(JsonCursor& c, std::string& out) {
    skipWs(c);
    if (c.pos >= c.text.size() || c.text[c.pos] != '"') return false;
    c.pos++;
    out.clear();
    while (c.pos < c.text.size()) {
        char ch = c.text[c.pos++];
        if (ch == '"') return true;
        if (ch != '\\') {
            out.push_back(ch);
            continue;
        }
        if (c.pos >= c.text.size()) return false;
        char esc = c.text[c.pos++];
        switch (esc) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                if (c.pos + 4 > c.text.size()) return false;
                c.pos += 4; // keep UTF-8 source as-is for this lightweight parser
                break;
            }
            default:
                return false;
        }
    }
    return false;
}

bool skipJsonValue(JsonCursor& c);

bool skipJsonObject(JsonCursor& c) {
    if (!consume(c, '{')) return false;
    skipWs(c);
    if (consume(c, '}')) return true;
    while (true) {
        std::string key;
        if (!parseJsonString(c, key)) return false;
        if (!consume(c, ':')) return false;
        if (!skipJsonValue(c)) return false;
        skipWs(c);
        if (consume(c, '}')) return true;
        if (!consume(c, ',')) return false;
    }
}

bool skipJsonArray(JsonCursor& c) {
    if (!consume(c, '[')) return false;
    skipWs(c);
    if (consume(c, ']')) return true;
    while (true) {
        if (!skipJsonValue(c)) return false;
        skipWs(c);
        if (consume(c, ']')) return true;
        if (!consume(c, ',')) return false;
    }
}

bool skipJsonLiteral(JsonCursor& c, const char* lit) {
    size_t n = std::strlen(lit);
    if (c.pos + n > c.text.size()) return false;
    if (c.text.compare(c.pos, n, lit) != 0) return false;
    c.pos += n;
    return true;
}

bool skipJsonNumber(JsonCursor& c) {
    skipWs(c);
    size_t start = c.pos;
    if (c.pos < c.text.size() && (c.text[c.pos] == '-' || c.text[c.pos] == '+')) c.pos++;
    bool hasDigit = false;
    while (c.pos < c.text.size() && std::isdigit(static_cast<unsigned char>(c.text[c.pos]))) {
        hasDigit = true;
        c.pos++;
    }
    if (c.pos < c.text.size() && c.text[c.pos] == '.') {
        c.pos++;
        while (c.pos < c.text.size() && std::isdigit(static_cast<unsigned char>(c.text[c.pos]))) {
            hasDigit = true;
            c.pos++;
        }
    }
    if (c.pos < c.text.size() && (c.text[c.pos] == 'e' || c.text[c.pos] == 'E')) {
        c.pos++;
        if (c.pos < c.text.size() && (c.text[c.pos] == '-' || c.text[c.pos] == '+')) c.pos++;
        while (c.pos < c.text.size() && std::isdigit(static_cast<unsigned char>(c.text[c.pos]))) {
            hasDigit = true;
            c.pos++;
        }
    }
    return hasDigit && c.pos > start;
}

bool skipJsonValue(JsonCursor& c) {
    skipWs(c);
    if (c.pos >= c.text.size()) return false;
    char ch = c.text[c.pos];
    if (ch == '{') return skipJsonObject(c);
    if (ch == '[') return skipJsonArray(c);
    if (ch == '"') {
        std::string dummy;
        return parseJsonString(c, dummy);
    }
    if (ch == '-' || ch == '+' || std::isdigit(static_cast<unsigned char>(ch))) {
        return skipJsonNumber(c);
    }
    if (skipJsonLiteral(c, "true")) return true;
    if (skipJsonLiteral(c, "false")) return true;
    if (skipJsonLiteral(c, "null")) return true;
    return false;
}

bool parseJsonInt(JsonCursor& c, int& out) {
    skipWs(c);
    size_t start = c.pos;
    if (c.pos < c.text.size() && (c.text[c.pos] == '-' || c.text[c.pos] == '+')) c.pos++;
    while (c.pos < c.text.size() && std::isdigit(static_cast<unsigned char>(c.text[c.pos]))) {
        c.pos++;
    }
    if (start == c.pos) return false;
    try {
        out = std::stoi(c.text.substr(start, c.pos - start));
        return true;
    } catch (...) {
        return false;
    }
}

bool parseJsonStringMap(JsonCursor& c, std::unordered_map<std::string, std::string>& out) {
    if (!consume(c, '{')) return false;
    skipWs(c);
    if (consume(c, '}')) return true;
    while (true) {
        std::string k;
        std::string v;
        if (!parseJsonString(c, k)) return false;
        if (!consume(c, ':')) return false;
        if (!parseJsonString(c, v)) return false;
        out[k] = v;
        skipWs(c);
        if (consume(c, '}')) return true;
        if (!consume(c, ',')) return false;
    }
}

bool parseLocaleJson(const std::string& jsonText,
                     int& version,
                     std::string& locale,
                     std::unordered_map<std::string, std::string>& entries) {
    JsonCursor c{jsonText};
    if (!consume(c, '{')) return false;

    bool hasVersion = false;
    bool hasEntries = false;

    skipWs(c);
    if (consume(c, '}')) return false;
    while (true) {
        std::string key;
        if (!parseJsonString(c, key)) return false;
        if (!consume(c, ':')) return false;

        if (key == "version") {
            if (!parseJsonInt(c, version)) return false;
            hasVersion = true;
        } else if (key == "locale") {
            if (!parseJsonString(c, locale)) return false;
        } else if (key == "entries") {
            if (!parseJsonStringMap(c, entries)) return false;
            hasEntries = true;
        } else {
            if (!skipJsonValue(c)) return false;
        }

        skipWs(c);
        if (consume(c, '}')) break;
        if (!consume(c, ',')) return false;
    }

    skipWs(c);
    if (c.pos != c.text.size()) return false;
    return hasVersion && hasEntries;
}

} // namespace

bool Runner::loadLocale(const std::string& path) {
    auto* story = asStory(story_);
    auto* pool = asPool(pool_);
    auto* lineIds = story ? story->line_ids() : nullptr;
    if (!pool || !lineIds || lineIds->size() == 0) {
        setError("No line_ids in story");
        return false;
    }

    std::unordered_map<std::string, int32_t> idMap;
    for (flatbuffers::uoffset_t i = 0; i < lineIds->size(); ++i) {
        auto* s = lineIds->Get(i);
        if (s && s->size() > 0) {
            idMap[s->str()] = static_cast<int32_t>(i);
        }
    }

    localePool_.clear();
    localePool_.resize(pool->size());
    std::string resolvedLocale = fileStem(path);

    std::string lowerPath = toLowerCopy(path);
    bool isJson = lowerPath.size() >= 5 && lowerPath.substr(lowerPath.size() - 5) == ".json";

    if (isJson) {
        std::ifstream ifs(path);
        if (!ifs.is_open()) {
            setError("Failed to open locale: " + path);
            return false;
        }

        std::ostringstream oss;
        oss << ifs.rdbuf();

        int version = 0;
        std::string localeName;
        std::unordered_map<std::string, std::string> entries;
        if (!parseLocaleJson(oss.str(), version, localeName, entries)) {
            setError("Invalid locale JSON: " + path);
            return false;
        }
        if (version != 1) {
            setError("Unsupported locale JSON version");
            return false;
        }

        for (const auto& kv : entries) {
            if (kv.second.empty()) continue;
            auto it = idMap.find(kv.first);
            if (it != idMap.end()) {
                localePool_[static_cast<size_t>(it->second)] = kv.second;
            }
        }
        if (!localeName.empty()) {
            resolvedLocale = localeName;
        }
    } else {
        std::ifstream ifs(path);
        if (!ifs.is_open()) {
            setError("Failed to open locale: " + path);
            return false;
        }

        std::string line;
        std::getline(ifs, line); // skip header
        while (std::getline(ifs, line)) {
            if (line.empty()) continue;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            auto cols = parseCSVLine(line);
            if (cols.size() < 5 || cols[4].empty()) continue;
            auto it = idMap.find(cols[0]);
            if (it != idMap.end()) {
                localePool_[static_cast<size_t>(it->second)] = cols[4];
            }
        }
    }

    currentLocale_ = resolvedLocale;
    recordTrace("LOCALE_LOAD", currentNodeName(), pc_, currentLocale_);
    return true;
}

void Runner::clearLocale() {
    localePool_.clear();
    currentLocale_.clear();
    recordTrace("LOCALE_CLEAR", currentNodeName(), pc_, "");
}

std::string Runner::getLocale() const {
    return currentLocale_;
}

} // namespace Gyeol
