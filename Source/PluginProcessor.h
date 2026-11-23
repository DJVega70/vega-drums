#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "DrumModel.h"

class VegaDrumsProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    VegaDrumsProcessor();
    ~VegaDrumsProcessor() override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Vega Productions Drums"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    juce::AudioProcessorValueTreeState apvts;

    // ---------------------------------------------------------------- pads (edit on the message thread only)
    std::array<vdr::PadSound, vdr::numPads> pads;
    /** current settings of a pad (bank A reads its parameters) */
    vdr::PadSound getPad (int p) const;
    /** apply an edited pad: copies it to the audio thread, loads its sample, mirrors bank A into the parameters */
    void setPad (int p, const vdr::PadSound& s);
    void loadKit (int kit);
    /** builds bank A (drums) and bank B (keys, bass, loops) from the samples in a folder or zip */
    int autoKitFromFolder (const juce::File& folderOrZip, int firstBank = 0);
    juce::String kitName { "909 House" };
    juce::File samplesFolder;                       // the browser's "My Samples" folder

    // ---------------------------------------------------------------- sequences (edit on the message thread only)
    vdr::SequencePtr getSequence (int i) const { return std::atomic_load (&sequences[(size_t) juce::jlimit (0, vdr::numSequences - 1, i)]); }
    void setSequence (int i, vdr::SequenceData s);
    int currentSequence() const { return (int) pPattern->load(); }
    juce::File exportMidi (const juce::File& dest = {}) const;
    static int timingDivisionTicks (int choice);    // 0 = off
    int quantize (int tick) const;

    // ---------------------------------------------------------------- playing pads from the screen
    void padDown (int pad, float velocity);
    void padUp (int pad);
    void audition (int pad, float vel = 1.0f) { padDown (pad, vel); padUp (pad); }
    /** plays a sample file through the main output (browser preview); empty path stops it */
    void previewSample (const juce::String& path);
    /** merges hits recorded on the audio thread into their sequences (runs on a timer; public for tests) */
    void mergeRecorded();
    std::atomic<bool> recordArmed { false };
    std::atomic<int> eraseHeld { 0 };               // ERASE button: events of held pads are removed as the playhead passes

    // ---------------------------------------------------------------- read by the editor
    std::atomic<int> uiTick { -1 };                 // playhead inside the current sequence, -1 when stopped
    std::array<std::atomic<float>, vdr::numPads> padFlash {};
    std::atomic<float> outL { 0 }, outR { 0 }, uiScale { 1.0f };
    std::atomic<bool> transportRunning { false };
    double bpm = 120;
    juce::ChangeBroadcaster stateChanged;           // a whole new kit / state was loaded

private:
    static BusesProperties makeBuses();
    void timerCallback() override;
    void startNote (int pad, float vel, int samplePos);
    void stopNote (int pad);
    void publishPad (int p);
    void convertOldPatterns (const juce::ValueTree& patterns);

    double sr = 48000;
    struct PadRuntime
    {
        std::atomic<int> source { 0 }, chord { 0 }, choke { 0 }; std::atomic<bool> oneShot { true };
        std::atomic<float> level { 0.75f }, tune { 0 }, decay { 0.5f }, tone { 0.5f }, pan { 0 }; std::atomic<bool> mute { false };
    };
    std::array<PadRuntime, vdr::numPads> runtime;
    std::array<std::shared_ptr<const vdr::SampleData>, vdr::numPads> padSamples;   // std::atomic_load / atomic_store
    std::array<vdr::SequencePtr, vdr::numSequences> sequences;
    std::array<std::array<vdr::Voice, 2>, vdr::numPads> voices;
    std::array<int, vdr::numPads> lastVoice {};
    std::array<float, vdr::numPads> holdVel {};

    // events from the screen -> audio thread, and recorded hits -> message thread
    struct PadMsg { int pad; float vel; };            // vel < 0 = release
    juce::AbstractFifo uiFifo { 512 }; std::array<PadMsg, 512> uiQueue {};
    struct RecMsg { int pad; int tick; int vel; bool off; int seq; };
    juce::AbstractFifo recFifo { 1024 }; std::array<RecMsg, 1024> recQueue {};
    struct OpenRec { int pad = -1, seq = 0, startTick = 0; };
    std::vector<OpenRec> openRecordings;
    std::array<bool, vdr::numPads> held {};           // pads held right now (screen or MIDI), for note repeat / erase

    std::atomic<float>* lvl[vdr::padsPerBank] {}; std::atomic<float>* tun[vdr::padsPerBank] {}; std::atomic<float>* dec[vdr::padsPerBank] {};
    std::atomic<float>* ton[vdr::padsPerBank] {}; std::atomic<float>* pan[vdr::padsPerBank] {}; std::atomic<float>* mute[vdr::padsPerBank] {};
    std::atomic<float>* pPattern = nullptr; std::atomic<float>* pSwing = nullptr; std::atomic<float>* pKit = nullptr; std::atomic<float>* pMaster = nullptr;
    std::atomic<float>* pTempo = nullptr; std::atomic<float>* pPlay = nullptr; std::atomic<float>* pAccent = nullptr; std::atomic<float>* pHuman = nullptr;
    std::atomic<float>* pTc = nullptr; std::atomic<float>* pRepeat = nullptr; std::atomic<float>* pMetro = nullptr; std::atomic<float>* pFullLevel = nullptr;
    double internalPpq = 0; bool wasPlaying = false; double lastPpqEnd = -1;
    juce::Random humanRng;
    float metroPhase = 0, metroEnv = 0, metroFreq = 1000;
    std::shared_ptr<const vdr::SampleData> previewData; std::atomic<int> previewRequest { 0 }; vdr::Voice previewVoice;
    struct Pending { int pad; float vel; int at; };   // vel < 0 = note off
    std::array<Pending, 1024> pending {}; int numPending = 0;
    void schedule (int pad, float vel, int at) { if (numPending < (int) pending.size()) pending[(size_t) numPending++] = { pad, vel, at }; }
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VegaDrumsProcessor)
};
