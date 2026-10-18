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
    setSize(650,485);
    title.setText("SKELL   |   KEY ANALYZER",juce::dontSendNotification);
    title.setColour(juce::Label::textColourId,juce::Colour(0xffa1adca));
    title.setFont(juce::Font(14.0f,juce::Font::bold));
    keyLabel.setJustificationType(juce::Justification::centred);
    keyLabel.setFont(juce::Font(34.0f,juce::Font::bold));
    keyLabel.setMinimumHorizontalScale(0.72f);
    subtitle.setJustificationType(juce::Justification::centred);
    subtitle.setFont(juce::Font(12.0f));
    candidatesLabel.setJustificationType(juce::Justification::centred);
    candidatesLabel.setFont(juce::Font(12.0f));
    chordLabel.setJustificationType(juce::Justification::centred);
    chordLabel.setFont(juce::Font(12.0f));
    bpmCaption.setText("DETECTED TEMPO", juce::dontSendNotification);
    bpmCaption.setColour(juce::Label::textColourId,juce::Colour(0xffa1adca));
    bpmValue.setFont(juce::Font(23.0f, juce::Font::bold));
    bpmValue.setColour(juce::Label::textColourId,juce::Colour(0xff92afff));
    hostTempo.setColour(juce::Label::textColourId,juce::Colour(0xffd0d7e8));
    bpmConfidenceLabel.setColour(juce::Label::textColourId,juce::Colour(0xffd0d7e8));
    tuningLabel.setText("AUTO A REF: Listening...",juce::dontSendNotification);
    tuningLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    tuningLabel.setMinimumHorizontalScale(0.75f);
    sensitivityLabel.setText("SILENCE GATE",juce::dontSendNotification);
    sensitivityLabel.setFont(juce::Font(10.0f,juce::Font::bold));
    responseLabel.setText("RESPONSIVENESS",juce::dontSendNotification);
    responseLabel.setFont(juce::Font(10.0f,juce::Font::bold));
    for(auto* label:{&subtitle,&candidatesLabel,&chordLabel,&tuningLabel,&sensitivityLabel,&responseLabel})
        label->setColour(juce::Label::textColourId,juce::Colour(0xffd0d7e8));
    for(auto* label: {&sensitivityLabel, &responseLabel, &tuningLabel})
        label->setJustificationType(juce::Justification::centred);
    for(auto* slider:{&sensitivity,&response}){
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow,false,62,19);
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
    fileStatus.setFont(juce::Font(10.0f));
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
    liveButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff24416c));
    getLookAndFeel().setColour(juce::ToggleButton::textColourId, juce::Colour(0xffdce6fb));
    startTimerHz(6);timerCallback();
}
TonicRecreationAudioProcessorEditor::~TonicRecreationAudioProcessorEditor(){stopTimer();}
void TonicRecreationAudioProcessorEditor::timerCallback(){
    const bool offline=processor.isOfflineMode();
    const bool hasInput=processor.hasLiveInput();
    openFileButton.setEnabled(!hasInput);
    liveButton.setVisible(true);
    juce::String status;
    if(offline){
        status="FILE  |  " + processor.getFileName();
        if(processor.fileFailed()) status+="  |  Cannot decode file";
        else if(processor.isFileBusy())
            status+="  |  " + juce::String(processor.getFileAnalyzedSeconds(),1) + " / " +
                    juce::String(processor.getFileDurationSeconds(),1) + " s  (" +
                    juce::String(juce::roundToInt(100.0f*processor.getFileProgress())) + "%)";
        else status+="  |  Done";
    }else if(hasInput)status="LIVE INPUT  |  Listening to the DAW";
    else status="NO INPUT  |  Drop audio anywhere or use Open Audio";
    fileStatus.setTooltip(status);
    // Keep long filenames on one line: the complete name is in the tooltip.
    if (status.length() > 49) status = status.substring(0, 46) + "...";
    fileStatus.setText(status,juce::dontSendNotification);
    if(offline && processor.isFileBusy())repaint();
    const auto* names=flats.getToggleState()?flatNames:sharpNames;
    const int root=processor.getDetectedRoot(),mode=processor.getDetectedMode();
    keyLabel.setText(root<0?(offline && !processor.isFileBusy()?"No key found":"Listening..."):juce::String(names[root])+(mode==0?" major":" minor"),juce::dontSendNotification);
    const int keyPct = juce::roundToInt(processor.getConfidence() * 100.0f);
    subtitle.setText(root < 0 ? "Waiting for musical input"
        : (keyPct < 10 ? "Low confidence  -  collecting more audio"
                       : "Key confidence: " + juce::String(keyPct) + "%"), juce::dontSendNotification);
    juce::String ranked="LIKELY KEYS    ";
    for(int i=0;i<3;++i){const int r=processor.getRankedRoot(i),m=processor.getRankedMode(i);
        if(r<0)continue;
        if(i>0)ranked+="    /    ";
        ranked+=juce::String(names[r])+(m==0?" major":" minor");
    }
    candidatesLabel.setText(root<0?"Waiting for audio...":ranked,juce::dontSendNotification);
    const int cr=processor.getChordRoot(),ck=processor.getChordKind();
    chordLabel.setVisible(chords.getToggleState());
    chordLabel.setText(cr<0||ck<0?"Not confident enough":""+juce::String(names[cr])+" "+chordKinds[ck]+"   (experimental)",juce::dontSendNotification);
    const float aHz=processor.getDetectedTuningHz();
    const float aConf=processor.getTuningConfidence();
    const float cents=1200.0f*std::log2(aHz/440.0f);
    tuningLabel.setText(aConf<0.03f ? "AUTO A REF\nListening..." :
        "AUTO A REF\n" + juce::String(aHz,1) + " Hz" +
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
    const auto background = juce::Colour(0xff0b1422);
    const auto panel = juce::Colour(0xff152338);
    const auto stroke = juce::Colour(0xff354b6c);
    const auto blue = juce::Colour(0xff6da8ff);
    g.fillAll(background);

    // The compact V18 layout is independent of the decorative concept mockup.
    // It is drawn with JUCE vectors, rather than embedding a non-interactive image.
    auto box = [&](float x, float y, float w, float h, float radius=10.0f) {
        juce::Rectangle<float> r(x,y,w,h);
        g.setColour(panel); g.fillRoundedRectangle(r,radius);
        g.setColour(stroke); g.drawRoundedRectangle(r,radius,0.8f);
    };
    g.setColour(juce::Colour(0xff101b2c));
    g.fillRoundedRectangle(juce::Rectangle<float>(7,7,636,471),13.0f);
    g.setColour(stroke);
    g.drawRoundedRectangle(juce::Rectangle<float>(7,7,636,471),13.0f,1.0f);

    // Signature audio bars and a thin accent rule distinguish the new design.
    for (int i=0;i<5;++i) {
        const float h=9.0f + (i==2?12.0f:(i==1||i==3?7.0f:0.0f));
        g.setColour(blue.withAlpha(0.65f + 0.07f*i));
        g.fillRoundedRectangle(juce::Rectangle<float>(23.0f+i*5.0f,33.0f-h/2.0f,2.5f,h),1.2f);
    }
    g.setColour(juce::Colour(0xff31445f));
    g.drawLine(16.0f,59.0f,634.0f,59.0f,1.0f);
    g.setColour(blue);
    g.fillRoundedRectangle(juce::Rectangle<float>(16,58,106,2),1.0f);

    box(16,67,618,188,12.0f);
    g.setColour(juce::Colour(0xffa9c2e8));
    g.setFont(juce::Font(10.0f,juce::Font::bold));
    g.drawText("DETECTED KEY",juce::Rectangle<int>(45,78,560,17),juce::Justification::centred);
    for (int side=0;side<2;++side) {
        for(int i=0;i<11;++i) {
            const float h=(i%5==2?27.0f:(i%3==0?17.0f:8.0f));
            const float x=side==0?84.0f+i*7.0f:566.0f-i*7.0f;
            g.setColour(blue.withAlpha(0.09f+0.045f*(11-i)/11.0f));
            g.fillRoundedRectangle(juce::Rectangle<float>(x,127-h*0.5f,2.0f,h),1.0f);
        }
    }
    g.setColour(stroke);
    g.drawLine(332.0f,185.0f,332.0f,217.0f,0.8f);
    g.drawLine(160.0f,224.0f,490.0f,224.0f,0.8f);

    box(16,263,618,71);
    g.setColour(stroke); g.drawLine(253.0f,276.0f,253.0f,320.0f,1.0f);
    box(16,342,258,91);
    box(282,342,171,91);
    box(461,342,173,91);
    g.setColour(juce::Colour(0xff0d1828));
    g.fillRoundedRectangle(juce::Rectangle<float>(16,441,618,31),7.0f);
    g.setColour(stroke);
    g.drawRoundedRectangle(juce::Rectangle<float>(16,441,618,31),7.0f,0.8f);

    if(processor.isOfflineMode()) {
        const float progress=juce::jlimit(0.0f,1.0f,processor.getFileProgress());
        g.setColour(blue);
        g.fillRoundedRectangle(juce::Rectangle<float>(18,438,614*progress,2),1.0f);
    }
    if(dragHover) {
        g.setColour(juce::Colour(0x99518bd2));
        g.fillRoundedRectangle(juce::Rectangle<float>(9,9,632,467),11.0f);
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(24.0f,juce::Font::bold));
        g.drawText("DROP AUDIO TO ANALYZE",getLocalBounds().reduced(40),juce::Justification::centred);
    }
}
void TonicRecreationAudioProcessorEditor::resized() {
    title.setBounds(56,20,360,30);
    pauseButton.setBounds(479,21,79,28);
    resetButton.setBounds(558,19,68,30);

    keyLabel.setBounds(104,100,442,49);
    subtitle.setBounds(107,150,436,22);
    candidatesLabel.setBounds(29,189,288,25);
    chordLabel.setBounds(343,189,274,25);
    flats.setBounds(230,226,90,25);
    chords.setBounds(337,226,99,25);

    bpmCaption.setBounds(30,269,205,18);
    bpmValue.setBounds(30,287,210,37);
    hostTempo.setBounds(269,273,340,24);
    bpmConfidenceLabel.setBounds(269,297,340,25);

    tuningLabel.setBounds(25,351,240,72);
    sensitivityLabel.setBounds(286,347,161,18);
    responseLabel.setBounds(464,347,165,18);
    sensitivity.setBounds(319,361,96,66);
    response.setBounds(500,361,94,66);

    fileStatus.setBounds(26,445,378,22);
    openFileButton.setBounds(409,445,118,23);
    liveButton.setBounds(532,445,92,23);
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
