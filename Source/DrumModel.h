#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "DrumVoices.h"

/** Pads, banks, built-in kits, sequences and sample loading for Vega Drums. */
namespace vdr
{
constexpr int padsPerBank = 16, numBanks = 8, numPads = padsPerBank * numBanks;   // A01 .. H16
constexpr int numSequences = 8, ppqTicks = 960, sixteenth = ppqTicks / 4, maxBars = 8;
inline juce::String padLabel (int p) { return juce::String::charToString ((juce::juce_wchar) ('A' + p / padsPerBank)) + juce::String (p % padsPerBank + 1).paddedLeft ('0', 2); }

/** One pad's sound. Bank A pads A01-A16 also have automatable parameters (level0..15 etc.). */
struct PadSound
{
    int source = srcKick;          // vdr::Source
    int chord = chMin7;            // for organ / piano / strings
    int choke = 0;                 // pads in the same choke group cut each other (0 = none)
    bool oneShot = true;           // false: the sound stops when you let go of the pad / the note ends
    juce::String name;             // shown on the pad
    juce::String samplePath;       // for srcSample: a file, or "zipfile.zip|inner/path.wav"
    float level = 0.75f, tune = 0, decay = 0.5f, tone = 0.5f, pan = 0;
    bool mute = false;

    juce::ValueTree toTree (int index) const
    {
        juce::ValueTree t ("Pad");
        t.setProperty ("i", index, nullptr); t.setProperty ("src", source, nullptr); t.setProperty ("chord", chord, nullptr);
        t.setProperty ("choke", choke, nullptr); t.setProperty ("oneShot", oneShot, nullptr); t.setProperty ("name", name, nullptr);
        t.setProperty ("sample", samplePath, nullptr); t.setProperty ("level", level, nullptr); t.setProperty ("tune", tune, nullptr);
        t.setProperty ("decay", decay, nullptr); t.setProperty ("tone", tone, nullptr); t.setProperty ("pan", pan, nullptr); t.setProperty ("mute", mute, nullptr);
        return t;
    }
    void fromTree (const juce::ValueTree& t)
    {
        source = juce::jlimit (0, (int) numSources - 1, (int) t.getProperty ("src", source)); chord = juce::jlimit (0, (int) numChords - 1, (int) t.getProperty ("chord", chord));
        choke = (int) t.getProperty ("choke", choke); oneShot = (bool) t.getProperty ("oneShot", oneShot); name = t.getProperty ("name", name).toString();
        samplePath = t.getProperty ("sample", samplePath).toString(); level = (float) t.getProperty ("level", level); tune = (float) t.getProperty ("tune", tune);
        decay = (float) t.getProperty ("decay", decay); tone = (float) t.getProperty ("tone", tone); pan = (float) t.getProperty ("pan", pan); mute = (bool) t.getProperty ("mute", mute);
    }
    juce::String displayName() const
    {
        if (name.isNotEmpty()) return name;
        if (source == srcSample) return juce::File (samplePath.upToFirstOccurrenceOf ("|", false, false)).getFileNameWithoutExtension();
        juce::String n = juce::String (sourceName (source));
        if (! isMelodic (source)) return n;
        static const char* notes[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const juce::String root = notes[((juce::roundToInt (tune) % 12) + 12) % 12];
        if (source == srcAcidBass || source == srcSubBass) return n + " " + root;
        return n.upToFirstOccurrenceOf (" ", false, false) + " " + root + (chord == chSingle ? juce::String() : juce::String (chordName (chord)));
    }
};

// ---------------------------------------------------------------- built-in kits (synthesized: they ship with the plug-in)
struct KitInfo { const char* name; const char* genres; int machine; };   // machine: 0 = 909, 1 = 808, 2 = deep
inline const std::vector<KitInfo>& builtInKits()
{
    static const std::vector<KitInfo> k {
        { "909 House",           "80s 90s House",            0 },   // indices 0-2 match the old Kit parameter
        { "808 Deep",            "80s Deep House",           1 },
        { "Deep House",          "90s Deep House",           2 },
        { "Chicago Jack 1986",   "80s House Chicago",        0 },
        { "Acid Trax 1987",      "80s Acid House Chicago",   1 },
        { "Detroit 808 1988",    "80s Techno Detroit",       1 },
        { "NYC Garage 1989",     "80s Garage House",         0 },
        { "Piano House 1991",    "90s House",                0 },
        { "Deep Jersey 1992",    "90s Deep House Garage",    2 },
        { "Organ Bump 1993",     "90s House",                0 },
        { "UK Garage 1997",      "90s Garage",               2 },
        { "French Filter 1998",  "90s House",                0 },
    };
    return k;
}

/** Fills all 128 pads for a built-in kit: bank A drums + percussion, bank B keys and bass, banks C-H empty. */
inline void makeKit (int kit, std::array<PadSound, numPads>& pads)
{
    for (auto& p : pads) p = PadSound {};
    for (int i = numPads / numBanks * 2; i < numPads; ++i) { pads[(size_t) i].source = srcSample; pads[(size_t) i].level = 0.8f; }   // C-H: drop your samples here
    const int drumSrc[16] { srcKick, srcSnare, srcClap, srcRim, srcClosedHat, srcOpenHat, srcLowTom, srcHighTom,
                            srcRide, srcShaker, srcCowbell, srcCongaHi, srcCongaLo, srcCrash, srcTambourine, srcClave };
    for (int i = 0; i < 16; ++i)
    {
        auto& p = pads[(size_t) i];
        p.source = drumSrc[i]; p.level = i == 0 ? 0.9f : 0.75f;
        if (p.source == srcClosedHat || p.source == srcOpenHat) p.choke = 1;    // closed hat cuts the open hat
    }
    // bank B: four organ stabs, four piano stabs, string pad + chords, acid and sub bass
    auto key = [&] (int i, int src, int chord, float tune, const char* nm = "")
    {
        auto& p = pads[(size_t) (16 + i)];
        p.source = src; p.chord = chord; p.tune = tune; p.oneShot = false; p.name = nm;
        p.level = src == srcSubBass ? 0.42f : src == srcAcidBass ? 0.38f : src == srcOrgan ? 0.45f : 0.5f;
        p.decay = src == srcOrgan ? 0.15f : src == srcPiano ? 0.45f : 0.5f;
    };
    const float roots[4] { 0, 3, 5, 7 };     // C, D#, F, G
    for (int i = 0; i < 4; ++i)
    {
        key (i, srcOrgan, chMin7, roots[i]);
        key (4 + i, srcPiano, chMin7, roots[i]);
        key (8 + i, srcAcidBass, chSingle, roots[i]);
        key (12 + i, srcSubBass, chSingle, roots[i]);
    }
    // per-kit flavour
    switch (kit)
    {
        case 3:  pads[16].chord = pads[17].chord = pads[18].chord = pads[19].chord = chMin7; break;                // Chicago: organ m7
        case 4:  for (int i = 8; i < 12; ++i) pads[(size_t) (16 + i)].tone = 0.75f; pads[0].decay = 0.7f; break;   // Acid: squelchier
        case 5:  pads[0].decay = 0.85f; break;                                                                      // Detroit 808 boom
        case 6:  for (int i = 4; i < 8; ++i) pads[(size_t) (16 + i)].chord = chMaj7; break;                          // NYC: maj7 pianos
        case 7:  for (int i = 4; i < 8; ++i) { pads[(size_t) (16 + i)].chord = chMajor; pads[(size_t) (16 + i)].tone = 0.7f; }   // Piano house
                 for (int i = 0; i < 4; ++i) { pads[(size_t) (16 + i)].source = srcStrings; pads[(size_t) (16 + i)].chord = chMaj7; }
                 break;
        case 8:  for (int i = 0; i < 4; ++i) pads[(size_t) (16 + i)].chord = chMin9; break;                          // Jersey: m9 organ
        case 9:  for (int i = 0; i < 4; ++i) { pads[(size_t) (16 + i)].chord = chDom7; pads[(size_t) (16 + i)].tone = 0.7f; } break;
        case 10: pads[1].tune = 2; pads[4].decay = 0.3f; for (int i = 12; i < 16; ++i) pads[(size_t) (16 + i)].decay = 0.25f; break;
        case 11: for (int i = 0; i < 4; ++i) { pads[(size_t) (16 + i)].source = srcStrings; pads[(size_t) (16 + i)].chord = chDom7; } break;
        default: break;
    }
}

// ---------------------------------------------------------------- sequences
struct NoteEvent { int pad = 0, start = 0, length = sixteenth; int velocity = 100; };   // ticks at 960 PPQ
struct SequenceData
{
    juce::String name;
    int bars = 1;
    std::vector<NoteEvent> events;
    int lengthTicks() const { return bars * 4 * ppqTicks; }
    juce::String toString() const
    {
        juce::String s;
        for (auto& e : events) s << e.pad << ',' << e.start << ',' << e.length << ',' << e.velocity << ';';
        return s;
    }
    void fromString (const juce::String& s)
    {
        events.clear();
        for (auto& tok : juce::StringArray::fromTokens (s, ";", ""))
        {
            auto f = juce::StringArray::fromTokens (tok, ",", "");
            if (f.size() != 4) continue;
            NoteEvent e { juce::jlimit (0, numPads - 1, f[0].getIntValue()), std::max (0, f[1].getIntValue()), std::max (1, f[2].getIntValue()), juce::jlimit (1, 127, f[3].getIntValue()) };
            events.push_back (e);
        }
    }
};
using SequencePtr = std::shared_ptr<const SequenceData>;

/** Factory sequences. Rows use one character per 16th: '.' off, 'x' hit (vel 100), 'X' accent (vel 127),
    '-' holds the previous note (for stabs and bass). */
inline SequenceData sequenceFromRows (const char* name, int bars, std::initializer_list<std::pair<int, const char*>> rows)
{
    SequenceData s; s.name = name; s.bars = bars;
    for (auto& [pad, row] : rows)
    {
        const juce::String r (row);
        for (int i = 0; i < r.length(); ++i)
        {
            const auto c = r[i];
            if (c != 'x' && c != 'X') continue;
            int len = 1; while (i + len < r.length() && r[i + len] == '-') ++len;
            s.events.push_back ({ pad, (i % (bars * 16)) * sixteenth, len * sixteenth - 20, c == 'X' ? 127 : 100 });
        }
    }
    return s;
}

inline std::array<SequenceData, numSequences> factorySequences()
{
    // pads: 0 kick 1 snare 2 clap 3 rim 4 ch 5 oh 6 ltom 7 htom 8 ride 9 shaker 10 cowbell 11 conga hi 12 conga lo 13 crash 14 tamb 15 clave
    //       16-19 organ C D# F G  20-23 piano  24-27 acid bass  28-31 sub bass
    return {
        sequenceFromRows ("Chicago Jack", 2, {
            { 0, "X...X...X...X...X...X...X...X..." }, { 2, "....X.......X.......X.......X..." },
            { 4, "..x...x...x...x...x...x...x...x." }, { 5, "..............x...............x." },
            { 10, "...x......x.......x.....x......." }, { 16, "..x--.....x--.........x--...x-.." },
            { 28, "x-.x..x-..x.x...x-.x..x-..x.x..." } }),
        sequenceFromRows ("Deep 1991", 2, {
            { 0, "X...X...X...X...X...X...X...X..." }, { 3, "...x......x.....................x" },
            { 2, "....x.......x.......x.......x..." }, { 4, "..x...x...x...x...x...x...x...x." },
            { 9, "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx" }, { 16, "x----.....x--.......x----...x--." },
            { 28, "x-..x-.x..x...x.x-..x-.x..x...x." } }),
        sequenceFromRows ("Garage Shuffle", 1, {
            { 0, "X......x..X....." }, { 1, "....X.......X..." }, { 4, "x.x.x.x.x.x.x.x." },
            { 14, "..x...x...x...x." }, { 21, "..x--.....x-...." } }),
        sequenceFromRows ("Detroit Drive", 1, {
            { 0, "X...X...X...X..." }, { 2, "....X.......X..." }, { 4, "xxXxxxXxxxXxxxXx" },
            { 5, "..X...X...X...X." }, { 25, "x.xx.xx.x.xx.xx." } }),
        sequenceFromRows ("Acid Trax", 2, {
            { 0, "X...X...X...X...X...X...X...X..." }, { 2, "....x.......x.......x.......x..." },
            { 4, "x.x.x.x.x.x.x.x.x.x.x.x.x.x.x.x." }, { 24, "x.xx.x.xx.x..x.xx.xx.x.xx.x.xx.x" },
            { 26, ".......x...............x........" } }),
        sequenceFromRows ("Piano Anthem", 2, {
            { 0, "X...X...X...X...X...X...X...X..." }, { 2, "....X.......X.......X.......X..." },
            { 5, "..x...x...x...x...x...x...x...x." }, { 13, "X..............................." },
            { 20, "x--.x--...x-.x--x--.x--...x-.x--" }, { 30, "x.......x.......x.......x......." } }),
        sequenceFromRows ("Disco Loop", 1, {
            { 0, "X...X...X...X..." }, { 1, "....X.......X..." }, { 4, "x.x.x.x.x.x.x.x." },
            { 5, "..X...X...X...X." }, { 7, "...........x..x." } }),
        sequenceFromRows ("Empty", 1, { { 0, "X..............." } }),
    };
}

// ---------------------------------------------------------------- sample files
inline bool isAudioFile (const juce::String& name)
{
    const auto n = name.toLowerCase();
    return (n.endsWith (".wav") || n.endsWith (".aif") || n.endsWith (".aiff") || n.endsWith (".flac") || n.endsWith (".mp3") || n.endsWith (".ogg"))
           && ! n.contains ("__macosx") && ! juce::File (n).getFileName().startsWith ("._");
}

/** Loads "file.wav" or "pack.zip|folder/file.wav" into a stereo buffer (max 60 s). */
inline std::shared_ptr<SampleData> loadSample (const juce::String& path)
{
    static juce::AudioFormatManager fm; static bool init = false;
    if (! init) { fm.registerBasicFormats(); init = true; }
    std::unique_ptr<juce::AudioFormatReader> reader;
    if (path.contains ("|"))
    {
        juce::ZipFile zip (juce::File (path.upToFirstOccurrenceOf ("|", false, false)));
        const auto inner = path.fromFirstOccurrenceOf ("|", false, false);
        const int idx = zip.getIndexOfFileName (inner);
        if (idx < 0) return {};
        std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (idx));
        if (in == nullptr) return {};
        juce::MemoryBlock mb; in->readIntoMemoryBlock (mb);
        reader.reset (fm.createReaderFor (std::make_unique<juce::MemoryInputStream> (mb, true)));
    }
    else reader.reset (fm.createReaderFor (juce::File (path)));
    if (reader == nullptr || reader->lengthInSamples <= 0) return {};
    auto s = std::make_shared<SampleData>();
    const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 60.0));
    s->audio.setSize (2, len + 1);
    s->audio.clear();
    reader->read (&s->audio, 0, len, 0, true, true);
    if (reader->numChannels == 1) s->audio.copyFrom (1, 0, s->audio, 0, 0, len);
    s->rate = reader->sampleRate; s->path = path;
    s->name = juce::File (path.fromLastOccurrenceOf ("|", false, false)).getFileNameWithoutExtension();
    return s;
}

