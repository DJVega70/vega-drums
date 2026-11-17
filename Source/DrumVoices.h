#pragma once
#include <juce_core/juce_core.h>
#include <cmath>
#include <array>

namespace vdr
{
constexpr int numVoices = 10;
inline const char* voiceName (int v)
{
    static const char* n[] { "KICK", "SNARE", "CLAP", "RIM", "CLOSED HAT", "OPEN HAT", "LOW TOM", "HIGH TOM", "RIDE", "SHAKER" };
    return n[v];
}
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
    juce::Random rng { 1234 };
    // settings
    float tune = 0, decay = 0.5f, tone = 0.5f; int kit = 0;

    void trigger (float velocity) { active = true; t = 0; vel = velocity; env = 1; phase = phase2 = 0; clickEnv = 1; mph = {}; }
    void choke() { if (active) decayFast = true; }
    bool decayFast = false;

    float noise() { return rng.nextFloat() * 2.0f - 1.0f; }
    static float tri (float ph) { return 4.0f * std::abs (ph - std::floor (ph + 0.5f)) - 1.0f; }

    float next()
    {
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
            default: break;
        }
        t += dt;
        return y * vel;
    }
};
} // namespace vdr
