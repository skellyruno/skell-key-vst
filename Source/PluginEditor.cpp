#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    constexpr int kWidth  = 720;   // same aspect as the artwork (2135 x 736)
    constexpr int kHeight = 248;

    const juce::Colour kLime (0xff39ff14);

    juce::String percent (double v) { return juce::String (juce::roundToInt (v * 100.0)); }

    juce::String hertz (double v)
    {
        return v >= 1000.0 ? juce::String (v / 1000.0, 1) + "k" : juce::String (juce::roundToInt (v));
    }

    juce::String milliseconds (double v)
    {
        if (v < 10.0)  return juce::String (v, 2) + " ms";
        if (v < 100.0) return juce::String (v, 1) + " ms";
        return juce::String (juce::roundToInt (v)) + " ms";
    }

    void styleLabel (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::FontOptions (10.5f).withStyle ("Bold Italic"));
        l.setColour (juce::Label::textColourId, kLime);
        l.setJustificationType (juce::Justification::centred);
    }
}

DelayAudioProcessorEditor::DelayAudioProcessorEditor (DelayAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    bgImage = juce::ImageFileFormat::loadFrom (BinaryData::skell_bg_png, BinaryData::skell_bg_pngSize);
    setLookAndFeel (&skellLookAndFeel);

    addAndMakeVisible (leftMeter);
    addAndMakeVisible (rightMeter);
    addAndMakeVisible (delayDisplay);

    // ---- knobs: (knob, parameter id, label, readout shown while hovering / dragging)
    setupKnob (feedback, "FEEDBACK", "FEEDBACK", percent);
    setupKnob (pan, "PAN", "PAN", [] (double v)
    {
        const int p = juce::roundToInt (std::abs (v) * 100.0);
        return p == 0 ? juce::String ("C") : (v < 0 ? "L" : "R") + juce::String (p);
    });
    setupKnob (hiCut,   "HICUT",   "TONE",   hertz);
    setupKnob (tape,    "TAPE",    "TAPE",   percent);
    setupKnob (ducking, "DUCKING", "DUCKER", percent);
    setupKnob (dry,     "DRY",     "DRY",    percent);
    setupKnob (wet,     "WET",     "WET",    percent);

    // TIME: its attachment (ms vs note value) is chosen by applyTimeMode()
    time.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    time.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (time.slider);
    styleLabel (time.label, "TIME");
    addAndMakeVisible (time.label);

    // ---- switches
    syncBtn.setClickingTogglesState (true);
    syncBtn.onClick = [this]() { setSync (syncBtn.getToggleState()); };
    addAndMakeVisible (syncBtn);

    pingBtn.setClickingTogglesState (true);
    pingBtn.onClick = [this]() { setChoice ("FB_MODE", pingBtn.getToggleState() ? 2 : 0); };
    addAndMakeVisible (pingBtn);

    // straight / dotted / triplet - only shown while tempo sync is on
    juce::TextButton* feel[3] = { &feelStraightBtn, &feelDottedBtn, &feelTripletBtn };
    for (int i = 0; i < 3; ++i)
    {
        auto* b = feel[i];
        b->setRadioGroupId (1001);
        b->setClickingTogglesState (true);
        int edges = 0;
        if (i > 0) edges |= juce::Button::ConnectedOnLeft;
        if (i < 2) edges |= juce::Button::ConnectedOnRight;
        b->setConnectedEdges (edges);
        b->onClick = [this, i]() { setChoice ("FEEL", i); };
        addChildComponent (b);
    }

    applyTimeMode (audioProcessor.apvts.getRawParameterValue ("SYNC")->load() > 0.5f);
    refreshButtons();

    startTimerHz (30);
    setSize (kWidth, kHeight);
}

DelayAudioProcessorEditor::~DelayAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void DelayAudioProcessorEditor::setupKnob (Knob& k, const juce::String& paramId, const juce::String& text,
                                           std::function<juce::String (double)> textFn)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (k.slider);

    styleLabel (k.label, text);
    addAndMakeVisible (k.label);

    k.attach = std::make_unique<SliderAttachment> (audioProcessor.apvts, paramId, k.slider);
    k.slider.textFromValueFunction = std::move (textFn);   // after the attachment, which installs its own
}

void DelayAudioProcessorEditor::setChoice (const juce::String& paramId, int index)
{
    if (auto* param = audioProcessor.apvts.getParameter (paramId))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (param->convertTo0to1 (static_cast<float> (index)));
        param->endChangeGesture();
    }
    refreshButtons();
}

// UI only - never writes a parameter, so host automation / preset recall stays in sync
void DelayAudioProcessorEditor::refreshButtons()
{
    const int feelIdx = static_cast<int> (audioProcessor.apvts.getRawParameterValue ("FEEL")->load() + 0.5f);
    feelStraightBtn.setToggleState (feelIdx == 0, juce::dontSendNotification);
    feelDottedBtn.setToggleState   (feelIdx == 1, juce::dontSendNotification);
    feelTripletBtn.setToggleState  (feelIdx == 2, juce::dontSendNotification);

    const int fbMode = static_cast<int> (audioProcessor.apvts.getRawParameterValue ("FB_MODE")->load() + 0.5f);
    pingBtn.setToggleState (fbMode == 2, juce::dontSendNotification);
}

