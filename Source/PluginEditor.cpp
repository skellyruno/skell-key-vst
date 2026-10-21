#include "PluginEditor.h"
#include <array>
#include <cmath>

namespace {
constexpr auto bg = 0xff0d111b;
constexpr auto card = 0xff171f2d;
constexpr auto border = 0xff293549;
constexpr auto primary = 0xffecf3ff;
constexpr auto muted = 0xff92a3b9;
constexpr auto blue = 0xff81b3ff;
const char* notes[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
const char* chordKinds[] = {"maj","min","dim","aug"};
void labelStyle(juce::Label& label, float fontSize, uint32_t colour, bool bold=false,
                juce::Justification justification=juce::Justification::centredLeft) {
    label.setColour(juce::Label::textColourId,juce::Colour(colour));
    label.setFont(juce::Font(fontSize, bold ? juce::Font::bold : juce::Font::plain));
    label.setJustificationType(justification);
    label.setMinimumHorizontalScale(0.7f);
}
bool isAudioFile(const juce::String& path) {
    const auto ext=juce::File(path).getFileExtension().toLowerCase();
    return juce::StringArray {".wav",".aiff",".aif",".flac",".ogg",".mp3"}.contains(ext);
}
}

TonicRecreationAudioProcessorEditor::TonicRecreationAudioProcessorEditor(TonicRecreationAudioProcessor& p)
  : AudioProcessorEditor(&p), processor(p) {
    setSize(570,340);
    brand.setText("SKELL  /  KEY", juce::dontSendNotification);
    labelStyle(brand,15.0f,primary,true);
    labelStyle(keyLabel,35.0f,primary,true,juce::Justification::centred);
    labelStyle(keyHint,11.0f,muted,false,juce::Justification::centred);
    labelStyle(candidates,11.0f,muted,false,juce::Justification::centred);
    labelStyle(chord,11.0f,primary,false,juce::Justification::centred);
    labelStyle(bpm,24.0f,primary,true,juce::Justification::centred);
    labelStyle(bpmHint,10.0f,muted,false,juce::Justification::centred);
    labelStyle(referenceA,24.0f,primary,true,juce::Justification::centred);
    labelStyle(referenceHint,10.0f,muted,false,juce::Justification::centred);
    labelStyle(fileStatus,11.0f,muted);
    fileStatus.setTooltip("Drop audio anywhere to analyze. File input takes priority over live input.");
    for(auto* c : std::array<juce::Component*,14>{&brand,&keyLabel,&keyHint,&candidates,&chord,
        &bpm,&bpmHint,&referenceA,&referenceHint,&fileStatus,&pauseButton,&resetButton,
        &openFileButton,&liveButton}) addAndMakeVisible(c);
    for(auto* b : {&pauseButton,&resetButton,&openFileButton,&liveButton}) {
        b->setColour(juce::TextButton::buttonColourId,juce::Colour(0xff26364e));
        b->setColour(juce::TextButton::textColourOffId,juce::Colour(primary));
    }
    openFileButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff375e98));
    openFileButton.setEnabled(true);
    pauseButton.setClickingTogglesState(true);
    pauseButton.onClick=[this] { processor.setPaused(pauseButton.getToggleState());
        pauseButton.setButtonText(pauseButton.getToggleState()?"Resume":"Pause"); };
    resetButton.onClick=[this] { if(processor.isOfflineMode()) processor.returnToLive();
        else processor.requestReset(); timerCallback(); };
    openFileButton.onClick=[this] {
        fileChooser=std::make_unique<juce::FileChooser>("Analyze an audio file",juce::File{},
                                  "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        juce::Component::SafePointer<TonicRecreationAudioProcessorEditor> safe(this);
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode |
                                juce::FileBrowserComponent::canSelectFiles,
            [safe](const juce::FileChooser& chooser) {
                if(safe != nullptr) safe->loadFile(chooser.getResult());
            });
    };
    liveButton.onClick=[this] { processor.returnToLive();
        pauseButton.setToggleState(false,juce::dontSendNotification);
        pauseButton.setButtonText("Pause"); timerCallback(); };
    startTimerHz(6); timerCallback();
}
TonicRecreationAudioProcessorEditor::~TonicRecreationAudioProcessorEditor(){ stopTimer(); }

