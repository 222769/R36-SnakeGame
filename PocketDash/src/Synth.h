#pragma once

// Built-in sound: every sound effect and three looping music tracks are
// synthesised at startup, so the game has full audio without any files.
// Sounds use soft, rounded tones (sine bells, warm band-limited squares,
// filtered noise) rather than harsh chiptune beeps.
//
// Everything here is plain C++ (no SDL), so it is unit-tested directly. The
// AudioManager wraps the results as in-memory WAV files for SDL_mixer, which
// converts them to whatever format the audio device uses.

#include <cstdint>
#include <vector>

namespace pd {

enum class Sfx;
enum class MusicTrack;

namespace synth {

constexpr int kSampleRate = 44100;

// Interleaved 16-bit stereo PCM.
struct Buffer {
    std::vector<int16_t> samples;
    int frames() const { return static_cast<int>(samples.size() / 2); }
    float seconds() const { return static_cast<float>(frames()) / kSampleRate; }
};

Buffer makeSfx(Sfx sfx);

// One seamless loop of a music track (the end flows back into the start).
Buffer makeMusic(MusicTrack track);
// Loop length in seconds for a track (bars x beats at its tempo).
float musicLoopSeconds(MusicTrack track);

// A complete RIFF/WAVE file in memory (PCM, 16-bit, stereo, 44.1 kHz).
std::vector<uint8_t> toWav(const Buffer& buffer);

} // namespace synth

} // namespace pd
