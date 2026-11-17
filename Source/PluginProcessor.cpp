#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace vdr;

juce::AudioProcessorValueTreeState::ParameterLayout VegaDrumsProcessor::layout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> v;
    auto pct = [] (float x, int) { return String (roundToInt (x * 100)) + "%"; };
    for (int i = 0; i < numVoices; ++i)
    {
        const String n = String (voiceName (i)).toLowerCase(), s (i);
        const String title = String (voiceName (i)).substring (0, 1) + String (voiceName (i)).substring (1).toLowerCase();
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "level" + s, 1 }, title + " Level", NormalisableRange<float> (0, 1), i == 0 ? 0.9f : 0.75f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tune" + s, 1 }, title + " Tune", NormalisableRange<float> (-12, 12), 0, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return (x > 0 ? "+" : "") + String (x, 1) + " st"; })));
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "decay" + s, 1 }, title + " Decay", NormalisableRange<float> (0, 1), 0.5f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tone" + s, 1 }, title + " Tone", NormalisableRange<float> (0, 1), 0.5f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "pan" + s, 1 }, title + " Pan", NormalisableRange<float> (-1, 1), 0, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return std::abs (x) < 0.01f ? String ("C") : (x < 0 ? "L" : "R") + String (roundToInt (std::abs (x) * 100)); })));
        v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "mute" + s, 1 }, title + " Mute", false));
    }
    v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "pattern", 1 }, "Pattern", StringArray { "A", "B", "C", "D", "E", "F", "G", "H" }, 0));
    v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "kit", 1 }, "Kit", StringArray { "909 House", "808 Deep", "Deep House" }, 0));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "swing", 1 }, "Swing", NormalisableRange<float> (50, 75), 54, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (roundToInt (x)) + "%"; })));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "accent", 1 }, "Accent", NormalisableRange<float> (0, 12), 5, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return "+" + String (x, 1) + " dB"; })));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "humanize", 1 }, "Humanize", NormalisableRange<float> (0, 100), 10, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (roundToInt (x)) + "%"; })));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "master", 1 }, "Master", NormalisableRange<float> (-24, 6), -3, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (x, 1) + " dB"; })));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tempo", 1 }, "Tempo (stand-alone)", NormalisableRange<float> (60, 200), 124, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (x, 1) + " BPM"; })));
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "play", 1 }, "Play (stand-alone)", false));
    return { v.begin(), v.end() };
}

juce::AudioProcessor::BusesProperties VegaDrumsProcessor::makeBuses()
{
    auto b = BusesProperties().withOutput ("Main", juce::AudioChannelSet::stereo(), true);
    for (int v = 0; v < numVoices; ++v)
    {
        juce::String n (voiceName (v)); n = n.substring (0, 1) + n.substring (1).toLowerCase();
        b = b.withOutput (n, juce::AudioChannelSet::stereo(), false);
    }
    return b;
}

VegaDrumsProcessor::VegaDrumsProcessor() : AudioProcessor (makeBuses()), apvts (*this, nullptr, "PARAMS", layout())
{
    for (int v = 0; v < numVoices; ++v)
    {
        const juce::String s (v);
        lvl[v] = apvts.getRawParameterValue ("level" + s); tun[v] = apvts.getRawParameterValue ("tune" + s); dec[v] = apvts.getRawParameterValue ("decay" + s);
        ton[v] = apvts.getRawParameterValue ("tone" + s); pan[v] = apvts.getRawParameterValue ("pan" + s); mute[v] = apvts.getRawParameterValue ("mute" + s);
        voices[(size_t) v].type = v;
        voices[(size_t) v].rng.setSeed (1000 + v);
    }
    pPattern = apvts.getRawParameterValue ("pattern"); pSwing = apvts.getRawParameterValue ("swing"); pKit = apvts.getRawParameterValue ("kit");
    pMaster = apvts.getRawParameterValue ("master"); pTempo = apvts.getRawParameterValue ("tempo"); pPlay = apvts.getRawParameterValue ("play");
    pAccent = apvts.getRawParameterValue ("accent"); pHuman = apvts.getRawParameterValue ("humanize");
    loadFactoryPatterns();
}

