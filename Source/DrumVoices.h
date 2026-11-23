#pragma once
#include <juce_core/juce_core.h>
#include <cmath>
#include <array>

namespace vdr
{
constexpr int numVoices = 10;           // the original ten (pads A01-A10 of the default kit)
/** every sound source a pad can use */
enum Source { srcKick, srcSnare, srcClap, srcRim, srcClosedHat, srcOpenHat, srcLowTom, srcHighTom, srcRide, srcShaker,
              srcCowbell, srcCongaHi, srcCongaLo, srcTambourine, srcCrash, srcClave,
              srcOrgan, srcPiano, srcStrings, srcAcidBass, srcSubBass, srcSample, numSources };
inline const char* sourceName (int v)
{
    static const char* n[] { "KICK", "SNARE", "CLAP", "RIM", "CLOSED HAT", "OPEN HAT", "LOW TOM", "HIGH TOM", "RIDE", "SHAKER",
                             "COWBELL", "CONGA HI", "CONGA LO", "TAMBOURINE", "CRASH", "CLAVE",
                             "ORGAN STAB", "PIANO STAB", "STRINGS", "ACID BASS", "SUB BASS", "SAMPLE" };
    return n[juce::jlimit (0, (int) numSources - 1, v)];
}
inline const char* voiceName (int v) { return sourceName (v); }
/** keys, strings and basses are pitched and hold while the note is held */
inline bool isMelodic (int src) { return src >= srcOrgan && src <= srcSubBass; }
enum Chord { chMin7, chMaj7, chMin9, chDom7, chMinor, chMajor, chSus4, chSingle, numChords };
inline const char* chordName (int c) { static const char* n[] { "m7", "maj7", "m9", "7", "m", "maj", "sus4", "note" }; return n[juce::jlimit (0, (int) numChords - 1, c)]; }
inline int chordNotes (int c, int* out)
{
    static const int t[numChords][5] { { 0, 3, 7, 10, -1 }, { 0, 4, 7, 11, -1 }, { 0, 3, 10, 14, 7 }, { 0, 4, 7, 10, -1 }, { 0, 3, 7, -1, -1 }, { 0, 4, 7, -1, -1 }, { 0, 5, 7, -1, -1 }, { 0, -1, -1, -1, -1 } };
    int n = 0; for (int k = 0; k < 5; ++k) if (t[c][k] >= 0) out[n++] = t[c][k];
    return n;
}

/** a decoded sample (from your WAV / AIFF / FLAC / MP3, or from inside a zip) */
struct SampleData
{
    juce::AudioBuffer<float> audio;    // always 2 channels
    double rate = 44100; juce::String name, path;
};
inline int gmNote (int v) { static const int n[] { 36, 38, 39, 37, 42, 46, 45, 50, 51, 70 }; return n[v]; }
inline int voiceForNote (int note)
{
    for (int v = 0; v < numVoices; ++v) if (gmNote (v) == note) return v;
    switch (note) { case 35: return 0; case 40: return 1; case 44: return 4; case 41: case 43: return 6; case 47: case 48: return 7; case 59: case 53: return 8; case 69: case 82: return 9; default: return -1; }
}
enum Kit { K909, K808, KDeep };

struct OnePole { float z = 0; float lp (float x, float a) { z += a * (x - z); return z; } float hp (float x, float a) { return x - lp (x, a); } };
struct Bp     // state-variable band-pass (TPT)
{
    float ic1 = 0, ic2 = 0;
    float process (float x, float g, float k, int out = 1)
    {
        const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2;
        return out == 1 ? v1 * k : (out == 0 ? v2 : x - k * v1 - v2);
    }
};

/** One drum voice. Parameters are read at trigger time and per block. */
struct Voice
{
    int type = 0; double sr = 48000;
    bool active = false; double t = 0;     // seconds since trigger
    float vel = 1, env = 0, phase = 0, phase2 = 0, clickEnv = 0;
    std::array<float, 6> mph {};           // metallic oscillator phases
    Bp bp1, bp2; OnePole hp1, hp2, lp1;
    float sideR = 0;                       // stereo difference for samples (0 for synth voices)
    juce::Random rng { 1234 };
    // settings
    float tune = 0, decay = 0.5f, tone = 0.5f; int kit = 0, chord = chMin7;
    std::shared_ptr<const SampleData> sample; double spos = 0;
    // keys / strings / bass state
    std::array<float, 40> kph {};          // oscillator phases: 5 notes x 8 partials (or detuned saws)
    float lad[4] {}, gateEnv = 0, relMul = 1; bool held = false, releasing = false;
    float fade = 1.0f; bool fading = false;

