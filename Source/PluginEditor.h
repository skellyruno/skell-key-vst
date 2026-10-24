#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class NeonButtonLookAndFeel;

class TonicRecreationAudioProcessorEditor final : public juce::AudioProcessorEditor,
    private juce::Timer, public juce::FileDragAndDropTarget
{
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
    void loadFile(const juce::File&);
    TonicRecreationAudioProcessor& processor;
    juce::Label brand, keyLabel, keyHint, candidates, chord, bpm, bpmHint, referenceA, referenceHint, fileStatus;
    juce::TextButton pauseButton {"Pause"}, resetButton {"Reset"};
    juce::TextButton openFileButton {"Open Audio"}, liveButton {"Live Input"};
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<NeonButtonLookAndFeel> buttonLook;
    bool dragHover = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TonicRecreationAudioProcessorEditor)
};
