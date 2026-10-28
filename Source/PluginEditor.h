#pragma once
#include <JuceHeader.h>
#include <array>
#include "PluginProcessor.h"

class SkellLookAndFeel;
class SceneHitButton;

// Skell Key editor. Everything is drawn in a fixed 550 x 450 design space and
// scaled to the window, so the plugin can be resized freely (fixed aspect).
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

    static constexpr int designWidth = 550, designHeight = 450;
    static constexpr int defaultWidth = 495, defaultHeight = 405;

private:
    void timerCallback() override;
    void loadFile(const juce::File&);
    float scale() const noexcept { return (float) getWidth() / (float) designWidth; }

    // --- painting helpers (all in design-space units) ---
    void paintHeader(juce::Graphics&);
    void paintKeyCard(juce::Graphics&);
    void paintSpine(juce::Graphics&, juce::Rectangle<float> area);
    void paintTempoCard(juce::Graphics&);
    void paintReferenceCard(juce::Graphics&);
    void paintFooter(juce::Graphics&);

    TonicRecreationAudioProcessor& processor;
    juce::TextButton pauseButton {"Pause"}, resetButton {"Reset"};
    juce::TextButton openFileButton {"Open Audio"}, liveButton {"Live Input"};
    juce::TextButton halfButton {"/2"}, doubleButton {"x2"};
    float bpmFactor = 1.0f;   // user octave correction (x0.5 / x2) applied to the displayed tempo
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<SkellLookAndFeel> look;
    std::unique_ptr<SceneHitButton> scene;
    const juce::Rectangle<float> sceneArea { 274.0f, 76.0f, 250.0f, 182.0f };
    float level = 0.0f, bpmNow = 0.0f, handL = 0.0f, handR = 0.0f;
    int previewPc = -1;

    // Display state, refreshed by the timer on the message thread.
    struct View {
        int root = -1, mode = 2;
        juce::String keyText {"Listening..."}, keyHint, candidates, chordText;
        float keyConfidence = 0.0f;
        juce::String bpmText {"--"}, bpmHint {"Waiting for rhythm"};
        float bpmConfidence = 0.0f;
        juce::String refText {"--"}, refHint {"Detecting tuning"};
        float cents = 0.0f; bool tuningValid = false;
        juce::String status;
        float progress = 0.0f; bool busy = false;
    } view;
    std::array<float,12> chromaTarget {}, chromaShown {};
    float tonicPulse = 0.0f;
    int lastRoot = -2;
    juce::TooltipWindow tooltipWindow {this, 600};
    bool dragHover = false;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TonicRecreationAudioProcessorEditor)
};
