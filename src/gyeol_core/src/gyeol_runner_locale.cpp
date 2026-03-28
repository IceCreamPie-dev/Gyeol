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
                c.pos += 4;
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
    const size_t n = std::strlen(lit);
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
        std::string key;
        std::string value;
        if (!parseJsonString(c, key)) return false;
        if (!consume(c, ':')) return false;
        if (!parseJsonString(c, value)) return false;
        out[key] = value;
        skipWs(c);
        if (consume(c, '}')) return true;
        if (!consume(c, ',')) return false;
    }
}

bool parseJsonNestedStringMap(
    JsonCursor& c,
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>>& out) {
    if (!consume(c, '{')) return false;
    skipWs(c);
    if (consume(c, '}')) return true;
    while (true) {
        std::string outerKey;
        if (!parseJsonString(c, outerKey)) return false;
        if (!consume(c, ':')) return false;
        std::unordered_map<std::string, std::string> inner;
        if (!parseJsonStringMap(c, inner)) return false;
        out[outerKey] = std::move(inner);
        skipWs(c);
        if (consume(c, '}')) return true;
        if (!consume(c, ',')) return false;
    }
}

struct SingleLocalePayload {
    std::string locale;
    std::unordered_map<std::string, std::string> lineEntries;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> characterEntries;
};

struct LocaleCatalogPayload {
    std::string defaultLocale;
    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> lineEntriesByLocale;
    std::unordered_map<std::string, std::unordered_map<std::string, std::unordered_map<std::string, std::string>>> characterEntriesByLocale;
};

bool parseSingleLocaleV1(const std::string& jsonText, SingleLocalePayload& payload) {
    JsonCursor c{jsonText};
    if (!consume(c, '{')) return false;

    bool hasVersion = false;
    bool hasEntries = false;
    int version = 0;

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
            if (!parseJsonString(c, payload.locale)) return false;
        } else if (key == "entries") {
            if (!parseJsonStringMap(c, payload.lineEntries)) return false;
            hasEntries = true;
        } else {
            if (!skipJsonValue(c)) return false;
        }

        skipWs(c);
        if (consume(c, '}')) break;
        if (!consume(c, ',')) return false;
    }
    skipWs(c);
    return c.pos == c.text.size() && hasVersion && hasEntries && version == 1;
}

bool parseSingleLocaleV2(const std::string& jsonText, SingleLocalePayload& payload) {
    JsonCursor c{jsonText};
    if (!consume(c, '{')) return false;

    bool hasVersion = false;
    bool hasFormat = false;
    int version = 0;
    std::string format;

    skipWs(c);
    if (consume(c, '}')) return false;
    while (true) {
        std::string key;
        if (!parseJsonString(c, key)) return false;
        if (!consume(c, ':')) return false;

        if (key == "format") {
            if (!parseJsonString(c, format)) return false;
            hasFormat = true;
        } else if (key == "version") {
            if (!parseJsonInt(c, version)) return false;
            hasVersion = true;
        } else if (key == "locale") {
            if (!parseJsonString(c, payload.locale)) return false;
        } else if (key == "line_entries") {
            if (!parseJsonStringMap(c, payload.lineEntries)) return false;
        } else if (key == "character_entries") {
            if (!parseJsonNestedStringMap(c, payload.characterEntries)) return false;
        } else {
            if (!skipJsonValue(c)) return false;
        }

        skipWs(c);
        if (consume(c, '}')) break;
        if (!consume(c, ',')) return false;
    }
    skipWs(c);
    return c.pos == c.text.size() && hasFormat && hasVersion &&
           format == "gyeol-locale" && version == 2;
}

bool parseLocaleCatalog(const std::string& jsonText, LocaleCatalogPayload& payload) {
    JsonCursor c{jsonText};
    if (!consume(c, '{')) return false;

    bool hasFormat = false;
    bool hasVersion = false;
    bool hasLocales = false;
    int version = 0;
    std::string format;

    skipWs(c);
    if (consume(c, '}')) return false;
    while (true) {
        std::string key;
        if (!parseJsonString(c, key)) return false;
        if (!consume(c, ':')) return false;

        if (key == "format") {
            if (!parseJsonString(c, format)) return false;
            hasFormat = true;
        } else if (key == "version") {
            if (!parseJsonInt(c, version)) return false;
            hasVersion = true;
        } else if (key == "default_locale") {
            if (!parseJsonString(c, payload.defaultLocale)) return false;
        } else if (key == "locales") {
            if (!consume(c, '{')) return false;
            skipWs(c);
            if (!consume(c, '}')) {
                while (true) {
                    std::string localeCode;
                    if (!parseJsonString(c, localeCode)) return false;
                    if (!consume(c, ':')) return false;

                    SingleLocalePayload single;
                    if (!consume(c, '{')) return false;
                    skipWs(c);
                    if (!consume(c, '}')) {
                        while (true) {
                            std::string localeField;
                            if (!parseJsonString(c, localeField)) return false;
                            if (!consume(c, ':')) return false;
                            if (localeField == "line_entries") {
                                if (!parseJsonStringMap(c, single.lineEntries)) return false;
                            } else if (localeField == "character_entries") {
                                if (!parseJsonNestedStringMap(c, single.characterEntries)) return false;
                            } else if (!skipJsonValue(c)) {
                                return false;
                            }
                            skipWs(c);
                            if (consume(c, '}')) break;
                            if (!consume(c, ',')) return false;
                        }
                    }

                    payload.lineEntriesByLocale[localeCode] = std::move(single.lineEntries);
                    payload.characterEntriesByLocale[localeCode] = std::move(single.characterEntries);

                    skipWs(c);
                    if (consume(c, '}')) break;
                    if (!consume(c, ',')) return false;
                }
            }
            hasLocales = true;
        } else {
            if (!skipJsonValue(c)) return false;
        }

        skipWs(c);
        if (consume(c, '}')) break;
        if (!consume(c, ',')) return false;
    }
    skipWs(c);
    return c.pos == c.text.size() && hasFormat && hasVersion && hasLocales &&
           format == "gyeol-locale-catalog" && version == 2;
}

} // namespace

