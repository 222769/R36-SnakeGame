#include "SaveManager.h"

#include <SDL.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace pd {

namespace {

std::string trim(const std::string& s) {
    size_t b = 0;
    size_t e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

} // namespace

void KeyValueStore::parse(const std::string& text) {
    std::istringstream in(text);
    std::string line;
    std::string section;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = trim(line.substr(1, line.size() - 2));
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(line.substr(0, eq));
        std::string value = trim(line.substr(eq + 1));
        if (key.empty()) continue;
        if (!section.empty()) key = section + "." + key;
        set(key, value);
    }
}

bool KeyValueStore::loadFromFile(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return false;
    std::ostringstream ss;
    ss << file.rdbuf();
    parse(ss.str());
    return true;
}

std::string KeyValueStore::serialize() const {
    std::string out;
    for (const auto& [key, value] : entries_) {
        out += key;
        out += '=';
        out += value;
        out += '\n';
    }
    return out;
}

bool KeyValueStore::saveToFile(const std::string& path) const {
    const std::string tmp = path + ".tmp";
    {
        std::ofstream file(tmp, std::ios::binary | std::ios::trunc);
        if (!file) return false;
        file << serialize();
        if (!file.flush()) return false;
    }
    // std::rename does not replace existing files on Windows.
    std::remove(path.c_str());
    return std::rename(tmp.c_str(), path.c_str()) == 0;
}

const std::string* KeyValueStore::find(const std::string& key) const {
    for (const auto& entry : entries_)
        if (entry.first == key) return &entry.second;
    return nullptr;
}

bool KeyValueStore::has(const std::string& key) const { return find(key) != nullptr; }

std::string KeyValueStore::getString(const std::string& key, const std::string& fallback) const {
    const std::string* v = find(key);
    return v ? *v : fallback;
}

int KeyValueStore::getInt(const std::string& key, int fallback) const {
    const std::string* v = find(key);
    if (!v || v->empty()) return fallback;
    char* end = nullptr;
    const long n = std::strtol(v->c_str(), &end, 10);
    return (end && *end == '\0') ? static_cast<int>(n) : fallback;
}

bool KeyValueStore::getBool(const std::string& key, bool fallback) const {
    const std::string* v = find(key);
    if (!v) return fallback;
    const std::string s = lower(*v);
    if (s == "1" || s == "true" || s == "yes" || s == "on") return true;
    if (s == "0" || s == "false" || s == "no" || s == "off") return false;
    return fallback;
}

float KeyValueStore::getFloat(const std::string& key, float fallback) const {
    const std::string* v = find(key);
    if (!v || v->empty()) return fallback;
    char* end = nullptr;
    const float f = std::strtof(v->c_str(), &end);
    return (end && *end == '\0') ? f : fallback;
}

void KeyValueStore::set(const std::string& key, const std::string& value) {
    for (auto& entry : entries_) {
        if (entry.first == key) {
            entry.second = value;
            return;
        }
    }
    entries_.emplace_back(key, value);
}

void KeyValueStore::set(const std::string& key, int value) { set(key, std::to_string(value)); }

void KeyValueStore::set(const std::string& key, bool value) { set(key, std::string(value ? "1" : "0")); }

// ---------------------------------------------------------------------------

SaveManager::SaveManager(std::string saveDir) : saveDir_(std::move(saveDir)) {}

Settings SaveManager::loadSettings() const {
    Settings s;
    KeyValueStore kv;
    if (!kv.loadFromFile(saveDir_ + "settings.ini")) return s;
    s.musicVolume = std::clamp(kv.getInt("music_volume", s.musicVolume), 0, 10);
    s.sfxVolume = std::clamp(kv.getInt("sfx_volume", s.sfxVolume), 0, 10);
    s.screenShake = kv.getBool("screen_shake", s.screenShake);
    s.difficulty = difficultyFromName(kv.getString("difficulty", difficultyName(s.difficulty)));
    return s;
}

bool SaveManager::saveSettings(const Settings& s) const {
    KeyValueStore kv;
    kv.set("music_volume", s.musicVolume);
    kv.set("sfx_volume", s.sfxVolume);
    kv.set("screen_shake", s.screenShake);
    kv.set("difficulty", std::string(difficultyName(s.difficulty)));
    const bool ok = kv.saveToFile(saveDir_ + "settings.ini");
    if (!ok) SDL_Log("[save] Failed to write %ssettings.ini", saveDir_.c_str());
    return ok;
}

Progress SaveManager::loadProgress() const {
    Progress p;
    KeyValueStore kv;
    if (kv.loadFromFile(saveDir_ + "progress.ini")) p.load(kv);
    return p;
}

bool SaveManager::saveProgress(const Progress& progress) const {
    KeyValueStore kv;
    progress.save(kv);
    const bool ok = kv.saveToFile(saveDir_ + "progress.ini");
    if (!ok) SDL_Log("[save] Failed to write %sprogress.ini", saveDir_.c_str());
    return ok;
}

} // namespace pd
