#pragma once

#include "SdlPtr.h"

#include <array>
#include <string>

namespace pd {

enum class Sfx {
    Coin, Jump, Dash, EnemyHit, PlayerHurt, PowerUp, Star, LevelComplete,
    MenuMove, MenuSelect, Pause,
    Count
};

enum class MusicTrack { None, Title, Meadow, Boss, Count };

// Thin wrapper over SDL2_mixer. Every call is safe when audio failed to
// initialise or a file is missing: the game simply stays silent.
//
// Files are looked up in assets/audio/ by name, e.g. "coin.wav" / "coin.ogg"
// for sound effects and "meadow.ogg" for music.
class AudioManager {
public:
    AudioManager() = default;
    ~AudioManager();
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    // Returns false if audio is unavailable (the game continues silently).
    bool init(const std::string& audioDir);
    void shutdown();
    bool enabled() const { return enabled_; }

    void play(Sfx sfx);
    void playMusic(MusicTrack track);
    void stopMusic();

    // Volumes are 0..10 to match the settings menu.
    void setMusicVolume(int level);
    void setSfxVolume(int level);

    static const char* sfxName(Sfx sfx);
    static const char* musicName(MusicTrack track);

private:
    bool enabled_ = false;
    bool mixerInit_ = false;
    std::array<ChunkPtr, static_cast<size_t>(Sfx::Count)> sfx_;
    std::array<MusicPtr, static_cast<size_t>(MusicTrack::Count)> music_;
    MusicTrack currentTrack_ = MusicTrack::None;
};

} // namespace pd