/** Guesses what a sample is from its file name, for auto-kits and the browser. */
inline int guessSource (const juce::String& fileName)
{
    const auto n = fileName.toLowerCase().replaceCharacters ("-_.", "   ");
    auto has = [&] (std::initializer_list<const char*> words) { for (auto w : words) if (n.contains (w)) return true; return false; };
    if (has ({ "loop", "bpm" }) && ! has ({ "one shot", "oneshot", "hit" })) return -1;           // loops aren't sorted into the kit
    if (has ({ "kick", " bd", "bd ", "bassdrum", "bass drum" })) return srcKick;
    if (has ({ "snare", " sd", "sd " })) return srcSnare;
    if (has ({ "clap", " cp", "cp " })) return srcClap;
    if (has ({ "rim", " rs", "rs ", "sidestick", "side stick", "stick" })) return srcRim;
    if (has ({ "open hat", "openhat", "open hh", " oh", "oh ", "ohh" })) return srcOpenHat;
    if (has ({ "hat", "hh", " ch", "ch " })) return srcClosedHat;
    if (has ({ "low tom", "lotom", " lt", "tom lo", "tom 1" })) return srcLowTom;
    if (has ({ "tom" })) return srcHighTom;
    if (has ({ "ride", " rd" })) return srcRide;
    if (has ({ "shaker", "shake", "maraca", "cabasa" })) return srcShaker;
    if (has ({ "cowbell", " cb" })) return srcCowbell;
    if (has ({ "conga", "bongo", "perc", "timbale" })) return srcCongaHi;
    if (has ({ "crash", "cymbal", " cy" })) return srcCrash;
    if (has ({ "tamb" })) return srcTambourine;
    if (has ({ "clave", "block" })) return srcClave;
    if (has ({ "organ" })) return srcOrgan;
    if (has ({ "piano", "keys", "rhodes", "epiano" })) return srcPiano;
    if (has ({ "string", "pad" })) return srcStrings;
    if (has ({ "acid", "303" })) return srcAcidBass;
    if (has ({ "bass", "sub" })) return srcSubBass;
    return -1;
}

