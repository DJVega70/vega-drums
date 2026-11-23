#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace vdr;

juce::AudioProcessorValueTreeState::ParameterLayout VegaDrumsProcessor::layout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> v;
    auto pct = [] (float x, int) { return String (roundToInt (x * 100)) + "%"; };
    for (int i = 0; i < padsPerBank; ++i)
    {
        // ids level0..level9 etc. are unchanged from the first Vega Drums, so old automation still works
        const String s (i), title = padLabel (i);
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "level" + s, 1 }, title + " Level", NormalisableRange<float> (0, 1), i == 0 ? 0.9f : 0.75f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tune" + s, 1 }, title + " Tune", NormalisableRange<float> (-24, 24), 0, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return (x > 0 ? "+" : "") + String (x, 1) + " st"; })));
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "decay" + s, 1 }, title + " Decay", NormalisableRange<float> (0, 1), 0.5f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tone" + s, 1 }, title + " Tone", NormalisableRange<float> (0, 1), 0.5f, AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
        v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "pan" + s, 1 }, title + " Pan", NormalisableRange<float> (-1, 1), 0, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return std::abs (x) < 0.01f ? String ("C") : (x < 0 ? "L" : "R") + String (roundToInt (std::abs (x) * 100)); })));
        v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "mute" + s, 1 }, title + " Mute", false));
    }
    StringArray seqs; for (int i = 0; i < numSequences; ++i) seqs.add ("Sequence " + String (i + 1));
    v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "pattern", 1 }, "Sequence", seqs, 0));
    StringArray kits; for (auto& k : builtInKits()) kits.add (k.name);
    v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "kit", 1 }, "Drum Machine", StringArray { "909", "808", "Deep" }, 0));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "swing", 1 }, "Swing", NormalisableRange<float> (50, 75), 54, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (roundToInt (x)) + "%"; })));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "accent", 1 }, "Accent", NormalisableRange<float> (0, 12), 5, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return "+" + String (x, 1) + " dB"; })));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "humanize", 1 }, "Humanize", NormalisableRange<float> (0, 100), 10, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (roundToInt (x)) + "%"; })));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "master", 1 }, "Master", NormalisableRange<float> (-24, 6), -3, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (x, 1) + " dB"; })));
    v.push_back (std::make_unique<AudioParameterFloat> (ParameterID { "tempo", 1 }, "Tempo (stand-alone)", NormalisableRange<float> (60, 200), 124, AudioParameterFloatAttributes().withStringFromValueFunction ([] (float x, int) { return String (x, 1) + " BPM"; })));
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "play", 1 }, "Play (stand-alone)", false));
    v.push_back (std::make_unique<AudioParameterChoice> (ParameterID { "tc", 1 }, "Timing Correct", StringArray { "Off", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 4));
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "noteRepeat", 1 }, "Note Repeat", false));
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "metronome", 1 }, "Metronome", false));
    v.push_back (std::make_unique<AudioParameterBool> (ParameterID { "fullLevel", 1 }, "Full Level", false));
    return { v.begin(), v.end() };
}

juce::AudioProcessor::BusesProperties VegaDrumsProcessor::makeBuses()
{
    auto b = BusesProperties().withOutput ("Main", juce::AudioChannelSet::stereo(), true);
    // one stereo output per pad position: A01 / B01 / ... share "Pad 1", etc. (the first ten keep their old names)
    for (int v = 0; v < padsPerBank; ++v)
    {
        juce::String n = v < numVoices ? juce::String (voiceName (v)) : "Pad " + juce::String (v + 1);
        if (v < numVoices) n = n.substring (0, 1) + n.substring (1).toLowerCase();
        b = b.withOutput (n, juce::AudioChannelSet::stereo(), false);
    }
    return b;
}

VegaDrumsProcessor::VegaDrumsProcessor() : AudioProcessor (makeBuses()), apvts (*this, nullptr, "PARAMS", layout())
{
    for (int v = 0; v < padsPerBank; ++v)
    {
        const juce::String s (v);
        lvl[v] = apvts.getRawParameterValue ("level" + s); tun[v] = apvts.getRawParameterValue ("tune" + s); dec[v] = apvts.getRawParameterValue ("decay" + s);
        ton[v] = apvts.getRawParameterValue ("tone" + s); pan[v] = apvts.getRawParameterValue ("pan" + s); mute[v] = apvts.getRawParameterValue ("mute" + s);
    }
    for (int p = 0; p < numPads; ++p) for (auto& vo : voices[(size_t) p]) vo.rng.setSeed (1000 + p);
    pPattern = apvts.getRawParameterValue ("pattern"); pSwing = apvts.getRawParameterValue ("swing"); pKit = apvts.getRawParameterValue ("kit");
    pMaster = apvts.getRawParameterValue ("master"); pTempo = apvts.getRawParameterValue ("tempo"); pPlay = apvts.getRawParameterValue ("play");
    pAccent = apvts.getRawParameterValue ("accent"); pHuman = apvts.getRawParameterValue ("humanize");
    pTc = apvts.getRawParameterValue ("tc"); pRepeat = apvts.getRawParameterValue ("noteRepeat"); pMetro = apvts.getRawParameterValue ("metronome");
    pFullLevel = apvts.getRawParameterValue ("fullLevel");

    makeKit (0, pads);
    for (int p = 0; p < numPads; ++p) publishPad (p);
    auto fs = factorySequences();
    for (int i = 0; i < numSequences; ++i) std::atomic_store (&sequences[(size_t) i], SequencePtr (std::make_shared<SequenceData> (fs[(size_t) i])));
    samplesFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Vega Drums Kits");
    startTimerHz (30);
}

