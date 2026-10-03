#include "LevelLoader.h"

#include "Platform.h"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace pd::levels {

namespace {

std::string trim(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r");
    if (b == std::string::npos) return {};
    const size_t e = s.find_last_not_of(" \t\r");
    return s.substr(b, e - b + 1);
}

bool parseInt(const std::string& s, int& out) {
    if (s.empty()) return false;
    char* end = nullptr;
    const long v = std::strtol(s.c_str(), &end, 10);
    if (!end || *end != '\0') return false;
    out = static_cast<int>(v);
    return true;
}

} // namespace

bool parse(const std::string& text, Level& out, std::string* error) {
    auto fail = [&](int line, const std::string& msg) {
        if (error) *error = (line > 0 ? "line " + std::to_string(line) + ": " : std::string()) + msg;
        return false;
    };

    enum class Section { Header, Map, Signs };
    Section section = Section::Header;
    std::vector<std::string> mapRows;
    std::vector<std::string> signTexts;
    int mapFirstLine = 0;
    bool mapEnded = false;

    std::string id, name, hint;
    int world = 1;
    int goal = 0;
    int timeLimit = 0;
    Objective objective = Objective::ReachExit;

    std::istringstream in(text);
    std::string raw;
    int lineNo = 0;
    while (std::getline(in, raw)) {
        ++lineNo;
        if (!raw.empty() && raw.back() == '\r') raw.pop_back();
        const std::string line = trim(raw);

        if (line == "[map]") {
            if (!mapRows.empty() || mapFirstLine) return fail(lineNo, "a level can only have one [map]");
            section = Section::Map;
            mapFirstLine = lineNo + 1;
            continue;
        }
        if (line == "[signs]") {
            section = Section::Signs;
            continue;
        }
        if (!line.empty() && line.front() == '[') return fail(lineNo, "unknown section " + line);

        switch (section) {
        case Section::Map:
            // '#' is a wall here, so no comments inside the map. A blank line
            // ends the map; anything after it must be another section.
            if (line.empty()) {
                if (!mapRows.empty()) mapEnded = true;
                continue;
            }
            if (mapEnded) return fail(lineNo, "map rows after a blank line");
            if (mapRows.empty()) mapFirstLine = lineNo;
            mapRows.push_back(line);
            break;

        case Section::Signs:
            if (line.empty() || line.front() == '#') continue;
            signTexts.push_back(line);
            break;

        case Section::Header: {
            if (line.empty() || line.front() == '#') continue;
            const size_t eq = line.find('=');
            if (eq == std::string::npos) return fail(lineNo, "expected key=value");
            const std::string key = trim(line.substr(0, eq));
            const std::string value = trim(line.substr(eq + 1));
            if (key == "id") {
                id = value;
            } else if (key == "name") {
                name = value;
            } else if (key == "hint") {
                hint = value;
            } else if (key == "world") {
                if (!parseInt(value, world) || world < 1 || world > kWorldCount)
                    return fail(lineNo, "world must be 1-" + std::to_string(kWorldCount));
            } else if (key == "goal") {
                if (!parseInt(value, goal) || goal < 0) return fail(lineNo, "goal must be a number >= 0");
            } else if (key == "time_limit") {
                if (!parseInt(value, timeLimit) || timeLimit < 0)
                    return fail(lineNo, "time_limit must be a number of seconds >= 0");
            } else if (key == "objective") {
                if (!objectiveFromName(value, objective))
                    return fail(lineNo, "unknown objective '" + value + "' (exit, coins, stars, rescue, defeat)");
            } else {
                return fail(lineNo, "unknown key '" + key + "'");
            }
            break;
        }
        }
    }

    if (id.empty()) return fail(0, "missing id=");
    if (mapRows.empty()) return fail(0, "missing [map] section");

    Level level;
    std::string mapError;
    if (!Level::fromAscii(mapRows, level, &mapError, mapFirstLine)) return fail(0, mapError);

    if (signTexts.size() != level.signs.size())
        return fail(0, "the map has " + std::to_string(level.signs.size()) + " sign(s) but [signs] lists " +
                           std::to_string(signTexts.size()));
    for (size_t i = 0; i < signTexts.size(); ++i) level.signs[i].text = signTexts[i];

    // The objective must be possible on this map.
    switch (objective) {
    case Objective::Coins:
        if (goal <= 0 || goal > static_cast<int>(level.coins.size()))
            return fail(0, "objective=coins needs goal=1.." + std::to_string(level.coins.size()));
        break;
    case Objective::Stars:
        if (level.stars.empty()) return fail(0, "objective=stars but the map has no stars");
        break;
    case Objective::Rescue:
        if (level.friends.empty()) return fail(0, "objective=rescue but the map has no friends (f)");
        break;
    case Objective::DefeatAll:
        if (level.enemies.empty()) return fail(0, "objective=defeat but the map has no enemies");
        break;
    case Objective::ReachExit: break;
    }

    level.id = id;
    level.name = name.empty() ? id : name;
    level.world = static_cast<WorldId>(world - 1);
    level.objective = objective;
    level.goal = goal;
    level.timeLimit = static_cast<float>(timeLimit);
    level.hint = hint;
    out = std::move(level);
    return true;
}

bool loadFile(const std::string& path, Level& out, std::string* error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (error) *error = "cannot open " + path;
        return false;
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    if (!parse(ss.str(), out, error)) {
        if (error) *error = path + ": " + *error;
        return false;
    }
    return true;
}

const std::vector<std::string>& worldLevelIds(int world) {
    static const std::vector<std::string> kWorld1 = {"1-1", "1-2", "1-3", "1-4", "1-5", "1-6", "1-7", "1-8"};
    static const std::vector<std::string> kNone;
    return world == 1 ? kWorld1 : kNone;
}

std::string nextLevelId(const std::string& id) {
    for (int w = 1; w <= kWorldCount; ++w) {
        const auto& ids = worldLevelIds(w);
        for (size_t i = 0; i < ids.size(); ++i)
            if (ids[i] == id) return i + 1 < ids.size() ? ids[i + 1] : std::string();
    }
    return {};
}

bool load(const std::string& id, Level& out, std::string* error) {
    return loadFile(platform::dataPath("assets/levels/" + id + ".lvl"), out, error);
}

const std::vector<Info>& catalog(int world) {
    static std::vector<Info> cache[kWorldCount + 1];
    static bool loaded[kWorldCount + 1] = {};
    if (world < 1 || world > kWorldCount) return cache[0];
    if (!loaded[world]) {
        loaded[world] = true;
        for (const std::string& id : worldLevelIds(world)) {
            Info info;
            info.id = id;
            info.name = "?";
            Level level;
            std::string error;
            if (load(id, level, &error)) {
                info.name = level.name;
                info.objective = level.objective;
                info.stars = static_cast<int>(level.stars.size());
                info.gems = static_cast<int>(level.gems.size());
                info.secrets = level.secretCount();
                info.loaded = true;
            }
            cache[world].push_back(std::move(info));
        }
    }
    return cache[world];
}

} // namespace pd::levels