    void trigger (float velocity)
    {
        active = true; t = 0; vel = velocity; env = 1; phase = phase2 = 0; clickEnv = 1; mph = {}; kph = {};
        for (auto& z : lad) z = 0;
        held = true; releasing = false; relMul = 1; gateEnv = 0; spos = 0; fade = 1; fading = false; decayFast = false;
    }
    void release() { held = false; }
    /** 3 ms fade-out, used when the pad is hit again while this voice still rings */
    void stopQuickly() { if (active) fading = true; }
    void choke() { if (active) decayFast = true; }
    bool decayFast = false;
    static float midiHz (float n) { return 440.0f * std::pow (2.0f, (n - 69.0f) / 12.0f); }
    /** release envelope for held sounds: 1 while held, then falls with time constant `secs` */
    float gate (float dt, float secs)
    {
        if (! held) relMul *= std::exp (-dt / std::max (0.005f, secs));
        return relMul;
    }

    float noise() { return rng.nextFloat() * 2.0f - 1.0f; }
    static float tri (float ph) { return 4.0f * std::abs (ph - std::floor (ph + 0.5f)) - 1.0f; }

    float next()
    {
        sideR = 0;
        if (! active) return 0.0f;
        const float dt = (float) (1.0 / sr);
        const float pitch = std::pow (2.0f, tune / 12.0f);
        float y = 0;
        auto expDecay = [&] (float secs) { return std::exp (-dt / std::max (0.002f, secs)); };
        const float tt = (float) t;
        switch (type)
        {
            case 0:  // kick: sine with pitch sweep + click
            {
                const bool k808 = kit == K808, deep = kit == KDeep;
                const float base = (k808 ? 47.0f : deep ? 50.0f : 54.0f) * pitch;
                const float sweep = k808 ? 2.2f : deep ? 2.8f : 3.6f;
                const float sweepT = k808 ? 0.045f : 0.028f;
                const float f = base * (1.0f + (sweep - 1.0f) * std::exp (-tt / sweepT));
                phase += f * dt; phase -= std::floor (phase);
                const float d = (k808 ? 0.12f : deep ? 0.08f : 0.05f) + decay * (k808 ? 0.8f : deep ? 0.4f : 0.25f);
                env *= expDecay (d);
                float body = std::sin (juce::MathConstants<float>::twoPi * phase) * env;
                if (! k808) body = std::tanh (body * (1.6f + tone)) / std::tanh (1.6f + tone);
                clickEnv *= expDecay (0.004f);
                const float click = hp1.hp (noise(), 0.3f) * clickEnv * tone * (k808 ? 0.3f : 0.9f);
                y = body + click;
                if (tt > 0.05f && env < 3.0e-4f) active = false;
                break;
            }
            case 1:  // snare: two tuned bodies + filtered noise
            {
                phase += 185.0f * pitch * dt; phase -= std::floor (phase);
                phase2 += 330.0f * pitch * dt; phase2 -= std::floor (phase2);
                env *= expDecay (0.08f + decay * 0.12f);
                clickEnv *= expDecay (0.06f + decay * 0.25f);
                const float body = (std::sin (juce::MathConstants<float>::twoPi * phase) * 0.6f + std::sin (juce::MathConstants<float>::twoPi * phase2) * 0.4f) * env;
                const float nz = hp1.hp (noise(), 0.2f);
                const float snappy = bp1.process (nz, std::tan (juce::MathConstants<float>::pi * std::min (7000.0f * pitch, (float) sr * 0.45f) / (float) sr), 1.2f) * clickEnv;
                y = body * (1.1f - tone) + snappy * (0.4f + tone * 1.2f);
                if (clickEnv < 1.0e-4f && env < 1.0e-4f) active = false;
                break;
            }
            case 2:  // clap: band-passed noise bursts then a tail
            {
                const float nz = bp1.process (noise(), std::tan (juce::MathConstants<float>::pi * 1150.0f * pitch / (float) sr), 1.0f / (1.5f + tone * 3.0f));
                float e = 0;
                for (int k = 0; k < 3; ++k) { const float tk = tt - k * 0.0105f; if (tk >= 0 && tk < 0.011f) e = std::max (e, std::exp (-tk / 0.0025f)); }
                const float tail = tt >= 0.021f ? std::exp (-(tt - 0.021f) / (0.08f + decay * 0.35f)) : 0.0f;
                e = std::max (e, tail);
                y = nz * e * 2.2f;
                if (tt > 0.05f && tail < 1.0e-4f) active = false;
                break;
            }
            case 3:  // rim shot
            {
                phase += 1700.0f * pitch * dt; phase -= std::floor (phase);
                phase2 += 480.0f * pitch * dt; phase2 -= std::floor (phase2);
                env *= expDecay (0.012f + decay * 0.03f);
                const float x = (tri (phase) * 0.6f + std::sin (juce::MathConstants<float>::twoPi * phase2) * 0.6f + noise() * 0.2f * tone) * env;
                y = hp1.hp (x, 0.08f) * 1.4f;
                if (env < 1.0e-4f) active = false;
                break;
            }
            case 4: case 5: case 8:   // hats and ride: six square oscillators (808 ratios) + noise, high-passed
            {
                static const float ratio[] { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
                const float mult = (type == 8 ? 1.4f : 1.65f) * pitch;
                float m = 0;
                for (int k = 0; k < 6; ++k) { mph[(size_t) k] += ratio[k] * mult * dt; mph[(size_t) k] -= std::floor (mph[(size_t) k]); m += mph[(size_t) k] < 0.5f ? 1.0f : -1.0f; }
                m /= 6.0f;
                const float nzAmt = kit == K808 ? 0.15f : 0.55f;
                float x = m * (1.0f - nzAmt) + noise() * nzAmt;
                const float centre = type == 8 ? 4200.0f : 9000.0f;
                x = bp1.process (x, std::tan (juce::MathConstants<float>::pi * std::min (centre * (0.7f + tone * 0.6f), (float) sr * 0.42f) / (float) sr), 0.8f);
                x = hp2.hp (x, 0.35f);
                const float d = type == 4 ? 0.025f + decay * 0.08f : type == 5 ? 0.08f + decay * 0.4f : 0.25f + decay * 0.9f;
                env *= expDecay (decayFast ? 0.006f : d);
                y = x * env * (type == 8 ? 1.4f : 2.0f);
                if (env < 3.0e-4f) { active = false; decayFast = false; }
                break;
            }
            case 6: case 7:  // toms
            {
                const float base = (type == 6 ? 95.0f : 165.0f) * pitch;
                const float f = base * (1.0f + 0.35f * std::exp (-tt / 0.06f));
                phase += f * dt; phase -= std::floor (phase);
                env *= expDecay (0.12f + decay * 0.5f);
                clickEnv *= expDecay (0.008f);
                y = std::sin (juce::MathConstants<float>::twoPi * phase) * env + lp1.lp (noise(), 0.3f) * clickEnv * tone * 0.5f;
                if (env < 1.0e-4f) active = false;
                break;
            }
            case 9:  // shaker: high noise with a soft attack
            {
                const float atk = std::min (1.0f, tt / 0.008f);
                env *= expDecay (0.03f + decay * 0.12f);
                float x = hp1.hp (hp2.hp (noise(), 0.6f), 0.6f);
                x = bp1.process (x, std::tan (juce::MathConstants<float>::pi * std::min (8000.0f * pitch, (float) sr * 0.42f) / (float) sr), 1.0f / (0.7f + tone));
                y = x * env * atk * 1.6f;
                if (tt > 0.01f && env < 1.0e-4f) active = false;
                break;
            }
            case srcCowbell:   // 808 cowbell: two square waves through a band-pass, fast drop then a ring
            {
                phase += 540.0f * pitch * dt; phase -= std::floor (phase);
                phase2 += 800.0f * pitch * dt; phase2 -= std::floor (phase2);
                const float x = (phase < 0.5f ? 1.0f : -1.0f) + (phase2 < 0.5f ? 1.0f : -1.0f);
                env *= expDecay (0.06f + decay * 0.35f);
                clickEnv *= expDecay (0.012f);
                const float b = bp1.process (x, std::tan (juce::MathConstants<float>::pi * std::min (2600.0f * (0.7f + tone * 0.6f) * pitch, (float) sr * 0.42f) / (float) sr), 0.9f);
                y = b * (env * 0.55f + clickEnv * 0.45f) * 0.8f;
                if (env < 2.0e-4f) active = false;
                break;
            }
            case srcCongaHi: case srcCongaLo:   // 808 / 727 congas: sine with a little pitch drop and a slap
            {
                const float base = (type == srcCongaHi ? 370.0f : 250.0f) * pitch;
                const float f = base * (1.0f + 0.12f * std::exp (-tt / 0.02f));
                phase += f * dt; phase -= std::floor (phase);
                env *= expDecay (0.1f + decay * 0.35f);
                clickEnv *= expDecay (0.005f);
                y = std::sin (juce::MathConstants<float>::twoPi * phase) * env + hp1.hp (noise(), 0.4f) * clickEnv * (0.2f + tone * 0.5f);
                if (env < 1.0e-4f) active = false;
                break;
            }
            case srcTambourine:   // jingles: a few quick bursts of bright metallic noise
            {
                float e = 0;
                for (int k = 0; k < 4; ++k) { const float tk = tt - k * 0.017f; if (tk >= 0) e += std::exp (-tk / (0.03f + decay * 0.18f)) * (k == 0 ? 1.0f : 0.45f); }
                float x = hp1.hp (hp2.hp (noise(), 0.7f), 0.7f);
                x = bp1.process (x, std::tan (juce::MathConstants<float>::pi * std::min (7500.0f * pitch * (0.8f + tone * 0.4f), (float) sr * 0.42f) / (float) sr), 0.5f) * 0.6f
                  + bp2.process (x, std::tan (juce::MathConstants<float>::pi * std::min (10500.0f * pitch, (float) sr * 0.45f) / (float) sr), 0.6f) * 0.5f;
                y = x * e * 1.6f;
                if (tt > 0.08f && e < 1.0e-4f) active = false;
                break;
            }
            case srcCrash:   // 909-style crash: dense metallic cluster + noise, long tail
            {
                static const float ratio[] { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f };
                float m = 0;
                for (int k = 0; k < 6; ++k) { mph[(size_t) k] += ratio[k] * 2.3f * pitch * dt; mph[(size_t) k] -= std::floor (mph[(size_t) k]); m += mph[(size_t) k] < 0.5f ? 1.0f : -1.0f; }
                float x = m / 6.0f * 0.5f + noise() * 0.5f;
                x = bp1.process (x, std::tan (juce::MathConstants<float>::pi * std::min (6500.0f * (0.7f + tone * 0.6f), (float) sr * 0.42f) / (float) sr), 0.6f);
                x = hp2.hp (x, 0.25f);
                env *= expDecay (decayFast ? 0.006f : 0.3f + decay * 0.9f);
                clickEnv *= expDecay (0.02f);
                y = x * (env * 1.3f + clickEnv * 0.8f);
                if (env < 2.0e-4f) active = false;
                break;
            }
            case srcClave:   // 808 clave: a short, high wooden ping
            {
                phase += 2500.0f * pitch * dt; phase -= std::floor (phase);
                env *= expDecay (0.018f + decay * 0.04f);
                y = std::sin (juce::MathConstants<float>::twoPi * phase) * env * 0.9f;
                if (env < 1.0e-4f) active = false;
                break;
            }
            case srcOrgan:   // house organ stab (M1-style): 16' + 8' + 4' + 2 2/3' drawbars, a fast 3rd-harmonic "percussion" and key click
            {
                int notes[5]; const int nn = chordNotes (chord, notes);
                const float root = 60.0f + tune;    // C4 + tune
                static const float mult[] { 0.5f, 1.0f, 2.0f, 3.0f, 4.0f, 6.0f };
                static const float amp[]  { 0.55f, 1.0f, 0.75f, 0.0f, 0.35f, 0.15f };
                const float percEnv = std::exp (-tt / (0.05f + tone * 0.1f));
                float x = 0;
                for (int k = 0; k < nn; ++k)
                {
                    const float f = midiHz (root + (float) notes[k]);
                    for (int h = 0; h < 6; ++h)
                    {
                        float& ph = kph[(size_t) (k * 8 + h)];
                        if (f * mult[h] > (float) sr * 0.45f) continue;
                        ph += f * mult[h] * dt; ph -= std::floor (ph);
                        const float a = h == 3 ? percEnv * (0.6f + tone * 0.8f) : amp[h] * (h >= 4 ? 0.4f + tone : 1.0f);
                        x += std::sin (juce::MathConstants<float>::twoPi * ph) * a;
                    }
                }
                x /= (float) nn * 1.8f;
                clickEnv *= expDecay (0.002f);
                x += hp1.hp (noise(), 0.5f) * clickEnv * 0.12f;
                const float atk = std::min (1.0f, tt / 0.002f);
                env *= expDecay (0.3f + decay * 3.0f);      // organ holds; Decay sets how long a hit rings if not held
                const float g = gate (dt, 0.05f + decay * 0.15f);
                y = x * atk * std::max (env, held ? 0.6f : 0.0f) * g;
                if (g < 1.0e-4f || (env < 1.0e-4f && ! held)) active = false;
                break;
            }
            case srcPiano:   // house piano stab (M1-style): bright partials that die quickly, a hammer thump, slight chorus
            {
                int notes[5]; const int nn = chordNotes (chord, notes);
                const float root = 60.0f + tune;
                float x = 0;
                for (int k = 0; k < nn; ++k)
                {
                    const float f = midiHz (root + (float) notes[k]);
                    for (int h = 0; h < 8; ++h)
                    {
                        float& ph = kph[(size_t) (k * 8 + h)];
                        const float fh = f * (float) (h + 1) * (1.0f + 0.0007f * (float) (h * h)) * (h % 2 ? 1.0015f : 1.0f);   // stiff-string stretch + detune
                        if (fh > (float) sr * 0.45f) continue;
                        ph += fh * dt; ph -= std::floor (ph);
                        const float pd = (0.6f + decay * 2.4f) / (1.0f + (float) h * (1.2f - tone * 0.6f));
                        x += std::sin (juce::MathConstants<float>::twoPi * ph) * std::exp (-tt / pd) / (1.0f + (float) h * 0.55f);
                    }
                }
                x /= (float) nn * 1.3f;
                clickEnv *= expDecay (0.006f);
                x += lp1.lp (noise(), 0.25f) * clickEnv * 0.35f;
                const float g = gate (dt, 0.12f + decay * 0.2f);
                y = x * g * std::min (1.0f, tt / 0.001f);
                if (g < 1.0e-4f || tt > 1.0f + decay * 6.0f) active = false;
                break;
            }
            case srcStrings:   // string machine pad: seven detuned saws per note, slow attack, soft top
            {
                int notes[5]; const int nn = std::min (4, chordNotes (chord, notes));
                const float root = 60.0f + tune;
                static const float det[] { -0.011f, -0.006f, -0.002f, 0.0f, 0.003f, 0.007f, 0.012f };
                float x = 0;
                for (int k = 0; k < nn; ++k)
                {
                    const float f = midiHz (root + (float) notes[k]);
                    for (int d = 0; d < 7; ++d)
                    {
                        float& ph = kph[(size_t) (k * 8 + d)];
                        ph += f * (1.0f + det[d]) * dt; ph -= std::floor (ph);
                        x += 2.0f * ph - 1.0f;
                    }
                }
                x /= (float) nn * 5.0f;
                const float a = 1.0f - std::exp (-tt / (0.06f + decay * 0.25f));
                x = lp1.lp (x, 0.08f + tone * 0.35f);
                x = hp1.hp (x, 0.01f);
                env *= expDecay (1.5f + decay * 6.0f);
                const float g = gate (dt, 0.25f + decay * 0.8f);
                y = x * a * std::max (env, held ? 0.7f : 0.0f) * g * 1.6f;
                if (g < 1.0e-4f || (env < 1.0e-4f && ! held)) active = false;
                break;
            }
            case srcAcidBass:   // 303-style: saw into a squelchy 4-pole low-pass, envelope on the cutoff
            {
                const float f = midiHz (36.0f + tune);           // C2 + tune
                phase += f * dt; phase -= std::floor (phase);
                const float saw = 2.0f * phase - 1.0f;
                const float fEnv = std::exp (-tt / (0.05f + decay * 0.4f));
                const float cutoff = std::min ((float) sr * 0.4f, 120.0f + (200.0f + tone * 2800.0f) * (0.25f + fEnv * vel));
                const float gC = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * cutoff / (float) sr);
                const float res = 3.2f;
                float in = std::tanh (saw - res * lad[3]);
                for (auto& z : lad) { z += gC * (in - z); in = z; }
                env *= expDecay (0.15f + decay * 0.9f);
                const float g = gate (dt, 0.02f);
                y = std::tanh (lad[3] * 2.2f) * std::max (env, held ? 0.5f : 0.0f) * g * 0.9f;
                if (g < 1.0e-4f || (env < 1.0e-4f && ! held)) active = false;
                break;
            }
            case srcSubBass:   // 808 sub: sine with a little drop, gently driven
            {
                const float f = midiHz (36.0f + tune) * (1.0f + 0.25f * std::exp (-tt / 0.012f));
                phase += f * dt; phase -= std::floor (phase);
                env *= expDecay (0.25f + decay * 1.8f);
                const float g = gate (dt, 0.04f);
                const float x = std::sin (juce::MathConstants<float>::twoPi * phase);
                y = std::tanh (x * (1.0f + tone * 2.0f)) / std::tanh (1.0f + tone * 2.0f) * env * g * std::min (1.0f, tt / 0.002f);
                if (g < 1.0e-4f || env < 1.0e-4f) active = false;
                break;
            }
            case srcSample:   // your sample: resampled for the tune, Decay shortens it, Tone is a low-pass
            {
                if (sample == nullptr || sample->audio.getNumSamples() == 0) { active = false; break; }
                const int len = sample->audio.getNumSamples();
                const double ratio = sample->rate / sr * std::pow (2.0, tune / 12.0);
                const int i0 = (int) spos; const float fr = (float) (spos - i0);
                if (i0 + 1 >= len) { active = false; break; }
                const float* L = sample->audio.getReadPointer (0); const float* R = sample->audio.getReadPointer (1);
                const float l = L[i0] + (L[i0 + 1] - L[i0]) * fr, r = R[i0] + (R[i0 + 1] - R[i0]) * fr;
                spos += ratio;
                // Decay < 100% fades the sample out early; held pads stop on release when Decay is low
                const float maxT = decay >= 0.99f ? 1.0e9f : 0.02f + decay * decay * 4.0f;
                const float e = tt < maxT ? 1.0f : std::exp (-(tt - maxT) / 0.03f);
                const float a = tone >= 0.99f ? 1.0f : 0.02f + tone * tone * 0.98f;
                const float m = (l + r) * 0.5f;
                y = lp1.lp (m, a) * e;
                sideR = (r - l) * 0.5f * e;      // keep the stereo width
                if (e < 1.0e-4f) active = false;
                break;
            }
            default: break;
        }
        t += dt;
        if (fading) { fade -= dt / 0.003f; if (fade <= 0.0f) { active = false; fade = 0.0f; } y *= std::max (0.0f, fade); sideR *= std::max (0.0f, fade); }
        return y * vel;
    }
};
} // namespace vdr
