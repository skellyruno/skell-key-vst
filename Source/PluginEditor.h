#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class TonicRecreationAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer {
public:
    explicit TonicRecreationAudioProcessorEditor(TonicRecreationAudioProcessor&);
    ~TonicRecreationAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    void timerCallback() override;
    TonicRecreationAudioProcessor& processor;
    juce::Label keyLabel, tuningLabel, modeLabel;
    juce::Slider tuning;
    juce::ToggleButton flats, chords;
    juce::AudioProcessorValueTreeState::SliderAttachment tuningAttachment;
    juce::AudioProcessorValueTreeState::ButtonAttachment flatsAttachment, chordsAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TonicRecreationAudioProcessorEditor)
};
