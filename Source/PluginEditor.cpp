#include "PluginEditor.h"
using namespace vdr;

namespace
{
class StepGrid : public juce::Component
{
public:
    explicit StepGrid (VegaDrumsProcessor& p) : proc (p) {}
    std::function<void (int)> onSelectVoice;
    int selected = 0;
    static constexpr int nameW = 150, muteW = 30;

    juce::Rectangle<float> cell (int v, int s) const
    {
        const float rowH = (float) getHeight() / numVoices;
        const float x0 = (float) (nameW + muteW + 10), w = (getWidth() - x0) / VegaDrumsProcessor::numSteps;
        return { x0 + s * w + (s / 4) * 0.0f, v * rowH, w, rowH };
    }
    void paint (juce::Graphics& g) override
    {
        const int pat = (int) proc.apvts.getRawParameterValue ("pattern")->load();
        const int cur = proc.uiStep.load();
        const float rowH = (float) getHeight() / numVoices;
        for (int v = 0; v < numVoices; ++v)
        {
            auto nameR = juce::Rectangle<float> (0, v * rowH, (float) nameW, rowH).reduced (2.0f);
            const float lv = proc.voiceLevel[(size_t) v].load();
            g.setColour (juce::Colour (v == selected ? 0xff26302f : 0xff1f1f1f)); g.fillRoundedRectangle (nameR, 4.0f);
            if (lv > 0.01f) { g.setColour (VegaColours::accent.withAlpha (0.35f * lv)); g.fillRoundedRectangle (nameR, 4.0f); }
            g.setColour (v == selected ? VegaColours::accent : VegaColours::line); g.drawRoundedRectangle (nameR, 4.0f, 1.0f);
            g.setColour (VegaColours::text); g.setFont (VegaFonts::ui (12.0f, true));
            g.drawText (voiceName (v), nameR.reduced (8, 0).toNearestInt(), juce::Justification::centredLeft);
            const bool muted = proc.apvts.getRawParameterValue ("mute" + juce::String (v))->load() > 0.5f;
            auto mr = juce::Rectangle<float> ((float) nameW + 4, v * rowH, (float) muteW, rowH).reduced (2.0f, rowH * 0.22f);
            g.setColour (muted ? juce::Colour (0xffd9534f) : juce::Colour (0xff2a2a2a)); g.fillRoundedRectangle (mr, 3.0f);
            g.setColour (muted ? juce::Colours::white : VegaColours::textDim); g.setFont (VegaFonts::ui (10.0f, true));
            g.drawText ("M", mr.toNearestInt(), juce::Justification::centred);
            for (int s = 0; s < VegaDrumsProcessor::numSteps; ++s)
            {
                auto c = cell (v, s).reduced (2.5f);
                const int st = proc.steps[(size_t) pat][(size_t) v][(size_t) s].load();
                const bool beatStart = (s / 4) % 2 == 0;
                g.setColour (juce::Colour (beatStart ? 0xff242424 : 0xff1c1c1c)); g.fillRoundedRectangle (c, 3.0f);
                if (st > 0)
                {
                    g.setColour (st == 2 ? VegaColours::gold : VegaColours::accent);
                    g.fillRoundedRectangle (c.reduced (st == 2 ? 1.0f : 3.0f), 3.0f);
                }
                if (s == cur) { g.setColour (juce::Colours::white.withAlpha (0.25f)); g.fillRoundedRectangle (c, 3.0f); }
            }
        }
        g.setColour (VegaColours::textDim); g.setFont (VegaFonts::ui (9.0f));
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const float rowH = (float) getHeight() / numVoices;
        const int v = juce::jlimit (0, numVoices - 1, (int) (e.position.y / rowH));
        if (e.position.x < nameW) { selected = v; proc.audition (v); if (onSelectVoice) onSelectVoice (v); repaint(); return; }
        if (e.position.x < nameW + muteW + 6)
        {
            auto* m = proc.apvts.getParameter ("mute" + juce::String (v));
            m->beginChangeGesture(); m->setValueNotifyingHost (m->getValue() > 0.5f ? 0.0f : 1.0f); m->endChangeGesture(); repaint(); return;
        }
        const int s = stepAt (e.position.x); if (s < 0) return;
        const int pat = (int) proc.apvts.getRawParameterValue ("pattern")->load();
        auto& cellRef = proc.steps[(size_t) pat][(size_t) v][(size_t) s];
        // click cycles off -> on -> accent -> off; right-click / shift sets accent directly
        int nv = (cellRef.load() + 1) % 3;
        if (e.mods.isPopupMenu() || e.mods.isShiftDown()) nv = cellRef.load() == 2 ? 0 : 2;
        cellRef.store (nv);
        paintValue = nv; dragVoice = v;
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragVoice < 0) return;
        const int s = stepAt (e.position.x); if (s < 0) return;
        const int pat = (int) proc.apvts.getRawParameterValue ("pattern")->load();
        proc.steps[(size_t) pat][(size_t) dragVoice][(size_t) s].store (paintValue);
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { dragVoice = -1; }
private:
    int stepAt (float x) const
    {
        const float x0 = (float) (nameW + muteW + 10), w = (getWidth() - x0) / VegaDrumsProcessor::numSteps;
        if (x < x0) return -1;
        return juce::jlimit (0, VegaDrumsProcessor::numSteps - 1, (int) ((x - x0) / w));
    }
    VegaDrumsProcessor& proc; int paintValue = 1, dragVoice = -1;
};

class MidiDrag : public juce::Component
{
public:
    explicit MidiDrag (VegaDrumsProcessor& p) : proc (p) { setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (VegaColours::accent.withAlpha (0.2f)); g.fillRoundedRectangle (b, 4.0f);
        g.setColour (VegaColours::accent); g.drawRoundedRectangle (b, 4.0f, 1.2f);
        g.setFont (VegaFonts::ui (11.0f, true)); g.drawText ("DRAG MIDI", b.toNearestInt(), juce::Justification::centred);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 6) return;
        dragging = true;
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ proc.exportMidi().getFullPathName() }, false, this, [this] { dragging = false; });
    }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
