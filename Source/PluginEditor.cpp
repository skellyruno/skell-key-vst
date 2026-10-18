#include <array>
#include "PluginEditor.h"
namespace {
const char* sharpNames[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
const char* flatNames[]={"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
const char* chordKinds[]={"maj","min","dim","aug"};
}
TonicRecreationAudioProcessorEditor::TonicRecreationAudioProcessorEditor(TonicRecreationAudioProcessor& p)
: AudioProcessorEditor(&p),processor(p),
  sensitivityAttachment(p.getState(),"sensitivity",sensitivity),
  responseAttachment(p.getState(),"responsiveness",response),
  flatsAttachment(p.getState(),"preferFlats",flats),
  chordsAttachment(p.getState(),"chords",chords){
    setSize(860,670);
    title.setText("SKELL   /   KEY ANALYZER",juce::dontSendNotification);
    title.setColour(juce::Label::textColourId,juce::Colour(0xffa1adca));
    title.setFont(juce::Font(16.0f,juce::Font::bold));
    keyLabel.setJustificationType(juce::Justification::centred);
    keyLabel.setFont(juce::Font(62.0f,juce::Font::bold));
    subtitle.setJustificationType(juce::Justification::centred);
    subtitle.setFont(juce::Font(16.0f));
    candidatesLabel.setJustificationType(juce::Justification::centred);
    candidatesLabel.setFont(juce::Font(16.0f));
    chordLabel.setJustificationType(juce::Justification::centred);
    chordLabel.setFont(juce::Font(15.0f));
    bpmCaption.setText("DETECTED TEMPO", juce::dontSendNotification);
    bpmCaption.setColour(juce::Label::textColourId,juce::Colour(0xffa1adca));
    bpmValue.setFont(juce::Font(31.0f, juce::Font::bold));
    bpmValue.setColour(juce::Label::textColourId,juce::Colour(0xff92afff));
    hostTempo.setColour(juce::Label::textColourId,juce::Colour(0xffd0d7e8));
    bpmConfidenceLabel.setColour(juce::Label::textColourId,juce::Colour(0xffd0d7e8));
    tuningLabel.setText("AUTO REFERENCE A",juce::dontSendNotification);
    sensitivityLabel.setText("SILENCE GATE",juce::dontSendNotification);
    responseLabel.setText("RESPONSIVENESS",juce::dontSendNotification);
    for(auto* label:{&subtitle,&candidatesLabel,&chordLabel,&tuningLabel,&sensitivityLabel,&responseLabel})
        label->setColour(juce::Label::textColourId,juce::Colour(0xffd0d7e8));
    for(auto* label: {&sensitivityLabel, &responseLabel, &tuningLabel})
        label->setJustificationType(juce::Justification::centred);
    for(auto* slider:{&sensitivity,&response}){
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow,false,85,22);
        slider->setColour(juce::Slider::rotarySliderOutlineColourId,juce::Colour(0xff3c4762));
        slider->setColour(juce::Slider::thumbColourId,juce::Colour(0xffadc7ff));
        slider->setColour(juce::Slider::textBoxTextColourId,juce::Colour(0xffe6ecfa));
        slider->setColour(juce::Slider::textBoxOutlineColourId,juce::Colours::transparentBlack);
        slider->setColour(juce::Slider::textBoxBackgroundColourId,juce::Colour(0xff1b2130));
        slider->setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xff92afff));
    }
    flats.setButtonText("Flats"); chords.setButtonText("Chords");pauseButton.setButtonText("Pause");
    pauseButton.setClickingTogglesState(true);
    pauseButton.onClick=[this]{processor.setPaused(pauseButton.getToggleState());};
    resetButton.onClick=[this]{if(processor.isOfflineMode())processor.returnToLive();else processor.requestReset();};
    openFileButton.onClick=[this]{
        if(processor.hasLiveInput())return;
        fileChooser=std::make_unique<juce::FileChooser>("Analyze audio",juce::File{},
                            "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        // Make the SafePointer from the editor, not from the lambda closure.
        auto* editor = this;
        juce::Component::SafePointer<TonicRecreationAudioProcessorEditor> safeEditor(editor);
        constexpr int flags = juce::FileBrowserComponent::openMode |
                              juce::FileBrowserComponent::canSelectFiles;
        fileChooser->launchAsync(flags,
            [safeEditor](const juce::FileChooser& chooser) {
                if (safeEditor != nullptr)
                    safeEditor->loadFile(chooser.getResult());
            });
    };
    liveButton.onClick=[this]{processor.returnToLive();};
    fileStatus.setJustificationType(juce::Justification::centredLeft);
    fileStatus.setFont(juce::Font(13.0f));
    fileStatus.setMinimumHorizontalScale(0.85f);
    fileStatus.setColour(juce::Label::textColourId,juce::Colour(0xffb1c9f1));
    // Explicit element type is required for mixed Label/Slider/Button pointers.
    const std::array<juce::Component*, 21> components {{
        &title, &keyLabel, &subtitle, &candidatesLabel, &chordLabel,
        &tuningLabel, &sensitivityLabel, &responseLabel, &bpmValue,
        &bpmCaption, &hostTempo, &bpmConfidenceLabel, &sensitivity,
        &response, &flats, &chords, &pauseButton, &resetButton,
        &fileStatus, &openFileButton, &liveButton
    }};
    for (auto* component : components)
        addAndMakeVisible(component);
    resetButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff313e50));
    openFileButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff34435d));
    liveButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff435e97));
    startTimerHz(6);timerCallback();
}
TonicRecreationAudioProcessorEditor::~TonicRecreationAudioProcessorEditor(){stopTimer();}
void TonicRecreationAudioProcessorEditor::timerCallback(){
    const bool offline=processor.isOfflineMode();
    const bool hasInput=processor.hasLiveInput();
    openFileButton.setEnabled(!hasInput);
    liveButton.setVisible(offline);
    juce::String status;
    if(offline){
        status="FILE  •  " + processor.getFileName();
        if(processor.fileFailed()) status+="  •  Cannot decode file";
        else if(processor.isFileBusy())
            status+="  •  " + juce::String(processor.getFileAnalyzedSeconds(),1) + " / " +
                    juce::String(processor.getFileDurationSeconds(),1) + " s  (" +
                    juce::String(juce::roundToInt(100.0f*processor.getFileProgress())) + "%)";
        else status+="  •  Done";
    }else if(hasInput)status="LIVE INPUT  •  Listening to the DAW";
    else status="NO INPUT  •  Drop WAV / AIFF / FLAC / OGG / MP3 anywhere, or open audio";
    fileStatus.setTooltip(status);
    // Keep long filenames on one line: the complete name is in the tooltip.
    if (status.length() > 77) status = status.substring(0, 74) + "...";
    fileStatus.setText(status,juce::dontSendNotification);
    if(offline && processor.isFileBusy())repaint();
    const auto* names=flats.getToggleState()?flatNames:sharpNames;
    const int root=processor.getDetectedRoot(),mode=processor.getDetectedMode();
    keyLabel.setText(root<0?(offline && !processor.isFileBusy()?"No key found":"Listening…"):juce::String(names[root])+(mode==0?" major":" minor"),juce::dontSendNotification);
    const int keyPct = juce::roundToInt(processor.getConfidence() * 100.0f);
    subtitle.setText(root < 0 ? "Waiting for musical input"
        : (keyPct < 10 ? "Low confidence  -  collecting more audio"
                       : "Key confidence: " + juce::String(keyPct) + "%"), juce::dontSendNotification);
    juce::String ranked="OTHER LIKELY KEYS    ";
    for(int i=0;i<3;++i){const int r=processor.getRankedRoot(i),m=processor.getRankedMode(i);
        if(r<0)continue;
        if(i>0)ranked+="    /    ";
        ranked+=juce::String(names[r])+(m==0?" major":" minor");
    }
    candidatesLabel.setText(root<0?"LIKELY KEYS   Waiting for audio":ranked,juce::dontSendNotification);
    const int cr=processor.getChordRoot(),ck=processor.getChordKind();
    chordLabel.setVisible(chords.getToggleState());
    chordLabel.setText(cr<0||ck<0?"CHORD   Not confident enough":"CHORD   "+juce::String(names[cr])+" "+chordKinds[ck]+"   (experimental)",juce::dontSendNotification);
    const float aHz=processor.getDetectedTuningHz();
    const float aConf=processor.getTuningConfidence();
    const float cents=1200.0f*std::log2(aHz/440.0f);
    tuningLabel.setText(aConf<0.03f ? "AUTO A REF:  Listening..." :
        "AUTO A REF:  " + juce::String(aHz,1) + " Hz" +
        "    " + (cents>=0 ? juce::String("+") : juce::String()) + juce::String(cents,1) + " cents",juce::dontSendNotification);
    const float detected = processor.getDetectedBpm();
    const float beatConfidence = processor.getBpmConfidence();
    bpmValue.setText(detected > 0.0f ? juce::String(detected, 1) + " BPM" : "-- BPM", juce::dontSendNotification);
    bpmConfidenceLabel.setText(detected > 0.0f ? "Confidence  " + juce::String(juce::roundToInt(beatConfidence*100)) + "%" : "Listening for rhythm (8s+)", juce::dontSendNotification);
    if(!offline && processor.hasHostBpm()) {
        const float host=processor.getHostBpm();
        hostTempo.setText("DAW " + juce::String(host,1) + " BPM" +
            (detected > 0.0f ? "     Difference " + juce::String(detected-host,1) : ""),juce::dontSendNotification);
    } else hostTempo.setText("DAW tempo unavailable",juce::dontSendNotification);
}
void TonicRecreationAudioProcessorEditor::paint(juce::Graphics& g) {
    const auto bg = juce::Colour(0xff151924);
    const auto panel = juce::Colour(0xff232a39);
    const auto border = juce::Colour(0xff37445e);
    const auto accent = juce::Colour(0xff91b3ff);
    g.fillAll(bg);
    auto card = [&](float x, float y, float w, float h, float rounding = 13.0f) {
        juce::Rectangle<float> area(x,y,w,h);
        g.setColour(panel); g.fillRoundedRectangle(area,rounding);
        g.setColour(border); g.drawRoundedRectangle(area,rounding,1.0f);
    };
    g.setColour(juce::Colour(0xff1d2331));
    g.fillRoundedRectangle(juce::Rectangle<float>(18,18,824,634),16.0f);
    g.setColour(border);
    g.drawRoundedRectangle(juce::Rectangle<float>(18,18,824,634),16.0f,1.0f);
    g.setColour(accent);
    g.fillRoundedRectangle(juce::Rectangle<float>(36,80,788,3),1.5f);

    card(36,99,788,269);
    g.setColour(juce::Colour(0xff9aabc9));
    g.setFont(juce::Font(12.0f,juce::Font::bold));
    g.drawText("DETECTED KEY",juce::Rectangle<int>(50,116,760,22),juce::Justification::centred);

    card(36,382,788,96);
    g.setColour(juce::Colour(0xff3a465e));
    g.fillRoundedRectangle(juce::Rectangle<float>(320,398,1,64),0.5f);
    card(36,493,288,111);
    card(335,493,236,111);
    card(582,493,242,111);

    // A separate thin progress line leaves the footer text legible.
    if (processor.isOfflineMode()) {
        const float progress = juce::jlimit(0.0f,1.0f,processor.getFileProgress());
        g.setColour(juce::Colour(0xff384459));
        g.fillRoundedRectangle(juce::Rectangle<float>(42,613,776,3),1.5f);
        if (progress > 0.0f) {
            g.setColour(accent);
            g.fillRoundedRectangle(juce::Rectangle<float>(42,613,776 * progress,3),1.5f);
        }
    }
    if (dragHover) {
        g.setColour(juce::Colour(0x66578cc9));
        g.fillRoundedRectangle(juce::Rectangle<float>(20,20,820,630),14.0f);
        g.setColour(accent);
        g.drawRoundedRectangle(juce::Rectangle<float>(22,22,816,626),14.0f,2.0f);
        g.setFont(juce::Font(23.0f,juce::Font::bold));
        g.drawText("DROP AUDIO TO ANALYZE",getLocalBounds().reduced(40),juce::Justification::centred);
    }
}
void TonicRecreationAudioProcessorEditor::resized() {
    title.setBounds(44,34,480,34);
    pauseButton.setBounds(634,35,92,32);
    resetButton.setBounds(737,34,78,34);
    keyLabel.setBounds(90,149,680,76);
    subtitle.setBounds(90,230,680,28);
    candidatesLabel.setBounds(77,275,706,28);
    chordLabel.setBounds(132,312,596,23);
    flats.setBounds(320,337,94,27);
    chords.setBounds(446,337,95,27);
    bpmCaption.setBounds(56,399,250,21);
    bpmValue.setBounds(54,426,254,39);
    hostTempo.setBounds(343,398,452,29);
    bpmConfidenceLabel.setBounds(343,431,452,29);
    tuningLabel.setBounds(45,511,271,77);
    sensitivityLabel.setBounds(345,502,216,23);
    responseLabel.setBounds(592,502,222,23);
    sensitivity.setBounds(375,522,156,76);
    response.setBounds(625,522,156,76);
    fileStatus.setBounds(44,622,550,25);
    openFileButton.setBounds(607,622,110,28);
    liveButton.setBounds(725,622,95,28);
}

namespace {
bool isAudioFile(const juce::String& path){
    const auto ext=juce::File(path).getFileExtension().toLowerCase();
    return juce::StringArray {".wav",".aiff",".aif",".flac",".ogg",".mp3"}.contains(ext);
}
}
bool TonicRecreationAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files){
    if(processor.hasLiveInput())return false;
    for(const auto& file:files)if(isAudioFile(file))return true;
    return false;
}
void TonicRecreationAudioProcessorEditor::fileDragEnter(const juce::StringArray&,int,int){
    dragHover=true;repaint();
}
void TonicRecreationAudioProcessorEditor::fileDragExit(const juce::StringArray&){
    dragHover=false;repaint();
}
void TonicRecreationAudioProcessorEditor::filesDropped(const juce::StringArray& files,int,int){
    dragHover=false;repaint();
    for(const auto& path:files)if(isAudioFile(path)){
        loadFile(juce::File(path));break;
    }
}
void TonicRecreationAudioProcessorEditor::loadFile(const juce::File& file){
    if(!file.existsAsFile())return;
    processor.requestFileAnalysis(file);
    timerCallback();
}
