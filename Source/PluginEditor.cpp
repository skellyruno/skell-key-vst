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
    setSize(780,540);
    title.setText("SKELL  /  KEY ANALYZER",juce::dontSendNotification);
    title.setColour(juce::Label::textColourId,juce::Colour(0xffa1adca));
    title.setFont(juce::Font(16.0f,juce::Font::bold));
    keyLabel.setJustificationType(juce::Justification::centred);
    keyLabel.setFont(juce::Font(56.0f,juce::Font::bold));
    subtitle.setJustificationType(juce::Justification::centred);
    candidatesLabel.setJustificationType(juce::Justification::centred);
    chordLabel.setJustificationType(juce::Justification::centred);
    bpmCaption.setText("DETECTED TEMPO", juce::dontSendNotification);
    bpmCaption.setColour(juce::Label::textColourId,juce::Colour(0xffa1adca));
    bpmValue.setFont(juce::Font(32.0f, juce::Font::bold));
    bpmValue.setColour(juce::Label::textColourId,juce::Colour(0xff92afff));
    hostTempo.setColour(juce::Label::textColourId,juce::Colour(0xffd0d7e8));
    bpmConfidenceLabel.setColour(juce::Label::textColourId,juce::Colour(0xffd0d7e8));
    tuningLabel.setText("REFERENCE A",juce::dontSendNotification);
    sensitivityLabel.setText("SILENCE GATE",juce::dontSendNotification);
    responseLabel.setText("RESPONSIVENESS",juce::dontSendNotification);
    for(auto* label:{&subtitle,&candidatesLabel,&chordLabel,&tuningLabel,&sensitivityLabel,&responseLabel})
        label->setColour(juce::Label::textColourId,juce::Colour(0xffd0d7e8));
    for(auto* slider:{&sensitivity,&response}){
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,21);
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
    fileStatus.setText(status,juce::dontSendNotification);
    if(offline && processor.isFileBusy())repaint();
    const auto* names=flats.getToggleState()?flatNames:sharpNames;
    const int root=processor.getDetectedRoot(),mode=processor.getDetectedMode();
    keyLabel.setText(root<0?(offline && !processor.isFileBusy()?"No key found":"Listening…"):juce::String(names[root])+(mode==0?" major":" minor"),juce::dontSendNotification);
    subtitle.setText(root<0?"Play audio to begin":"Key confidence: "+juce::String(juce::roundToInt(processor.getConfidence()*100))+"%",juce::dontSendNotification);
    juce::String ranked="TOP KEYS   ";
    for(int i=0;i<3;++i){const int r=processor.getRankedRoot(i),m=processor.getRankedMode(i);
        if(r<0)continue;
        if(i>0)ranked+="      •      ";
        ranked+=juce::String(names[r])+(m==0?" major":" minor");
    }
    candidatesLabel.setText(root<0?"TOP KEYS   Awaiting signal":ranked,juce::dontSendNotification);
    const int cr=processor.getChordRoot(),ck=processor.getChordKind();
    chordLabel.setVisible(chords.getToggleState());
    chordLabel.setText(cr<0||ck<0?"CHORD   —":"CHORD   "+juce::String(names[cr])+chordKinds[ck]+"  (experimental)",juce::dontSendNotification);
    const float aHz=processor.getDetectedTuningHz();
    const float aConf=processor.getTuningConfidence();
    const float cents=1200.0f*std::log2(aHz/440.0f);
    tuningLabel.setText(aConf<0.03f ? "AUTO A REF   —  listening" :
        "AUTO A REF   " + juce::String(aHz,1) + " Hz   (" +
        (cents>=0 ? "+" : "") + juce::String(cents,1) + " cents)",juce::dontSendNotification);
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
void TonicRecreationAudioProcessorEditor::paint(juce::Graphics& g){
    g.fillAll(juce::Colour(0xff151820));
    g.setColour(juce::Colour(0xff222837));g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(16),15.0f);
    g.setColour(juce::Colour(0xff313a4c));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(16),15.0f,1.0f);
    g.setColour(juce::Colour(0xff667baf));g.fillRoundedRectangle(juce::Rectangle<float>(28,62,724,3),1.5f);
    g.setColour(juce::Colour(0xff33425f));g.drawRoundedRectangle(juce::Rectangle<float>(100,88,580,205),15.0f,1.0f);
    g.drawRoundedRectangle(juce::Rectangle<float>(36,312,708,61),9.0f,1.0f);
    if(processor.isOfflineMode()) {
        const float progress=juce::jlimit(0.0f,1.0f,processor.getFileProgress());
        g.setColour(juce::Colour(0xff303849));
        g.fillRoundedRectangle(juce::Rectangle<float>(37,486,510,4),2.0f);
        g.setColour(juce::Colour(0xff92afff));
        g.fillRoundedRectangle(juce::Rectangle<float>(37,486,510*progress,4),2.0f);
    }
    if(dragHover){
        g.setColour(juce::Colour(0x885f99ff));
        g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(18),14.0f);
        g.setColour(juce::Colour(0xffc5ddff));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(20),12.0f,3.0f);
    }
}
void TonicRecreationAudioProcessorEditor::resized(){
    title.setBounds(35,25,300,30);
    pauseButton.setBounds(575,27,85,28);resetButton.setBounds(665,27,85,28);
    keyLabel.setBounds(130,95,520,80);subtitle.setBounds(170,171,440,27);
    candidatesLabel.setBounds(105,215,570,35);chordLabel.setBounds(225,252,330,30);
    bpmCaption.setBounds(50,319,155,20);
    bpmValue.setBounds(50,339,205,32);
    hostTempo.setBounds(275,317,450,25);
    bpmConfidenceLabel.setBounds(275,343,450,25);
    fileStatus.setBounds(35,496,520,26);
    openFileButton.setBounds(555,496,100,28);
    liveButton.setBounds(660,496,91,28);
    tuningLabel.setBounds(39,392,310,38);
    sensitivityLabel.setBounds(362,382,170,23);sensitivity.setBounds(375,404,130,85);
    responseLabel.setBounds(555,382,170,23);response.setBounds(570,404,130,85);
    flats.setBounds(341,273,85,30);chords.setBounds(432,273,90,30);
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
