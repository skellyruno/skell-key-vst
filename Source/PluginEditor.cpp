#include "PluginEditor.h"
namespace { const char* sharpNames[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
const char* flatNames[]={"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"}; }
TonicRecreationAudioProcessorEditor::TonicRecreationAudioProcessorEditor(TonicRecreationAudioProcessor& p)
: AudioProcessorEditor(&p), processor(p),
  tuningAttachment(p.getState(),"referenceHz",tuning), flatsAttachment(p.getState(),"preferFlats",flats),
  chordsAttachment(p.getState(),"chords",chords) {
    setSize(620,470);
    keyLabel.setJustificationType(juce::Justification::centred); keyLabel.setFont(juce::Font(42.0f,juce::Font::bold));
    tuningLabel.setText("A (Hz)",juce::dontSendNotification);
    modeLabel.setJustificationType(juce::Justification::centred);
    tuning.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); tuning.setTextBoxStyle(juce::Slider::TextBoxBelow,false,90,24);
    flats.setButtonText("Prefer flats"); chords.setButtonText("Chords");
    addAndMakeVisible(keyLabel); addAndMakeVisible(tuningLabel); addAndMakeVisible(tuning);
    addAndMakeVisible(modeLabel); addAndMakeVisible(flats); addAndMakeVisible(chords);
    startTimerHz(8); timerCallback();
}
TonicRecreationAudioProcessorEditor::~TonicRecreationAudioProcessorEditor(){stopTimer();}
void TonicRecreationAudioProcessorEditor::timerCallback(){
    int r=processor.getDetectedRoot(), m=processor.getDetectedMode();
    auto* spell=flats.getToggleState()?flatNames:sharpNames;
    keyLabel.setText(r<0?"Listening…":juce::String(spell[r])+(m==0?" maj":m==1?" min":""),juce::dontSendNotification);
    modeLabel.setText(r<0?"Play audio to detect a key":"Confidence  "+juce::String(processor.getConfidence(),2),juce::dontSendNotification);
    tuningLabel.setText("A (Hz)  "+juce::String(tuning.getValue(),1),juce::dontSendNotification);
}
void TonicRecreationAudioProcessorEditor::paint(juce::Graphics& g){
    g.fillAll(juce::Colour(0xff202126));
    auto bounds=getLocalBounds().toFloat().reduced(18);
    g.setColour(juce::Colour(0xff292b32)); g.fillRoundedRectangle(bounds,8.0f);
    auto circle=bounds.withSizeKeepingCentre(300,300).translated(0,-8);
    g.setColour(juce::Colour(0xff454b5c)); g.drawEllipse(circle,16.0f);
    g.setColour(juce::Colour(0xff9db9ff)); juce::Path arc;
    arc.addCentredArc(circle.getCentreX(), circle.getCentreY(), circle.getWidth()/2-2, circle.getHeight()/2-2, 0.0f, -2.5f, 2.1f, true);
    g.strokePath(arc, juce::PathStrokeType(16.0f));
}
void TonicRecreationAudioProcessorEditor::resized(){
    auto r=getLocalBounds().reduced(28);
    keyLabel.setBounds(r.getCentreX()-130,r.getY()+100,260,70);
    modeLabel.setBounds(r.getCentreX()-130,r.getY()+175,260,30);
    tuningLabel.setBounds(r.getX(),r.getBottom()-88,150,24);
    tuning.setBounds(r.getX(),r.getBottom()-70,150,60);
    flats.setBounds(r.getCentreX()-80,r.getBottom()-38,100,26);
    chords.setBounds(r.getCentreX()+20,r.getBottom()-38,100,26);
}
