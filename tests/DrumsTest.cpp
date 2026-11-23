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
        p->setSequence (0, sequenceFromRows ("kick", 1, { { 0, "x...x...x...x..." } }));
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
        p->setSequence (7, sequenceFromRows ("hats", 1, { { 4, "XXXXXXXXXXXXXXXX" } }));
        set (*p, "pattern", 7); set (*p, "humanize", 0); set (*p, "swing", 66); set (*p, "decay4", 0);
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
        const int expected = (int) p->getSequence (0)->events.size();
        check (ok && notes == expected && expected > 20, "sequence 1 exports as MIDI (" + juce::String (notes) + " of " + juce::String (expected) + " notes)");
        f.deleteFile();
    }
    {   // state keeps sequences, pads and settings
        auto p = make();
        p->setSequence (3, sequenceFromRows ("Mine", 2, { { 2, "......x" }, { 17, "x---" } })); set (*p, "swing", 61);
        auto pad = p->getPad (40); pad.source = srcPiano; pad.chord = chMaj7; pad.tune = 5; p->setPad (40, pad);
        juce::MemoryBlock mb; p->getStateInformation (mb);
        auto q = make(); q->setStateInformation (mb.getData(), (int) mb.getSize());
        auto s3 = q->getSequence (3);
        const bool seqOk = s3->name == "Mine" && s3->bars == 2 && s3->events.size() == 2 && s3->events[0].pad == 17 && s3->events[0].length == 4 * sixteenth - 20;
        const auto qp = q->getPad (40);
        check (seqOk && qp.source == srcPiano && qp.chord == chMaj7 && std::abs (qp.tune - 5) < 0.01f && std::abs (q->apvts.getRawParameterValue ("swing")->load() - 61) < 0.5f,
               "sequences, pads and settings saved with the project");
    }
    {   // projects from the first Vega Drums: 16-step patterns become sequences
        juce::ValueTree st ("PARAMS"); juce::ValueTree pt ("Patterns");
        juce::String pat; for (int v = 0; v < numVoices; ++v) for (int i = 0; i < 16; ++i) pat << (v == 0 && i % 4 == 0 ? (i == 0 ? "2" : "1") : "0");
        pt.setProperty ("p2", pat, nullptr); st.appendChild (pt, nullptr);
        juce::ValueTree kitP ("PARAM"); kitP.setProperty ("id", "kit", nullptr); kitP.setProperty ("value", 1.0f, nullptr); st.appendChild (kitP, nullptr);
        juce::MemoryBlock mb; { auto x = st.createXml(); juce::AudioProcessor::copyXmlToBinary (*x, mb); }
        auto q = make(); q->setStateInformation (mb.getData(), (int) mb.getSize());
        auto s2 = q->getSequence (2);
        check (s2->bars == 1 && s2->events.size() == 4 && s2->events[0].velocity == 127 && s2->events[1].start == ppqTicks && (int) q->apvts.getRawParameterValue ("kit")->load() == 1,
               "old 16-step patterns load as sequences (" + juce::String ((int) s2->events.size()) + " hits), old kit kept");
    }
    {   // every new sound: makes sound, stays finite, ends (held sounds after release)
        bool ok = true; juce::String bad;
        for (int v = srcCowbell; v < srcSample; ++v)
        {
            Voice vo; vo.type = v; vo.sr = fs; vo.trigger (1.0f);
            float pk = 0;
            for (int i = 0; i < (int) fs * 12 && vo.active; ++i)
            {
                if (i == (int) fs / 2) vo.release();
                const float y = vo.next(); if (! std::isfinite (y)) { ok = false; break; } pk = std::max (pk, std::abs (y));
            }
            if (pk < 0.05f || pk > 3.0f || vo.active) { ok = false; bad << sourceName (v) << " (peak " << pk << (vo.active ? ", never ends" : "") << ") "; }
        }
        check (ok, "cowbell, congas, tambourine, crash, clave, organ, piano, strings, acid and sub bass all sound and end " + bad);
    }
    {   // organ stab is pitched: C minor 7 root at C4 -> strongest low partial near 130.8 Hz (16') / 261.6 Hz
        Voice vo; vo.type = srcOrgan; vo.sr = fs; vo.chord = chSingle; vo.tune = 0; vo.decay = 1; vo.trigger (1);
        std::vector<float> y; for (int i = 0; i < 24000; ++i) y.push_back (vo.next());
        auto mag = [&] (double f) { double re = 0, im = 0; for (int i = 4800; i < 24000; ++i) { re += y[(size_t) i] * std::cos (juce::MathConstants<double>::twoPi * f * i / fs); im += y[(size_t) i] * std::sin (juce::MathConstants<double>::twoPi * f * i / fs); } return std::sqrt (re * re + im * im); };
        check (mag (261.63) > 10 * mag (300.0) && mag (130.81) > 10 * mag (150.0), "organ stab plays C (16' and 8' drawbars at 130.8 / 261.6 Hz)");
    }
    // a WAV to load: a 440 Hz burst, 0.2 s
    const auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("vega-drums-test");
    tmp.deleteRecursively(); tmp.createDirectory();
    auto writeWav = [&] (const juce::File& f, double hz)
    {
        juce::WavAudioFormat wav; juce::AudioBuffer<float> b (1, 9600);
        for (int i = 0; i < 9600; ++i) b.setSample (0, i, 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * hz * i / 48000.0));
        std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
        if (auto* w = wav.createWriterFor (os.get(), 48000, 1, 24, {}, 0)) { os.release(); std::unique_ptr<juce::AudioFormatWriter> wr (w); wr->writeFromAudioSampleBuffer (b, 0, 9600); }
    };
    writeWav (tmp.getChildFile ("Kick 909.wav"), 60); writeWav (tmp.getChildFile ("Snare 1.wav"), 200); writeWav (tmp.getChildFile ("Organ Stab Cm.wav"), 261.63);
    {   // a sample on a pad plays, at the right pitch, from bank D (pad 50)
        auto p = make();
        PadSound s; s.source = srcSample; s.samplePath = tmp.getChildFile ("Organ Stab Cm.wav").getFullPathName(); s.decay = 1; s.tone = 1; s.level = 1;
        p->setPad (50, s);
        p->padDown (50, 1.0f);
        juce::AudioBuffer<float> b (2, blk); juce::MidiBuffer m; std::vector<float> out;
        for (int k = 0; k < 14; ++k) { p->processBlock (b, m); for (int i = 0; i < blk; ++i) out.push_back (b.getSample (0, i)); }
        int zc = 0; for (int i = 1000; i < 5800; ++i) zc += (out[(size_t) i - 1] < 0) != (out[(size_t) i] < 0);
        const double f = zc / 2.0 / (4800.0 / fs);
        check (std::abs (f - 261.6) < 8, "your WAV plays from pad D03 at its own pitch (" + juce::String (f, 1) + " Hz)");
    }
    {   // auto-kit: file names decide the pads, also from inside a zip
        juce::ZipFile::Builder zb;
        for (auto n : { "Kick 909.wav", "Snare 1.wav", "Organ Stab Cm.wav" }) zb.addFile (tmp.getChildFile (n), 0, juce::String ("pack/") + n);
        const auto zipF = tmp.getChildFile ("Old School Pack.zip");
        { juce::FileOutputStream os (zipF); zb.writeToStream (os, nullptr); }
        auto p = make();
        const int placed = p->autoKitFromFolder (zipF, 2);      // banks C + D
        const auto k = p->getPad (32), sn = p->getPad (33), org = p->getPad (48);
        check (placed == 3 && k.samplePath.endsWith ("Kick 909.wav") && sn.samplePath.endsWith ("Snare 1.wav") && org.samplePath.endsWith ("Organ Stab Cm.wav") && p->getPad (0).source == srcKick,
               "auto-kit from a zip: kick -> C01, snare -> C02, organ -> D01, bank A untouched (" + juce::String (placed) + " placed)");
        p->padDown (32, 1.0f); juce::AudioBuffer<float> b (2, blk); juce::MidiBuffer m; p->processBlock (b, m);
        check (b.getMagnitude (0, 0, blk) > 0.05f, "a sample read straight out of the zip plays");
    }
    {   // held keys: a 4-step organ note in the sequence stops when it ends
        auto p = make(); Head head; p->setPlayHead (&head);
        p->setSequence (0, sequenceFromRows ("organ", 1, { { 16, "x---............" } }));
        set (*p, "pattern", 0); set (*p, "humanize", 0);
        juce::AudioBuffer<float> b (2, blk); juce::MidiBuffer m; std::vector<float> out;
        for (int k = 0; k < 100; ++k) { head.ppq = k * blk / 24000.0; p->processBlock (b, m); for (int i = 0; i < blk; ++i) out.push_back (b.getSample (0, i)); }
        auto rms = [&] (int a, int bnd) { double e = 0; for (int i = a; i < bnd; ++i) e += out[(size_t) i] * out[(size_t) i]; return std::sqrt (e / (bnd - a)); };
        // one 16th at 120 BPM = 6000 samples; the note lasts 4 of them (24000) minus a little
        check (rms (2000, 20000) > 0.02 && rms (30000, 46000) < rms (2000, 20000) * 0.05, "organ stab holds for its length, then stops (" + juce::String (rms (2000, 20000), 3) + " -> " + juce::String (rms (30000, 46000), 4) + ")");
        p->setPlayHead (nullptr);
    }
    {   // live recording with timing correct 1/16: a slightly late hit lands on the grid
        auto p = make(); Head head; p->setPlayHead (&head);
        p->setSequence (5, sequenceFromRows ("blank", 1, {})); set (*p, "pattern", 5); set (*p, "tc", 4);
        p->recordArmed.store (true);
        juce::AudioBuffer<float> b (2, blk); juce::MidiBuffer m;
        for (int k = 0; k < 30; ++k)
        {
            head.ppq = k * blk / 24000.0;
            m.clear(); if (k == 13) m.addEvent (juce::MidiMessage::noteOn (10, 39, (juce::uint8) 100), 100);   // ~0.27 beats: near the 2nd 16th (0.25)
            if (k == 15) m.addEvent (juce::MidiMessage::noteOff (10, 39), 0);
            p->processBlock (b, m);
        }
        p->mergeRecorded();
        auto s = p->getSequence (5);
        check (s->events.size() == 1 && s->events[0].pad == 2 && s->events[0].start == sixteenth, "recorded clap is corrected onto the 2nd 16th (" + juce::String (s->events.empty() ? -1 : s->events[0].start) + " ticks)");
        p->setPlayHead (nullptr);
    }
    {   // note repeat: holding a pad at 1/16 retriggers it four times per beat
        auto p = make(); Head head; p->setPlayHead (&head);
        p->setSequence (6, sequenceFromRows ("blank", 1, {})); set (*p, "pattern", 6); set (*p, "tc", 4); set (*p, "noteRepeat", 1); set (*p, "decay4", 0);
        juce::AudioBuffer<float> b (2, blk); juce::MidiBuffer m; std::vector<float> out;
        for (int k = 0; k < 50; ++k)
        {
            head.ppq = k * blk / 24000.0;
            m.clear(); if (k == 0) m.addEvent (juce::MidiMessage::noteOn (10, 42, (juce::uint8) 120), 0);
            p->processBlock (b, m); for (int i = 0; i < blk; ++i) out.push_back (b.getSample (0, i));
        }
        auto on = onsets (out);
        check (on.size() >= 3 && std::abs ((on[2] - on[1]) - 6000) < 200, "note repeat retriggers every 16th (" + juce::String ((int) on.size()) + " hits)");
        p->setPlayHead (nullptr);
    }
    tmp.deleteRecursively();
    if (auto out = juce::SystemStats::getEnvironmentVariable ("VEGA_RENDER", {}); out.isNotEmpty())
    {   // optional: render the factory sequences to a WAV demo (not part of CI)
        juce::AudioBuffer<float> all (2, 0); int pos = 0;
        for (int sq : { 0, 1, 4, 5 })
        {
            auto p = make(); Head head; p->setPlayHead (&head);
            if (sq == 4) p->loadKit (4); if (sq == 5) p->loadKit (7);
            set (*p, "pattern", (float) sq);
            const int bars = p->getSequence (sq)->bars, len = (int) (fs * 60.0 / 124.0 * 4 * bars * (bars == 1 ? 8 : 4));
            all.setSize (2, pos + len, true);
            juce::AudioBuffer<float> b (2, blk); juce::MidiBuffer m;
            for (int k = 0; k * blk < len; ++k)
            {
                head.ppq = k * blk / 24000.0 * 124.0 / 120.0;
                p->processBlock (b, m);
                for (int c = 0; c < 2; ++c) all.copyFrom (c, pos + k * blk, b, c, 0, std::min (blk, len - k * blk));
            }
            pos += len; p->setPlayHead (nullptr);
        }
        juce::WavAudioFormat wav; juce::File f (out); f.deleteFile();
        std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
        if (auto* w = wav.createWriterFor (os.get(), fs, 2, 24, {}, 0)) { os.release(); std::unique_ptr<juce::AudioFormatWriter> wr (w); wr->writeFromAudioSampleBuffer (all, 0, all.getNumSamples()); }
        std::cout << "  rendered " << all.getNumSamples() / fs << " s to " << out << "\n";
    }
    if (auto real = juce::SystemStats::getEnvironmentVariable ("VEGA_REAL_PACK", {}); real.isNotEmpty())
    {   // optional: show how a real pack is sorted (not part of CI)
        auto p = make(); const int n = p->autoKitFromFolder (juce::File (real), 0);
        std::cout << "  " << juce::File (real).getFileName() << ": " << n << " pads\n";
        for (int i = 0; i < 32; ++i) std::cout << "    " << padLabel (i) << "  " << p->getPad (i).samplePath.fromLastOccurrenceOf ("/", false, false) << "\n";
    }
    std::cout << (fails == 0 ? "ALL PASSED" : "FAILURES: " + std::to_string (fails)) << "\n";
    return fails == 0 ? 0 : 1;
}
