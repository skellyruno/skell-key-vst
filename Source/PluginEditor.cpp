#include <array>
#include "PluginEditor.h"
namespace {
const char* sharpNames[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
const char* flatNames[]={"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
const char* chordKinds[]={"maj","min","dim","aug"};
}
TonicRecreationAudioProcessorEditor::TonicRecreationAudioProcessorEditor(TonicRecreationAudioProcessor& p)
: AudioProcessorEditor(&p),processor(p),
  flatsAttachment(p.getState(),"preferFlats",flats),
  chordsAttachment(p.getState(),"chords",chords){
    setSize(570,340);
    title.setText("SKELL   |   KEY ANALYZER",juce::dontSendNotification);
    title.setColour(juce::Label::textColourId,juce::Colour(0xffa1adca));
    title.setFont(juce::Font(14.0f,juce::Font::bold));
    keyLabel.setJustificationType(juce::Justification::centred);
    keyLabel.setFont(juce::Font(29.0f,juce::Font::bold));
    keyLabel.setMinimumHorizontalScale(0.72f);
    subtitle.setJustificationType(juce::Justification::centred);
    subtitle.setFont(juce::Font(12.0f));
    candidatesLabel.setJustificationType(juce::Justification::centred);
    candidatesLabel.setFont(juce::Font(12.0f));
    chordLabel.setJustificationType(juce::Justification::centred);
    chordLabel.setFont(juce::Font(12.0f));
    bpmCaption.setText("DETECTED TEMPO", juce::dontSendNotification);
    bpmCaption.setColour(juce::Label::textColourId,juce::Colour(0xffa1adca));
    bpmValue.setFont(juce::Font(21.0f, juce::Font::bold));
    bpmValue.setColour(juce::Label::textColourId,juce::Colour(0xff92afff));
    tuningLabel.setText("AUTO A REF: Listening...",juce::dontSendNotification);
    tuningLabel.setFont(juce::Font(12.0f, juce::Font::bold));
    tuningLabel.setMinimumHorizontalScale(0.75f);
    for (auto* label : {&subtitle, &candidatesLabel, &chordLabel, &tuningLabel})
        label->setColour(juce::Label::textColourId, juce::Colour(0xffd0d7e8));
    tuningLabel.setJustificationType(juce::Justification::centred);
    pauseButton.setButtonText("Pause");
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
    const std::array<juce::Component*, 12> components {{
        &title, &keyLabel, &subtitle, &candidatesLabel, &chordLabel,
        &tuningLabel, &bpmValue, &bpmCaption, &pauseButton, &resetButton,
        &fileStatus, &openFileButton
    }};
    for (auto* component : components) addAndMakeVisible(component);
    addAndMakeVisible(liveButton);
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
    if (status.length() > 37) status = status.substring(0, 34) + "...";
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
    // Keep the detector visible even though the old Chords toggle is hidden.
    chordLabel.setVisible(true);
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
    // Host tempo and BPM confidence remain internal to the engine; no DAW tempo panel.

}
void TonicRecreationAudioProcessorEditor::paint(juce::Graphics& g) {
    const auto background=juce::Colour(0xff0b1422);
    const auto panel=juce::Colour(0xff152338);
    const auto stroke=juce::Colour(0xff354b6c);
    const auto blue=juce::Colour(0xff6da8ff);
    g.fillAll(background);
    auto box=[&](float x,float y,float w,float h,float radius=9.0f){
        juce::Rectangle<float> r(x,y,w,h);
        g.setColour(panel);g.fillRoundedRectangle(r,radius);
        g.setColour(stroke);g.drawRoundedRectangle(r,radius,0.8f);
    };
    g.setColour(juce::Colour(0xff101b2c));
    g.fillRoundedRectangle(juce::Rectangle<float>(6,6,558,328),12.0f);
    g.setColour(stroke);g.drawRoundedRectangle(juce::Rectangle<float>(6,6,558,328),12.0f,1.0f);
    for(int i=0;i<5;++i){
        const float h=8.0f+(i==2?11.0f:(i==1||i==3?6.0f:0.0f));
        g.setColour(blue.withAlpha(0.65f+0.07f*i));
        g.fillRoundedRectangle(juce::Rectangle<float>(19.0f+i*5.0f,26.0f-h/2.0f,2.5f,h),1.2f);
    }
    g.setColour(stroke);g.drawLine(14.0f,50.0f,556.0f,50.0f,1.0f);
    g.setColour(blue);g.fillRoundedRectangle(juce::Rectangle<float>(14,49,93,2),1.0f);
    box(14,57,542,145,11.0f);
    g.setColour(juce::Colour(0xffa9c2e8));
    g.setFont(juce::Font(9.5f,juce::Font::bold));
    g.drawText("DETECTED KEY",juce::Rectangle<int>(30,63,510,15),juce::Justification::centred);
    for(int side=0;side<2;++side)for(int i=0;i<9;++i){
        const float h=(i%4==2?22.0f:(i%3==0?15.0f:7.0f));
        const float x=side==0?72.0f+i*6.0f:498.0f-i*6.0f;
        g.setColour(blue.withAlpha(0.08f+0.05f*(9-i)/9.0f));
        g.fillRoundedRectangle(juce::Rectangle<float>(x,104-h*0.5f,2.0f,h),1.0f);
    }
    g.setColour(stroke);g.drawLine(285.0f,165.0f,285.0f,193.0f,0.8f);
    box(14,209,542,66);
    g.setColour(stroke);g.drawLine(215.0f,220.0f,215.0f,264.0f,1.0f);
    box(14,282,542,25);
    g.setColour(juce::Colour(0xffa9c2e8));g.setFont(juce::Font(9.5f,juce::Font::bold));
    g.drawText("AUTO A REF",juce::Rectangle<int>(22,285,95,17),juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff0d1828));
    g.fillRoundedRectangle(juce::Rectangle<float>(14,313,542,21),6.0f);
    if(processor.isOfflineMode()){
        const float progress=juce::jlimit(0.0f,1.0f,processor.getFileProgress());
        g.setColour(blue);g.fillRoundedRectangle(juce::Rectangle<float>(16,310,538*progress,2),1.0f);
    }
    if(dragHover){
        g.setColour(juce::Colour(0x99518bd2));
        g.fillRoundedRectangle(juce::Rectangle<float>(8,8,554,324),10.0f);
        g.setColour(juce::Colours::white);g.setFont(juce::Font(20.0f,juce::Font::bold));
        g.drawText("DROP AUDIO TO ANALYZE",getLocalBounds().reduced(30),juce::Justification::centred);
    }
}
void TonicRecreationAudioProcessorEditor::resized(){
    title.setBounds(48,13,315,28);
    pauseButton.setBounds(407,11,75,30);
    resetButton.setBounds(488,11,65,29);
    keyLabel.setBounds(70,85,430,44);
    subtitle.setBounds(90,129,390,20);
    candidatesLabel.setBounds(20,164,255,26);
    chordLabel.setBounds(296,164,250,26);
    bpmCaption.setBounds(26,215,185,18);
    bpmValue.setBounds(26,236,185,30);
    tuningLabel.setBounds(120,281,415,26);
    fileStatus.setBounds(21,313,305,19);
    openFileButton.setBounds(335,313,125,20);
    liveButton.setBounds(465,313,87,20);
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