std::string Runner::baseLocaleCode(const std::string& localeCode) const {
    size_t sep = localeCode.find_first_of("-_");
    if (sep == std::string::npos) return localeCode;
    if (sep == 0) return "";
    return localeCode.substr(0, sep);
}

bool Runner::applyLocaleSelection(const std::string& requestedLocale, bool recordTraceEvent) {
    auto* pool = asPool(pool_);
    if (!pool) {
        setError("No story loaded for locale selection");
        return false;
    }
    if (!hasLocaleCatalog_) {
        setError("Locale catalog is not loaded");
        return false;
    }

    currentLocale_ = requestedLocale;
    resolvedLocale_.clear();
    localePool_.assign(pool->size(), "");
    localeCharacterProps_.clear();

    std::vector<std::string> chain;
    if (!requestedLocale.empty()) {
        chain.push_back(requestedLocale);
        const std::string base = baseLocaleCode(requestedLocale);
        if (!base.empty() && base != requestedLocale) {
            chain.push_back(base);
        }
    }
    if (!catalogDefaultLocale_.empty() &&
        std::find(chain.begin(), chain.end(), catalogDefaultLocale_) == chain.end()) {
        chain.push_back(catalogDefaultLocale_);
    }

    bool appliedAny = false;
    for (const auto& code : chain) {
        auto lineIt = catalogLineEntriesByLocale_.find(code);
        auto charIt = catalogCharacterEntriesByLocale_.find(code);
        if (lineIt == catalogLineEntriesByLocale_.end() &&
            charIt == catalogCharacterEntriesByLocale_.end()) {
            continue;
        }

        if (resolvedLocale_.empty()) {
            resolvedLocale_ = code;
        }
        appliedAny = true;

        if (lineIt != catalogLineEntriesByLocale_.end()) {
            for (const auto& kv : lineIt->second) {
                const int32_t idx = kv.first;
                if (idx < 0 || static_cast<size_t>(idx) >= localePool_.size()) continue;
                if (localePool_[static_cast<size_t>(idx)].empty()) {
                    localePool_[static_cast<size_t>(idx)] = kv.second;
                }
            }
        }

        if (charIt != catalogCharacterEntriesByLocale_.end()) {
            for (const auto& charEntry : charIt->second) {
                auto& dst = localeCharacterProps_[charEntry.first];
                for (const auto& propEntry : charEntry.second) {
                    if (dst.find(propEntry.first) == dst.end()) {
                        dst[propEntry.first] = propEntry.second;
                    }
                }
            }
        }
    }

    if (!appliedAny) {
        localePool_.clear();
        localeCharacterProps_.clear();
        resolvedLocale_.clear();
        setError("Requested locale is not available in catalog");
        return false;
    }

    if (recordTraceEvent) {
        recordTrace("LOCALE_SET", currentNodeName(), pc_, currentLocale_ + "->" + resolvedLocale_);
    }
    return true;
}

