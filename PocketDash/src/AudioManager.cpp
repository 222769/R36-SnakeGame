#include "AudioManager.h"

#include "Synth.h"

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

// Loads a synthesised buffer through an in-memory WAV, so SDL_mixer converts
// it to the device format. The chunk owns its own copy of the audio.
ChunkPtr loadSynth(const synth::Buffer& buffer) {
    const std::vector<uint8_t> wav = synth::toWav(buffer);
    SDL_RWops* rw = SDL_RWFromConstMem(wav.data(), static_cast<int>(wav.size()));
    if (!rw) return nullptr;
    return ChunkPtr(Mix_LoadWAV_RW(rw, 1));
}

constexpr int kMusicChannels = 2; // channels 0 and 1 are reserved for music
constexpr int kMusicFadeMs = 700;

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

bool AudioManager::init(const std::string& audioDir, bool synthesizeMissing) {
    synthesize_ = synthesizeMissing;
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
    Mix_ReserveChannels(kMusicChannels);
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
    int made = 0;
    if (synthesize_) {
        const Uint64 start = SDL_GetPerformanceCounter();
        for (size_t i = 0; i < sfx_.size(); ++i) {
            if (sfx_[i]) continue;
            sfx_[i] = loadSynth(synth::makeSfx(static_cast<Sfx>(i)));
            if (sfx_[i]) ++made;
        }
        const double ms = static_cast<double>(SDL_GetPerformanceCounter() - start) * 1000.0 /
                          static_cast<double>(SDL_GetPerformanceFrequency());
        SDL_Log("[audio] Synthesised %d sound effect(s) in %.0f ms", made, ms);
    }
    SDL_Log("[audio] Audio ready, %d sound file(s) loaded from %s", loaded, audioDir.c_str());
    if (synthesize_) startComposer();
    return true;
}

void AudioManager::startComposer() {
    std::vector<MusicTrack> todo;
    for (MusicTrack t : {MusicTrack::Title, MusicTrack::Meadow, MusicTrack::Boss})
        if (!music_[static_cast<size_t>(t)]) todo.push_back(t);
    if (todo.empty()) return;
    stopComposer_ = false;
    composer_ = std::thread([this, todo] {
        for (MusicTrack t : todo) {
            if (stopComposer_) return;
            const Uint64 start = SDL_GetPerformanceCounter();
            std::vector<uint8_t> wav = synth::toWav(synth::makeMusic(t));
            const double ms = static_cast<double>(SDL_GetPerformanceCounter() - start) * 1000.0 /
                              static_cast<double>(SDL_GetPerformanceFrequency());
            SDL_Log("[audio] Composed the %s music (%.1f s loop) in %.0f ms", musicName(t),
                    static_cast<double>(synth::musicLoopSeconds(t)), ms);
            std::lock_guard<std::mutex> lock(composedMutex_);
            composed_[static_cast<size_t>(t)] = std::move(wav);
        }
    });
}

void AudioManager::collectComposed() {
    std::lock_guard<std::mutex> lock(composedMutex_);
    for (size_t i = 0; i < composed_.size(); ++i) {
        std::vector<uint8_t>& wav = composed_[i];
        if (wav.empty() || musicChunks_[i]) continue;
        SDL_RWops* rw = SDL_RWFromConstMem(wav.data(), static_cast<int>(wav.size()));
        if (rw) musicChunks_[i].reset(Mix_LoadWAV_RW(rw, 1));
        std::vector<uint8_t>().swap(wav); // free the bytes: the chunk has its own copy
    }
}

void AudioManager::shutdown() {
    stopComposer_ = true;
    if (composer_.joinable()) composer_.join();
    if (enabled_) {
        Mix_HaltMusic();
        Mix_HaltChannel(-1);
    }
    // Chunks and music must be freed before the device closes.
    for (auto& s : sfx_) s.reset();
    for (auto& m : music_) m.reset();
    for (auto& m : musicChunks_) m.reset();
    if (enabled_) Mix_CloseAudio();
    if (mixerInit_) Mix_Quit();
    enabled_ = false;
    mixerInit_ = false;
    currentTrack_ = MusicTrack::None;
    pendingFileTrack_ = MusicTrack::None;
}