void DelayAudioProcessorEditor::applyTimeMode (bool sync)
{
    // Re-create the attachment so the knob takes the other parameter's range and value
    time.attach.reset();
    time.attach = std::make_unique<SliderAttachment> (audioProcessor.apvts,
                                                      sync ? "SYNC_DIV" : "DELAY_TIME", time.slider);
    timeIsSync = sync;

    if (sync)
    {
        auto* feelParam = audioProcessor.apvts.getRawParameterValue ("FEEL");
        time.slider.textFromValueFunction = [feelParam] (double v)
        {
            const int idx  = juce::jlimit (0, kNumSyncDivisions - 1, juce::roundToInt (v));
            const int feel = static_cast<int> (feelParam->load() + 0.5f);
            return juce::String (kSyncDivisions[idx].name) + (feel == 1 ? "." : feel == 2 ? "T" : "");
        };
    }
    else
    {
        time.slider.textFromValueFunction = milliseconds;
    }

    syncBtn.setToggleState (sync, juce::dontSendNotification);
    syncBtn.setButtonText (sync ? "SYNC" : "FREE");
    feelStraightBtn.setVisible (sync);
    feelDottedBtn.setVisible (sync);
    feelTripletBtn.setVisible (sync);

    time.slider.updateText();
    time.slider.repaint();
}

void DelayAudioProcessorEditor::setSync (bool shouldSync)
{
    if (auto* param = audioProcessor.apvts.getParameter ("SYNC"))
    {
        param->beginChangeGesture();
        param->setValueNotifyingHost (shouldSync ? 1.0f : 0.0f);
        param->endChangeGesture();
    }
    applyTimeMode (shouldSync);
}

void DelayAudioProcessorEditor::timerCallback()
{
    leftMeter.setLevel (audioProcessor.getLeftLevel());
    rightMeter.setLevel (audioProcessor.getRightLevel());

    // follow host automation / preset recall
    const bool syncParam = audioProcessor.apvts.getRawParameterValue ("SYNC")->load() > 0.5f;
    if (syncParam != timeIsSync)
        applyTimeMode (syncParam);
    refreshButtons();

    const int fbMode = static_cast<int> (audioProcessor.apvts.getRawParameterValue ("FB_MODE")->load() + 0.5f);
    delayDisplay.setParams (audioProcessor.getEffectiveDelayMs(),
                            static_cast<float> (feedback.slider.getValue()),
                            static_cast<float> (ducking.slider.getValue()),
                            fbMode);
}

void DelayAudioProcessorEditor::paint (juce::Graphics& g)
{
    // The artwork PNG is ~75% transparent, so always paint an opaque base first;
    // otherwise the host's window colour shows through the middle.
    g.fillAll (juce::Colour (0xff050a04));

    if (bgImage.isValid())
        g.drawImage (bgImage, getLocalBounds().toFloat());
}

void DelayAudioProcessorEditor::placeKnob (Knob& k, int centreX, int topY, int size)
{
    k.slider.setBounds (centreX - size / 2, topY, size, size);
    k.label.setBounds  (centreX - 34, topY + size + 1, 68, 14);
}

void DelayAudioProcessorEditor::resized()
{
    // Level meters on the frame edges
    leftMeter.setBounds  (11, 47, 9, 144);
    rightMeter.setBounds (700, 47, 9, 144);

    const int small = 44, big = 62;
    const int smallTop = 113, bigTop = 96;   // labels of both sizes share one baseline

    // left: PAN  TONE  TAPE  |  TIME
    placeKnob (pan,   62,  smallTop, small);
    placeKnob (hiCut, 116, smallTop, small);
    placeKnob (tape,  170, smallTop, small);
    placeKnob (time,  240, bigTop,   big);

    // centre: readout + switches
    const int cx = 298, cw = 124;
    delayDisplay.setBounds (cx, 82, cw, 38);
    syncBtn.setBounds (cx,            128, 62, 16);
    pingBtn.setBounds (cx + 64,       128, 60, 16);
    feelStraightBtn.setBounds (cx,                 148, cw / 3, 16);
    feelDottedBtn.setBounds   (cx + cw / 3,        148, cw / 3, 16);
    feelTripletBtn.setBounds  (cx + 2 * (cw / 3),  148, cw - 2 * (cw / 3), 16);

    // right: FEEDBACK  |  DUCKER  DRY  WET
    placeKnob (feedback, 480, bigTop,   big);
    placeKnob (ducking,  550, smallTop, small);
    placeKnob (dry,      606, smallTop, small);
    placeKnob (wet,      660, smallTop, small);
}
