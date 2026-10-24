#include "PluginEditor.h"
#include <array>
#include <cmath>

namespace {
constexpr auto bg = 0xff090e0a;
constexpr auto card = 0xff141e16;
constexpr auto border = 0xff334a36;
constexpr auto primary = 0xfff0ffec;
constexpr auto muted = 0xff9db49b;
constexpr auto neon = 0xffb2ff52;
constexpr auto darkNeon = 0xff283c20;
const char* notes[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
const char* chordKinds[] = {"maj","min","dim","aug","7","maj7","min7","m7b5"};
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


// Flat, squared controls with host-independent hover and focus treatment.
class NeonButtonLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool highlighted, bool down) override {
        auto r = button.getLocalBounds().toFloat().reduced(0.5f);
        auto fill = backgroundColour;
        if (down) fill = juce::Colour(0xff476b33);
        else if (highlighted) fill = fill.brighter(0.18f);
        g.setColour(fill);
        g.fillRect(r);
        g.setColour(button.hasKeyboardFocus(true) ? juce::Colour(neon)
                    : (highlighted ? juce::Colour(0xff87cb4a) : juce::Colour(border)));
        g.drawRect(r, 1.0f);
        if (button.getToggleState()) {
            g.setColour(juce::Colour(neon));
            g.fillRect(r.getX(), r.getY(), 2.0f, r.getHeight());
        }
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool /*highlighted*/, bool /*down*/) override {
        g.setColour(button.isEnabled() ? juce::Colour(primary) : juce::Colour(muted));
        g.setFont(juce::Font(12.0f, juce::Font::bold));
        g.drawFittedText(button.getButtonText(), button.getLocalBounds().reduced(3,1),
                         juce::Justification::centred, 1, 0.8f);
    }
};

TonicRecreationAudioProcessorEditor::TonicRecreationAudioProcessorEditor(TonicRecreationAudioProcessor& p)
  : AudioProcessorEditor(&p), processor(p) {
    setSize(525,313);
    buttonLook = std::make_unique<NeonButtonLookAndFeel>();
    for (auto* button : { &pauseButton, &resetButton, &openFileButton, &liveButton })
        button->setLookAndFeel(buttonLook.get());
    brand.setText("SKELL   /   KEY ANALYZER", juce::dontSendNotification);
    labelStyle(brand,13.5f,primary,true);
    labelStyle(keyLabel,31.0f,primary,true,juce::Justification::centred);
    labelStyle(keyHint,11.0f,muted,false,juce::Justification::centred);
    labelStyle(candidates,11.0f,muted,false,juce::Justification::centred);
    labelStyle(chord,11.0f,primary,false,juce::Justification::centred);
    labelStyle(bpm,23.0f,neon,true,juce::Justification::centred);
    labelStyle(bpmHint,10.0f,muted,false,juce::Justification::centred);
    labelStyle(referenceA,23.0f,neon,true,juce::Justification::centred);
    labelStyle(referenceHint,10.0f,muted,false,juce::Justification::centred);
    labelStyle(fileStatus,11.0f,muted);
    fileStatus.setTooltip("Drop audio anywhere to analyze. File input takes priority over live input.");
    for (auto* label : { &brand, &keyLabel, &keyHint, &candidates, &chord, &bpm, &bpmHint, &referenceA, &referenceHint, &fileStatus })
        label->setInterceptsMouseClicks(false, false);
    for(auto* c : std::array<juce::Component*,14>{&brand,&keyLabel,&keyHint,&candidates,&chord,
        &bpm,&bpmHint,&referenceA,&referenceHint,&fileStatus,&pauseButton,&resetButton,
        &openFileButton,&liveButton}) addAndMakeVisible(c);
    for(auto* b : {&pauseButton,&resetButton,&openFileButton,&liveButton}) {
        b->setColour(juce::TextButton::buttonColourId,juce::Colour(0xff202e21));
        b->setColour(juce::TextButton::textColourOffId,juce::Colour(primary));
    }
    openFileButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff365c2b));
    liveButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff243a24));
    resetButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff28372a));
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
TonicRecreationAudioProcessorEditor::~TonicRecreationAudioProcessorEditor(){
    stopTimer();
    for (auto* button : { &pauseButton, &resetButton, &openFileButton, &liveButton })
        button->setLookAndFeel(nullptr);
    buttonLook.reset();
}