VegaDrumsProcessor::~VegaDrumsProcessor() { stopTimer(); }

// ---------------------------------------------------------------- pads
PadSound VegaDrumsProcessor::getPad (int p) const
{
    auto s = pads[(size_t) p];
    if (p < padsPerBank)
    {
        s.level = lvl[p]->load(); s.tune = tun[p]->load(); s.decay = dec[p]->load(); s.tone = ton[p]->load(); s.pan = pan[p]->load(); s.mute = mute[p]->load() > 0.5f;
    }
    return s;
}

void VegaDrumsProcessor::setPad (int p, const PadSound& s)
{
    jassert (juce::MessageManager::getInstanceWithoutCreating() == nullptr || juce::MessageManager::getInstance()->isThisTheMessageThread());
    const bool newSample = s.samplePath != pads[(size_t) p].samplePath || (s.source == srcSample && std::atomic_load (&padSamples[(size_t) p]) == nullptr);
    pads[(size_t) p] = s;
    if (p < padsPerBank)
    {
        auto set = [&] (const char* id, float v) { if (auto* q = apvts.getParameter (juce::String (id) + juce::String (p))) q->setValueNotifyingHost (q->convertTo0to1 (v)); };
        set ("level", s.level); set ("tune", s.tune); set ("decay", s.decay); set ("tone", s.tone); set ("pan", s.pan); set ("mute", s.mute ? 1.0f : 0.0f);
    }
    if (newSample) std::atomic_store (&padSamples[(size_t) p], std::shared_ptr<const SampleData> (s.samplePath.isNotEmpty() ? loadSample (s.samplePath) : nullptr));
    publishPad (p);
}

void VegaDrumsProcessor::publishPad (int p)
{
    const auto& s = pads[(size_t) p]; auto& r = runtime[(size_t) p];
    r.source = s.source; r.chord = s.chord; r.choke = s.choke; r.oneShot = s.oneShot;
    r.level = s.level; r.tune = s.tune; r.decay = s.decay; r.tone = s.tone; r.pan = s.pan; r.mute = s.mute;
}

void VegaDrumsProcessor::loadKit (int kit)
{
    kit = juce::jlimit (0, (int) builtInKits().size() - 1, kit);
    std::array<PadSound, numPads> k; makeKit (kit, k);
    for (int p = 0; p < numPads; ++p) setPad (p, k[(size_t) p]);
    kitName = builtInKits()[(size_t) kit].name;
    if (auto* q = apvts.getParameter ("kit")) q->setValueNotifyingHost (q->convertTo0to1 ((float) builtInKits()[(size_t) kit].machine));
    stateChanged.sendChangeMessage();
}