void VegaDrumsProcessor::clearPattern (int pat) { for (auto& t : steps[(size_t) pat]) for (auto& s : t) s.store (0); }

void VegaDrumsProcessor::loadFactoryPatterns()
{
    // rows: KICK SNARE CLAP RIM CH OH LTOM HTOM RIDE SHAKER ; '.' off, 'x' on, 'X' accent
    static const char* pats[numPatterns][numVoices] = {
        { "X...X...X...X...", "................", "....X.......X...", "................", "..x...x...x...x.", "................", "................", "................", "................", "x.x.x.x.x.x.x.x." },  // A House
        { "X...X...X...X...", "................", "....x.......x...", "...x......x.....", "..x...x...x...x.", "..............x.", "................", "................", "................", "xxxxxxxxxxxxxxxx" },  // B Deep
        { "X......x..X.....", "....X.......X...", "................", "................", "x.x.x.x.x.x.x.x.", "................", "................", "................", "................", "..x...x...x...x." },  // C 2-Step Garage
        { "X...X...X...X...", "................", "....X.......X...", "................", "xxXxxxXxxxXxxxXx", "..X...X...X...X.", "................", "................", "................", "................" },  // D Techno
        { "X...X...X...X...", "................", "....X.......X...", "...x.......x..x.", "..x...x...x...x.", "......x.......x.", "................", "................", "x...x...x...x...", "x.xxx.xxx.xxx.xx" },  // E Tech House
        { "X...X...X...X...", "....X.......X...", "................", "................", "x.x.x.x.x.x.x.x.", "..X...X...X...X.", "................", "...........x..x.", "................", "................" },  // F Disco
        { "X.........X.....", "....X.......X..x", "................", "................", "x.x.x.x.x.x.x.x.", "..............x.", "................", "................", "................", "................" },  // G Breakbeat
        { "X...............", "................", "................", "................", "................", "................", "................", "................", "................", "................" },  // H blank-ish
    };
    for (int p = 0; p < numPatterns; ++p)
        for (int v = 0; v < numVoices; ++v)
            for (int s = 0; s < numSteps; ++s)
            {
                const char c = pats[p][v][s];
                steps[(size_t) p][(size_t) v][(size_t) s].store (c == 'X' ? 2 : c == 'x' ? 1 : 0);
            }
}

bool VegaDrumsProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (! l.inputBuses.isEmpty()) for (auto& b : l.inputBuses) if (! b.isDisabled()) return false;
    if (l.outputBuses.isEmpty() || l.outputBuses[0] != juce::AudioChannelSet::stereo()) return false;
    for (int i = 1; i < l.outputBuses.size(); ++i)
        if (! l.outputBuses[i].isDisabled() && l.outputBuses[i] != juce::AudioChannelSet::stereo()) return false;
    return true;
}

void VegaDrumsProcessor::prepareToPlay (double s, int)
{
    sr = s;
    for (auto& v : voices) { v.sr = s; v.active = false; }
    numPending = 0; lastStepIndex = -1;
}

void VegaDrumsProcessor::triggerVoice (int v, float vel)
{
    if (v < 0 || v >= numVoices || mute[v]->load() > 0.5f) return;
    auto& vo = voices[(size_t) v];
    vo.kit = (int) pKit->load(); vo.tune = tun[v]->load(); vo.decay = dec[v]->load(); vo.tone = ton[v]->load();
    vo.decayFast = false;
    vo.trigger (vel);
    if (v == 4) voices[5].choke();                 // closed hat chokes the open hat
    voiceLevel[(size_t) v].store (vel);
}

void VegaDrumsProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n == 0) return;

    // ---- transport: host, or internal clock for the stand-alone app
    double ppq = 0; bool playing = false;
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition())
    {
        if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 400.0, *b);
        if (pos->getIsPlaying()) if (auto q = pos->getPpqPosition()) { ppq = *q; playing = true; }
    }
    if (! playing && pPlay->load() > 0.5f)
    {
        bpm = pTempo->load();
        if (! wasPlaying) internalPpq = 0;
        ppq = internalPpq; playing = true;
        internalPpq += n * bpm / 60.0 / sr;
    }
    else if (pPlay->load() < 0.5f) internalPpq = 0;
    wasPlaying = playing;

    // ---- schedule sequencer steps that fall inside this block
    const int pat = (int) pPattern->load();
    const double spBeat = sr * 60.0 / bpm;
    const double swing = (pSwing->load() - 50.0) / 50.0 * 0.25;     // ppq delay of odd 16ths
    const float accentGain = juce::Decibels::decibelsToGain (pAccent->load());
    const float human = pHuman->load() * 0.01f;
    if (playing)
    {
        const double end = ppq + n / spBeat;
        uiStep.store ((int) (((long long) std::floor (ppq * 4.0)) % numSteps));
        for (long long s = (long long) std::floor (ppq * 4.0) - 1; s <= (long long) std::floor (end * 4.0); ++s)
        {
            if (s < 0) continue;
            const double at = s * 0.25 + ((s & 1) ? swing : 0.0);
            if (at < ppq || at >= end) continue;
            const int stepIdx = (int) (s % numSteps);
            for (int v = 0; v < numVoices; ++v)
            {
                const int st = steps[(size_t) pat][(size_t) v][(size_t) stepIdx].load();
                if (st == 0 || numPending >= (int) pending.size()) continue;
                int off = (int) std::floor ((at - ppq) * spBeat);
                float vel = st == 2 ? 0.8f * accentGain : 0.8f;
                if (human > 0 && v != 0)     // keep the kick on the grid
                {
                    off += (int) (humanRng.nextFloat() * human * 0.008 * sr);
                    vel *= 1.0f - human * 0.25f * humanRng.nextFloat();
                }
                pending[(size_t) numPending++] = { v, vel, off };
            }
        }
    }
    else uiStep.store (-1);

    // audition requests from the editor
    if (const int mask = auditionMask.exchange (0))
        for (int v = 0; v < numVoices; ++v) if (mask & (1 << v)) triggerVoice (v, auditionVel.load());

    // ---- render
    auto main = getBusBuffer (buffer, false, 0);
    float* out[numVoices][2];
    for (int v = 0; v < numVoices; ++v)
    {
        auto* bus = getBus (false, v + 1);
        if (bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() == 2)
        { auto b = getBusBuffer (buffer, false, v + 1); out[v][0] = b.getWritePointer (0); out[v][1] = b.getWritePointer (1); }
        else { out[v][0] = main.getWritePointer (0); out[v][1] = main.getWritePointer (1); }
    }
    const float master = juce::Decibels::decibelsToGain (pMaster->load());
    float gl[numVoices], gr[numVoices];
    for (int v = 0; v < numVoices; ++v)
    {
        const float pn = pan[v]->load(), a = (pn + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        const float l = lvl[v]->load() * master * 1.4142f;
        gl[v] = l * std::cos (a); gr[v] = l * std::sin (a);
        voices[(size_t) v].decay = dec[v]->load(); voices[(size_t) v].tone = ton[v]->load();
    }
    auto midiIt = midi.cbegin();
    for (int i = 0; i < n; ++i)
    {
        while (midiIt != midi.cend() && (*midiIt).samplePosition <= i)
        {
            const auto msg = (*midiIt).getMessage();
            if (msg.isNoteOn()) triggerVoice (voiceForNote (msg.getNoteNumber()), msg.getFloatVelocity());
            ++midiIt;
        }
        for (int k = 0; k < numPending;)
        {
            if (pending[(size_t) k].at <= i) { triggerVoice (pending[(size_t) k].voice, pending[(size_t) k].vel); pending[(size_t) k] = pending[(size_t) --numPending]; }
            else ++k;
        }
        for (int v = 0; v < numVoices; ++v)
        {
            auto& vo = voices[(size_t) v];
            if (! vo.active) continue;
            const float y = vo.next();
            out[v][0][i] += y * gl[v]; out[v][1][i] += y * gr[v];
        }
    }
    // events that land after this block keep waiting
    for (int k = 0; k < numPending; ++k) pending[(size_t) k].at -= n;
    outL.store (std::max (outL.load(), main.getMagnitude (0, 0, n))); outR.store (std::max (outR.load(), main.getMagnitude (1, 0, n)));
}

