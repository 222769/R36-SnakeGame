#pragma once

#include "Settings.h"

#include <string>
#include <utility>
#include <vector>

namespace pd {

// Minimal INI-style "key=value" store used for config and save files.
//  - '#' or ';' start a comment line
//  - "[section]" lines are kept as a key prefix: "section.key"
//  - whitespace around keys and values is trimmed
// Insertion order is preserved so saved files stay readable and diffable.
class KeyValueStore {
public:
    void parse(const std::string& text);
    bool loadFromFile(const std::string& path);
    std::string serialize() const;
    // Writes atomically (temp file + rename) so a power-off mid-save cannot
    // corrupt the previous save.
    bool saveToFile(const std::string& path) const;

    bool has(const std::string& key) const;
    std::string getString(const std::string& key, const std::string& fallback = {}) const;
    int getInt(const std::string& key, int fallback) const;
    bool getBool(const std::string& key, bool fallback) const;
    float getFloat(const std::string& key, float fallback) const;

    void set(const std::string& key, const std::string& value);
    void set(const std::string& key, int value);
    void set(const std::string& key, bool value);

    const std::vector<std::pair<std::string, std::string>>& entries() const { return entries_; }
    void clear() { entries_.clear(); }

private:
    const std::string* find(const std::string& key) const;
    std::vector<std::pair<std::string, std::string>> entries_;
};

// Owns the on-disk save files. Phase 1 persists settings; level progress,
// collectibles and high scores are added in Phase 6.
class SaveManager {
public:
    explicit SaveManager(std::string saveDir = {});

    const std::string& saveDir() const { return saveDir_; }

    Settings loadSettings() const;
    bool saveSettings(const Settings& settings) const;

private:
    std::string saveDir_;
};

} // namespace pd