int VegaDrumsProcessor::autoKitFromFolder (const juce::File& f, int firstBank)
{
    juce::StringArray files;
    if (f.hasFileExtension ("zip"))
    {
        juce::ZipFile zip (f);
        for (int i = 0; i < zip.getNumEntries(); ++i) if (auto* z = zip.getEntry (i)) if (isAudioFile (z->filename)) files.add (f.getFullPathName() + "|" + z->filename);
    }
    else files = scanSamples (f, 4000);
    files.sort (true);
    // bank A: one of each drum in the classic order; bank B: keys, bass, then loops
    const int order[16] { srcKick, srcSnare, srcClap, srcRim, srcClosedHat, srcOpenHat, srcLowTom, srcHighTom,
                          srcRide, srcShaker, srcCowbell, srcCongaHi, srcCongaLo, srcCrash, srcTambourine, srcClave };
    const int bankA = juce::jlimit (0, numBanks - 2, firstBank) * padsPerBank, bankB = bankA + padsPerBank;
    juce::StringArray used; int placed = 0;
    for (int i = 0; i < 16; ++i)
        for (auto& path : files)
            if (! used.contains (path) && ! isLoopPath (path.fromLastOccurrenceOf ("|", false, false)) && guessSource (juce::File (path.fromLastOccurrenceOf ("|", false, false)).getFileName()) == order[i])
            {
                PadSound s; s.source = srcSample; s.samplePath = path; s.level = 0.8f; s.decay = 1.0f; s.tone = 1.0f;
                if (order[i] == srcClosedHat || order[i] == srcOpenHat) s.choke = 1;
                setPad (bankA + i, s); used.add (path); ++placed; break;
            }
    // empty drum pads: spare one-shot hits from the same pack (a second kick, clap, hat ...), round-robin by type
    {
        std::vector<juce::String> spare[16];
        for (auto& path : files)
        {
            if (used.contains (path) || isLoopPath (path.fromLastOccurrenceOf ("|", false, false))) continue;
            const int g = guessSource (juce::File (path.fromLastOccurrenceOf ("|", false, false)).getFileName());
            for (int i = 0; i < 16; ++i) if (order[i] == g) spare[i].push_back (path);
        }
        size_t round = 0; bool any = true;
        for (int i = 0; i < 16 && any; )
        {
            if (pads[(size_t) (bankA + i)].samplePath.isNotEmpty()) { ++i; continue; }
            any = false;
            for (int k = 0; k < 16; ++k)
            {
                const int type = (int) ((round + (size_t) k) % 16);
                if (! spare[type].empty())
                {
                    PadSound s; s.source = srcSample; s.samplePath = spare[type].front(); spare[type].erase (spare[type].begin());
                    s.level = 0.8f; s.decay = 1.0f; s.tone = 1.0f;
                    setPad (bankA + i, s); used.add (s.samplePath); ++placed; any = true; ++round; break;
                }
            }
            ++i;
        }
    }
    // bank B: one sample per instrument folder (basses, pads, keys, arps), then loops
    int slot = 0;
    auto innerOf = [] (const juce::String& path) { return path.contains ("|") ? path.fromFirstOccurrenceOf ("|", false, false) : path; };
    auto folderOf = [&] (const juce::String& path) { return innerOf (path).upToLastOccurrenceOf ("/", false, false).upToLastOccurrenceOf ("\\", false, false); };
    auto place = [&] (const juce::String& path)
    {
        PadSound s; s.source = srcSample; s.samplePath = path; s.level = 0.75f; s.decay = 1.0f; s.tone = 1.0f; s.oneShot = true;
        setPad (bankB + slot++, s); used.add (path); ++placed;
    };
    juce::StringArray instrumentFolders;
    for (auto& path : files)
    {
        if (used.contains (path) || isLoopPath (innerOf (path))) continue;
        const int g = guessSource (juce::File (innerOf (path)).getFileName());
        if (g >= 0 && g < srcOrgan) continue;                       // a spare drum hit
        instrumentFolders.addIfNotAlreadyThere (folderOf (path));
    }
    for (auto& folder : instrumentFolders)
    {
        if (slot >= 16) break;
        juce::StringArray inFolder;
        for (auto& path : files) if (! used.contains (path) && folderOf (path) == folder && ! isLoopPath (innerOf (path))) inFolder.add (path);
        if (inFolder.isEmpty()) continue;
        // a multi-sampled instrument: take the note nearest middle C, else the middle file
        juce::String pick = inFolder[inFolder.size() / 2];
        for (auto& path : inFolder) { const auto n = juce::File (innerOf (path)).getFileNameWithoutExtension().toUpperCase(); if (n.endsWith ("C3") || n.endsWith ("C2") || n.endsWith (" C")) { pick = path; break; } }
        place (pick);
    }
    juce::StringArray loopStems;      // the same loop at another tempo counts once
    auto stemOf = [&] (const juce::String& path)
    {
        auto n = juce::File (innerOf (path)).getFileNameWithoutExtension().toLowerCase();
        // drop tempo-looking numbers (60-200) and keep the rest, so "Beat 01" and "Beat 02" stay different
        juce::String out;
        for (int i = 0; i < n.length();)
        {
            if (juce::CharacterFunctions::isDigit (n[i]))
            {
                int j = i; while (j < n.length() && juce::CharacterFunctions::isDigit (n[j])) ++j;
                const int v = n.substring (i, j).getIntValue();
                if (! (j - i >= 2 && v >= 60 && v <= 200)) out << n.substring (i, j);
                i = j;
            }
            else out << n[i++];
        }
        return out.replace ("bpm", "").removeCharacters ("() _-");
    };
    juce::StringArray loopFolders;
    for (auto& path : files) if (! used.contains (path) && isLoopPath (innerOf (path))) loopFolders.addIfNotAlreadyThere (folderOf (path));
    for (int round = 0; round < 16 && slot < 16; ++round)          // loops: take them round-robin across the loop folders
        for (auto& folder : loopFolders)
        {
            if (slot >= 16) break;
            int seen = 0;
            for (auto& path : files)
                if (folderOf (path) == folder && isLoopPath (innerOf (path)) && seen++ == round && ! used.contains (path))
                {
                    if (! loopStems.contains (stemOf (path))) { loopStems.add (stemOf (path)); place (path); }
                    break;
                }
        }
    kitName = f.getFileNameWithoutExtension();
    stateChanged.sendChangeMessage();
    return placed;
}

