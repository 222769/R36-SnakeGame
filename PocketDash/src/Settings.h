#pragma once

#include <string>

namespace pd {

enum class Difficulty { Relaxed, Normal, Challenge };

inline const char* difficultyName(Difficulty d) {
    switch (d) {
    case Difficulty::Relaxed: return "RELAXED";
    case Difficulty::Normal: return "NORMAL";
    case Difficulty::Challenge: return "CHALLENGE";
    }
    return "NORMAL";
}

inline Difficulty difficultyFromName(const std::string& s) {
    std::string up;
    for (char c : s) up += static_cast<char>(c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
    if (up == "RELAXED" || up == "EASY") return Difficulty::Relaxed;
    if (up == "CHALLENGE" || up == "HARD") return Difficulty::Challenge;
    return Difficulty::Normal;
}

// User-facing options, persisted in save/settings.ini.
struct Settings {
    int musicVolume = 7;   // 0..10
    int sfxVolume = 8;     // 0..10
    bool screenShake = true;
    Difficulty difficulty = Difficulty::Normal;
};

} // namespace pd