void TonicRecreationAudioProcessorEditor::timerCallback() {
    const bool offline=processor.isOfflineMode();
    const bool startingFile = offline && processor.isFileBusy()
        && processor.getFileAnalyzedSeconds() < 0.25f;
    const int root=startingFile ? -1 : processor.getDetectedRoot(),mode=processor.getDetectedMode();
    keyLabel.setText(root<0 ? (startingFile ? "Analyzing..." : offline && !processor.isFileBusy()?"No key found":"Listening...")
       : juce::String(notes[root])+(mode==0?" major":" minor"),juce::dontSendNotification);
    const int confidence=juce::roundToInt(processor.getConfidence()*100.0f);
    keyHint.setText(root<0?"Waiting for musical input":
        "Key evidence  " + juce::String(confidence) + "%", juce::dontSendNotification);
    juce::String keys;
    for(int i=0;i<3 && !startingFile;++i) {
        const int r=processor.getRankedRoot(i), m=processor.getRankedMode(i);
        if(r<0) continue;
        if(keys.isNotEmpty()) keys+="  /  ";
        keys+=juce::String(notes[r])+(m==0?" maj":" min");
    }
    candidates.setText(keys.isEmpty()?"Likely keys will appear here":keys,juce::dontSendNotification);
    const int newRoot=startingFile ? -1 : processor.getTranscribedChordRoot();
    const int newQuality=processor.getTranscribedChordQuality();
    const int cr=(newRoot>=0) ? newRoot : (startingFile ? -1 : processor.getChordRoot());
    const int kind=(newRoot>=0) ? newQuality : processor.getChordKind();
    const int inversionBass=(newRoot>=0) ? processor.getTranscribedChordBass() : -1;
    juce::String chordText="Chord  --";
    if(cr>=0 && cr<12 && kind>=0 && kind<8){
        chordText="Chord  "+juce::String(notes[cr])+" "+chordKinds[kind];
        if(inversionBass>=0 && inversionBass<12 && inversionBass!=cr)
            chordText+="/"+juce::String(notes[inversionBass]);
    }
    chord.setText(chordText,juce::dontSendNotification);
    const float detected=startingFile ? 0.0f : processor.getDetectedBpm();
    const float bConf=startingFile ? 0.0f : processor.getBpmConfidence();
    bpm.setText(detected>0?juce::String(detected,1):"--",juce::dontSendNotification);
    bpmHint.setText(detected>0?"Confidence "+juce::String(juce::roundToInt(bConf*100.0f))+"%":"Waiting for rhythm",
        juce::dontSendNotification);
    const float hz=processor.getDetectedTuningHz(),tConf=startingFile ? 0.0f : processor.getTuningConfidence();
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
    // Coordinate system is the original compact design; all painting scales to 525 x 313.
    g.addTransform(juce::AffineTransform::scale(525.0f / 570.0f, 313.0f / 340.0f));
    auto box=[&](float x,float y,float w,float h){
        juce::Rectangle<float> r(x,y,w,h);
        g.setColour(juce::Colour(card));g.fillRect(r);
        g.setColour(juce::Colour(border));g.drawRect(r,1.0f);
    };
    g.setColour(juce::Colour(neon)); g.fillRect(15.0f,16.0f,3.0f,25.0f);
    g.setColour(juce::Colour(border));g.drawLine(14,50,556,50,1.0f);
    g.setColour(juce::Colour(neon));g.fillRect(14.0f,49.0f,108.0f,2.0f);
    box(14,59,542,133);
    g.setColour(juce::Colour(darkNeon)); g.fillRect(16.0f,61.0f,3.0f,129.0f);
    g.setColour(juce::Colour(muted));g.setFont(juce::Font(10.0f,juce::Font::bold));
    g.drawText("DETECTED KEY",juce::Rectangle<int>(24,68,522,16),juce::Justification::centred);
    g.setColour(juce::Colour(border));g.drawLine(30,153,540,153,1.0f);
    box(14,201,264,75);box(286,201,270,75);
    g.setColour(juce::Colour(darkNeon));g.fillRect(16.0f,203.0f,3.0f,71.0f);
    g.fillRect(288.0f,203.0f,3.0f,71.0f);
    g.setColour(juce::Colour(muted));g.setFont(juce::Font(10.0f,juce::Font::bold));
    g.drawText("TEMPO",juce::Rectangle<int>(24,206,244,15),juce::Justification::centred);
    g.drawText("REFERENCE A",juce::Rectangle<int>(296,206,250,15),juce::Justification::centred);
    g.setColour(juce::Colour(neon));
    g.fillRect(juce::Rectangle<float>(23,211,3,17));
    g.fillRect(juce::Rectangle<float>(295,211,3,17));
    box(14,285,542,43);
    if(processor.isOfflineMode() && processor.isFileBusy()) {
        g.setColour(juce::Colour(neon));
        g.fillRect(juce::Rectangle<float>(17,326,
            536*juce::jlimit(0.0f,1.0f,processor.getFileProgress()),2));
    }
    if(dragHover){
        g.setColour(juce::Colour(0xee0b170d));
        g.fillRect(juce::Rectangle<float>(7,7,556,326));
        g.setColour(juce::Colour(neon));
        g.drawRect(juce::Rectangle<float>(9,9,552,322),2.0f);
        g.setColour(juce::Colour(primary));g.setFont(juce::Font(22.0f,juce::Font::bold));
        g.drawText("DROP AUDIO TO ANALYZE",juce::Rectangle<int>(0,0,570,340),juce::Justification::centred);
    }
}
void TonicRecreationAudioProcessorEditor::resized(){
    auto fit = [](int x, int y, int w, int h) {
        return juce::Rectangle<int>(juce::roundToInt(x * 525.0f / 570.0f),
            juce::roundToInt(y * 313.0f / 340.0f),
            juce::roundToInt(w * 525.0f / 570.0f),
            juce::roundToInt(h * 313.0f / 340.0f));
    };
    brand.setBounds(fit(25,12,300,32));
    pauseButton.setBounds(fit(408,15,68,25));resetButton.setBounds(fit(482,15,74,25));
    keyLabel.setBounds(fit(32,88,506,45));keyHint.setBounds(fit(35,131,500,18));
    candidates.setBounds(fit(23,157,520,17));chord.setBounds(fit(22,176,526,15));
    bpm.setBounds(fit(30,223,230,31));bpmHint.setBounds(fit(30,252,230,16));
    referenceA.setBounds(fit(302,223,236,31));referenceHint.setBounds(fit(302,252,236,16));
    fileStatus.setBounds(fit(24,294,258,24));
    openFileButton.setBounds(fit(297,294,119,25));
    liveButton.setBounds(fit(423,294,123,25));
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
