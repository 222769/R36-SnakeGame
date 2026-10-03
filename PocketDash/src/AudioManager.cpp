#include "AudioManager.h"

#include <SDL.h>

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace pd {

namespace {

bool fileExists(const std::string& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

} // namespace

const char* AudioManager::sfxName(Sfx sfx) {
    switch (sfx) {
    case Sfx::Coin: return "coin";
    case Sfx::Jump: return "jump";
    case Sfx::Dash: return "dash";
    case Sfx::EnemyHit: return "enemy_hit";
    case Sfx::PlayerHurt: return "player_hurt";
    case Sfx::PowerUp: return "powerup";
    case Sfx::Star: return "star";
    case Sfx::LevelComplete: return "level_complete";
    case Sfx::MenuMove: return "menu_move";
    case Sfx::MenuSelect: return "menu_select";
    case Sfx::Pause: return "pause";
    case Sfx::Heart: return "heart";
    case Sfx::Checkpoint: return "checkpoint";
    case Sfx::Splash: return "splash";
    case Sfx::Gem: return "gem";
    case Sfx::Break: return "break";
    case Sfx::ShieldPop: return "shield_pop";
    case Sfx::PowerUpUse: return "powerup_use";
    case Sfx::Denied: return "denied";
    case Sfx::Count: break;
    }
    return "unknown";
}

const char* AudioManager::musicName(MusicTrack track) {
    switch (track) {
    case MusicTrack::Title: return "title";
    case MusicTrack::Meadow: return "meadow";
    case MusicTrack::Boss: return "boss";
    case MusicTrack::None:
    case MusicTrack::Count: break;
    }
    return "none";
}

AudioManager::~AudioManager() { shutdown(); }

bool AudioManager::init(const std::string& audioDir) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        SDL_Log("[audio] SDL audio unavailable (%s) - running silent", SDL_GetError());
        return false;
    }
    Mix_Init(MIX_INIT_OGG);
    mixerInit_ = true;

    // 1024-sample buffer keeps latency low without crackling on the RK3326.
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 1024) != 0) {
        SDL_Log("[audio] Mix_OpenAudio failed (%s) - running silent", Mix_GetError());
        return false;
    }
    Mix_AllocateChannels(16);
    enabled_ = true;

    int loaded = 0;
    for (size_t i = 0; i < sfx_.size(); ++i) {
        const std::string base = audioDir + sfxName(static_cast<Sfx>(i));
        for (const char* ext : {".wav", ".ogg"}) {
            if (!fileExists(base + ext)) continue;
            sfx_[i].reset(Mix_LoadWAV((base + ext).c_str()));
            if (sfx_[i]) {
                ++loaded;
                break;
            }
            SDL_Log("[audio] Failed to load %s%s: %s", base.c_str(), ext, Mix_GetError());
        }
    }
    for (size_t i = 1; i < music_.size(); ++i) {
        const std::string base = audioDir + musicName(static_cast<MusicTrack>(i));
        for (const char* ext : {".ogg", ".mp3", ".wav"}) {
            if (!fileExists(base + ext)) continue;
            music_[i].reset(Mix_LoadMUS((base + ext).c_str()));
            if (music_[i]) {
                ++loaded;
                break;
            }
            SDL_Log("[audio] Failed to load %s%s: %s", base.c_str(), ext, Mix_GetError());
        }
    }
    SDL_Log("[audio] Audio ready, %d sound file(s) loaded from %s", loaded, audioDir.c_str());
    return true;
}

void AudioManager::shutdown() {
    if (enabled_) {
        Mix_HaltMusic();
        Mix_HaltChannel(-1);
    }
    // Chunks and music must be freed before the device closes.
    for (auto& s : sfx_) s.reset();
    for (auto& m : music_) m.reset();
    if (enabled_) Mix_CloseAudio();
    if (mixerInit_) Mix_Quit();
    enabled_ = false;
    mixerInit_ = false;
    currentTrack_ = MusicTrack::None;
}

void AudioManager::play(Sfx sfx) {
    if (!enabled_) return;
    Mix_Chunk* chunk = sfx_[static_cast<size_t>(sfx)].get();
    if (chunk) Mix_PlayChannel(-1, chunk, 0);
}

void AudioManager::playMusic(MusicTrack track) {
    if (!enabled_ || track == currentTrack_) return;
    currentTrack_ = track;
    Mix_Music* music = music_[static_cast<size_t>(track)].get();
    if (music)
        Mix_FadeInMusic(music, -1, 400);
    else
        Mix_HaltMusic();
}

void AudioManager::stopMusic() {
    currentTrack_ = MusicTrack::None;
    if (enabled_) Mix_HaltMusic();
}

void AudioManager::setMusicVolume(int level) {
    if (enabled_) Mix_VolumeMusic(std::clamp(level, 0, 10) * MIX_MAX_VOLUME / 10);
}

void AudioManager::setSfxVolume(int level) {
    if (enabled_) Mix_Volume(-1, std::clamp(level, 0, 10) * MIX_MAX_VOLUME / 10);
}

} // namespace pd
