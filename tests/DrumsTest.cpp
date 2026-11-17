#include "../Source/PluginProcessor.h"
#include <iostream>
using namespace vdr;
static int fails = 0;
static void check (bool ok, const juce::String& m) { std::cout << (ok ? "PASS " : "FAIL ") << m << "\n"; if (! ok) ++fails; }
static void set (VegaDrumsProcessor& p, const char* id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }
const double fs = 48000; const int blk = 480;
struct Head : juce::AudioPlayHead
{
    double ppq = 0; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo i; i.setBpm (120); i.setPpqPosition (ppq); i.setIsPlaying (playing); return i; }
};

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    // ---- every voice on every kit: makes sound, stays finite, dies away
    for (int kit = 0; kit < 3; ++kit)
    {
        bool ok = true; juce::String bad;
        for (int v = 0; v < numVoices; ++v)
        {
            Voice vo; vo.type = v; vo.sr = fs; vo.kit = kit; vo.trigger (1.0f);
            double e = 0; float pk = 0; int len = 0;
            for (int i = 0; i < (int) fs * 8 && vo.active; ++i) { const float y = vo.next(); if (! std::isfinite (y)) { ok = false; break; } e += y * y; pk = std::max (pk, std::abs (y)); ++len; }
            if (pk < 0.05f || pk > 3.0f || vo.active) { ok = false; bad << voiceName (v) << " (peak " << pk << (vo.active ? ", never ends" : "") << ") "; }
        }
        check (ok, "kit " + juce::String (kit) + ": all 10 voices sound, stay finite and decay " + bad);
    }
    {   // kick pitch settles near its base frequency (909 = 54 Hz)
        Voice vo; vo.type = 0; vo.sr = fs; vo.kit = K909; vo.trigger (1); std::vector<float> y; for (int i = 0; i < 24000; ++i) y.push_back (vo.next());
        int zc = 0; for (int i = 9600 + 1; i < 14400; ++i) zc += (y[(size_t) i - 1] < 0) != (y[(size_t) i] < 0);
        const double f = zc / 2.0 / 0.1;
        check (std::abs (f - 54) < 6, "909 kick body settles at ~" + juce::String (f, 1) + " Hz");
        vo.tune = 12; vo.trigger (1); y.clear(); for (int i = 0; i < 24000; ++i) y.push_back (vo.next());
        zc = 0; for (int i = 9600 + 1; i < 14400; ++i) zc += (y[(size_t) i - 1] < 0) != (y[(size_t) i] < 0);
        check (std::abs (zc / 0.2 - 108) < 10, "tune +12 doubles it (" + juce::String (zc / 0.2, 1) + " Hz)");
    }
    auto make = [&] { auto* p = new VegaDrumsProcessor(); p->setPlayConfigDetails (0, 2, fs, blk); p->prepareToPlay (fs, blk); return std::unique_ptr<VegaDrumsProcessor> (p); };
    // onset detector on channel 0
    auto onsets = [] (const std::vector<float>& o) { std::vector<int> r; float env = 0; bool armed = true; for (int i = 0; i < (int) o.size(); ++i) { env = std::max (std::abs (o[(size_t) i]), env * 0.999f); if (armed && env > 0.1f) { r.push_back (i); armed = false; } if (env < 0.02f) armed = true; } return r; };
    {   // sequencer: kick-only four on the floor at 120 BPM -> hits every 24000 samples
        auto p = make(); Head head; p->setPlayHead (&head);
        for (int v = 1; v < numVoices; ++v) set (*p, ("mute" + juce::String (v)).toRawUTF8(), 1);
        set (*p, "pattern", 0); set (*p, "humanize", 0); set (*p, "decay0", 0.1f);
        juce::AudioBuffer<float> b (2, blk); juce::MidiBuffer m; std::vector<float> out;
        for (int k = 0; k < 200; ++k) { head.ppq = k * blk / 24000.0; p->processBlock (b, m); for (int i = 0; i < blk; ++i) out.push_back (b.getSample (0, i)); }
        auto on = onsets (out); bool ok = on.size() >= 3; juce::String pos;
        for (size_t i = 0; i < on.size(); ++i) { pos << on[i] << " "; ok &= std::abs (on[i] - (int) i * 24000) < 40; }
        check (ok, "four-on-the-floor kick lands on every beat: " + pos);
        p->setPlayHead (nullptr);
    }
    {   // swing pushes odd 16ths later: closed hat on all 16ths, compare spacing
        auto p = make(); Head head; p->setPlayHead (&head);
        for (int v = 0; v < numVoices; ++v) if (v != 4) set (*p, ("mute" + juce::String (v)).toRawUTF8(), 1);
        set (*p, "pattern", 7); set (*p, "humanize", 0); set (*p, "swing", 66); set (*p, "decay4", 0);
        for (int s = 0; s < 16; ++s) p->steps[7][4][(size_t) s].store (2);
        juce::AudioBuffer<float> b (2, blk); juce::MidiBuffer m; std::vector<float> out;
        for (int k = 0; k < 100; ++k) { head.ppq = k * blk / 24000.0; p->processBlock (b, m); for (int i = 0; i < blk; ++i) out.push_back (b.getSample (0, i)); }
        auto on = onsets (out);
        const bool ok = on.size() > 6 && on[1] - on[0] > on[2] - on[1] + 2000;
        check (ok, "swing 66%: long-short 16ths (" + (on.size() > 2 ? juce::String (on[1] - on[0]) + " / " + juce::String (on[2] - on[1]) : juce::String ("?")) + " samples)");
        p->setPlayHead (nullptr);
    }
    {   // MIDI: note 38 plays the snare
        auto p = make(); juce::AudioBuffer<float> b (2, blk); juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (10, 38, (juce::uint8) 127), 100);
        p->processBlock (b, m);
        check (b.getMagnitude (0, 0, 100) < 1e-6f && b.getMagnitude (0, 100, blk - 100) > 0.05f, "MIDI note 38 triggers the snare at the right sample");
    }
    {   // multi-out: enable the kick bus -> kick leaves the main mix
        auto* raw = new VegaDrumsProcessor();
        auto layout = raw->getBusesLayout(); layout.outputBuses.getReference (1) = juce::AudioChannelSet::stereo();
        check (raw->setBusesLayout (layout), "kick output bus can be enabled");
        std::unique_ptr<VegaDrumsProcessor> p (raw); p->setRateAndBufferSizeDetails (fs, blk); p->prepareToPlay (fs, blk);
        juce::AudioBuffer<float> b (p->getTotalNumOutputChannels(), blk); juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 127), 0);
        p->processBlock (b, m);
        check (b.getMagnitude (0, 0, blk) < 1e-6f && b.getMagnitude (2, 0, blk) > 0.05f, "kick plays on its own output, main is silent");
    }
    {   // open hat is choked by the closed hat
        auto p = make(); juce::AudioBuffer<float> b (2, blk); juce::MidiBuffer m;
        set (*p, "decay5", 1.0f); set (*p, "level4", 0.0f);
        m.addEvent (juce::MidiMessage::noteOn (10, 46, (juce::uint8) 127), 0); p->processBlock (b, m); m.clear();
        for (int k = 0; k < 5; ++k) p->processBlock (b, m);
        const float before = b.getMagnitude (0, 0, blk);
        m.addEvent (juce::MidiMessage::noteOn (10, 42, (juce::uint8) 127), 0); p->processBlock (b, m); m.clear();
        for (int k = 0; k < 5; ++k) p->processBlock (b, m);
        check (before > 0.01f && b.getMagnitude (0, 0, blk) < before * 0.05f, "closed hat chokes the open hat (" + juce::String (before, 3) + " -> " + juce::String (b.getMagnitude (0, 0, blk), 5) + ")");
    }
    {   // MIDI export
        auto p = make(); auto f = p->exportMidi();
        juce::MidiFile mf; juce::FileInputStream in (f); const bool ok = in.openedOk() && mf.readFrom (in);
        int notes = 0; if (ok) for (auto* e : *mf.getTrack (0)) if (e->message.isNoteOn()) ++notes;
        check (ok && notes == 4 + 2 + 4 + 8, "pattern A exports as MIDI (" + juce::String (notes) + " notes)");
        f.deleteFile();
    }
    {   // state keeps patterns
        auto p = make(); p->clearPattern (3); p->steps[3][2][5].store (2); set (*p, "swing", 61);
        juce::MemoryBlock mb; p->getStateInformation (mb);
        auto q = make(); q->setStateInformation (mb.getData(), (int) mb.getSize());
        check (q->steps[3][2][5].load() == 2 && q->steps[3][0][0].load() == 0 && std::abs (q->apvts.getRawParameterValue ("swing")->load() - 61) < 0.5f, "patterns and settings saved with the project");
    }
    std::cout << (fails == 0 ? "ALL PASSED" : "FAILURES: " + std::to_string (fails)) << "\n";
    return fails == 0 ? 0 : 1;
}