void AudioManager::update() {
    if (!enabled_) return;
    if (duckUntil_ && SDL_TICKS_PASSED(SDL_GetTicks(), duckUntil_)) {
        duckUntil_ = 0;
        applyMusicVolume(musicVolume_);
    }
    if (synthesize_) {
        collectComposed();
        if (waitingForSynth_ && musicChunks_[static_cast<size_t>(currentTrack_)]) {
            waitingForSynth_ = false;
            startSynthTrack(currentTrack_);
        }
    }
    if (pendingFileTrack_ == MusicTrack::None || Mix_PlayingMusic()) return;
    if (Mix_Music* music = music_[static_cast<size_t>(pendingFileTrack_)].get()) Mix_FadeInMusic(music, -1, kMusicFadeMs);
    pendingFileTrack_ = MusicTrack::None;
}

void AudioManager::play(Sfx sfx) {
    if (!enabled_) return;
    Mix_Chunk* chunk = sfx_[static_cast<size_t>(sfx)].get();
    if (chunk) Mix_PlayChannel(-1, chunk, 0);
}

void AudioManager::playMusic(MusicTrack track) {
    if (!enabled_ || track == currentTrack_) return;
    currentTrack_ = track;
    pendingFileTrack_ = MusicTrack::None;
    waitingForSynth_ = false;
    // Fade out whatever plays now.
    if (Mix_PlayingMusic()) Mix_FadeOutMusic(kMusicFadeMs);
    for (int ch = 0; ch < kMusicChannels; ++ch)
        if (Mix_Playing(ch)) Mix_FadeOutChannel(ch, kMusicFadeMs);
    if (track == MusicTrack::None) return;

    const size_t i = static_cast<size_t>(track);
    if (music_[i]) {
        pendingFileTrack_ = track; // starts in update() once the old music has faded
        update();
        return;
    }
    if (!synthesize_) return;
    collectComposed();
    if (musicChunks_[i])
        startSynthTrack(track);
    else
        waitingForSynth_ = true; // still being composed: starts in update()
}

void AudioManager::startSynthTrack(MusicTrack track) {
    // Crossfade: the new track fades in on the other music channel.
    musicChannel_ = 1 - musicChannel_;
    Mix_Volume(musicChannel_, duckUntil_ ? musicVolume_ / 4 : musicVolume_);
    Mix_FadeInChannel(musicChannel_, musicChunks_[static_cast<size_t>(track)].get(), -1, kMusicFadeMs);
}

void AudioManager::stopMusic() {
    currentTrack_ = MusicTrack::None;
    pendingFileTrack_ = MusicTrack::None;
    waitingForSynth_ = false;
    if (!enabled_) return;
    Mix_FadeOutMusic(kMusicFadeMs);
    for (int ch = 0; ch < kMusicChannels; ++ch)
        if (Mix_Playing(ch)) Mix_FadeOutChannel(ch, kMusicFadeMs);
}

void AudioManager::applyMusicVolume(int volume) {
    if (!enabled_) return;
    Mix_VolumeMusic(volume);
    for (int ch = 0; ch < kMusicChannels; ++ch) Mix_Volume(ch, volume);
}

void AudioManager::setMusicVolume(int level) {
    musicVolume_ = std::clamp(level, 0, 10) * MIX_MAX_VOLUME / 10;
    applyMusicVolume(duckUntil_ ? musicVolume_ / 4 : musicVolume_);
}

void AudioManager::duckMusic(int ms) {
    if (!enabled_) return;
    duckUntil_ = SDL_GetTicks() + static_cast<Uint32>(std::max(1, ms));
    applyMusicVolume(musicVolume_ / 4);
}

void AudioManager::setSfxVolume(int level) {
    if (!enabled_) return;
    Mix_Volume(-1, std::clamp(level, 0, 10) * MIX_MAX_VOLUME / 10);
    applyMusicVolume(duckUntil_ ? musicVolume_ / 4 : musicVolume_); // keep music at its own level
}

} // namespace pd
