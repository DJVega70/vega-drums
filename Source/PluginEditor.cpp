#include "PluginEditor.h"

using namespace vdr;
using namespace VegaColours;

namespace
{
// ---------------------------------------------------------------- colours: VEGA FL49 scheme, pads coloured by sound family
juce::Colour padColour (int source)
{
    switch (source)
    {
        case srcKick:                               return juce::Colour (0xffc0504a);
        case srcSnare: case srcClap: case srcRim:   return juce::Colour (0xffc97a3c);
        case srcClosedHat: case srcOpenHat: case srcRide: case srcCrash: return juce::Colour (0xffc9a63c);
        case srcLowTom: case srcHighTom: case srcCongaHi: case srcCongaLo: return juce::Colour (0xff3fa6a0);
        case srcShaker: case srcTambourine: case srcCowbell: case srcClave: return juce::Colour (0xff5aa25a);
        case srcOrgan: case srcPiano: case srcStrings: return juce::Colour (0xff9b59c0);
        case srcAcidBass: case srcSubBass:          return juce::Colour (0xff3a9bb5);
        default:                                    return juce::Colour (0xff7d8a99);
    }
}
juce::Colour padColourFor (const PadSound& s)
{
    if (s.source != srcSample) return padColour (s.source);
    const int g = guessSource (juce::File (s.samplePath.fromLastOccurrenceOf ("|", false, false)).getFileName());
    return g >= 0 ? padColour (g) : padColour (srcSample);
}

void panelBox (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title = {})
{
    g.setColour (panel); g.fillRect (r);
    g.setColour (line); g.drawRect (r);
    if (title.isNotEmpty())
    {
        auto t = r.removeFromTop (22);
        g.setColour (bar); g.fillRect (t.reduced (1));
        g.setColour (textDim); g.setFont (VegaFonts::ui (11.0f, true));
        g.drawText (title, t.reduced (8, 0), juce::Justification::centredLeft);
    }
}

/** flat button in the FL49 style: dark key, teal when on */
class FlatButton : public juce::Button
{
public:
    FlatButton (const juce::String& t, bool toggle = false) : juce::Button (t) { setClickingTogglesState (toggle); }
    juce::Colour onColour = accent;
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const bool on = getToggleState();
        g.setColour (on ? onColour.withAlpha (0.9f) : juce::Colour (0xff35363a).brighter (over ? 0.15f : 0.0f).darker (down ? 0.3f : 0.0f));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (on ? onColour.brighter (0.4f) : juce::Colour (0xff4a4b50)); g.drawRoundedRectangle (r, 3.0f, 1.0f);
        g.setColour (on ? juce::Colour (0xff101214) : VegaColours::text); g.setFont (VegaFonts::ui (11.5f, true));
        g.drawFittedText (getButtonText(), getLocalBounds().reduced (3, 0), juce::Justification::centred, 1, 0.8f);
    }
};

/** a Q-Link knob: works on whatever getter / setter it is given */
class QKnob : public juce::Component
{
public:
    std::function<float()> get; std::function<void (float)> set; std::function<juce::String (float)> fmt;
    juce::String title; float minV = 0, maxV = 1, defV = 0.5f, step = 0;
    QKnob() { setRepaintsOnMouseActivity (true); }
    void configure (const juce::String& t, float lo, float hi, float def, float st, std::function<float()> g, std::function<void (float)> s, std::function<juce::String (float)> f)
    { title = t; minV = lo; maxV = hi; defV = def; step = st; get = std::move (g); set = std::move (s); fmt = std::move (f); repaint(); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float v = get ? get() : 0.0f, n = maxV > minV ? (v - minV) / (maxV - minV) : 0.0f;
        auto k = r.withTrimmedTop (14).withTrimmedBottom (16);
        const float rad = std::min (k.getWidth(), k.getHeight()) * 0.5f - 4.0f;
        const auto c = k.getCentre();
        const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f, a = a0 + juce::jlimit (0.0f, 1.0f, n) * (a1 - a0);
        juce::Path bg; bg.addCentredArc (c.x, c.y, rad + 3, rad + 3, 0, a0, a1, true);
        g.setColour (juce::Colour (0xff3a3b40)); g.strokePath (bg, juce::PathStrokeType (2.5f));
        juce::Path fg; fg.addCentredArc (c.x, c.y, rad + 3, rad + 3, 0, minV < 0 && maxV > 0 ? (a0 + a1) * 0.5f : a0, a, true);
        g.setColour (accent); g.strokePath (fg, juce::PathStrokeType (2.5f));
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff4a4c52), c.x, c.y - rad, juce::Colour (0xff1b1c1f), c.x, c.y + rad, false));
        g.fillEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2);
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.drawEllipse (c.x - rad, c.y - rad, rad * 2, rad * 2, 1.0f);
        g.setColour (isMouseOverOrDragging() ? accent : text);
        g.drawLine (c.x + std::sin (a) * rad * 0.35f, c.y - std::cos (a) * rad * 0.35f, c.x + std::sin (a) * rad * 0.9f, c.y - std::cos (a) * rad * 0.9f, 2.2f);
        g.setColour (text); g.setFont (VegaFonts::ui (10.5f, true));
        g.drawFittedText (title, r.removeFromTop (14).toNearestInt(), juce::Justification::centred, 1, 0.7f);
        g.setColour (textDim); g.setFont (VegaFonts::ui (10.5f));
        g.drawFittedText (fmt ? fmt (v) : juce::String (v, 2), r.removeFromBottom (15).toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
    void mouseDown (const juce::MouseEvent&) override { startV = get ? get() : 0.0f; }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! set) return;
        float v = startV - (float) e.getDistanceFromDragStartY() / (e.mods.isShiftDown() ? 800.0f : 200.0f) * (maxV - minV);
        v = juce::jlimit (minV, maxV, v);
        if (step > 0) v = std::round (v / step) * step;
        set (v); repaint();
    }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (set) { set (defV); repaint(); } }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (! set || ! get) return;
        float v = get() + (step > 0 ? (w.deltaY > 0 ? step : -step) : w.deltaY * 0.1f * (maxV - minV));
        set (juce::jlimit (minV, maxV, v)); repaint();
    }
private:
    float startV = 0;
};

juce::String pct (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }

// ---------------------------------------------------------------- shared settings (sample folders), shared by every instance
juce::PropertiesFile& settings()
{
    static std::unique_ptr<juce::PropertiesFile> f;
    if (f == nullptr)
    {
        juce::PropertiesFile::Options o; o.applicationName = "Vega Drums"; o.filenameSuffix = ".settings"; o.osxLibrarySubFolder = "Application Support"; o.folderName = "Vega Productions";
        f = std::make_unique<juce::PropertiesFile> (o);
    }
    return *f;
}
} // namespace

// =====================================================================================================  Main
class VegaDrumsEditor::Main : public juce::Component, public juce::ChangeListener
{
public:
    explicit Main (VegaDrumsProcessor& p) : proc (p), grid (*this), pads (*this), browser (*this)
    {
        for (auto* b : { &playB, &stopB, &recB, &metroB, &repeatB, &fullB, &sixteenB, &eraseB, &dragB, &toStartB })
            addAndMakeVisible (b);
        playB.setClickingTogglesState (false); recB.onColour = juce::Colour (0xffe0493c);
        toStartB.onClick = [this] { proc.apvts.getParameter ("play")->setValueNotifyingHost (0.0f); };
        playB.onClick = [this] { auto* q = proc.apvts.getParameter ("play"); q->setValueNotifyingHost (q->getValue() > 0.5f ? 0.0f : 1.0f); };
        stopB.onClick = [this] { proc.apvts.getParameter ("play")->setValueNotifyingHost (0.0f); recB.setToggleState (false, juce::sendNotification); };
        recB.onClick = [this] { proc.recordArmed.store (recB.getToggleState()); };
        auto bindToggle = [this] (FlatButton& b, const char* id)
        {
            b.onClick = [this, &b, id] { proc.apvts.getParameter (id)->setValueNotifyingHost (b.getToggleState() ? 1.0f : 0.0f); };
        };
        bindToggle (metroB, "metronome"); bindToggle (repeatB, "noteRepeat"); bindToggle (fullB, "fullLevel");
        sixteenB.onClick = [this] { pads.repaint(); };
        eraseB.onStateChange = [this] { proc.eraseHeld.store (eraseB.isDown() ? 1 : 0); };
        eraseB.onClick = [this] { if (! proc.transportRunning.load()) eraseMenu(); };
        dragB.onClick = [this] { auto f = proc.exportMidi(); juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this); };
        dragB.setTooltip ("Drag the current sequence into your DAW as a MIDI clip");