/** loops and tempo folders: by the whole path, since packs often keep loops in "Loops" or "124bpm" folders */
inline bool isLoopPath (const juce::String& path)
{
    const auto n = path.toLowerCase();
    return n.contains ("loop") || n.contains ("bpm");
}

/** All audio files in a folder (recursively) and inside any zip files in it. Paths use "zip|inner" for zip entries. */
inline juce::StringArray scanSamples (const juce::File& folder, int limit = 20000)
{
    juce::StringArray out;
    if (! folder.isDirectory()) return out;
    for (const auto& e : juce::RangedDirectoryIterator (folder, true, "*", juce::File::findFiles))
    {
        const auto f = e.getFile();
        if (f.hasFileExtension ("zip"))
        {
            juce::ZipFile zip (f);
            for (int i = 0; i < zip.getNumEntries() && out.size() < limit; ++i)
                if (auto* z = zip.getEntry (i)) if (isAudioFile (z->filename)) out.add (f.getFullPathName() + "|" + z->filename);
        }
        else if (isAudioFile (f.getFileName())) out.add (f.getFullPathName());
        if (out.size() >= limit) break;
    }
    return out;
}

/** genre / era tags from a sample's folders and name, for the browser filter */
inline juce::StringArray tagsFor (const juce::String& path)
{
    const auto n = path.toLowerCase();
    juce::StringArray t;
    auto add = [&] (const char* tag, std::initializer_list<const char*> words) { for (auto w : words) if (n.contains (w)) { t.addIfNotAlreadyThere (tag); return; } };
    add ("80s", { "80s", "808", "707", "727", "linn", "retro", "vintage", "chicago", "1987", "1988", "1989" });
    add ("90s", { "90s", "909", "1991", "1992", "1993", "garage", "m1" });
    add ("House", { "house", "jack" });
    add ("Deep House", { "deep" });
    add ("Acid", { "acid", "303" });
    add ("Techno", { "techno", "detroit" });
    add ("Garage", { "garage" });
    add ("Loops", { "loop", "bpm" });
    return t;
}
} // namespace vdr
