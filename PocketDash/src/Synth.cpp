#include "Synth.h"

#include "AudioManager.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace pd::synth {

namespace {

constexpr float kPi = 3.14159265f;

// --- Oscillators -----------------------------------------------------------------

constexpr int kTableSize = 4096;

const std::array<float, kTableSize>& sineTable() {
    static const std::array<float, kTableSize> table = [] {
        std::array<float, kTableSize> t{};
        for (int i = 0; i < kTableSize; ++i) t[static_cast<size_t>(i)] = std::sin(2.0f * kPi * i / kTableSize);
        return t;
    }();
    return table;
}

// Sine of a phase given in cycles.
float sine(double cycles) {
    const double frac = cycles - std::floor(cycles);
    return sineTable()[static_cast<size_t>(frac * kTableSize) & (kTableSize - 1)];
}

enum class Wave {
    Sine,
    Bell,     // sine with fading upper partials: marimba / glockenspiel
    Soft,     // band-limited square (odd harmonics): warm, never buzzy
    Triangle,
    Noise,    // low-passed white noise (whoosh, splash, snare)
    Hiss,     // high-passed noise (hats, shakers, sparkle)
};

struct Note {
    float start = 0.0f;
    float dur = 0.1f;   // until the release starts
    float f0 = 440.0f;
    float f1 = 0.0f;    // end pitch of a glide (0 = none)
    Wave wave = Wave::Sine;
    float vol = 0.5f;
    float attack = 0.004f;
    float decay = 0.0f;  // exponential decay per second (0 = hold)
    float release = 0.04f;
    float pan = 0.0f;    // -1 left .. 1 right
    float vibrato = 0.0f; // semitones, 6 Hz
    float filter = 0.3f;  // noise smoothing 0..1 (higher = brighter)
};

// Cheap tanh-like soft clip (Pade approximation, exact enough below 3).
float softClip(float x) {
    x = std::clamp(x, -3.0f, 3.0f);
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

float midiHz(float note) { return 440.0f * std::pow(2.0f, (note - 69.0f) / 12.0f); }

// Stereo float mix. Music mixes wrap around so the loop is seamless.
struct Mix {
    std::vector<float> l, r;
    bool wrap = false;
    uint32_t rng = 0x9E3779B9u;

    Mix(float seconds, bool wrapAround)
        : l(static_cast<size_t>(std::max(1.0f, seconds * kSampleRate))), r(l.size()), wrap(wrapAround) {}

    float noise() {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return static_cast<float>(rng & 0xFFFF) / 32767.5f - 1.0f;
    }

    void render(const Note& n) {
        const int total = static_cast<int>((n.dur + n.release) * kSampleRate);
        const int first = static_cast<int>(n.start * kSampleRate);
        const float gl = std::cos((n.pan + 1.0f) * kPi * 0.25f) * 1.4142f;
        const float gr = std::sin((n.pan + 1.0f) * kPi * 0.25f) * 1.4142f;
        // Decay and glide advance by a constant factor per sample (no exp() per sample).
        const float decayStep = n.decay > 0.0f ? std::exp(-n.decay / kSampleRate) : 1.0f;
        const int glideSamples = std::max(1, static_cast<int>(n.dur * kSampleRate));
        const float glideStep = n.f1 > 0.0f ? std::exp(std::log(n.f1 / n.f0) / static_cast<float>(glideSamples)) : 1.0f;
        const int attackSamples = std::max(1, static_cast<int>(n.attack * kSampleRate));
        const int releaseSamples = std::max(1, static_cast<int>(n.release * kSampleRate));
        const int durSamples = static_cast<int>(n.dur * kSampleRate);
        double phase = 0.0;
        float low = 0.0f, low2 = 0.0f;
        float decayEnv = 1.0f;
        float partial = 1.0f; // bell overtones fade as exp(-7t); exp(-14t) is its square
        const float partialStep = std::exp(-7.0f / kSampleRate);
        float level = 0.0f;
        float f = n.f0;
        const size_t size = l.size();
        for (int i = 0; i < total; ++i) {
            // Envelope: attack, exponential decay, linear release.
            float env = i < attackSamples ? static_cast<float>(i) / attackSamples : decayEnv;
            if (i >= attackSamples) decayEnv *= decayStep;
            if (i < durSamples) {
                level = env;
            } else {
                env = level * std::max(0.0f, 1.0f - static_cast<float>(i - durSamples) / releaseSamples);
            }
            if (env < 1e-4f && i > attackSamples) {
                if (i >= durSamples) break; // faded out for good
                continue;
            }
            if (i < glideSamples) f *= glideStep;
            float fm = f;
            if (n.vibrato > 0.0f) fm *= std::pow(2.0f, n.vibrato * sine(static_cast<double>(i) * 6.0 / kSampleRate) / 12.0f);
            phase += fm / kSampleRate;
            float s = 0.0f;
            switch (n.wave) {
            case Wave::Sine: s = sine(phase); break;
            case Wave::Bell:
                s = sine(phase) + 0.3f * sine(phase * 2.0) * partial + 0.12f * sine(phase * 4.01) * partial * partial;
                partial *= partialStep;
                s *= 0.8f;
                break;
            case Wave::Soft:
                s = (sine(phase) + sine(phase * 3.0) / 3.0f + 0.7f * sine(phase * 5.0) / 5.0f) * 0.8f;
                break;
            case Wave::Triangle: {
                const double frac = phase - std::floor(phase);
                s = static_cast<float>(4.0 * std::fabs(frac - 0.5) - 1.0);
                break;
            }
            case Wave::Noise:
                low += n.filter * (noise() - low);
                low2 += n.filter * (low - low2);
                s = low2 * 2.2f;
                break;
            case Wave::Hiss: {
                const float x = noise();
                low += 0.25f * (x - low);
                s = (x - low) * 0.8f;
                break;
            }
            }
            const float v = s * env * n.vol;
            size_t idx = static_cast<size_t>(first + i);
            if (idx >= size) {
                if (!wrap) break;
                idx %= size;
            }
            l[idx] += v * gl;
            r[idx] += v * gr;
        }
    }

    // Scales to `peak` (louder sounds are brought down, quiet ones up) and
    // converts to 16-bit with a gentle soft clip.
    Buffer finish(float peak) const {
        float max = 1e-6f;
        for (size_t i = 0; i < l.size(); ++i) max = std::max({max, std::fabs(l[i]), std::fabs(r[i])});
        const float gain = peak / max;
        Buffer b;
        b.samples.resize(l.size() * 2);
        for (size_t i = 0; i < l.size(); ++i) {
            b.samples[i * 2] = static_cast<int16_t>(softClip(l[i] * gain) * 32000.0f);
            b.samples[i * 2 + 1] = static_cast<int16_t>(softClip(r[i] * gain) * 32000.0f);
        }
        return b;
    }
};

// Quick helpers for sound-effect recipes.
Note bell(float start, float midi, float dur, float vol = 0.5f, float decay = 9.0f) {
    Note n;
    n.start = start;
    n.f0 = midiHz(midi);
    n.dur = dur;
    n.wave = Wave::Bell;
    n.vol = vol;
    n.decay = decay;
    n.release = 0.06f;
    return n;
}

Note sweep(float start, float dur, float fromHz, float toHz, Wave wave, float vol, float decay = 0.0f) {
    Note n;
    n.start = start;
    n.dur = dur;
    n.f0 = fromHz;
    n.f1 = toHz;
    n.wave = wave;
    n.vol = vol;
    n.decay = decay;
    n.release = 0.03f;
    return n;
}

Note noise(float start, float dur, float vol, float filter, float decay, Wave wave = Wave::Noise) {
    Note n;
    n.start = start;
    n.dur = dur;
    n.wave = wave;
    n.vol = vol;
    n.filter = filter;
    n.decay = decay;
    n.attack = 0.002f;
    n.release = 0.03f;
    return n;
}

} // namespace

// --- Sound effects ------------------------------------------------------------------

Buffer makeSfx(Sfx sfx) {
    float length = 0.3f;
    std::vector<Note> notes;
    notes.reserve(16);
    switch (sfx) {
    case Sfx::Coin: // bright two-note "ka-ching"
        notes = {bell(0.0f, 83, 0.05f, 0.5f, 6.0f), bell(0.055f, 88, 0.22f, 0.6f, 9.0f)};
        length = 0.35f;
        break;
    case Sfx::Jump:
        notes = {sweep(0.0f, 0.12f, 280.0f, 620.0f, Wave::Sine, 0.6f, 6.0f)};
        length = 0.18f;
        break;
    case Sfx::Dash:
        notes = {noise(0.0f, 0.2f, 0.5f, 0.12f, 9.0f), sweep(0.0f, 0.16f, 520.0f, 180.0f, Wave::Sine, 0.25f, 10.0f)};
        notes[0].attack = 0.04f;
        length = 0.28f;
        break;
    case Sfx::EnemyHit: // springy "boing" and a pop
        notes = {sweep(0.0f, 0.16f, 700.0f, 220.0f, Wave::Sine, 0.6f, 8.0f), bell(0.0f, 84, 0.08f, 0.25f, 20.0f)};
        notes[0].vibrato = 0.6f;
        length = 0.25f;
        break;
    case Sfx::PlayerHurt:
        notes = {sweep(0.0f, 0.28f, 440.0f, 150.0f, Wave::Soft, 0.45f, 4.0f), noise(0.0f, 0.06f, 0.2f, 0.3f, 30.0f)};
        notes[0].vibrato = 0.8f;
        length = 0.38f;
        break;
    case Sfx::PowerUp: // rising major arpeggio
        for (int i = 0; i < 4; ++i) notes.push_back(bell(i * 0.065f, 72.0f + std::array<float, 4>{0, 4, 7, 12}[static_cast<size_t>(i)], 0.18f, 0.45f));
        length = 0.5f;
        break;
    case Sfx::Star: // sparkling arpeggio with a shimmering tail
        for (int i = 0; i < 5; ++i)
            notes.push_back(bell(i * 0.06f, 79.0f + std::array<float, 5>{0, 4, 7, 12, 16}[static_cast<size_t>(i)], 0.3f, 0.4f, 6.0f));
        notes.push_back(noise(0.15f, 0.4f, 0.06f, 0.0f, 5.0f, Wave::Hiss));
        length = 0.8f;
        break;
    case Sfx::LevelComplete: // little fanfare ending on a warm chord
        notes = {bell(0.0f, 72, 0.12f, 0.45f, 4.0f), bell(0.13f, 76, 0.12f, 0.45f, 4.0f), bell(0.26f, 79, 0.12f, 0.45f, 4.0f)};
        for (float m : {72.0f, 76.0f, 79.0f, 84.0f}) {
            Note n = bell(0.42f, m, 0.9f, 0.35f, 2.5f);
            n.release = 0.3f;
            notes.push_back(n);
        }
        for (float m : {48.0f, 55.0f}) {
            Note n = sweep(0.42f, 0.9f, midiHz(m), 0.0f, Wave::Triangle, 0.25f, 2.0f);
            n.release = 0.3f;
            notes.push_back(n);
        }
        length = 1.7f;
        break;
    case Sfx::MenuMove:
        notes = {bell(0.0f, 81, 0.03f, 0.35f, 30.0f)};
        length = 0.1f;
        break;
    case Sfx::MenuSelect:
        notes = {bell(0.0f, 76, 0.05f, 0.4f, 15.0f), bell(0.06f, 83, 0.14f, 0.45f, 12.0f)};
        length = 0.28f;
        break;
    case Sfx::Pause:
        notes = {bell(0.0f, 79, 0.06f, 0.4f, 12.0f), bell(0.08f, 72, 0.16f, 0.4f, 10.0f)};
        length = 0.3f;
        break;
    case Sfx::Heart:
        notes = {bell(0.0f, 76, 0.08f, 0.45f, 8.0f), bell(0.09f, 81, 0.25f, 0.5f, 7.0f)};
        notes.push_back(sweep(0.09f, 0.25f, midiHz(57), 0.0f, Wave::Triangle, 0.15f, 6.0f));
        length = 0.45f;
        break;
    case Sfx::Checkpoint:
        notes = {bell(0.0f, 72, 0.07f, 0.45f, 8.0f), bell(0.08f, 79, 0.07f, 0.45f, 8.0f), bell(0.16f, 84, 0.3f, 0.5f, 6.0f)};
        length = 0.55f;
        break;
    case Sfx::Splash:
        notes = {noise(0.0f, 0.35f, 0.6f, 0.2f, 7.0f), sweep(0.0f, 0.12f, 320.0f, 110.0f, Wave::Sine, 0.4f, 12.0f)};
        length = 0.45f;
        break;
    case Sfx::Gem: // crystal chime
        for (int i = 0; i < 4; ++i)
            notes.push_back(bell(i * 0.045f, 91.0f + std::array<float, 4>{0, 5, 7, 12}[static_cast<size_t>(i)], 0.25f, 0.35f, 7.0f));
        notes.push_back(noise(0.05f, 0.3f, 0.05f, 0.0f, 6.0f, Wave::Hiss));
        length = 0.6f;
        break;
    case Sfx::Break: // crunch with a woody thud
        notes = {noise(0.0f, 0.18f, 0.6f, 0.35f, 14.0f), sweep(0.0f, 0.14f, 150.0f, 60.0f, Wave::Sine, 0.6f, 12.0f)};
        length = 0.3f;
        break;
    case Sfx::ShieldPop:
        notes = {sweep(0.0f, 0.07f, 1100.0f, 300.0f, Wave::Sine, 0.5f, 20.0f), noise(0.0f, 0.04f, 0.3f, 0.6f, 40.0f, Wave::Hiss)};
        length = 0.15f;
        break;
    case Sfx::PowerUpUse: // whoosh up and a shimmer
        notes = {sweep(0.0f, 0.3f, 300.0f, 1200.0f, Wave::Sine, 0.4f, 2.0f), noise(0.0f, 0.3f, 0.12f, 0.0f, 4.0f, Wave::Hiss)};
        notes.push_back(bell(0.25f, 96, 0.2f, 0.25f, 10.0f));
        length = 0.55f;
        break;
    case Sfx::Denied: // soft low "uh-uh"
        notes = {sweep(0.0f, 0.09f, midiHz(57), 0.0f, Wave::Soft, 0.35f, 6.0f), sweep(0.12f, 0.12f, midiHz(53), 0.0f, Wave::Soft, 0.35f, 6.0f)};
        length = 0.3f;
        break;
    case Sfx::Count: break;
    }
    Mix mix(length, false);
    for (const Note& n : notes) mix.render(n);
    return mix.finish(0.7f);
}

// --- Music ------------------------------------------------------------------------------

namespace {

struct Song {
    float bpm;
    int root;          // MIDI note of the key's tonic (around C4)
    bool minor;
    int progression[4]; // scale degrees (0-based), one chord per bar
    int style;          // 0 calm (title), 1 bouncy (meadow), 2 driving (boss)
    uint32_t seed;
};
constexpr int kBars = 8;

Song songFor(MusicTrack track) {
    switch (track) {
    case MusicTrack::Meadow: return {116.0f, 55, false, {0, 5, 3, 4}, 1, 0x51u};
    case MusicTrack::Boss: return {134.0f, 57, true, {0, 5, 6, 0}, 2, 0x77u};
    case MusicTrack::Title:
    case MusicTrack::None:
    case MusicTrack::Count: break;
    }
    return {92.0f, 60, false, {0, 4, 5, 3}, 0, 0x13u};
}

int scaleSemitone(bool minor, int degree) {
    static const int kMajor[7] = {0, 2, 4, 5, 7, 9, 11};
    static const int kMinor[7] = {0, 2, 3, 5, 7, 8, 10};
    const int octave = degree >= 0 ? degree / 7 : -((-degree + 6) / 7);
    const int d = degree - octave * 7;
    return octave * 12 + (minor ? kMinor[d] : kMajor[d]);
}

struct Rng {
    uint32_t s;
    uint32_t next() {
        s = s * 1664525u + 1013904223u;
        return s >> 8;
    }
    int range(int n) { return static_cast<int>(next() % static_cast<uint32_t>(n)); }
};

// Rhythm patterns for one bar, in sixteenth notes: {start, length}.
struct Hit {
    int start, len;
};
const std::vector<std::vector<Hit>>& patterns() {
    static const std::vector<std::vector<Hit>> p = {
        {{0, 4}, {4, 4}, {8, 4}, {12, 4}},
        {{0, 6}, {6, 2}, {8, 4}, {12, 4}},
        {{0, 2}, {2, 2}, {4, 4}, {8, 2}, {10, 2}, {12, 4}},
        {{0, 8}, {8, 4}, {12, 4}},
        {{0, 4}, {4, 2}, {6, 2}, {8, 8}},
        {{0, 3}, {3, 3}, {6, 2}, {8, 3}, {11, 3}, {14, 2}},
    };
    return p;
}

} // namespace

float musicLoopSeconds(MusicTrack track) {
    const Song s = songFor(track);
    return kBars * 4.0f * 60.0f / s.bpm;
}

Buffer makeMusic(MusicTrack track) {
    const Song song = songFor(track);
    const float beat = 60.0f / song.bpm;
    const float bar = beat * 4.0f;
    const float six = beat / 4.0f;
    Mix mix(musicLoopSeconds(track), true);
    Rng rng{song.seed};
    auto pitch = [&](int degree, int octaveShift) {
        return static_cast<float>(song.root + octaveShift * 12 + scaleSemitone(song.minor, degree));
    };

    // Melody rhythm choices per style.
    static const int kStylePatterns[3][3] = {{3, 4, 0}, {1, 2, 5}, {2, 5, 1}};
    std::vector<int> barPattern(kBars);
    for (int b = 0; b < 4; ++b) barPattern[static_cast<size_t>(b)] = kStylePatterns[song.style][rng.range(3)];
    for (int b = 4; b < kBars; ++b) barPattern[static_cast<size_t>(b)] = barPattern[static_cast<size_t>(b - 4)]; // answer phrase
    int melodyDegree = 4; // start on the fifth

    for (int b = 0; b < kBars; ++b) {
        const int chord = song.progression[b % 4];
        const float t0 = static_cast<float>(b) * bar;

        // Pad: soft detuned chord held for the bar.
        for (int k = 0; k < 3; ++k) {
            for (float detune : {-0.12f, 0.12f}) {
                Note n;
                n.start = t0;
                n.dur = bar - 0.05f;
                n.f0 = midiHz(pitch(chord + k * 2, 0) + detune);
                n.wave = Wave::Triangle;
                n.vol = song.style == 2 ? 0.035f : 0.045f;
                n.attack = song.style == 0 ? 0.35f : 0.12f;
                n.release = 0.35f;
                n.pan = (k - 1) * 0.5f;
                mix.render(n);
            }
        }

        // Bass.
        const float bassRoot = pitch(chord, -2);
        auto bassNote = [&](float start, float len, float midi, float vol) {
            Note n;
            n.start = start;
            n.dur = len * 0.85f;
            n.f0 = midiHz(midi);
            n.wave = song.style == 2 ? Wave::Soft : Wave::Sine;
            n.vol = vol;
            n.attack = 0.01f;
            n.decay = 2.0f;
            n.release = 0.06f;
            mix.render(n);
        };
        if (song.style == 0) {
            bassNote(t0, beat * 2.0f, bassRoot, 0.22f);
            bassNote(t0 + beat * 2.0f, beat * 2.0f, bassRoot + 7.0f, 0.19f);
        } else if (song.style == 1) {
            const float steps[8] = {0, 12, 7, 12, 0, 12, 7, 5};
            for (int i = 0; i < 8; ++i) bassNote(t0 + i * beat * 0.5f, beat * 0.5f, bassRoot + steps[i], i % 2 ? 0.15f : 0.21f);
        } else {
            for (int i = 0; i < 8; ++i) bassNote(t0 + i * beat * 0.5f, beat * 0.5f, bassRoot + (i == 6 ? 12.0f : 0.0f), 0.26f);
        }

        // Gentle arpeggio (title and meadow).
        if (song.style != 2) {
            for (int i = 0; i < 8; ++i) {
                Note n = bell(t0 + i * beat * 0.5f, pitch(chord + (i % 4 == 3 ? 2 : (i % 4) * 2), 1), beat * 0.4f,
                              song.style == 0 ? 0.07f : 0.06f, 7.0f);
                n.pan = (i % 2 ? 0.45f : -0.45f);
                mix.render(n);
            }
        }

        // Melody: chord tones on strong beats, steps in between.
        const auto& pattern = patterns()[static_cast<size_t>(barPattern[static_cast<size_t>(b)])];
        for (size_t h = 0; h < pattern.size(); ++h) {
            const Hit& hit = pattern[h];
            if (hit.start % 8 == 0) {
                // Nearest chord tone (root, third, fifth) to where the tune is.
                int best = chord;
                for (int octave = -1; octave <= 1; ++octave)
                    for (int k = 0; k < 3; ++k) {
                        const int cand = chord + k * 2 + octave * 7;
                        if (std::abs(cand - melodyDegree) < std::abs(best - melodyDegree)) best = cand;
                    }
                melodyDegree = best;
            } else {
                melodyDegree += std::array<int, 5>{-1, 1, 1, -2, 2}[static_cast<size_t>(rng.range(5))];
            }
            melodyDegree = std::clamp(melodyDegree, 0, 9);
            int degree = melodyDegree;
            // The answer phrase resolves home at the very end.
            if (b == kBars - 1 && h + 1 == pattern.size()) degree = 7;
            const float len = static_cast<float>(hit.len) * six;
            Note n = bell(t0 + static_cast<float>(hit.start) * six, pitch(degree, 1), len * 0.9f,
                          song.style == 2 ? 0.16f : 0.24f, song.style == 0 ? 3.0f : 4.5f);
            if (song.style == 2) {
                n.wave = Wave::Soft;
                n.vol = 0.12f;
                n.decay = 3.0f;
            }
            n.release = std::min(0.25f, len);
            n.pan = 0.1f;
            mix.render(n);
        }

        // Percussion.
        for (int s = 0; s < 16; ++s) {
            const float t = t0 + static_cast<float>(s) * six;
            const bool kick = song.style == 0 ? (s == 0 || s == 10) : song.style == 1 ? (s % 8 == 0) : (s % 4 == 0);
            const bool snare = song.style != 0 && (s == 4 || s == 12);
            const bool hat = song.style == 0 ? (s % 4 == 2) : (s % 2 == 0);
            if (kick) {
                Note k = sweep(t, 0.12f, 130.0f, 45.0f, Wave::Sine, song.style == 0 ? 0.16f : 0.34f, 14.0f);
                mix.render(k);
            }
            if (snare) {
                mix.render(noise(t, 0.12f, 0.11f, 0.4f, 24.0f));
                mix.render(sweep(t, 0.06f, 220.0f, 160.0f, Wave::Sine, 0.12f, 30.0f));
            }
            if (hat) {
                Note hh = noise(t, 0.03f, song.style == 0 ? 0.03f : 0.028f, 0.0f, 60.0f, Wave::Hiss);
                hh.pan = 0.35f;
                mix.render(hh);
            }
        }
        if (song.style == 2 && b % 2 == 1) {
            // Tom fill at the end of every other bar.
            for (int i = 0; i < 3; ++i)
                mix.render(sweep(t0 + bar - (3 - i) * six, six * 0.9f, 200.0f - i * 40.0f, 90.0f, Wave::Sine, 0.3f, 10.0f));
        }
    }
    return mix.finish(0.62f);
}

// --- WAV container -------------------------------------------------------------------------

std::vector<uint8_t> toWav(const Buffer& buffer) {
    const uint32_t dataBytes = static_cast<uint32_t>(buffer.samples.size() * sizeof(int16_t));
    std::vector<uint8_t> out(44 + dataBytes);
    auto put32 = [&](size_t at, uint32_t v) {
        for (int i = 0; i < 4; ++i) out[at + static_cast<size_t>(i)] = static_cast<uint8_t>(v >> (8 * i));
    };
    auto put16 = [&](size_t at, uint16_t v) {
        out[at] = static_cast<uint8_t>(v);
        out[at + 1] = static_cast<uint8_t>(v >> 8);
    };
    std::memcpy(out.data(), "RIFF", 4);
    put32(4, 36 + dataBytes);
    std::memcpy(out.data() + 8, "WAVEfmt ", 8);
    put32(16, 16);                 // fmt chunk size
    put16(20, 1);                  // PCM
    put16(22, 2);                  // stereo
    put32(24, kSampleRate);
    put32(28, kSampleRate * 4);    // byte rate
    put16(32, 4);                  // block align
    put16(34, 16);                 // bits per sample
    std::memcpy(out.data() + 36, "data", 4);
    put32(40, dataBytes);
    // Little-endian samples.
    for (size_t i = 0; i < buffer.samples.size(); ++i)
        put16(44 + i * 2, static_cast<uint16_t>(buffer.samples[i]));
    return out;
}

} // namespace pd::synth
