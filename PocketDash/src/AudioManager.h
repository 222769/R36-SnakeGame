#pragma once

#include "SdlPtr.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace pd {

enum class Sfx {
    Coin, Jump, Dash, EnemyHit, PlayerHurt, PowerUp, Star, LevelComplete,
    MenuMove, MenuSelect, Pause, Heart, Checkpoint, Splash,
    Gem, Break, ShieldPop, PowerUpUse, Denied,
    Count
};

enum class MusicTrack { None, Title, Meadow, Boss, Count };

// Wrapper over SDL2_mixer. Every call is safe when audio failed to
// initialise: the game simply stays silent.
//
// Files are looked up in assets/audio/ by name, e.g. "coin.wav" / "coin.ogg"
// for sound effects and "meadow.ogg" for music. Anything missing is
// synthesised (Synth.h), so the game always has its full soundtrack.
// Synthesised music tracks crossfade on two reserved mixer channels; file
// music fades out, then the next track fades in.
class AudioManager {
public:
    AudioManager() = default;
    ~AudioManager();
    AudioManager(const AudioManager&) = delete;
    AudioManager& operator=(const AudioManager&) = delete;

    // Returns false if audio is unavailable (the game continues silently).
    bool init(const std::string& audioDir, bool synthesizeMissing = true);
    // Call once per frame (starts file music queued behind a fade-out).
    void update();
    void shutdown();
    bool enabled() const { return enabled_; }

    void play(Sfx sfx);
    void playMusic(MusicTrack track);
    void stopMusic();

    // Lowers the music for `ms` milliseconds (e.g. under a fanfare).
    void duckMusic(int ms);

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
    void startComposer();
    void collectComposed();
    void startSynthTrack(MusicTrack track);

    std::array<ChunkPtr, static_cast<size_t>(MusicTrack::Count)> musicChunks_; // synthesised tracks
    // Music is composed on a worker thread (it takes a moment on the device);
    // finished tracks wait here as WAV bytes until update() wraps them.
    std::thread composer_;
    std::mutex composedMutex_;
    std::array<std::vector<uint8_t>, static_cast<size_t>(MusicTrack::Count)> composed_;
    std::atomic<bool> stopComposer_{false};
    bool waitingForSynth_ = false; // current track is still being composed
    MusicTrack currentTrack_ = MusicTrack::None;
    MusicTrack pendingFileTrack_ = MusicTrack::None;
    bool synthesize_ = true;
    int musicChannel_ = 0; // reserved channel 0 or 1 playing the current synthesised track
    int musicVolume_ = MIX_MAX_VOLUME;
    Uint32 duckUntil_ = 0; // SDL ticks; 0 = not ducked
    void applyMusicVolume(int volume);
};

} // namespace pd
