#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "DrumVoices.h"

class VegaDrumsProcessor : public juce::AudioProcessor
{
public:
    VegaDrumsProcessor();
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

    // sequencer: 8 patterns x 10 tracks x 16 steps; 0 = off, 1 = on, 2 = accent
    static constexpr int numPatterns = 8, numSteps = 16;
    std::array<std::array<std::array<std::atomic<int>, numSteps>, vdr::numVoices>, numPatterns> steps {};
    void loadFactoryPatterns();
    void clearPattern (int pat);
    juce::File exportMidi (const juce::File& dest = {}) const;

    void audition (int voice, float vel = 1.0f) { auditionMask.fetch_or (1 << voice); auditionVel.store (vel); }

    std::atomic<int> uiStep { -1 };
    std::array<std::atomic<float>, vdr::numVoices> voiceLevel {};
    std::atomic<float> outL { 0 }, outR { 0 }, uiScale { 1.0f };
    double bpm = 120;

private:
    static BusesProperties makeBuses();
    void triggerVoice (int v, float vel);
    double sr = 48000;
    std::array<vdr::Voice, vdr::numVoices> voices;
    std::atomic<int> auditionMask { 0 }; std::atomic<float> auditionVel { 1 };
    double internalPpq = 0; long long lastStepIndex = -1; bool wasPlaying = false;
    juce::Random humanRng;
    std::atomic<float>* lvl[vdr::numVoices] {}; std::atomic<float>* tun[vdr::numVoices] {}; std::atomic<float>* dec[vdr::numVoices] {};
    std::atomic<float>* ton[vdr::numVoices] {}; std::atomic<float>* pan[vdr::numVoices] {}; std::atomic<float>* mute[vdr::numVoices] {};
    std::atomic<float>* pPattern = nullptr; std::atomic<float>* pSwing = nullptr; std::atomic<float>* pKit = nullptr; std::atomic<float>* pMaster = nullptr;
    std::atomic<float>* pTempo = nullptr; std::atomic<float>* pPlay = nullptr; std::atomic<float>* pAccent = nullptr; std::atomic<float>* pHuman = nullptr;
    struct Pending { int voice; float vel; int at; };
    std::array<Pending, 64> pending {}; int numPending = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VegaDrumsProcessor)
};