bool Runner::loadLocale(const std::string& path) {
    auto* story = asStory(story_);
    auto* pool = asPool(pool_);
    auto* lineIds = story ? story->line_ids() : nullptr;
    if (!pool || !lineIds || lineIds->size() == 0) {
        setError("No line_ids in story");
        return false;
    }

    std::unordered_map<std::string, int32_t> lineIdToPoolIndex;
    for (flatbuffers::uoffset_t i = 0; i < lineIds->size(); ++i) {
        auto* s = lineIds->Get(i);
        if (s && s->size() > 0) {
            lineIdToPoolIndex[s->str()] = static_cast<int32_t>(i);
        }
    }

    hasLocaleCatalog_ = false;
    catalogDefaultLocale_.clear();
    catalogLineEntriesByLocale_.clear();
    catalogCharacterEntriesByLocale_.clear();
    localeCharacterProps_.clear();

    localePool_.clear();
    localePool_.resize(pool->size());
    currentLocale_ = fileStem(path);
    resolvedLocale_ = currentLocale_;

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
        const std::string jsonText = oss.str();

        SingleLocalePayload localePayload;
        if (parseSingleLocaleV2(jsonText, localePayload)) {
            for (const auto& kv : localePayload.lineEntries) {
                if (kv.second.empty()) continue;
                auto it = lineIdToPoolIndex.find(kv.first);
                if (it != lineIdToPoolIndex.end()) {
                    localePool_[static_cast<size_t>(it->second)] = kv.second;
                }
            }
            for (const auto& charEntry : localePayload.characterEntries) {
                for (const auto& propEntry : charEntry.second) {
                    if (!propEntry.second.empty()) {
                        localeCharacterProps_[charEntry.first][propEntry.first] = propEntry.second;
                    }
                }
            }
            if (!localePayload.locale.empty()) {
                currentLocale_ = localePayload.locale;
                resolvedLocale_ = localePayload.locale;
            }
        } else {
            localePayload = SingleLocalePayload{};
            if (!parseSingleLocaleV1(jsonText, localePayload)) {
                setError("Invalid locale JSON: " + path);
                return false;
            }
            for (const auto& kv : localePayload.lineEntries) {
                if (kv.second.empty()) continue;
                auto it = lineIdToPoolIndex.find(kv.first);
                if (it != lineIdToPoolIndex.end()) {
                    localePool_[static_cast<size_t>(it->second)] = kv.second;
                }
            }
            if (!localePayload.locale.empty()) {
                currentLocale_ = localePayload.locale;
                resolvedLocale_ = localePayload.locale;
            }
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
            auto it = lineIdToPoolIndex.find(cols[0]);
            if (it != lineIdToPoolIndex.end()) {
                localePool_[static_cast<size_t>(it->second)] = cols[4];
            }
        }
    }

    recordTrace("LOCALE_LOAD", currentNodeName(), pc_, currentLocale_);
    return true;
}

bool Runner::loadLocaleCatalog(const std::string& path) {
    auto* story = asStory(story_);
    auto* lineIds = story ? story->line_ids() : nullptr;
    if (!lineIds || lineIds->size() == 0) {
        setError("No line_ids in story");
        return false;
    }

    std::ifstream ifs(path);
    if (!ifs.is_open()) {
        setError("Failed to open locale catalog: " + path);
        return false;
    }
    std::ostringstream oss;
    oss << ifs.rdbuf();

    LocaleCatalogPayload payload;
    if (!parseLocaleCatalog(oss.str(), payload)) {
        setError("Invalid locale catalog JSON: " + path);
        return false;
    }

    std::unordered_map<std::string, int32_t> lineIdToPoolIndex;
    for (flatbuffers::uoffset_t i = 0; i < lineIds->size(); ++i) {
        auto* lid = lineIds->Get(i);
        if (lid && lid->size() > 0) {
            lineIdToPoolIndex[lid->str()] = static_cast<int32_t>(i);
        }
    }

    catalogLineEntriesByLocale_.clear();
    catalogCharacterEntriesByLocale_.clear();
    for (const auto& localeEntry : payload.lineEntriesByLocale) {
        auto& dst = catalogLineEntriesByLocale_[localeEntry.first];
        for (const auto& lineEntry : localeEntry.second) {
            auto it = lineIdToPoolIndex.find(lineEntry.first);
            if (it != lineIdToPoolIndex.end() && !lineEntry.second.empty()) {
                dst[it->second] = lineEntry.second;
            }
        }
    }
    for (const auto& localeEntry : payload.characterEntriesByLocale) {
        catalogCharacterEntriesByLocale_[localeEntry.first] = localeEntry.second;
    }

    hasLocaleCatalog_ = true;
    catalogDefaultLocale_ = payload.defaultLocale;
    if (catalogDefaultLocale_.empty() && !payload.lineEntriesByLocale.empty()) {
        catalogDefaultLocale_ = payload.lineEntriesByLocale.begin()->first;
    }
    if (catalogDefaultLocale_.empty() && !payload.characterEntriesByLocale.empty()) {
        catalogDefaultLocale_ = payload.characterEntriesByLocale.begin()->first;
    }

    if (catalogDefaultLocale_.empty()) {
        setError("Locale catalog has no locales");
        return false;
    }

    if (!applyLocaleSelection(catalogDefaultLocale_, false)) {
        return false;
    }

    recordTrace("LOCALE_CATALOG_LOAD", currentNodeName(), pc_, path);
    return true;
}

bool Runner::setLocale(const std::string& localeCode) {
    return applyLocaleSelection(localeCode, true);
}

void Runner::clearLocale() {
    localePool_.clear();
    localeCharacterProps_.clear();
    currentLocale_.clear();
    resolvedLocale_.clear();
    recordTrace("LOCALE_CLEAR", currentNodeName(), pc_, "");
}

std::string Runner::getLocale() const {
    return currentLocale_;
}

std::string Runner::getResolvedLocale() const {
    return resolvedLocale_;
}

} // namespace Gyeol