// ---------------------------------------------------------------- sequences
void VegaDrumsProcessor::setSequence (int i, SequenceData s)
{
    s.bars = juce::jlimit (1, maxBars, s.bars);
    std::sort (s.events.begin(), s.events.end(), [] (auto& a, auto& b) { return a.start < b.start; });
    std::atomic_store (&sequences[(size_t) juce::jlimit (0, numSequences - 1, i)], SequencePtr (std::make_shared<SequenceData> (std::move (s))));
}

int VegaDrumsProcessor::timingDivisionTicks (int c)
{
    static const int t[] { 0, ppqTicks, ppqTicks / 2, ppqTicks / 3, ppqTicks / 4, ppqTicks / 6, ppqTicks / 8 };
    return t[juce::jlimit (0, 6, c)];
}

int VegaDrumsProcessor::quantize (int tick) const
{
    const int d = timingDivisionTicks ((int) pTc->load());
    return d <= 0 ? tick : (int) std::round ((double) tick / d) * d;
}

void VegaDrumsProcessor::timerCallback() { mergeRecorded(); }

void VegaDrumsProcessor::mergeRecorded()
{
    // merge hits recorded on the audio thread into their sequences (with timing correct)
    int s1, n1, s2, n2;
    const int num = recFifo.getNumReady();
    if (num == 0) return;
    recFifo.prepareToRead (num, s1, n1, s2, n2);
    std::vector<RecMsg> msgs;
    for (int i = 0; i < n1; ++i) msgs.push_back (recQueue[(size_t) (s1 + i)]);
    for (int i = 0; i < n2; ++i) msgs.push_back (recQueue[(size_t) (s2 + i)]);
    recFifo.finishedRead (n1 + n2);
    std::map<int, SequenceData> edited;
    auto seqFor = [&] (int i) -> SequenceData& { if (! edited.count (i)) edited[i] = *getSequence (i); return edited[i]; };
    for (auto& m : msgs)
    {
        auto& s = seqFor (m.seq);
        const int len = s.lengthTicks();
        if (m.vel == -2)      // erase: remove the held pad's events around this tick
        {
            s.events.erase (std::remove_if (s.events.begin(), s.events.end(), [&] (const NoteEvent& e) { return e.pad == m.pad && std::abs (e.start - m.tick) < sixteenth / 2; }), s.events.end());
            continue;
        }
        if (! m.off)
        {
            int t = quantize (m.tick) % len;
            // don't stack a second hit of the same pad on the same spot (note repeat / double taps)
            bool dup = false; for (auto& e : s.events) if (e.pad == m.pad && std::abs (e.start - t) < 20) dup = true;
            if (dup) continue;
            s.events.push_back ({ m.pad, t, isMelodic (runtime[(size_t) m.pad].source.load()) ? sixteenth : sixteenth - 20, m.vel });
            openRecordings.push_back ({ m.pad, m.seq, t });
        }
        else
            for (auto it = openRecordings.begin(); it != openRecordings.end(); ++it)
                if (it->pad == m.pad && it->seq == m.seq)
                {
                    if (isMelodic (runtime[(size_t) m.pad].source.load()))
                        for (auto& e : s.events)
                            if (e.pad == m.pad && e.start == it->startTick)
                            {
                                int l = m.tick - it->startTick; if (l <= 0) l += len;
                                e.length = juce::jlimit (sixteenth / 2, len, l);
                            }
                    openRecordings.erase (it); break;
                }
    }
    for (auto& [i, s] : edited) setSequence (i, s);
    if (openRecordings.size() > 256) openRecordings.erase (openRecordings.begin(), openRecordings.begin() + 128);
}

// ---------------------------------------------------------------- audio
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
    for (auto& pv : voices) for (auto& v : pv) { v.sr = s; v.active = false; }
    numPending = 0; lastPpqEnd = -1;
}

void VegaDrumsProcessor::padDown (int pad, float vel)
{
    int s1, n1, s2, n2; uiFifo.prepareToWrite (1, s1, n1, s2, n2);
    if (n1 > 0) uiQueue[(size_t) s1] = { pad, vel }; else if (n2 > 0) uiQueue[(size_t) s2] = { pad, vel };
    uiFifo.finishedWrite (n1 + n2);
}
void VegaDrumsProcessor::padUp (int pad) { padDown (pad, -1.0f); }

void VegaDrumsProcessor::previewSample (const juce::String& path)
{
    std::atomic_store (&previewData, std::shared_ptr<const SampleData> (path.isNotEmpty() ? loadSample (path) : nullptr));
    previewRequest.store (path.isNotEmpty() ? 1 : 2);
}