juce::File VegaDrumsProcessor::exportMidi (const juce::File& dest) const
{
    juce::MidiMessageSequence seq; const int tpq = 960;
    const int pat = (int) pPattern->load();
    const double swing = (pSwing->load() - 50.0) / 50.0 * 0.25;
    for (int s = 0; s < numSteps; ++s)
        for (int v = 0; v < numVoices; ++v)
        {
            const int st = steps[(size_t) pat][(size_t) v][(size_t) s].load();
            if (st == 0) continue;
            const double t = (s * 0.25 + ((s & 1) ? swing : 0.0)) * tpq;
            const auto vel = (juce::uint8) (st == 2 ? 120 : 90);
            seq.addEvent (juce::MidiMessage::noteOn (10, gmNote (v), vel), t);
            seq.addEvent (juce::MidiMessage::noteOff (10, gmNote (v)), t + tpq / 8.0);
        }
    seq.updateMatchedPairs();
    juce::MidiFile mf; mf.setTicksPerQuarterNote (tpq); mf.addTrack (seq);
    auto f = dest != juce::File() ? dest : juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Vega Drums Pattern " + juce::String::charToString ((juce::juce_wchar) ('A' + pat)) + ".mid");
    f.deleteFile(); juce::FileOutputStream os (f); if (os.openedOk()) mf.writeTo (os);
    return f;
}

void VegaDrumsProcessor::getStateInformation (juce::MemoryBlock& d)
{
    auto s = apvts.copyState(); s.setProperty ("uiScale", uiScale.load(), nullptr);
    s.removeChild (s.getChildWithName ("Patterns"), nullptr);
    juce::ValueTree pt ("Patterns");
    for (int p = 0; p < numPatterns; ++p)
    {
        juce::String str;
        for (int v = 0; v < numVoices; ++v) for (int st = 0; st < numSteps; ++st) str << steps[(size_t) p][(size_t) v][(size_t) st].load();
        pt.setProperty ("p" + juce::String (p), str, nullptr);
    }
    s.appendChild (pt, nullptr);
    if (auto x = s.createXml()) copyXmlToBinary (*x, d);
}
void VegaDrumsProcessor::setStateInformation (const void* data, int size)
{
    if (auto x = getXmlFromBinary (data, size))
    {
        auto s = juce::ValueTree::fromXml (*x);
        if (! s.hasType (apvts.state.getType())) return;
        uiScale.store ((float) s.getProperty ("uiScale", 1.0f));
        auto pt = s.getChildWithName ("Patterns");
        if (pt.isValid())
            for (int p = 0; p < numPatterns; ++p)
            {
                const auto str = pt.getProperty ("p" + juce::String (p)).toString();
                if (str.length() != numVoices * numSteps) continue;
                for (int v = 0; v < numVoices; ++v) for (int st = 0; st < numSteps; ++st)
                    steps[(size_t) p][(size_t) v][(size_t) st].store (juce::jlimit (0, 2, (int) (str[v * numSteps + st] - '0')));
            }
        s.removeChild (pt, nullptr);
        apvts.replaceState (s);
    }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VegaDrumsProcessor(); }