private:
    VegaDrumsProcessor& proc; bool dragging = false;
};
}

class VegaDrumsEditor::Main : public juce::Component
{
public:
    explicit Main (VegaDrumsProcessor& p) : proc (p), a (p.apvts), grid (p), midiDrag (p),
        kit (a, "kit", "Kit"), swing (a, "swing", "Swing"), accent (a, "accent", "Accent"), human (a, "humanize", "Humanize"), master (a, "master", "Master"), tempo (a, "tempo", "Tempo"),
        patAtt (a, "pattern", patternBox), playAtt (a, "play", playBtn)
    {
        for (auto* c : std::initializer_list<juce::Component*> { &grid, &midiDrag, &kit, &swing, &accent, &human, &master, &tempo, &meter }) addAndMakeVisible (c);
        for (int i = 0; i < VegaDrumsProcessor::numPatterns; ++i)
        {
            auto* b = patButtons.add (new juce::TextButton (juce::String::charToString ((juce::juce_wchar) ('A' + i))));
            b->setClickingTogglesState (false); b->setColour (juce::TextButton::buttonOnColourId, VegaColours::accent);
            b->onClick = [this, i] { if (auto* q = a.getParameter ("pattern")) { q->beginChangeGesture(); q->setValueNotifyingHost (q->convertTo0to1 ((float) i)); q->endChangeGesture(); } };
            addAndMakeVisible (b);
        }
        patternBox.addItemList ({ "A", "B", "C", "D", "E", "F", "G", "H" }, 1);
        playBtn.setButtonText ("PLAY"); playBtn.setClickingTogglesState (true); playBtn.setColour (juce::TextButton::buttonOnColourId, VegaColours::accent);
        addAndMakeVisible (playBtn);
        auto btn = [this] (juce::TextButton& b, const char* t, std::function<void()> f) { b.setButtonText (t); b.onClick = std::move (f); addAndMakeVisible (b); };
        btn (clearBtn, "CLEAR", [this] { proc.clearPattern ((int) a.getRawParameterValue ("pattern")->load()); grid.repaint(); });
        btn (copyBtn, "COPY TO NEXT", [this]
        {
            const int p = (int) a.getRawParameterValue ("pattern")->load(), q = (p + 1) % VegaDrumsProcessor::numPatterns;
            for (int v = 0; v < numVoices; ++v) for (int s = 0; s < VegaDrumsProcessor::numSteps; ++s) proc.steps[(size_t) q][(size_t) v][(size_t) s].store (proc.steps[(size_t) p][(size_t) v][(size_t) s].load());
        });
        btn (factoryBtn, "FACTORY", [this] { proc.loadFactoryPatterns(); grid.repaint(); });
        btn (saveMidi, "SAVE MIDI...", [this]
        {
            chooser = std::make_unique<juce::FileChooser> ("Save pattern as MIDI", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Vega Drums.mid"), "*.mid");
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                                  [this] (const juce::FileChooser& fc) { if (fc.getResult() != juce::File()) proc.exportMidi (fc.getResult().withFileExtension (".mid")); });
        });
        grid.onSelectVoice = [this] (int v) { selectVoice (v); };
        selectVoice (0);
    }
    void selectVoice (int v)
    {
        selected = v; grid.selected = v;
        const juce::String s (v);
        vLevel = std::make_unique<vegaui::Knob> (a, "level" + s, "Level", true);
        vTune = std::make_unique<vegaui::Knob> (a, "tune" + s, "Tune");
        vDecay = std::make_unique<vegaui::Knob> (a, "decay" + s, "Decay");
        vTone = std::make_unique<vegaui::Knob> (a, "tone" + s, v == 0 ? "Click" : v == 1 ? "Snappy" : v == 4 || v == 5 || v == 8 ? "Brightness" : "Tone");
        vPan = std::make_unique<vegaui::Knob> (a, "pan" + s, "Pan");
        for (auto* k : { vLevel.get(), vTune.get(), vDecay.get(), vTone.get(), vPan.get() }) addAndMakeVisible (k);
        resized(); repaint();
    }
    void refresh()
    {
        grid.repaint();
        meter.setLevels (proc.outL.exchange (0.0f), proc.outR.exchange (0.0f));
        for (auto& l : proc.voiceLevel) l.store (l.load() * 0.8f);
        const int pat = (int) a.getRawParameterValue ("pattern")->load();
        for (int i = 0; i < patButtons.size(); ++i) patButtons[i]->setToggleState (i == pat, juce::dontSendNotification);
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (VegaColours::background);
        vegaui::drawTopBar (g, W, "DRUMS  VP-18");
        vegaui::drawBody (g, { 0, 35, W, H - 65 });
        g.setFont (VegaFonts::ui (10.0f, true)); g.setColour (VegaColours::textDim);
        g.drawText ("PATTERN", 14, 44, 80, 14, juce::Justification::left);
        g.setColour (VegaColours::line); g.drawHorizontalLine (118, 0, (float) W); g.drawHorizontalLine (536, 0, (float) W);
        vegaui::drawSection (g, { 0, 540, 760, 150 }, juce::String (voiceName (selected)), false);
        vegaui::drawSection (g, { 760, 540, W - 760, 150 }, "GROOVE");
        // beat numbers above the grid
        g.setFont (VegaFonts::ui (9.5f)); g.setColour (VegaColours::textDim);
        for (int s = 0; s < VegaDrumsProcessor::numSteps; ++s)
        {
            auto c = grid.cell (0, s).translated ((float) grid.getX(), (float) grid.getY() - 14.0f);
            g.setColour (s % 4 == 0 ? VegaColours::text : VegaColours::textDim.withAlpha (0.6f));
            g.drawText (s % 4 == 0 ? juce::String (s / 4 + 1) : juce::String ("."), c.withHeight (12).toNearestInt(), juce::Justification::centred);
        }
        vegaui::drawBottomBar (g, { 0, H - 30, W, 30 }, "CLICK = ON, CLICK AGAIN = ACCENT (GOLD)  -  DRAG TO PAINT  -  CLICK A NAME TO HEAR AND EDIT IT  -  MIDI NOTES 36-51 PLAY THE KIT (GM)");
    }
    void resized() override
    {
        int x = 14;
        for (auto* b : patButtons) { b->setBounds (x, 62, 40, 40); x += 44; }
        clearBtn.setBounds (370, 62, 70, 40); copyBtn.setBounds (444, 62, 100, 40); factoryBtn.setBounds (548, 62, 80, 40);
        kit.setBounds (640, 58, 140, 46);
        playBtn.setBounds (790, 62, 70, 40); tempo.setBounds (866, 40, 80, 76);
        midiDrag.setBounds (956, 62, 110, 22); saveMidi.setBounds (956, 88, 110, 22);
        meter.setVisible (false);
        grid.setBounds (14, 140, W - 28, 390);
        if (vLevel) { vLevel->setBounds (20, 556, 130, 130); vTune->setBounds (170, 560, 100, 112); vDecay->setBounds (290, 560, 100, 112); vTone->setBounds (410, 560, 100, 112); vPan->setBounds (530, 560, 100, 112); }
        swing.setBounds (774, 560, 100, 112); accent.setBounds (880, 560, 100, 112); human.setBounds (986, 560, 100, 112); master.setBounds (1092, 560, 100, 112);
    }
private:
    VegaDrumsProcessor& proc; juce::AudioProcessorValueTreeState& a;
    StepGrid grid; MidiDrag midiDrag;
    vegaui::Choice kit; vegaui::Knob swing, accent, human, master, tempo;
    juce::ComboBox patternBox; juce::TextButton playBtn;
    juce::AudioProcessorValueTreeState::ComboBoxAttachment patAtt; juce::AudioProcessorValueTreeState::ButtonAttachment playAtt;
    juce::OwnedArray<juce::TextButton> patButtons; juce::TextButton clearBtn, copyBtn, factoryBtn, saveMidi;
    std::unique_ptr<vegaui::Knob> vLevel, vTune, vDecay, vTone, vPan;
    vegaui::StereoMeter meter; int selected = 0;
    std::unique_ptr<juce::FileChooser> chooser;
};

VegaDrumsEditor::VegaDrumsEditor (VegaDrumsProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    main = std::make_unique<Main> (p);
    addAndMakeVisible (*main);
    constrainer.setFixedAspectRatio ((double) W / H);
    constrainer.setSizeLimits (W / 2, H / 2, W * 2, H * 2);
    setConstrainer (&constrainer);
    setResizable (true, true);
    const float s = juce::jlimit (0.5f, 2.0f, proc.uiScale.load());
    setSize (juce::roundToInt (W * s), juce::roundToInt (H * s));
    startTimerHz (30);
}
VegaDrumsEditor::~VegaDrumsEditor() { stopTimer(); setConstrainer (nullptr); main.reset(); setLookAndFeel (nullptr); }
void VegaDrumsEditor::timerCallback() { main->refresh(); }
void VegaDrumsEditor::resized()
{
    main->setBounds (0, 0, W, H);
    const float s = (float) getWidth() / (float) W;
    main->setTransform (juce::AffineTransform::scale (s));
    proc.uiScale.store (s);
}
juce::AudioProcessorEditor* VegaDrumsProcessor::createEditor() { return new VegaDrumsEditor (*this); }