void VegaDrumsProcessor::startNote (int p, float vel, int)
{
    if (p < 0 || p >= numPads) return;
    auto& r = runtime[(size_t) p];
    const bool isA = p < padsPerBank;
    if ((isA ? mute[p]->load() > 0.5f : r.mute.load())) return;
    const int src = r.source.load();
    if (src == srcSample && std::atomic_load (&padSamples[(size_t) p]) == nullptr) return;
    auto& pv = voices[(size_t) p];
    for (auto& v : pv) v.stopQuickly();
    const int vi = (lastVoice[(size_t) p] + 1) & 1; lastVoice[(size_t) p] = vi;
    auto& vo = pv[(size_t) vi];
    vo.type = src; vo.kit = (int) pKit->load(); vo.chord = r.chord.load();
    vo.tune = isA ? tun[p]->load() : r.tune.load(); vo.decay = isA ? dec[p]->load() : r.decay.load(); vo.tone = isA ? ton[p]->load() : r.tone.load();
    vo.sample = src == srcSample ? std::atomic_load (&padSamples[(size_t) p]) : nullptr;
    vo.trigger (vel);
    if (src == srcClosedHat) for (int q = 0; q < numPads; ++q) if (runtime[(size_t) q].source.load() == srcOpenHat && runtime[(size_t) q].choke.load() == 0) for (auto& v : voices[(size_t) q]) v.choke();
    if (const int g = r.choke.load())
        for (int q = 0; q < numPads; ++q)
            if (q != p && runtime[(size_t) q].choke.load() == g) for (auto& v : voices[(size_t) q]) v.choke();
    padFlash[(size_t) p].store (vel);
}

void VegaDrumsProcessor::stopNote (int p)
{
    if (p < 0 || p >= numPads) return;
    if (runtime[(size_t) p].oneShot.load() && ! isMelodic (runtime[(size_t) p].source.load())) return;
    for (auto& v : voices[(size_t) p]) v.release();
}

