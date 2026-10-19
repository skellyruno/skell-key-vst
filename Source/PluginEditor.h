#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class TonicRecreationAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer, public juce::FileDragAndDropTarget {
public:
    explicit TonicRecreationAudioProcessorEditor(TonicRecreationAudioProcessor&);
    ~TonicRecreationAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray&) override;
    void fileDragEnter(const juce::StringArray&, int, int) override;
    void fileDragExit(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray&, int, int) override;
private:
    void timerCallback() override;
    TonicRecreationAudioProcessor& processor;
    juce::Label title, keyLabel, subtitle, candidatesLabel, chordLabel, tuningLabel;
    juce::Label bpmValue, bpmCaption, fileStatus;
    juce::ToggleButton pauseButton, flats, chords;
    juce::TextButton openFileButton {"Open Audio"}, liveButton {"Live Input"};
    std::unique_ptr<juce::FileChooser> fileChooser;
    bool dragHover=false;
    void loadFile(const juce::File&);
    juce::TextButton resetButton {"Reset"};
    juce::AudioProcessorValueTreeState::ButtonAttachment flatsAttachment, chordsAttachment;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TonicRecreationAudioProcessorEditor)
};
