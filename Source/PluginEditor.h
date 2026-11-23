#pragma once
#include "PluginProcessor.h"
#include "VegaUI.h"

class VegaDrumsEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, private juce::Timer
{
public:
    explicit VegaDrumsEditor (VegaDrumsProcessor&);
    ~VegaDrumsEditor() override;
    void paint (juce::Graphics&) override {}
    void resized() override;
    static constexpr int W = 1600, H = 960;
    class Main;
private:
    void timerCallback() override;
    VegaDrumsProcessor& proc;
    VegaLookAndFeel lnf;
    std::unique_ptr<Main> main;
    juce::ComponentBoundsConstrainer constrainer;
};