static int padForNote (int note)
{
    if (const int v = voiceForNote (note); v >= 0) return v;            // GM drum notes -> bank A (the classic layout)
    switch (note) { case 56: return 10; case 62: case 63: return 11; case 64: return 12; case 49: case 57: return 13; case 54: return 14; case 75: return 15; default: break; }
    if (note >= 60 && note < 60 + 64) return padsPerBank + (note - 60);  // C4 and up: B01, B02 ... E16
    return -1;
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
    transportRunning.store (playing);

    const int seqIndex = (int) pPattern->load();
    const auto seq = getSequence (seqIndex);
    const double spBeat = sr * 60.0 / bpm;
    const double ticksPerSample = ppqTicks / spBeat;
    const float accentGain = juce::Decibels::decibelsToGain (pAccent->load());
    const float human = pHuman->load() * 0.01f;
    const double swingTicks = (pSwing->load() / 100.0 - 0.5) * 2.0 * sixteenth;     // 66% -> the off 16th lands 77 ticks late
    const int loopLen = seq->lengthTicks();
    const double t0 = ppq * ppqTicks, t1 = t0 + n * ticksPerSample;
    auto velGain = [&] (int v) { return v >= 120 ? 0.55f * accentGain : 0.55f * (float) v / 100.0f; };
    auto offsetOf = [&] (double tick) { return juce::jlimit (0, n - 1, (int) std::floor ((tick - t0) / ticksPerSample)); };

    if (playing)
    {
        uiTick.store ((int) std::fmod (t0, (double) loopLen));
        for (auto& e : seq->events)
        {
            double st = e.start;
            if (e.start % (2 * sixteenth) == sixteenth) st += swingTicks;
            // first repeat of this event at or after t0
            const double k = std::ceil ((t0 - st) / loopLen);
            const double at = st + k * loopLen;
            if (at < t1)
            {
                float vel = velGain (e.velocity);
                int off = offsetOf (at);
                if (human > 0 && e.pad != 0)     // keep the kick on the grid
                {
                    off = juce::jlimit (0, n - 1, off + (int) (humanRng.nextFloat() * human * 0.008 * sr));
                    vel *= 1.0f - human * 0.25f * humanRng.nextFloat();
                }
                if (eraseHeld.load() && held[(size_t) e.pad] && recordArmed.load())
                {
                    int s1, n1, s2, n2; recFifo.prepareToWrite (1, s1, n1, s2, n2);
                    const RecMsg m { e.pad, e.start, -2, false, seqIndex };
                    if (n1 > 0) recQueue[(size_t) s1] = m; else if (n2 > 0) recQueue[(size_t) s2] = m;
                    recFifo.finishedWrite (n1 + n2);
                }
                else schedule (e.pad, vel, off);
            }
            // note-off for held sounds
            const double offT = st + e.length;
            const double k2 = std::ceil ((t0 - offT) / loopLen);
            const double atOff = offT + k2 * loopLen;
            if (atOff < t1 && isMelodic (runtime[(size_t) e.pad].source.load())) schedule (e.pad, -1.0f, offsetOf (atOff));
        }
        // note repeat: held pads retrigger on the timing-correct grid
        if (pRepeat->load() > 0.5f)
            if (const int d = timingDivisionTicks ((int) pTc->load()))
                for (double g = std::ceil (t0 / d) * d; g < t1; g += d)
                    for (int p = 0; p < numPads; ++p)
                        if (held[(size_t) p])
                        {
                            schedule (p, holdVel[(size_t) p], offsetOf (g));
                            if (recordArmed.load())
                            {
                                int s1, n1, s2, n2; recFifo.prepareToWrite (2, s1, n1, s2, n2);
                                const RecMsg on { p, (int) std::fmod (g, (double) loopLen), juce::roundToInt (holdVel[(size_t) p] / 0.55f * 100.0f), false, seqIndex };
                                const RecMsg offm { p, (int) std::fmod (g + d * 0.5, (double) loopLen), 0, true, seqIndex };
                                if (n1 > 0) recQueue[(size_t) s1] = on; if (n1 > 1) recQueue[(size_t) s1 + 1] = offm;
                                if (n1 == 1 && n2 > 0) recQueue[(size_t) s2] = offm; else if (n1 == 0 && n2 > 1) { recQueue[(size_t) s2] = on; recQueue[(size_t) s2 + 1] = offm; }
                                recFifo.finishedWrite (n1 + n2);
                            }
                        }
        // metronome clicks on the beat (accent on the bar)
        if (pMetro->load() > 0.5f)
            for (double b = std::ceil (t0 / ppqTicks) * ppqTicks; b < t1; b += ppqTicks)
                schedule (-2 - ((int) (b / ppqTicks) % 4 == 0 ? 1 : 0), 1.0f, offsetOf (b));
    }
    else uiTick.store (-1);

    // ---- hits from the screen and from MIDI (recorded when REC is on)
    auto record = [&] (int pad, int vel, bool off, int sampleOffset)
    {
        if (! recordArmed.load() || ! playing || pad < 0) return;
        const int tick = (int) std::fmod (t0 + sampleOffset * ticksPerSample, (double) loopLen);
        int s1, n1, s2, n2; recFifo.prepareToWrite (1, s1, n1, s2, n2);
        const RecMsg m { pad, tick, vel, off, seqIndex };
        if (n1 > 0) recQueue[(size_t) s1] = m; else if (n2 > 0) recQueue[(size_t) s2] = m;
        recFifo.finishedWrite (n1 + n2);
    };
    const bool full = pFullLevel->load() > 0.5f;
    {
        int s1, n1, s2, n2; uiFifo.prepareToRead (uiFifo.getNumReady(), s1, n1, s2, n2);
        auto handle = [&] (const PadMsg& m)
        {
            if (m.pad < 0 || m.pad >= numPads) return;
            if (m.vel >= 0) { const float v = full ? 1.0f : m.vel; held[(size_t) m.pad] = true; holdVel[(size_t) m.pad] = 0.55f * v;
                              if (! (pRepeat->load() > 0.5f && playing)) schedule (m.pad, 0.55f * v * (v >= 0.95f ? accentGain / 1.25f : 1.0f), 0);
                              if (! (pRepeat->load() > 0.5f && playing)) record (m.pad, juce::roundToInt (v * 127.0f), false, 0); }
            else { held[(size_t) m.pad] = false; schedule (m.pad, -1.0f, 0); record (m.pad, 0, true, 0); }
        };
        for (int i = 0; i < n1; ++i) handle (uiQueue[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) handle (uiQueue[(size_t) (s2 + i)]);
        uiFifo.finishedRead (n1 + n2);
    }
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int pad = (msg.isNoteOn() || msg.isNoteOff()) ? padForNote (msg.getNoteNumber()) : -1;
        if (pad < 0) continue;
        if (msg.isNoteOn())
        {
            const float v = (full ? 1.0f : msg.getFloatVelocity()) * 0.7f;
            held[(size_t) pad] = true; holdVel[(size_t) pad] = v;
            if (! (pRepeat->load() > 0.5f && playing)) { schedule (pad, v, meta.samplePosition); record (pad, msg.getVelocity(), false, meta.samplePosition); }
        }
        else { held[(size_t) pad] = false; schedule (pad, -1.0f, meta.samplePosition); record (pad, 0, true, meta.samplePosition); }
    }

    // ---- render
    auto main = getBusBuffer (buffer, false, 0);
    float* out[padsPerBank][2];
    for (int v = 0; v < padsPerBank; ++v)
    {
        auto* bus = getBus (false, v + 1);
        if (bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() == 2)
        { auto b = getBusBuffer (buffer, false, v + 1); out[v][0] = b.getWritePointer (0); out[v][1] = b.getWritePointer (1); }
        else { out[v][0] = main.getWritePointer (0); out[v][1] = main.getWritePointer (1); }
    }
    const float master = juce::Decibels::decibelsToGain (pMaster->load());
    std::array<float, numPads> gl {}, gr {};
    std::array<bool, numPads> any {};
    for (int p = 0; p < numPads; ++p)
    {
        auto& pv = voices[(size_t) p];
        any[(size_t) p] = pv[0].active || pv[1].active;
        const bool isA = p < padsPerBank;
        const float pn = isA ? pan[p]->load() : runtime[(size_t) p].pan.load(), a = (pn + 1.0f) * juce::MathConstants<float>::pi * 0.25f;
        const float l = (isA ? lvl[p]->load() : runtime[(size_t) p].level.load()) * master * 1.4142f;
        gl[(size_t) p] = l * std::cos (a); gr[(size_t) p] = l * std::sin (a);
        if (any[(size_t) p])
            for (auto& vo : pv) { vo.decay = isA ? dec[p]->load() : runtime[(size_t) p].decay.load(); vo.tone = isA ? ton[p]->load() : runtime[(size_t) p].tone.load(); }
    }
    float* mL = main.getWritePointer (0); float* mR = main.getWritePointer (1);
    if (const int pr = previewRequest.exchange (0))
    {
        previewVoice.stopQuickly();
        if (pr == 1)
            if (auto d = std::atomic_load (&previewData))
            { previewVoice.sr = sr; previewVoice.type = srcSample; previewVoice.sample = d; previewVoice.tune = 0; previewVoice.decay = 1; previewVoice.tone = 1; previewVoice.trigger (0.8f); }
    }
    for (int i = 0; i < n; ++i)
    {
        for (int k = 0; k < numPending;)
        {
            const auto pe = pending[(size_t) k];
            if (pe.at <= i)
            {
                if (pe.pad <= -2) { metroEnv = 1.0f; metroFreq = pe.pad == -3 ? 1500.0f : 1000.0f; }
                else if (pe.vel >= 0) { startNote (pe.pad, pe.vel, i); any[(size_t) pe.pad] = true;
                                        if (runtime[(size_t) pe.pad].oneShot.load() && isMelodic (runtime[(size_t) pe.pad].source.load()) && pe.vel >= 0)
                                        { const float secs = 0.15f + (pe.pad < padsPerBank ? dec[pe.pad]->load() : runtime[(size_t) pe.pad].decay.load()) * 1.5f;
                                          schedule (pe.pad, -1.0f, i + (int) (secs * sr)); } }
                else stopNote (pe.pad);
                pending[(size_t) k] = pending[(size_t) --numPending];
            }
            else ++k;
        }
        for (int p = 0; p < numPads; ++p)
        {
            if (! any[(size_t) p]) continue;
            auto& pv = voices[(size_t) p];
            float yL = 0, yR = 0;
            for (auto& vo : pv)
            {
                if (! vo.active) continue;
                const float y = vo.next(); const float side = vo.sideR * vo.vel;
                yL += y - side; yR += y + side;
            }
            const int o = p % padsPerBank;
            out[o][0][i] += yL * gl[(size_t) p]; out[o][1][i] += yR * gr[(size_t) p];
        }
        if (previewVoice.active) { const float y = previewVoice.next(); const float sd = previewVoice.sideR * previewVoice.vel; mL[i] += (y - sd) * master; mR[i] += (y + sd) * master; }
        if (metroEnv > 1.0e-4f)
        {
            metroPhase += metroFreq / (float) sr; metroPhase -= std::floor (metroPhase);
            const float c = std::sin (juce::MathConstants<float>::twoPi * metroPhase) * metroEnv * 0.3f;
            metroEnv *= 0.9985f;
            mL[i] += c; mR[i] += c;
        }
    }
    // safety limiter on the main output: transparent below -3 dBFS, rounds peaks off smoothly up to 0 dBFS
    for (int c = 0; c < 2; ++c)
    {
        float* x = main.getWritePointer (c);
        for (int i = 0; i < n; ++i)
        {
            const float a = std::abs (x[i]);
            if (a > 0.708f) x[i] = std::copysign (0.708f + 0.292f * std::tanh ((a - 0.708f) / 0.292f), x[i]);
        }
    }
    // events that land after this block keep waiting
    for (int k = 0; k < numPending; ++k) pending[(size_t) k].at -= n;
    outL.store (std::max (outL.load(), main.getMagnitude (0, 0, n))); outR.store (std::max (outR.load(), main.getMagnitude (1, 0, n)));
}