void TonicRecreationAudioProcessorEditor::timerCallback() {
    const bool offline=processor.isOfflineMode();
    const int root=processor.getDetectedRoot(),mode=processor.getDetectedMode();
    keyLabel.setText(root<0 ? (offline && !processor.isFileBusy()?"No key found":"Listening...")
       : juce::String(notes[root])+(mode==0?" major":" minor"),juce::dontSendNotification);
    const int confidence=juce::roundToInt(processor.getConfidence()*100.0f);
    keyHint.setText(root<0?"Waiting for musical input":
        "Key confidence  " + juce::String(confidence) + "%", juce::dontSendNotification);
    juce::String keys;
    for(int i=0;i<3;++i) {
        const int r=processor.getRankedRoot(i), m=processor.getRankedMode(i);
        if(r<0) continue;
        if(keys.isNotEmpty()) keys+="  /  ";
        keys+=juce::String(notes[r])+(m==0?" maj":" min");
    }
    candidates.setText(keys.isEmpty()?"Likely keys will appear here":keys,juce::dontSendNotification);
    const int cr=processor.getChordRoot(),kind=processor.getChordKind();
    chord.setText(cr<0||kind<0?"Chord  --":
        "Chord  " + juce::String(notes[cr])+" "+chordKinds[juce::jlimit(0,3,kind)],
        juce::dontSendNotification);
    const float detected=processor.getDetectedBpm();
    const float bConf=processor.getBpmConfidence();
    bpm.setText(detected>0?juce::String(detected,1):"--",juce::dontSendNotification);
    bpmHint.setText(detected>0?"Confidence "+juce::String(juce::roundToInt(bConf*100.0f))+"%":"Waiting for rhythm",
        juce::dontSendNotification);
    const float hz=processor.getDetectedTuningHz(),tConf=processor.getTuningConfidence();
    const bool valid=tConf>=0.03f && std::isfinite(hz) && hz>0;
    referenceA.setText(valid?juce::String(hz,1):"--",juce::dontSendNotification);
    const float cents=valid?1200.0f*std::log2(hz/440.0f):0.0f;
    referenceHint.setText(valid ? (juce::String(cents>=0?"+":"")+juce::String(cents,1)+" cents"):
        "Detecting tuning",juce::dontSendNotification);
    juce::String status;
    if(offline){
        status=processor.getFileName();
        if(processor.fileFailed())status+="  -  Cannot decode";
        else if(processor.isFileBusy())
            status+="  -  "+juce::String(juce::roundToInt(100.0f*processor.getFileProgress()))+"%";
        else status+="  -  Complete";
    } else status=processor.hasLiveInput()?"Live input active":"Drop audio anywhere to analyze";
    fileStatus.setTooltip(status);
    if(status.length()>34)status=status.substring(0,31)+"...";
    fileStatus.setText(status,juce::dontSendNotification);
    if(offline && processor.isFileBusy())repaint();
}

void TonicRecreationAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(juce::Colour(bg));
    auto box=[&](float x,float y,float w,float h,float radius=11.0f){
        juce::Rectangle<float> r(x,y,w,h);
        g.setColour(juce::Colour(card));g.fillRoundedRectangle(r,radius);
        g.setColour(juce::Colour(border));g.drawRoundedRectangle(r,radius,1.0f);
    };
    g.setColour(juce::Colour(blue)); g.fillRoundedRectangle(15.0f,18.0f,3.0f,22.0f,1.5f);
    g.setColour(juce::Colour(border));g.drawLine(14,50,556,50,1.0f);
    box(14,59,542,133);
    g.setColour(juce::Colour(muted));g.setFont(juce::Font(10.0f,juce::Font::bold));
    g.drawText("DETECTED KEY",juce::Rectangle<int>(24,68,522,16),juce::Justification::centred);
    g.setColour(juce::Colour(border));g.drawLine(30,153,540,153,1.0f);
    box(14,201,264,75);box(286,201,270,75);
    g.setColour(juce::Colour(muted));g.setFont(juce::Font(10.0f,juce::Font::bold));
    g.drawText("TEMPO",juce::Rectangle<int>(24,206,244,15),juce::Justification::centred);
    g.drawText("REFERENCE A",juce::Rectangle<int>(296,206,250,15),juce::Justification::centred);
    g.setColour(juce::Colour(blue));
    g.fillRoundedRectangle(juce::Rectangle<float>(23,211,3,17),1.5f);
    g.fillRoundedRectangle(juce::Rectangle<float>(295,211,3,17),1.5f);
    box(14,285,542,43,9.0f);
    if(processor.isOfflineMode() && processor.isFileBusy()) {
        g.setColour(juce::Colour(blue));
        g.fillRoundedRectangle(juce::Rectangle<float>(17,326,
            536*juce::jlimit(0.0f,1.0f,processor.getFileProgress()),2),1);
    }
    if(dragHover){
        g.setColour(juce::Colour(0xed14243d));
        g.fillRoundedRectangle(juce::Rectangle<float>(7,7,556,326),13);
        g.setColour(juce::Colour(blue));
        g.drawRoundedRectangle(juce::Rectangle<float>(9,9,552,322),13,2);
        g.setColour(juce::Colour(primary));g.setFont(juce::Font(22.0f,juce::Font::bold));
        g.drawText("DROP AUDIO TO ANALYZE",getLocalBounds(),juce::Justification::centred);
    }
}
void TonicRecreationAudioProcessorEditor::resized(){
    brand.setBounds(25,12,300,32);
    pauseButton.setBounds(408,15,68,25);resetButton.setBounds(482,15,74,25);
    keyLabel.setBounds(32,88,506,45);keyHint.setBounds(35,131,500,18);
    candidates.setBounds(23,157,520,17);chord.setBounds(22,176,526,15);
    bpm.setBounds(30,223,230,31);bpmHint.setBounds(30,252,230,16);
    referenceA.setBounds(302,223,236,31);referenceHint.setBounds(302,252,236,16);
    fileStatus.setBounds(24,294,258,24);
    openFileButton.setBounds(297,294,119,25);
    liveButton.setBounds(423,294,123,25);
}
bool TonicRecreationAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files){
    for(const auto& file:files)if(isAudioFile(file))return true;
    return false;
}
void TonicRecreationAudioProcessorEditor::fileDragEnter(const juce::StringArray&,int,int){dragHover=true;repaint();}
void TonicRecreationAudioProcessorEditor::fileDragExit(const juce::StringArray&){dragHover=false;repaint();}
void TonicRecreationAudioProcessorEditor::filesDropped(const juce::StringArray& files,int,int){
    dragHover=false;repaint();
    for(const auto& f:files)if(isAudioFile(f)){loadFile(juce::File(f));break;}
}
void TonicRecreationAudioProcessorEditor::loadFile(const juce::File& f){
    if(!f.existsAsFile())return;
    if(processor.requestFileAnalysis(f)){
        pauseButton.setToggleState(false,juce::dontSendNotification);
        pauseButton.setButtonText("Pause");timerCallback();
    }
}