        tcBox.addItemList ({ "TC Off", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }, 1);
        tcAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "tc", tcBox);
        addAndMakeVisible (tcBox);
        for (int i = 0; i < numSequences; ++i) seqBox.addItem (juce::String (i + 1) + "  " + proc.getSequence (i)->name, i + 1);
        seqAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "pattern", seqBox);
        seqBox.onChange = [this] { grid.repaint(); refreshSequencePanel(); };
        addAndMakeVisible (seqBox);
        for (auto* b : { &barsMinus, &barsPlus, &renameB, &clearSeqB, &copySeqB, &doubleB }) addAndMakeVisible (b);
        barsMinus.onClick = [this] { editSequence ([] (SequenceData& s) { s.bars = std::max (1, s.bars - 1); const int L = s.lengthTicks(); s.events.erase (std::remove_if (s.events.begin(), s.events.end(), [L] (auto& e) { return e.start >= L; }), s.events.end()); }); };
        barsPlus.onClick  = [this] { editSequence ([] (SequenceData& s) { s.bars = std::min (maxBars, s.bars + 1); }); };
        doubleB.onClick   = [this] { editSequence ([] (SequenceData& s) { if (s.bars * 2 > maxBars) return; const int L = s.lengthTicks(); auto ev = s.events; for (auto e : ev) { e.start += L; s.events.push_back (e); } s.bars *= 2; }); };
        clearSeqB.onClick = [this] { editSequence ([] (SequenceData& s) { s.events.clear(); }); };
        renameB.onClick = [this] { renameSequence(); };
        copySeqB.onClick = [this] { copySequenceMenu(); };

        // pad inspector
        for (int s = 0; s < numSources; ++s) srcBox.addItem (sourceName (s), s + 1);
        for (int c = 0; c < numChords; ++c) chordBox.addItem (juce::String ("Chord: ") + chordName (c), c + 1);
        chokeBox.addItem ("No choke", 1); for (int i = 1; i <= 8; ++i) chokeBox.addItem ("Choke group " + juce::String (i), i + 1);
        modeBox.addItem ("One shot", 1); modeBox.addItem ("Note on / off", 2);
        for (auto* b : { &srcBox, &chordBox, &chokeBox, &modeBox }) { addAndMakeVisible (b); b->onChange = [this] { applyInspector(); }; }
        for (auto* b : { &loadSampleB, &clearSampleB }) addAndMakeVisible (b);
        loadSampleB.onClick = [this] { chooseSampleForPad(); };
        clearSampleB.onClick = [this] { auto s = proc.getPad (selectedPad); s.source = srcSample; s.samplePath = {}; s.name = {}; proc.setPad (selectedPad, s); refreshAll(); };

        addAndMakeVisible (grid); addAndMakeVisible (pads); addAndMakeVisible (browser);
        for (auto& q : qk) addAndMakeVisible (q);
        configureQLinks();
        proc.stateChanged.addChangeListener (this);
        refreshSequencePanel(); refreshInspector();
    }
    ~Main() override { proc.stateChanged.removeChangeListener (this); }
    void changeListenerCallback (juce::ChangeBroadcaster*) override { refreshAll(); }

    void refreshAll() { refreshSequencePanel(); refreshInspector(); grid.repaint(); pads.repaint(); for (auto& q : qk) q.repaint(); repaint(); }

    // ------------------------------------------------------------ state shared by the views
    VegaDrumsProcessor& proc;
    int bank = 0, selectedPad = 0;
    void selectPad (int p, bool audition = false)
    {
        selectedPad = juce::jlimit (0, numPads - 1, p);
        bank = selectedPad / padsPerBank;
        if (audition) proc.audition (selectedPad, 0.8f);
        refreshInspector(); grid.repaint(); pads.repaint(); for (auto& q : qk) q.repaint(); repaint();
    }
    void editSequence (const std::function<void (SequenceData&)>& fn)
    {
        auto s = *proc.getSequence (proc.currentSequence());
        fn (s); proc.setSequence (proc.currentSequence(), s);
        grid.repaint(); refreshSequencePanel();
    }
    int divisionTicks() const { const int d = VegaDrumsProcessor::timingDivisionTicks ((int) proc.apvts.getRawParameterValue ("tc")->load()); return d > 0 ? d : sixteenth / 4; }

    void timerTick()
    {
        playB.setToggleState (proc.transportRunning.load(), juce::dontSendNotification);
        metroB.setToggleState (proc.apvts.getRawParameterValue ("metronome")->load() > 0.5f, juce::dontSendNotification);
        repeatB.setToggleState (proc.apvts.getRawParameterValue ("noteRepeat")->load() > 0.5f, juce::dontSendNotification);
        fullB.setToggleState (proc.apvts.getRawParameterValue ("fullLevel")->load() > 0.5f, juce::dontSendNotification);
        const int tick = proc.uiTick.load();
        if (tick != lastTick) { lastTick = tick; grid.repaint(); repaint (counterArea); }
        bool flash = false;
        for (auto& f : proc.padFlash) if (f.load() > 0.01f) { f.store (f.load() * 0.8f); flash = true; }
        if (flash || flashing) pads.repaint();
        flashing = flash;
        const float l = std::max (proc.outL.exchange (0.0f), proc.outR.exchange (0.0f));
        meter = std::max (l, meter * 0.85f);
        repaint (meterArea);
        if (++slow % 6 == 0) { for (auto& q : qk) if (! q.isMouseButtonDown()) q.repaint(); repaint (counterArea); }
        if (seqCount != proc.getSequence (proc.currentSequence())->events.size()) { seqCount = proc.getSequence (proc.currentSequence())->events.size(); grid.repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (background);
        // ---- top bar
        auto top = getLocalBounds().removeFromTop (56);
        g.setColour (bar); g.fillRect (top);
        g.setColour (line); g.drawHorizontalLine (55, 0, (float) getWidth());
        g.setColour (text); g.setFont (VegaFonts::ui (22.0f, true)); g.drawText ("VEGA", 16, 6, 70, 30, juce::Justification::centredLeft);
        g.setColour (accent); g.drawText ("DRUMS", 80, 6, 100, 30, juce::Justification::centredLeft);
        g.setColour (textDim); g.setFont (VegaFonts::ui (10.5f)); g.drawText ("by Vega Productions", 18, 33, 160, 14, juce::Justification::centredLeft);
        // counter: BAR : BEAT : TICK + BPM
        auto c = counterArea;
        g.setColour (oled); g.fillRoundedRectangle (c.toFloat(), 4.0f);
        g.setColour (line); g.drawRoundedRectangle (c.toFloat(), 4.0f, 1.0f);
        const int tick = std::max (0, proc.uiTick.load());
        const int bar1 = tick / (4 * ppqTicks) + 1, beat = tick / ppqTicks % 4 + 1, tk = tick % ppqTicks;
        auto cell = [&] (int x, int w, const juce::String& label, const juce::String& value, juce::Colour col)
        {
            g.setColour (textDim); g.setFont (VegaFonts::ui (9.5f, true)); g.drawText (label, c.getX() + x, c.getY() + 3, w, 11, juce::Justification::centred);
            g.setColour (col); g.setFont (VegaFonts::mono (21.0f)); g.drawText (value, c.getX() + x, c.getY() + 14, w, 26, juce::Justification::centred);
        };
        cell (6, 56, "BAR", juce::String (bar1), text); cell (62, 56, "BEAT", juce::String (beat), text); cell (118, 66, "TICK", juce::String (tk).paddedLeft ('0', 3), text);
        const bool app = proc.wrapperType == juce::AudioProcessor::wrapperType_Standalone;
        const float tempo = app ? proc.apvts.getRawParameterValue ("tempo")->load() : (float) proc.bpm;
        cell (190, 100, "BPM", juce::String (tempo, 2), accent);
        cell (296, 70, "SWING", juce::String (juce::roundToInt (proc.apvts.getRawParameterValue ("swing")->load())), text);
        // master meter
        auto m = meterArea.toFloat();
        g.setColour (oled); g.fillRoundedRectangle (m, 2.0f);
        const float db = juce::jlimit (-60.0f, 6.0f, vegaui::gainToDb (meter)), fr = (db + 60.0f) / 66.0f;
        g.setColour (db > -3 ? juce::Colour (0xffe0493c) : accent); g.fillRect (m.reduced (2).withWidth ((m.getWidth() - 4) * fr));
        g.setColour (textDim); g.setFont (VegaFonts::ui (9.5f, true)); g.drawText ("OUT", meterArea.translated (0, -13).withHeight (12), juce::Justification::centredLeft);

        // ---- left column: sequence + pad inspector
        panelBox (g, seqPanel, "SEQUENCE");
        g.setColour (textDim); g.setFont (VegaFonts::ui (11.0f));
        auto sp = seqPanel.reduced (10).withTrimmedTop (22);
        g.drawText ("Bars", sp.getX(), barsMinus.getY(), 50, barsMinus.getHeight(), juce::Justification::centredLeft);
        g.setColour (text); g.setFont (VegaFonts::ui (15.0f, true));
        g.drawText (juce::String (proc.getSequence (proc.currentSequence())->bars), barsMinus.getRight(), barsMinus.getY(), barsPlus.getX() - barsMinus.getRight(), barsMinus.getHeight(), juce::Justification::centred);
        g.setColour (textDim); g.setFont (VegaFonts::ui (11.0f));
        g.drawText (juce::String ((int) proc.getSequence (proc.currentSequence())->events.size()) + " events", sp.getX(), doubleB.getBottom() + 6, sp.getWidth(), 16, juce::Justification::centredLeft);

        panelBox (g, padPanel, "PAD  " + padLabel (selectedPad));
        auto ps = proc.getPad (selectedPad);
        auto sw = padPanel.reduced (10).withTrimmedTop (24).removeFromTop (34);
        g.setColour (padColourFor (ps)); g.fillRoundedRectangle (sw.toFloat(), 4.0f);
        g.setColour (juce::Colour (0xff101214)); g.setFont (VegaFonts::ui (14.0f, true));
        g.drawFittedText (ps.displayName(), sw.reduced (8, 0), juce::Justification::centredLeft, 1, 0.7f);
        g.setColour (textDim); g.setFont (VegaFonts::ui (10.5f));
        auto info = padPanel.reduced (10); info = info.withTop (clearSampleB.getBottom() + 8);
        juce::String outName = selectedPad % padsPerBank < numVoices ? juce::String (voiceName (selectedPad % padsPerBank)) : "Pad " + juce::String (selectedPad % padsPerBank + 1);
        g.drawFittedText ("Output: " + outName.substring (0, 1) + outName.substring (1).toLowerCase() + " (or Main)\n"
                          "MIDI: " + midiNoteText (selectedPad) + "\nKit: " + proc.kitName, info.removeFromTop (54), juce::Justification::topLeft, 3);

        // ---- section frames
        panelBox (g, qArea, "Q-LINKS");
        panelBox (g, padArea, {});
        g.setColour (textDim); g.setFont (VegaFonts::ui (10.0f));
        g.drawText (statusText(), getLocalBounds().removeFromBottom (20).withTrimmedLeft (240).withTrimmedRight (330), juce::Justification::centredLeft);
    }

    juce::String midiNoteText (int pad) const
    {
        if (pad < numVoices) return juce::MidiMessage::getMidiNoteName (gmNote (pad), true, true, 3) + " (GM drums)";
        static const int extra[] { 56, 62, 64, 49, 54, 75 };
        if (pad < padsPerBank) return juce::MidiMessage::getMidiNoteName (extra[pad - numVoices], true, true, 3) + " (GM drums)";
        if (pad < padsPerBank + 64) return juce::MidiMessage::getMidiNoteName (60 + pad - padsPerBank, true, true, 3);
        return "screen / sequencer only";
    }
    juce::String statusText() const
    {
        return "DRUM GRID: click to add a hit, drag to move, drag the right edge to lengthen, right-click to delete. "
               "Drag the velocity lane to shape the groove. Drag samples from the browser onto a pad.";
    }

    void resized() override
    {
        auto r = getLocalBounds();
        auto top = r.removeFromTop (56).reduced (8, 10);
        top.removeFromLeft (190);
        for (auto* b : { &toStartB, &playB, &stopB, &recB }) { b->setBounds (top.removeFromLeft (52)); top.removeFromLeft (4); }
        top.removeFromLeft (10);
        metroB.setBounds (top.removeFromLeft (64)); top.removeFromLeft (6);
        tcBox.setBounds (top.removeFromLeft (86).reduced (0, 4)); top.removeFromLeft (10);
        counterArea = top.removeFromLeft (370); top.removeFromLeft (10);
        for (auto* b : { &repeatB, &fullB, &sixteenB, &eraseB }) { b->setBounds (top.removeFromLeft (92)); top.removeFromLeft (4); }
        top.removeFromLeft (6);
        dragB.setBounds (top.removeFromLeft (92));
        meterArea = top.removeFromRight (120).withSizeKeepingCentre (120, 10).translated (0, 6);

        r.removeFromBottom (20);
        browser.setBounds (r.removeFromRight (330).reduced (0, 0));
        auto left = r.removeFromLeft (230);
        seqPanel = left.removeFromTop (190).reduced (6);
        padPanel = left.reduced (6);
        {
            auto s = seqPanel.reduced (10).withTrimmedTop (22);
            seqBox.setBounds (s.removeFromTop (26)); s.removeFromTop (8);
            auto row = s.removeFromTop (26); barsMinus.setBounds (row.withX (row.getX() + 44).withWidth (30)); barsPlus.setBounds (row.withX (row.getX() + 110).withWidth (30));
            doubleB.setBounds (row.withX (row.getX() + 148).withWidth (row.getRight() - row.getX() - 148));
            s.removeFromTop (28);
            auto row2 = s.removeFromTop (26);
            const int w = (row2.getWidth() - 8) / 3;
            renameB.setBounds (row2.removeFromLeft (w)); row2.removeFromLeft (4); copySeqB.setBounds (row2.removeFromLeft (w)); row2.removeFromLeft (4); clearSeqB.setBounds (row2);
        }
        {
            auto s = padPanel.reduced (10).withTrimmedTop (24 + 34 + 8);
            for (auto* b : { &srcBox, &chordBox, &chokeBox, &modeBox }) { b->setBounds (s.removeFromTop (26)); s.removeFromTop (6); }
            auto row = s.removeFromTop (26);
            loadSampleB.setBounds (row.removeFromLeft (row.getWidth() * 2 / 3 - 2)); row.removeFromLeft (4); clearSampleB.setBounds (row);
        }
        auto gridArea = r.removeFromTop (r.getHeight() - 384).reduced (4, 6);
        grid.setBounds (gridArea);
        auto bottom = r.reduced (4, 4);
        qArea = bottom.removeFromLeft (440);
        bottom.removeFromLeft (6);
        padArea = bottom;
        pads.setBounds (padArea.reduced (8));
        auto q = qArea.reduced (8).withTrimmedTop (22);
        const int kw = q.getWidth() / 4, kh = q.getHeight() / 4;
        for (int i = 0; i < 16; ++i) qk[(size_t) i].setBounds (q.getX() + (i % 4) * kw, q.getY() + (i / 4) * kh, kw, kh);
    }

    // ------------------------------------------------------------ Q-Links: pad row x2, groove, sequence
    void configureQLinks()
    {
        auto padGet = [this] (float PadSound::* m) { return [this, m] { return proc.getPad (selectedPad).*m; }; };
        auto padSet = [this] (float PadSound::* m) { return [this, m] (float v) { auto s = proc.getPad (selectedPad); s.*m = v; proc.setPad (selectedPad, s); }; };
        auto param = [this] (const char* id) { return std::pair<std::function<float()>, std::function<void (float)>> (
            [this, id] { return proc.apvts.getRawParameterValue (id)->load(); },
            [this, id] (float v) { auto* q = proc.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }); };
        qk[0].configure ("PAD LEVEL", 0, 1, 0.75f, 0, padGet (&PadSound::level), padSet (&PadSound::level), pct);
        qk[1].configure ("PAD TUNE", -24, 24, 0, 1, padGet (&PadSound::tune), padSet (&PadSound::tune), [] (float v) { return (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v)) + " st"; });
        qk[2].configure ("DECAY", 0, 1, 0.5f, 0, padGet (&PadSound::decay), padSet (&PadSound::decay), pct);
        qk[3].configure ("TONE", 0, 1, 0.5f, 0, padGet (&PadSound::tone), padSet (&PadSound::tone), pct);
        qk[4].configure ("PAN", -1, 1, 0, 0, padGet (&PadSound::pan), padSet (&PadSound::pan), [] (float v) { return std::abs (v) < 0.01f ? juce::String ("C") : (v < 0 ? "L" : "R") + juce::String (juce::roundToInt (std::abs (v) * 100)); });
        qk[5].configure ("CHORD", 0, numChords - 1, 0, 1, [this] { return (float) proc.getPad (selectedPad).chord; },
                         [this] (float v) { auto s = proc.getPad (selectedPad); s.chord = juce::roundToInt (v); proc.setPad (selectedPad, s); refreshInspector(); },
                         [] (float v) { return juce::String (chordName (juce::roundToInt (v))); });
        qk[6].configure ("CHOKE", 0, 8, 0, 1, [this] { return (float) proc.getPad (selectedPad).choke; },
                         [this] (float v) { auto s = proc.getPad (selectedPad); s.choke = juce::roundToInt (v); proc.setPad (selectedPad, s); refreshInspector(); },
                         [] (float v) { return juce::roundToInt (v) == 0 ? juce::String ("Off") : "Group " + juce::String (juce::roundToInt (v)); });
        qk[7].configure ("SOUND", 0, numSources - 1, 0, 1, [this] { return (float) proc.getPad (selectedPad).source; },
                         [this] (float v) { auto s = proc.getPad (selectedPad); s.source = juce::roundToInt (v); if (isMelodic (s.source)) s.oneShot = false; proc.setPad (selectedPad, s); refreshInspector(); pads.repaint(); grid.repaint(); },
                         [] (float v) { return juce::String (sourceName (juce::roundToInt (v))); });
        auto [sg, ss] = param ("swing");     qk[8].configure ("SWING", 50, 75, 54, 1, sg, ss, [] (float v) { return juce::String (juce::roundToInt (v)) + "%"; });
        auto [ag, as] = param ("accent");    qk[9].configure ("ACCENT", 0, 12, 5, 0, ag, as, [] (float v) { return "+" + juce::String (v, 1) + " dB"; });
        auto [hg, hs] = param ("humanize");  qk[10].configure ("HUMANIZE", 0, 100, 10, 1, hg, hs, [] (float v) { return juce::String (juce::roundToInt (v)) + "%"; });
        auto [mg, ms] = param ("master");    qk[11].configure ("MASTER", -24, 6, -3, 0, mg, ms, [] (float v) { return juce::String (v, 1) + " dB"; });
        auto [tg, ts] = param ("tc");        qk[12].configure ("TIME DIVISION", 0, 6, 4, 1, tg, ts, [] (float v) { static const char* n[] { "Off", "1/4", "1/8", "1/8T", "1/16", "1/16T", "1/32" }; return juce::String (n[juce::jlimit (0, 6, juce::roundToInt (v))]); });
        auto [pg, pss] = param ("tempo");    qk[13].configure ("TEMPO (APP)", 60, 200, 124, 0.5f, pg, pss, [] (float v) { return juce::String (v, 1) + " BPM"; });
        qk[14].configure ("SEQ LENGTH", 1, maxBars, 1, 1, [this] { return (float) proc.getSequence (proc.currentSequence())->bars; },
                          [this] (float v) { editSequence ([v] (SequenceData& s) { s.bars = juce::roundToInt (v); }); }, [] (float v) { return juce::String (juce::roundToInt (v)) + (juce::roundToInt (v) == 1 ? " bar" : " bars"); });
        auto [kg, ks] = param ("kit");       qk[15].configure ("DRUM MACHINE", 0, 2, 0, 1, kg, ks, [] (float v) { static const char* n[] { "909", "808", "Deep" }; return juce::String (n[juce::jlimit (0, 2, juce::roundToInt (v))]); });
    }

    void refreshSequencePanel()
    {
        for (int i = 0; i < numSequences; ++i) seqBox.changeItemText (i + 1, juce::String (i + 1) + "  " + proc.getSequence (i)->name);
        repaint (seqPanel);
    }
    void refreshInspector()
    {
        const auto s = proc.getPad (selectedPad);
        srcBox.setSelectedId (s.source + 1, juce::dontSendNotification);
        chordBox.setSelectedId (s.chord + 1, juce::dontSendNotification);
        chordBox.setEnabled (s.source == srcOrgan || s.source == srcPiano || s.source == srcStrings);
        chokeBox.setSelectedId (s.choke + 1, juce::dontSendNotification);
        modeBox.setSelectedId (s.oneShot ? 1 : 2, juce::dontSendNotification);
        repaint (padPanel);
    }
    void applyInspector()
    {
        auto s = proc.getPad (selectedPad);
        s.source = srcBox.getSelectedId() - 1; s.chord = chordBox.getSelectedId() - 1; s.choke = chokeBox.getSelectedId() - 1; s.oneShot = modeBox.getSelectedId() == 1;
        proc.setPad (selectedPad, s);
        refreshInspector(); pads.repaint(); grid.repaint();
    }
    void assignSample (int pad, const juce::String& path)
    {
        auto s = proc.getPad (pad);
        s.source = srcSample; s.samplePath = path; s.name = {}; s.decay = 1.0f; s.tone = 1.0f; s.tune = 0;
        const int g = guessSource (juce::File (path.fromLastOccurrenceOf ("|", false, false)).getFileName());
        s.choke = (g == srcClosedHat || g == srcOpenHat) ? 1 : 0;
        proc.setPad (pad, s);
        selectPad (pad, true);
    }
    void chooseSampleForPad()
    {
        chooser = std::make_unique<juce::FileChooser> ("Load a sample onto " + padLabel (selectedPad), proc.samplesFolder, "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
        { if (fc.getResult().existsAsFile()) assignSample (selectedPad, fc.getResult().getFullPathName()); });
    }
    void renameSequence()
    {
        auto* aw = new juce::AlertWindow ("Rename sequence", {}, juce::MessageBoxIconType::NoIcon);
        aw->addTextEditor ("name", proc.getSequence (proc.currentSequence())->name);
        aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create ([this, aw] (int r)
        { if (r == 1) { const auto n = aw->getTextEditorContents ("name"); editSequence ([n] (SequenceData& s) { s.name = n; }); } }), true);
    }
    void copySequenceMenu()
    {
        juce::PopupMenu m; m.addSectionHeader ("Copy this sequence to...");
        for (int i = 0; i < numSequences; ++i) if (i != proc.currentSequence()) m.addItem (i + 1, juce::String (i + 1) + "  " + proc.getSequence (i)->name);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&copySeqB), [this] (int r)
        { if (r > 0) { proc.setSequence (r - 1, *proc.getSequence (proc.currentSequence())); refreshSequencePanel(); } });
    }
    void eraseMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Erase " + padLabel (selectedPad) + " (" + proc.getPad (selectedPad).displayName() + ") from this sequence");
        m.addItem (2, "Erase bank " + juce::String::charToString ((juce::juce_wchar) ('A' + bank)) + " from this sequence");
        m.addItem (3, "Erase the whole sequence");
        m.addSeparator(); m.addItem (0, "Tip: hold ERASE + a pad while recording to erase as it plays", false);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&eraseB), [this] (int r)
        {
            const int sp = selectedPad, b = bank;
            if (r == 1) editSequence ([sp] (SequenceData& s) { s.events.erase (std::remove_if (s.events.begin(), s.events.end(), [sp] (auto& e) { return e.pad == sp; }), s.events.end()); });
            if (r == 2) editSequence ([b] (SequenceData& s) { s.events.erase (std::remove_if (s.events.begin(), s.events.end(), [b] (auto& e) { return e.pad / padsPerBank == b; }), s.events.end()); });
            if (r == 3) editSequence ([] (SequenceData& s) { s.events.clear(); });
        });
    }

    // =========================================================================================  the drum grid
    class Grid : public juce::Component, public juce::DragAndDropTarget
    {
    public:
        explicit Grid (Main& m) : main (m) {}
        static constexpr int headerW = 190, rulerH = 24, laneH = 72;
        juce::Rectangle<int> notesArea() const { return getLocalBounds().withTrimmedLeft (headerW).withTrimmedTop (rulerH).withTrimmedBottom (laneH + 4); }
        juce::Rectangle<int> laneArea() const { return getLocalBounds().withTrimmedLeft (headerW).removeFromBottom (laneH); }
        float rowH() const { return notesArea().getHeight() / (float) padsPerBank; }
        int padAtY (float y) const { const int row = (int) ((y - notesArea().getY()) / rowH()); return main.bank * padsPerBank + (padsPerBank - 1 - juce::jlimit (0, padsPerBank - 1, row)); }
        float yOfPad (int pad) const { return notesArea().getY() + (padsPerBank - 1 - pad % padsPerBank) * rowH(); }
        float xOfTick (double t, int len) const { auto a = notesArea(); return a.getX() + (float) (t / len) * a.getWidth(); }
        int tickAtX (float x, int len) const { auto a = notesArea(); return juce::jlimit (0, len - 1, (int) ((x - a.getX()) / a.getWidth() * len)); }

        void paint (juce::Graphics& g) override
        {
            const auto seq = main.proc.getSequence (main.proc.currentSequence());
            const int len = seq->lengthTicks();
            auto na = notesArea();
            g.setColour (juce::Colour (0xff1a1b1e)); g.fillRect (getLocalBounds());
            // ruler
            auto ruler = getLocalBounds().withTrimmedLeft (headerW).removeFromTop (rulerH);
            g.setColour (bar); g.fillRect (ruler);
            for (int b = 0; b < seq->bars * 4; ++b)
            {
                const float x = xOfTick (b * ppqTicks, len);
                g.setColour (b % 4 == 0 ? text : textDim); g.setFont (VegaFonts::ui (b % 4 == 0 ? 12.0f : 10.5f, b % 4 == 0));
                g.drawText (b % 4 == 0 ? juce::String (b / 4 + 1) : juce::String (b / 4 + 1) + "." + juce::String (b % 4 + 1), (int) x + 4, ruler.getY(), 60, rulerH, juce::Justification::centredLeft);
                g.setColour (line); g.drawVerticalLine ((int) x, (float) ruler.getY() + (b % 4 == 0 ? 2 : 12), (float) ruler.getBottom());
            }
            // rows
            const int sel = main.selectedPad;
            for (int r = 0; r < padsPerBank; ++r)
            {
                const int pad = main.bank * padsPerBank + (padsPerBank - 1 - r);
                const float y = na.getY() + r * rowH();
                const auto ps = main.proc.getPad (pad);
                if (r % 2 == 0) { g.setColour (juce::Colour (0xff202125)); g.fillRect ((float) na.getX(), y, (float) na.getWidth(), rowH()); }
                if (pad == sel) { g.setColour (accent.withAlpha (0.07f)); g.fillRect ((float) na.getX(), y, (float) na.getWidth(), rowH()); }
                // header: colour tab, pad id, name, mute
                auto h = juce::Rectangle<float> (0, y, (float) headerW - 4, rowH() - 1);
                g.setColour (pad == sel ? juce::Colour (0xff2f3034) : juce::Colour (0xff26272b)); g.fillRect (h);
                g.setColour (padColourFor (ps)); g.fillRect (h.removeFromLeft (32));
                g.setColour (juce::Colour (0xff101214)); g.setFont (VegaFonts::ui (10.5f, true));
                g.drawText (padLabel (pad), juce::Rectangle<float> (0, y, 32, rowH() - 1), juce::Justification::centred);
                g.setColour (ps.mute ? textDim : text); g.setFont (VegaFonts::ui (11.5f, pad == sel));
                auto mb = h.removeFromRight (22).reduced (3);
                g.drawFittedText (ps.displayName(), h.reduced (6, 0).toNearestInt(), juce::Justification::centredLeft, 1, 0.75f);
                g.setColour (ps.mute ? juce::Colour (0xffe0a03c) : juce::Colour (0xff3a3b40)); g.fillRoundedRectangle (mb, 2.0f);
                g.setColour (ps.mute ? juce::Colour (0xff101214) : textDim); g.setFont (VegaFonts::ui (10.0f, true)); g.drawText ("M", mb, juce::Justification::centred);
            }
            // grid lines: division, beats, bars
            const int d = main.divisionTicks();
            for (int t = 0; t < len; t += std::max (d, sixteenth / 4))
            {
                const float x = xOfTick (t, len);
                g.setColour (t % (4 * ppqTicks) == 0 ? juce::Colour (0xff6a6b70) : t % ppqTicks == 0 ? juce::Colour (0xff434449) : juce::Colour (0xff2c2d31));
                g.drawVerticalLine ((int) x, (float) na.getY(), (float) na.getBottom());
            }
            g.setColour (line); g.drawHorizontalLine (na.getBottom(), (float) na.getX(), (float) na.getRight());
            // notes
            for (auto& e : seq->events)
            {
                if (e.pad / padsPerBank != main.bank) continue;
                const auto c = padColourFor (main.proc.getPad (e.pad));
                auto nr = juce::Rectangle<float> (xOfTick (e.start, len) + 1, yOfPad (e.pad) + 3, std::max (4.0f, xOfTick (e.start + e.length, len) - xOfTick (e.start, len) - 1), rowH() - 6);
                g.setColour (c.withAlpha (0.35f + 0.65f * (float) e.velocity / 127.0f)); g.fillRect (nr);
                g.setColour (c.brighter (0.5f)); g.drawRect (nr, 1.0f);
                if (&e == hoverEvent) { g.setColour (juce::Colours::white.withAlpha (0.25f)); g.fillRect (nr); }
            }
            // velocity lane
            auto lane = laneArea();
            g.setColour (juce::Colour (0xff141517)); g.fillRect (lane);
            g.setColour (textDim); g.setFont (VegaFonts::ui (10.5f, true));
            g.drawText ("VELOCITY  " + padLabel (sel), 6, lane.getY() + 4, headerW - 12, 14, juce::Justification::centredLeft);
            g.setFont (VegaFonts::ui (10.0f)); g.drawText ("drag here to shape the hits", 6, lane.getY() + 20, headerW - 12, 14, juce::Justification::centredLeft);
            for (auto& e : seq->events)
            {
                if (e.pad != sel) continue;
                const float x = xOfTick (e.start, len) + 2, h = (lane.getHeight() - 8) * (float) e.velocity / 127.0f;
                g.setColour (padColourFor (main.proc.getPad (e.pad))); g.fillRect (x, lane.getBottom() - 4 - h, 5.0f, h);
                g.fillEllipse (x - 1.5f, lane.getBottom() - 4 - h - 3, 8, 8);
            }
            // playhead
            if (const int tick = main.proc.uiTick.load(); tick >= 0)
            {
                const float x = xOfTick (tick % len, len);
                g.setColour (accent); g.drawVerticalLine ((int) x, (float) rulerH, (float) getHeight());
                juce::Path tri; tri.addTriangle (x - 5, 0, x + 5, 0, x, 8); g.fillPath (tri);
            }
            if (dropHighlight >= 0) { g.setColour (accent.withAlpha (0.3f)); g.fillRect (0.0f, yOfPad (dropHighlight), (float) getWidth(), rowH()); }
        }

        const NoteEvent* hoverEvent = nullptr;
        int hitTest (const SequenceData& s, juce::Point<float> p, bool& onEdge) const
        {
            const int len = s.lengthTicks();
            for (int i = (int) s.events.size() - 1; i >= 0; --i)
            {
                const auto& e = s.events[(size_t) i];
                if (e.pad / padsPerBank != main.bank) continue;
                auto nr = juce::Rectangle<float> (xOfTick (e.start, len), yOfPad (e.pad), std::max (6.0f, xOfTick (e.start + e.length, len) - xOfTick (e.start, len)), rowH());
                if (nr.contains (p)) { onEdge = p.x > nr.getRight() - 6; return i; }
            }
            return -1;
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            auto s = *main.proc.getSequence (main.proc.currentSequence());
            const int len = s.lengthTicks();
            const auto p = e.position;
            if (laneArea().contains (p.toInt())) { laneDrag (p); return; }
            if (p.x < headerW && p.y > rulerH && p.y < notesArea().getBottom())
            {
                const int pad = padAtY (p.y);
                if (p.x > headerW - 26)   // mute
                { auto ps = main.proc.getPad (pad); ps.mute = ! ps.mute; main.proc.setPad (pad, ps); repaint(); return; }
                main.selectPad (pad, true); return;
            }
            if (! notesArea().contains (p.toInt())) return;
            bool edge = false;
            const int hit = hitTest (s, p, edge);
            if (e.mods.isPopupMenu())
            {
                if (hit >= 0) { s.events.erase (s.events.begin() + hit); main.proc.setSequence (main.proc.currentSequence(), s); repaint(); }
                return;
            }
            const int d = main.divisionTicks();
            if (hit >= 0)
            {
                drag = { true, edge, hit, s.events[(size_t) hit], tickAtX (p.x, len), padAtY (p.y) };
                main.selectPad (s.events[(size_t) hit].pad);
                return;
            }
            // new hit on the grid
            const int pad = padAtY (p.y);
            const int t = (tickAtX (p.x, len) / d) * d;
            const bool mel = isMelodic (main.proc.getPad (pad).source);
            NoteEvent ne { pad, t, mel ? std::max (d, sixteenth) * 2 : std::max (d - 20, 30), e.mods.isShiftDown() ? 127 : 100 };
            s.events.push_back (ne);
            main.proc.setSequence (main.proc.currentSequence(), s);
            // find it again (the list is re-sorted) and keep dragging its length for held sounds
            auto s2 = *main.proc.getSequence (main.proc.currentSequence());
            for (int i = 0; i < (int) s2.events.size(); ++i) if (s2.events[(size_t) i].pad == pad && s2.events[(size_t) i].start == t) drag = { true, mel, i, s2.events[(size_t) i], tickAtX (p.x, len), pad };
            main.selectPad (pad, true);
        }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (laneArea().contains (e.getMouseDownPosition())) { laneDrag (e.position); return; }
            if (! drag.active) return;
            auto s = *main.proc.getSequence (main.proc.currentSequence());
            if (drag.index >= (int) s.events.size()) return;
            const int len = s.lengthTicks(), d = main.divisionTicks();
            auto& ev = s.events[(size_t) drag.index];
            const int dt = tickAtX (e.position.x, len) - drag.startTick;
            if (drag.resize) ev.length = juce::jlimit (30, len, (int) std::round ((drag.original.length + dt) / (double) d) * d - (isMelodic (main.proc.getPad (ev.pad).source) ? 0 : 20));
            else
            {
                ev.start = juce::jlimit (0, len - 1, (int) std::round ((drag.original.start + dt) / (double) d) * d);
                const int pad = padAtY (e.position.y);
                ev.pad = pad;
            }
            // store without re-sorting so the index stays valid while dragging
            auto ptr = std::make_shared<SequenceData> (s);
            main.proc.setSequence (main.proc.currentSequence(), *ptr);
            auto s2 = *main.proc.getSequence (main.proc.currentSequence());
            for (int i = 0; i < (int) s2.events.size(); ++i) if (s2.events[(size_t) i].pad == ev.pad && s2.events[(size_t) i].start == ev.start) drag.index = i;
            repaint();
        }
        void mouseUp (const juce::MouseEvent&) override { drag.active = false; }
        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            auto s = *main.proc.getSequence (main.proc.currentSequence());
            bool edge; const int hit = hitTest (s, e.position, edge);
            if (hit >= 0) { s.events.erase (s.events.begin() + hit); main.proc.setSequence (main.proc.currentSequence(), s); repaint(); }
        }
        void mouseMove (const juce::MouseEvent& e) override
        {
            const auto seq = main.proc.getSequence (main.proc.currentSequence());
            bool edge = false; const int hit = hitTest (*seq, e.position, edge);
            const NoteEvent* h = hit >= 0 ? &seq->events[(size_t) hit] : nullptr;
            setMouseCursor (hit >= 0 && edge ? juce::MouseCursor::LeftRightResizeCursor : hit >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
            if (h != hoverEvent) { hoverEvent = h; held = seq; repaint(); }
        }
        void laneDrag (juce::Point<float> p)
        {
            auto s = *main.proc.getSequence (main.proc.currentSequence());
            const int len = s.lengthTicks();
            auto lane = laneArea();
            const int vel = juce::jlimit (1, 127, (int) ((lane.getBottom() - 4 - p.y) / (lane.getHeight() - 8) * 127.0f));
            bool changed = false;
            for (auto& e : s.events)
                if (e.pad == main.selectedPad && std::abs (xOfTick (e.start, len) + 4 - p.x) < 8) { e.velocity = vel; changed = true; }
            if (changed) { main.proc.setSequence (main.proc.currentSequence(), s); repaint(); }
        }
        // samples dragged from the browser onto a row
        bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("sample:"); }
        void itemDragMove (const SourceDetails& d) override { dropHighlight = d.localPosition.x < headerW ? padAtY ((float) d.localPosition.y) : -1; repaint(); }
        void itemDragExit (const SourceDetails&) override { dropHighlight = -1; repaint(); }
        void itemDropped (const SourceDetails& d) override
        {
            dropHighlight = -1;
            if (d.localPosition.y > rulerH && d.localPosition.y < notesArea().getBottom()) main.assignSample (padAtY ((float) d.localPosition.y), d.description.toString().fromFirstOccurrenceOf ("sample:", false, false));
            repaint();
        }
    private:
        Main& main;
        struct Drag { bool active = false, resize = false; int index = -1; NoteEvent original; int startTick = 0, pad = 0; } drag;
        SequencePtr held;      // keeps hoverEvent's sequence alive
        int dropHighlight = -1;
    };

    // =========================================================================================  pads 4x4 + banks
    class Pads : public juce::Component, public juce::DragAndDropTarget
    {
    public:
        explicit Pads (Main& m) : main (m)
        {
            for (int b = 0; b < numBanks; ++b)
            {
                auto* btn = bankButtons.add (new FlatButton (juce::String::charToString ((juce::juce_wchar) ('A' + b))));
                btn->onClick = [this, b] { main.bank = b; main.selectPad (b * padsPerBank + main.selectedPad % padsPerBank); };
                addAndMakeVisible (btn);
            }
        }
        juce::Rectangle<int> padRect (int i) const
        {
            auto a = getLocalBounds().withTrimmedTop (34).withTrimmedRight (6);
            const int w = a.getWidth() / 4, h = a.getHeight() / 4;
            const int col = i % 4, row = 3 - i / 4;               // A01 bottom-left, like the hardware
            return { a.getX() + col * w, a.getY() + row * h, w, h };
        }
        int padAt (juce::Point<int> p) const { for (int i = 0; i < 16; ++i) if (padRect (i).contains (p)) return i; return -1; }
        void resized() override
        {
            auto r = getLocalBounds().removeFromTop (28);
            const int w = r.getWidth() / numBanks;
            for (int b = 0; b < numBanks; ++b) bankButtons[b]->setBounds (r.getX() + b * w, r.getY(), w - 4, 26);
        }
        void paint (juce::Graphics& g) override
        {
            const auto seq = main.proc.getSequence (main.proc.currentSequence());
            for (int b = 0; b < numBanks; ++b)
            {
                bankButtons[b]->setToggleState (b == main.bank, juce::dontSendNotification);
                bool used = false; for (auto& e : seq->events) if (e.pad / padsPerBank == b) { used = true; break; }
                const auto t = juce::String::charToString ((juce::juce_wchar) ('A' + b)) + (used ? juce::String (juce::CharPointer_UTF8 (" \xe2\x80\xa2")) : juce::String());
                if (bankButtons[b]->getButtonText() != t) bankButtons[b]->setButtonText (t);
            }
            const bool sixteen = main.sixteenB.getToggleState();
            for (int i = 0; i < 16; ++i)
            {
                const int pad = main.bank * padsPerBank + i;
                const auto ps = main.proc.getPad (sixteen ? main.selectedPad : pad);
                auto r = padRect (i).reduced (5).toFloat();
                const float flash = main.proc.padFlash[(size_t) (sixteen ? main.selectedPad : pad)].load();
                const auto col = padColourFor (ps);
                const bool empty = ps.source == srcSample && ps.samplePath.isEmpty();
                g.setColour (juce::Colour (0xff16171a)); g.fillRoundedRectangle (r, 5.0f);
                g.setColour (empty ? juce::Colour (0xff3a3b40) : col.withAlpha (0.25f + 0.75f * std::min (1.0f, flash * 1.3f)));
                g.fillRoundedRectangle (r.reduced (3), 4.0f);
                g.setColour (empty ? juce::Colour (0xff4a4b50) : col); g.drawRoundedRectangle (r.reduced (1.5f), 5.0f, pad == main.selectedPad && ! sixteen ? 3.0f : 1.5f);
                if (pad == main.selectedPad && ! sixteen) { g.setColour (juce::Colours::white.withAlpha (0.7f)); g.drawRoundedRectangle (r.reduced (1.5f), 5.0f, 1.0f); }
                g.setColour (text); g.setFont (VegaFonts::ui (10.5f, true));
                g.drawText (sixteen ? "VEL " + juce::String ((i + 1) * 8 - 1) : padLabel (pad), r.reduced (8, 6).toNearestInt(), juce::Justification::topLeft);
                g.setFont (VegaFonts::ui (12.0f, true)); g.setColour (empty ? textDim : juce::Colours::white);
                g.drawFittedText (empty ? juce::String ("drop a sample") : ps.displayName(), r.reduced (8, 8).toNearestInt(), juce::Justification::bottomLeft, 2, 0.75f);
            }
            if (dropPad >= 0) { g.setColour (accent.withAlpha (0.35f)); g.fillRoundedRectangle (padRect (dropPad).reduced (5).toFloat(), 5.0f); }
        }
        void mouseDown (const juce::MouseEvent& e) override
        {
            const int i = padAt (e.getPosition()); if (i < 0) return;
            const bool sixteen = main.sixteenB.getToggleState();
            const int pad = sixteen ? main.selectedPad : main.bank * padsPerBank + i;
            // velocity from how high on the pad you click (top = harder), or the 16 Level step
            const float vel = sixteen ? (float) (i + 1) / 16.0f : juce::jlimit (0.35f, 1.0f, 1.0f - (float) (e.y - padRect (i).getY()) / padRect (i).getHeight() * 0.65f);
            down = pad;
            if (e.mods.isPopupMenu()) { padMenu (pad); down = -1; return; }
            if (! sixteen) main.selectPad (pad);
            main.proc.padDown (pad, vel);
        }
        void mouseUp (const juce::MouseEvent&) override { if (down >= 0) main.proc.padUp (down); down = -1; }
        void padMenu (int pad)
        {
            juce::PopupMenu m;
            m.addSectionHeader (padLabel (pad) + "  " + main.proc.getPad (pad).displayName());
            m.addItem (1, "Load sample..."); m.addItem (2, "Clear pad");
            juce::PopupMenu src; for (int s = 0; s < numSources - 1; ++s) src.addItem (100 + s, sourceName (s));
            m.addSubMenu ("Built-in sound", src);
            m.addItem (3, "Copy this pad to the selected pad (" + padLabel (main.selectedPad) + ")", pad != main.selectedPad);
            m.showMenuAsync (juce::PopupMenu::Options(), [this, pad] (int r)
            {
                if (r == 1) { main.selectPad (pad); main.chooseSampleForPad(); }
                if (r == 2) { PadSound s; s.source = srcSample; main.proc.setPad (pad, s); main.refreshAll(); }
                if (r == 3) { main.proc.setPad (main.selectedPad, main.proc.getPad (pad)); main.refreshAll(); }
                if (r >= 100) { auto s = main.proc.getPad (pad); s.source = r - 100; s.samplePath = {}; s.name = {}; s.oneShot = ! isMelodic (s.source); main.proc.setPad (pad, s); main.selectPad (pad, true); }
            });
        }
        bool isInterestedInDragSource (const SourceDetails& d) override { return d.description.toString().startsWith ("sample:"); }
        void itemDragMove (const SourceDetails& d) override { dropPad = padAt (d.localPosition); repaint(); }
        void itemDragExit (const SourceDetails&) override { dropPad = -1; repaint(); }
        void itemDropped (const SourceDetails& d) override
        {
            const int i = padAt (d.localPosition); dropPad = -1;
            if (i >= 0) main.assignSample (main.bank * padsPerBank + i, d.description.toString().fromFirstOccurrenceOf ("sample:", false, false));
            repaint();
        }
    private:
        Main& main; juce::OwnedArray<FlatButton> bankButtons; int down = -1, dropPad = -1;
    };

    // =========================================================================================  browser: kits + samples
    class Browser : public juce::Component, public juce::ListBoxModel, private juce::Thread
    {
    public:
        explicit Browser (Main& m) : juce::Thread ("Vega Drums sample scan"), main (m)
        {
            for (auto* b : { &kitsTab, &samplesTab }) { addAndMakeVisible (b); b->setRadioGroupId (77); b->setClickingTogglesState (true); b->onClick = [this] { rebuild(); }; }
            kitsTab.setToggleState (true, juce::dontSendNotification);
            const char* tags[] { "80s", "90s", "House", "Deep House", "Acid", "Techno", "Garage", "Loops" };
            for (auto t : tags) { auto* b = chips.add (new FlatButton (t, true)); b->onClick = [this] { rebuild(); }; addAndMakeVisible (b); }
            search.setTextToShowWhenEmpty ("Search", textDim);
            search.onTextChange = [this] { rebuild(); };
            addAndMakeVisible (search);
            list.setModel (this); list.setRowHeight (32); list.setColour (juce::ListBox::backgroundColourId, juce::Colour (0xff1c1d20));
            addAndMakeVisible (list);
            for (auto* b : { &addFolderB, &rescanB, &openFolderB }) addAndMakeVisible (b);
            addFolderB.onClick = [this] { addFolder(); };
            rescanB.onClick = [this] { startScan(); };
            openFolderB.onClick = [this] { auto f = main.proc.samplesFolder; f.createDirectory(); f.startAsProcess(); };
            startScan();
        }
        ~Browser() override { stopThread (4000); }

        juce::StringArray folders() const
        {
            auto s = juce::StringArray::fromTokens (settings().getValue ("folders"), "\n", "");
            s.removeEmptyStrings();
            if (s.isEmpty())
            {
                s.add (main.proc.samplesFolder.getFullPathName());
                s.add ("shallow:" + juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Downloads").getFullPathName());
            }
            return s;
        }
        void addFolder()
        {
            chooser = std::make_unique<juce::FileChooser> ("Add a folder with samples (zips inside are read too)", main.proc.samplesFolder);
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
            {
                if (! fc.getResult().isDirectory()) return;
                auto s = folders(); s.addIfNotAlreadyThere (fc.getResult().getFullPathName());
                settings().setValue ("folders", s.joinIntoString ("\n")); settings().saveIfNeeded();
                startScan();
            });
        }
        void startScan()
        {
            stopThread (2000);
            scanning = true; repaint();
            startThread();
        }
        void run() override
        {
            juce::StringArray found, packs;
            for (auto entry : folders())
            {
                const bool shallow = entry.startsWith ("shallow:");
                const juce::File dir (entry.fromFirstOccurrenceOf ("shallow:", false, false).isEmpty() ? entry : entry.fromFirstOccurrenceOf ("shallow:", false, false));
                if (! dir.isDirectory()) continue;
                if (shallow)
                {
                    // Downloads: only zips / folders that look like sample packs
                    for (const auto& e : juce::RangedDirectoryIterator (dir, false, "*", juce::File::findFilesAndDirectories))
                    {
                        if (threadShouldExit()) return;
                        const auto f = e.getFile(); const auto n = f.getFileName().toLowerCase();
                        const bool looksLikeSamples = n.contains ("sample") || n.contains ("musicradar") || n.contains ("drum") || n.contains ("808") || n.contains ("909") || n.contains ("house") || n.contains ("kit") || n.contains ("loops");
                        if (! looksLikeSamples) continue;
                        if (f.hasFileExtension ("zip") || f.isDirectory())
                        {
                            const auto got = f.isDirectory() ? scanSamples (f, 6000) : [&f] { juce::StringArray z; juce::ZipFile zip (f); for (int i = 0; i < zip.getNumEntries(); ++i) if (auto* ze = zip.getEntry (i)) if (isAudioFile (ze->filename)) z.add (f.getFullPathName() + "|" + ze->filename); return z; }();
                            if (! got.isEmpty()) { found.addArray (got); packs.add (f.getFullPathName()); }
                        }
                    }
                }
                else
                {
                    found.addArray (scanSamples (dir, 20000));
                    for (const auto& e : juce::RangedDirectoryIterator (dir, false, "*", juce::File::findFilesAndDirectories))
                        if (e.getFile().hasFileExtension ("zip") || e.getFile().isDirectory()) packs.add (e.getFile().getFullPathName());
                }
                if (threadShouldExit()) return;
            }
            juce::MessageManager::callAsync ([sp = juce::Component::SafePointer<Browser> (this), found, packs]
            { if (sp != nullptr) { sp->allSamples = found; sp->allPacks = packs; sp->scanning = false; sp->rebuild(); } });
        }

        void rebuild()
        {
            juce::StringArray on; for (auto* c : chips) if (c->getToggleState()) on.add (c->getButtonText());
            const auto q = search.getText().toLowerCase();
            rows.clear();
            auto matches = [&] (const juce::String& hay, const juce::StringArray& tags)
            {
                if (q.isNotEmpty() && ! hay.toLowerCase().contains (q)) return false;
                for (auto& t : on) if (! tags.contains (t)) return false;
                return true;
            };
            if (kitsTab.getToggleState())
            {
                for (int i = 0; i < (int) builtInKits().size(); ++i)
                {
                    const auto& k = builtInKits()[(size_t) i];
                    juce::StringArray tags = juce::StringArray::fromTokens (k.genres, " ", "");
                    if (juce::String (k.genres).contains ("Deep House")) tags.add ("Deep House");
                    if (matches (juce::String (k.name) + " " + k.genres, tags)) rows.push_back ({ 0, juce::String (k.name), k.genres, i });
                }
                for (auto& p : allPacks)
                {
                    const juce::File f (p);
                    if (matches (f.getFileName(), tagsFor (p))) rows.push_back ({ 1, f.getFileNameWithoutExtension(), "Auto-kit from your samples", 0, p });
                }
            }
            else
                for (auto& s : allSamples)
                {
                    const auto name = juce::File (s.fromLastOccurrenceOf ("|", false, false)).getFileName();
                    auto tags = tagsFor (s);
                    if (! matches (s, tags)) continue;
                    const int g = guessSource (name);
                    rows.push_back ({ 2, name, (g >= 0 ? juce::String (sourceName (g)).toLowerCase() : tags.contains ("Loops") ? juce::String ("loop") : juce::String ("sample")) + "  -  " + juce::File (s.upToFirstOccurrenceOf ("|", false, false)).getFileNameWithoutExtension(), g, s });
                    if (rows.size() > 6000) break;
                }
            list.updateContent(); list.repaint(); repaint();
        }

        int getNumRows() override { return (int) rows.size(); }
        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
        {
            if (row < 0 || row >= (int) rows.size()) return;
            const auto& r = rows[(size_t) row];
            if (selected) { g.setColour (accent.withAlpha (0.18f)); g.fillRect (0, 0, w, h); }
            g.setColour (r.type == 0 ? accent : r.type == 1 ? juce::Colour (0xffc9a63c) : padColour (r.extra >= 0 ? r.extra : srcSample));
            g.fillRect (6, 6, 10, h - 12);
            g.setColour (text); g.setFont (VegaFonts::ui (12.0f, r.type < 2));
            g.drawFittedText (r.title, 24, 2, w - 30, h / 2, juce::Justification::bottomLeft, 1, 0.8f);
            g.setColour (textDim); g.setFont (VegaFonts::ui (10.0f));
            g.drawFittedText (r.sub, 24, h / 2 + 2, w - 30, h / 2 - 4, juce::Justification::topLeft, 1, 0.8f);
        }
        void listBoxItemClicked (int row, const juce::MouseEvent& e) override
        {
            if (row < 0 || row >= (int) rows.size()) return;
            const auto& r = rows[(size_t) row];
            if (r.type == 2) main.proc.previewSample (r.path);
            if (e.mods.isPopupMenu() && r.type == 1)
            {
                juce::PopupMenu m;
                for (int b = 0; b < numBanks - 1; b += 2) m.addItem (b + 1, "Load into banks " + juce::String::charToString ((juce::juce_wchar) ('A' + b)) + " + " + juce::String::charToString ((juce::juce_wchar) ('B' + b)));
                m.showMenuAsync (juce::PopupMenu::Options(), [this, p = r.path] (int res) { if (res > 0) loadPack (p, res - 1); });
            }
            if (e.mods.isPopupMenu() && r.type == 2)
            {
                juce::PopupMenu m; m.addItem (1, "Load onto " + padLabel (main.selectedPad));
                m.showMenuAsync (juce::PopupMenu::Options(), [this, p = r.path] (int res) { if (res == 1) main.assignSample (main.selectedPad, p); });
            }
        }
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
        {
            if (row < 0 || row >= (int) rows.size()) return;
            const auto& r = rows[(size_t) row];
            if (r.type == 0) { main.proc.loadKit (r.extra); main.selectPad (0); }
            if (r.type == 1) loadPack (r.path, 0);
            if (r.type == 2) main.assignSample (main.selectedPad, r.path);
        }
        void loadPack (const juce::String& path, int bank)
        {
            const int n = main.proc.autoKitFromFolder (juce::File (path), bank);
            main.selectPad (bank * padsPerBank);
            if (n == 0) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon, "Auto-kit", "No samples in that pack could be matched to drums. Open the SAMPLES tab to drag them onto pads yourself.");
        }
        juce::var getDragSourceDescription (const juce::SparseSet<int>& sel) override
        {
            if (sel.size() == 1 && sel[0] < (int) rows.size() && rows[(size_t) sel[0]].type == 2) return "sample:" + rows[(size_t) sel[0]].path;
            return {};
        }
        void resized() override
        {
            auto r = getLocalBounds().reduced (6);
            r.removeFromTop (24);
            auto tabs = r.removeFromTop (28); kitsTab.setBounds (tabs.removeFromLeft (tabs.getWidth() / 2).reduced (2, 0)); samplesTab.setBounds (tabs.reduced (2, 0));
            r.removeFromTop (8);
            auto chipsArea = r.removeFromTop (54);
            int x = chipsArea.getX(), y = chipsArea.getY();
            for (auto* c : chips)
            {
                const int w = juce::roundToInt (juce::GlyphArrangement::getStringWidth (VegaFonts::ui (11.5f, true), c->getButtonText())) + 18;
                if (x + w > chipsArea.getRight()) { x = chipsArea.getX(); y += 26; }
                c->setBounds (x, y, w, 23); x += w + 4;
            }
            r.removeFromTop (6);
            search.setBounds (r.removeFromTop (26)); r.removeFromTop (6);
            auto bottom = r.removeFromBottom (28);
            addFolderB.setBounds (bottom.removeFromLeft (bottom.getWidth() / 3).reduced (2, 0)); rescanB.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2).reduced (2, 0)); openFolderB.setBounds (bottom.reduced (2, 0));
            r.removeFromBottom (30);
            list.setBounds (r);
        }
        void paint (juce::Graphics& g) override
        {
            panelBox (g, getLocalBounds().reduced (6), "BROWSER");
            g.setColour (textDim); g.setFont (VegaFonts::ui (10.0f));
            const auto info = scanning ? juce::String ("Scanning your sample folders...")
                                       : juce::String (allSamples.size()) + " samples in " + juce::String (allPacks.size()) + " packs.  Double-click to load, drag onto a pad.";
            g.drawFittedText (info, list.getBounds().getX(), list.getBottom() + 4, list.getWidth(), 24, juce::Justification::centredLeft, 2);
        }
    private:
        Main& main;
        FlatButton kitsTab { "KITS" }, samplesTab { "SAMPLES" };
        juce::OwnedArray<FlatButton> chips;
        juce::TextEditor search; juce::ListBox list;
        FlatButton addFolderB { "+ FOLDER" }, rescanB { "RESCAN" }, openFolderB { "OPEN KITS" };
        struct Row { int type; juce::String title, sub; int extra; juce::String path; };   // 0 built-in kit, 1 pack, 2 sample
        std::vector<Row> rows;
        juce::StringArray allSamples, allPacks; std::atomic<bool> scanning { false };
        std::unique_ptr<juce::FileChooser> chooser;
    };

    // ------------------------------------------------------------ members
    FlatButton toStartB { "|<" }, playB { "PLAY" }, stopB { "STOP" }, recB { "REC", true }, metroB { "METRO", true };
    FlatButton repeatB { "NOTE REPEAT", true }, fullB { "FULL LEVEL", true }, sixteenB { "16 LEVEL", true }, eraseB { "ERASE" }, dragB { "DRAG MIDI" };
    juce::ComboBox tcBox, seqBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> tcAtt, seqAtt;
    FlatButton barsMinus { "-" }, barsPlus { "+" }, doubleB { "DOUBLE" }, renameB { "RENAME" }, copySeqB { "COPY TO" }, clearSeqB { "CLEAR" };
    juce::ComboBox srcBox, chordBox, chokeBox, modeBox;
    FlatButton loadSampleB { "LOAD SAMPLE..." }, clearSampleB { "CLEAR" };
    std::array<QKnob, 16> qk;
    Grid grid; Pads pads; Browser browser;
    juce::Rectangle<int> counterArea, meterArea, seqPanel, padPanel, qArea, padArea;
    std::unique_ptr<juce::FileChooser> chooser;
    float meter = 0; int lastTick = -2, slow = 0; bool flashing = false; size_t seqCount = 0;
};

// =====================================================================================================  editor shell
VegaDrumsEditor::VegaDrumsEditor (VegaDrumsProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&lnf);
    main = std::make_unique<Main> (p);
    main->setSize (W, H);
    addAndMakeVisible (*main);
    constrainer.setFixedAspectRatio ((double) W / H);
    constrainer.setSizeLimits (W / 2, H / 2, W * 2, H * 2);
    setConstrainer (&constrainer);
    setResizable (true, true);
    const float s = juce::jlimit (0.5f, 2.0f, p.uiScale.load() * 0.8f);
    setSize (juce::roundToInt (W * s), juce::roundToInt (H * s));
    startTimerHz (30);
}
VegaDrumsEditor::~VegaDrumsEditor() { stopTimer(); main = nullptr; setLookAndFeel (nullptr); }
void VegaDrumsEditor::resized()
{
    const float s = (float) getWidth() / W;
    main->setTransform (juce::AffineTransform::scale (s));
    proc.uiScale.store (s / 0.8f);
}
void VegaDrumsEditor::timerCallback() { main->timerTick(); }
juce::AudioProcessorEditor* VegaDrumsProcessor::createEditor() { return new VegaDrumsEditor (*this); }