// ---------------------------------------------------------------- MIDI export
juce::File VegaDrumsProcessor::exportMidi (const juce::File& dest) const
{
    juce::MidiMessageSequence ms; const int tpq = ppqTicks;
    const auto seq = getSequence (currentSequence());
    const double swingTicks = (pSwing->load() / 100.0 - 0.5) * 2.0 * sixteenth;
    auto noteFor = [] (int pad)
    {
        if (pad < numVoices) return gmNote (pad);
        static const int extra[] { 56, 62, 64, 49, 54, 75 };
        if (pad < padsPerBank) return extra[pad - numVoices];
        return 60 + (pad - padsPerBank);
    };
    for (auto& e : seq->events)
    {
        double t = e.start; if (e.start % (2 * sixteenth) == sixteenth) t += swingTicks;
        ms.addEvent (juce::MidiMessage::noteOn (10, noteFor (e.pad), (juce::uint8) e.velocity), t);
        ms.addEvent (juce::MidiMessage::noteOff (10, noteFor (e.pad)), t + e.length);
    }
    ms.updateMatchedPairs();
    juce::MidiFile mf; mf.setTicksPerQuarterNote (tpq); mf.addTrack (ms);
    auto name = seq->name.isNotEmpty() ? seq->name : "Sequence " + juce::String (currentSequence() + 1);
    auto f = dest != juce::File() ? dest : juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Vega Drums - " + juce::File::createLegalFileName (name) + ".mid");
    f.deleteFile(); juce::FileOutputStream os (f); if (os.openedOk()) mf.writeTo (os);
    return f;
}

// ---------------------------------------------------------------- state
void VegaDrumsProcessor::getStateInformation (juce::MemoryBlock& d)
{
    auto s = apvts.copyState(); s.setProperty ("uiScale", uiScale.load(), nullptr);
    s.setProperty ("kitName", kitName, nullptr); s.setProperty ("samplesFolder", samplesFolder.getFullPathName(), nullptr);
    for (auto name : { "Patterns", "Pads", "Sequences" }) s.removeChild (s.getChildWithName (name), nullptr);
    juce::ValueTree pt ("Pads");
    for (int p = 0; p < numPads; ++p) pt.appendChild (getPad (p).toTree (p), nullptr);
    s.appendChild (pt, nullptr);
    juce::ValueTree sq ("Sequences");
    for (int i = 0; i < numSequences; ++i)
    {
        auto q = getSequence (i);
        juce::ValueTree t ("Seq"); t.setProperty ("name", q->name, nullptr); t.setProperty ("bars", q->bars, nullptr); t.setProperty ("events", q->toString(), nullptr);
        sq.appendChild (t, nullptr);
    }
    s.appendChild (sq, nullptr);
    if (auto x = s.createXml()) copyXmlToBinary (*x, d);
}

void VegaDrumsProcessor::convertOldPatterns (const juce::ValueTree& pt)
{
    // the first Vega Drums stored 8 patterns x 10 drums x 16 steps as digits: 0 off, 1 hit, 2 accent
    for (int p = 0; p < numSequences; ++p)
    {
        const auto str = pt.getProperty ("p" + juce::String (p)).toString();
        if (str.length() != numVoices * 16) continue;
        SequenceData s; s.bars = 1; s.name = "Pattern " + juce::String::charToString ((juce::juce_wchar) ('A' + p));
        for (int v = 0; v < numVoices; ++v)
            for (int st = 0; st < 16; ++st)
                if (const int c = str[v * 16 + st] - '0'; c > 0) s.events.push_back ({ v, st * sixteenth, sixteenth - 20, c == 2 ? 127 : 100 });
        setSequence (p, s);
    }
}

void VegaDrumsProcessor::setStateInformation (const void* data, int size)
{
    if (auto x = getXmlFromBinary (data, size))
    {
        auto s = juce::ValueTree::fromXml (*x);
        if (! s.hasType (apvts.state.getType())) return;
        uiScale.store ((float) s.getProperty ("uiScale", 1.0f));
        const auto padsTree = s.getChildWithName ("Pads"), seqTree = s.getChildWithName ("Sequences"), oldPatterns = s.getChildWithName ("Patterns");
        // pads first, from the old Kit choice for old projects, then the parameters on top (bank A levels etc.)
        if (! padsTree.isValid())
        {
            const int oldKit = (int) s.getChildWithProperty ("id", "kit").getProperty ("value", 0.0f);
            std::array<PadSound, numPads> k; makeKit (juce::jlimit (0, 2, oldKit), k);
            for (int p = padsPerBank; p < numPads; ++p) setPad (p, k[(size_t) p]);
            for (int p = 0; p < padsPerBank; ++p) { pads[(size_t) p] = k[(size_t) p]; publishPad (p); }
        }
        kitName = s.getProperty ("kitName", kitName).toString();
        if (auto f = s.getProperty ("samplesFolder").toString(); f.isNotEmpty()) samplesFolder = juce::File (f);
        for (auto name : { "Patterns", "Pads", "Sequences" }) s.removeChild (s.getChildWithName (name), nullptr);
        apvts.replaceState (s);
        if (padsTree.isValid())
            for (const auto& t : padsTree)
            {
                const int p = (int) t.getProperty ("i", -1);
                if (p < 0 || p >= numPads) continue;
                PadSound ps; ps.fromTree (t);
                if (p < padsPerBank) { pads[(size_t) p] = ps; std::atomic_store (&padSamples[(size_t) p], std::shared_ptr<const SampleData> (ps.samplePath.isNotEmpty() ? loadSample (ps.samplePath) : nullptr)); publishPad (p); }
                else { pads[(size_t) p].samplePath = {}; setPad (p, ps); }
            }
        if (seqTree.isValid())
            for (int i = 0; i < std::min (numSequences, seqTree.getNumChildren()); ++i)
            {
                auto t = seqTree.getChild (i); SequenceData q;
                q.name = t.getProperty ("name").toString(); q.bars = (int) t.getProperty ("bars", 1); q.fromString (t.getProperty ("events").toString());
                setSequence (i, q);
            }
        else if (oldPatterns.isValid()) convertOldPatterns (oldPatterns);
        stateChanged.sendChangeMessage();
    }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VegaDrumsProcessor(); }
