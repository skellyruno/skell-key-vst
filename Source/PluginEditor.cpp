#include "PluginEditor.h"
#include <array>
#include <cmath>

// ---------------------------------------------------------------------------
//  Skell Key -- skeleton-key identity.
//  Warm "ossuary" black, bone-white type, a single ecto-green accent. The key
//  mark has a skull for a bow, and the 12 pitch classes are drawn as a spine of
//  vertebrae that rises and falls with the music.
// ---------------------------------------------------------------------------
namespace {
const juce::Colour kBg      (0xff0b0a09);   // warm black
const juce::Colour kPanel   (0xff131210);
const juce::Colour kPanelHi (0xff1b1915);
const juce::Colour kLine    (0xff2f2b23);
const juce::Colour kBone    (0xffe9e1cc);   // ivory bone
const juce::Colour kBoneDim (0xff9b937d);
const juce::Colour kBoneFaint(0xff4d483c);
const juce::Colour kAccent  (0xffb6f56a);   // ecto green

const char* const sharpNames[12] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
const char* const flatNames [12] = {"C","Db","D","Eb","E","F","Gb","G","Ab","A","Bb","B"};
const char* const chordKinds[] = {"maj","min","dim","aug","7","maj7","min7","m7b5"};

juce::Font makeFont(float size, bool bold = false, float kerning = 0.0f) {
    juce::Font f(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
    return kerning != 0.0f ? f.withExtraKerningFactor(kerning) : f;
}
float textWidth(const juce::Font& f, const juce::String& s) {
    juce::GlyphArrangement ga;
    ga.addLineOfText(f, s, 0.0f, 0.0f);
    return ga.getBoundingBox(0, -1, true).getWidth();
}
void drawText(juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, float size,
              bool bold, juce::Colour colour,
              juce::Justification j = juce::Justification::centredLeft, float kerning = 0.0f) {
    g.setColour(colour);
    g.setFont(makeFont(size, bold, kerning));
    g.drawText(text, r, j, true);
}
bool isAudioFile(const juce::String& path) {
    const auto ext = juce::File(path).getFileExtension().toLowerCase();
    return juce::StringArray {".wav",".aiff",".aif",".flac",".ogg",".mp3"}.contains(ext);
}

// A card with corner "bone brackets" instead of a full, heavy frame.
void drawCard(juce::Graphics& g, juce::Rectangle<float> r) {
    g.setColour(kPanel);  g.fillRect(r);
    g.setColour(kLine);   g.drawRect(r, 1.0f);
    const float L = 9.0f;
    g.setColour(kBoneDim.withAlpha(0.75f));
    const auto a = r.getTopLeft(), b = r.getTopRight(), c = r.getBottomLeft(), d = r.getBottomRight();
    juce::Path p;
    p.startNewSubPath(a.x, a.y + L); p.lineTo(a.x, a.y); p.lineTo(a.x + L, a.y);
    p.startNewSubPath(b.x - L, b.y); p.lineTo(b.x, b.y); p.lineTo(b.x, b.y + L);
    p.startNewSubPath(c.x, c.y - L); p.lineTo(c.x, c.y); p.lineTo(c.x + L, c.y);
    p.startNewSubPath(d.x - L, d.y); p.lineTo(d.x, d.y); p.lineTo(d.x, d.y - L);
    g.strokePath(p, juce::PathStrokeType(1.4f));
}

// Skull-bowed skeleton key, about 74 x 30 units.
void drawKeyMark(juce::Graphics& g, float x, float y, juce::Colour bone, juce::Colour hole) {
    g.setColour(bone);
    g.fillEllipse(x + 1.0f, y + 0.0f, 25.0f, 23.0f);                     // cranium
    g.fillRoundedRectangle(x + 6.5f, y + 17.0f, 14.0f, 11.0f, 3.0f);     // jaw
    g.fillRoundedRectangle(x + 25.0f, y + 10.5f, 48.0f, 4.2f, 1.6f);     // shaft
    g.fillRoundedRectangle(x + 33.0f, y + 7.0f, 3.2f, 11.5f, 1.2f);      // collar
    g.fillRect(x + 56.0f, y + 14.0f, 4.2f, 9.0f);                        // bit teeth
    g.fillRect(x + 63.0f, y + 14.0f, 4.2f, 6.0f);
    g.fillRect(x + 69.5f, y + 14.0f, 3.5f, 10.0f);
    g.setColour(hole);
    g.fillEllipse(x + 6.2f, y + 8.4f, 6.4f, 6.8f);                       // eye sockets
    g.fillEllipse(x + 14.6f, y + 8.4f, 6.4f, 6.8f);
    juce::Path nose; nose.addTriangle(x + 13.5f, y + 15.0f, x + 11.6f, y + 19.0f, x + 15.4f, y + 19.0f);
    g.fillPath(nose);
    for (int i = 0; i < 3; ++i)                                           // teeth gaps
        g.fillRect(x + 9.6f + 3.4f * (float) i, y + 22.0f, 1.0f, 5.0f);
}

// Row of key-bit teeth used as a meter. Filled teeth show the value.
void drawTeeth(juce::Graphics& g, juce::Rectangle<float> r, float value, int count = 10) {
    static const float heights[10] = {0.55f, 0.85f, 0.65f, 1.0f, 0.5f, 0.9f, 0.7f, 1.0f, 0.6f, 0.8f};
    const float slot = r.getWidth() / (float) count, w = slot * 0.62f;
    const int lit = juce::jlimit(0, count, juce::roundToInt(value * (float) count));
    for (int i = 0; i < count; ++i) {
        const float h = r.getHeight() * heights[i % 10];
        const juce::Rectangle<float> t(r.getX() + slot * (float) i, r.getBottom() - h, w, h);
        g.setColour(i < lit ? kAccent : kBoneFaint.withAlpha(0.8f));
        g.fillRect(t);
    }
}

bool inScale(int pc, int root, int mode) {
    static const bool maj[12] = {1,0,1,0,1,1,0,1,0,1,0,1};
    static const bool min[12] = {1,0,1,1,0,1,0,1,1,0,1,0};
    const int rel = ((pc - root) % 12 + 12) % 12;
    return mode == 0 ? maj[rel] : min[rel];
}
} // namespace

// Flat, squared bone buttons.
class SkellLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    SkellLookAndFeel() {
        setColour(juce::TextButton::buttonColourId, kPanelHi);
        setColour(juce::TextButton::textColourOffId, kBone);
        setColour(juce::TextButton::textColourOnId, kBone);
    }
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                              bool highlighted, bool down) override {
        auto r = button.getLocalBounds().toFloat().reduced(0.5f);
        auto fill = kPanelHi;
        if (down) fill = juce::Colour(0xff3a4a22);
        else if (highlighted) fill = juce::Colour(0xff26231c);
        g.setColour(fill); g.fillRect(r);
        const bool focus = button.hasKeyboardFocus(true);
        g.setColour(focus ? kAccent : (highlighted ? kBoneDim : kLine));
        g.drawRect(r, 1.0f);
        if (button.getToggleState()) {
            g.setColour(kAccent);
            g.fillRect(r.getX(), r.getY(), 2.5f, r.getHeight());
        }
    }
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override {
        return makeFont(juce::jmin(12.5f, (float) buttonHeight * 0.46f), true, 0.06f);
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override {
        g.setColour(button.isEnabled() ? kBone : kBoneDim);
        g.setFont(getTextButtonFont(button, button.getHeight()));
        g.drawFittedText(button.getButtonText().toUpperCase(), button.getLocalBounds().reduced(4, 1),
                         juce::Justification::centred, 1, 0.85f);
    }
};

TonicRecreationAudioProcessorEditor::TonicRecreationAudioProcessorEditor(TonicRecreationAudioProcessor& p)
  : AudioProcessorEditor(&p), processor(p) {
    setTitle("Skell Key");
    setSize(designWidth, designHeight);
    setResizable(true, true);
    setResizeLimits(designWidth * 4 / 5, designHeight * 4 / 5, designWidth * 2, designHeight * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio((double) designWidth / (double) designHeight);

    look = std::make_unique<SkellLookAndFeel>();
    for (auto* b : { &pauseButton, &resetButton, &openFileButton, &liveButton }) {
        b->setLookAndFeel(look.get());
        addAndMakeVisible(b);
    }
    pauseButton.setClickingTogglesState(true);
    pauseButton.setTooltip("Freeze analysis of live input");
    resetButton.setTooltip("Forget everything heard so far and start again");
    openFileButton.setTooltip("Analyze an audio file. You can also drop one anywhere on this window.");
    liveButton.setTooltip("Leave file mode and analyze the DAW input again");

    pauseButton.onClick = [this] {
        processor.setPaused(pauseButton.getToggleState());
        pauseButton.setButtonText(pauseButton.getToggleState() ? "Resume" : "Pause");
    };
    resetButton.onClick = [this] {
        if (processor.isOfflineMode()) processor.returnToLive(); else processor.requestReset();
        timerCallback();
    };
    openFileButton.onClick = [this] {
        fileChooser = std::make_unique<juce::FileChooser>("Analyze an audio file", juce::File{},
                                                          "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
        juce::Component::SafePointer<TonicRecreationAudioProcessorEditor> safe(this);
        fileChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe](const juce::FileChooser& chooser) { if (safe != nullptr) safe->loadFile(chooser.getResult()); });
    };
    liveButton.onClick = [this] {
        processor.returnToLive();
        pauseButton.setToggleState(false, juce::dontSendNotification);
        pauseButton.setButtonText("Pause");
        timerCallback();
    };
    startTimerHz(20);
    timerCallback();
}

TonicRecreationAudioProcessorEditor::~TonicRecreationAudioProcessorEditor() {
    stopTimer();
    for (auto* b : { &pauseButton, &resetButton, &openFileButton, &liveButton }) b->setLookAndFeel(nullptr);
    look.reset();
}

void TonicRecreationAudioProcessorEditor::timerCallback() {
    const bool offline = processor.isOfflineMode();
    const bool startingFile = offline && processor.isFileBusy() && processor.getFileAnalyzedSeconds() < 0.25f;
    const bool flats = processor.getState().getRawParameterValue("preferFlats")->load() > 0.5f;
    const char* const* names = flats ? flatNames : sharpNames;

    const int root = startingFile ? -1 : processor.getDetectedRoot(), mode = processor.getDetectedMode();
    view.root = root; view.mode = mode;
    if (root < 0) {
        view.keyText = startingFile ? "Analyzing..."
                     : (offline && !processor.isFileBusy() ? "No key found" : "Listening...");
        view.keyHint = "Waiting for musical input";
        view.keyConfidence = 0.0f;
    } else {
        view.keyText = juce::String(names[root]) + (mode == 0 ? " major" : " minor");
        view.keyConfidence = juce::jlimit(0.0f, 1.0f, processor.getConfidence());
        view.keyHint = "Key evidence  " + juce::String(juce::roundToInt(view.keyConfidence * 100.0f)) + "%";
    }
    juce::String also;
    int shown = 0;
    for (int i = 0; i < 3 && shown < 2 && !startingFile && root >= 0; ++i) {
        const int r = processor.getRankedRoot(i), m = processor.getRankedMode(i);
        if (r < 0 || (r == root && m == mode)) continue;
        if (also.isNotEmpty()) also += "   /   ";
        also += juce::String(names[r]) + (m == 0 ? " maj" : " min");
        ++shown;
    }
    view.candidates = also.isEmpty() ? "Runner-up keys appear here" : "Also likely   " + also;

    const int newRoot = startingFile ? -1 : processor.getTranscribedChordRoot();
    const int newQuality = processor.getTranscribedChordQuality();
    const int cr = (newRoot >= 0) ? newRoot : (startingFile ? -1 : processor.getChordRoot());
    const int kind = (newRoot >= 0) ? newQuality : processor.getChordKind();
    const int bass = (newRoot >= 0) ? processor.getTranscribedChordBass() : -1;
    view.chordText = "Chord   --";
    if (cr >= 0 && cr < 12 && kind >= 0 && kind < 8) {
        view.chordText = "Chord   " + juce::String(names[cr]) + " " + chordKinds[kind];
        if (bass >= 0 && bass < 12 && bass != cr) view.chordText += "/" + juce::String(names[bass]);
    }

    const float bpmValue = startingFile ? 0.0f : processor.getDetectedBpm();
    view.bpmConfidence = startingFile ? 0.0f : juce::jlimit(0.0f, 1.0f, processor.getBpmConfidence());
    view.bpmText = bpmValue > 0 ? juce::String(bpmValue, 1) : "--";
    view.bpmHint = bpmValue > 0 ? "Confidence  " + juce::String(juce::roundToInt(view.bpmConfidence * 100.0f)) + "%"
                                : "Waiting for rhythm";

    const float hz = processor.getDetectedTuningHz();
    const float tConf = startingFile ? 0.0f : processor.getTuningConfidence();
    view.tuningValid = tConf >= 0.03f && std::isfinite(hz) && hz > 0.0f;
    view.cents = view.tuningValid ? 1200.0f * std::log2(hz / 440.0f) : 0.0f;
    view.refText = view.tuningValid ? juce::String(hz, 1) : "--";
    view.refHint = view.tuningValid ? juce::String(view.cents >= 0 ? "+" : "") + juce::String(view.cents, 1) + " cents"
                                    : "Detecting tuning";

    juce::String status;
    view.busy = offline && processor.isFileBusy();
    view.progress = juce::jlimit(0.0f, 1.0f, processor.getFileProgress());
    if (offline) {
        status = processor.getFileName();
        if (processor.fileFailed()) status += "  -  Cannot decode";
        else if (processor.isFileBusy()) status += "  -  " + juce::String(juce::roundToInt(100.0f * view.progress)) + "%";
        else status += "  -  Complete";
    } else {
        status = processor.hasLiveInput() ? "Live input active" : "Drop audio anywhere to analyze";
    }
    view.status = status;

    // Spine: ease toward the live short-term chroma so vertebrae move smoothly.
    float peak = 1.0e-6f;
    for (int i = 0; i < 12; ++i) peak = juce::jmax(peak, processor.getChroma(i));
    for (int i = 0; i < 12; ++i) {
        chromaTarget[(size_t) i] = (root < 0 && !offline) ? 0.0f : processor.getChroma(i) / peak;
        chromaShown[(size_t) i] += (chromaTarget[(size_t) i] - chromaShown[(size_t) i]) * 0.35f;
    }
    tonicPulse = juce::jmax(0.0f, tonicPulse - 0.08f);
    if (root != lastRoot) { tonicPulse = 1.0f; lastRoot = root; }
    repaint();
}

// --------------------------------------------------------------------------- painting
void TonicRecreationAudioProcessorEditor::paint(juce::Graphics& g) {
    g.fillAll(kBg);
    g.addTransform(juce::AffineTransform::scale(scale()));
    paintHeader(g);
    paintKeyCard(g);
    paintTempoCard(g);
    paintReferenceCard(g);
    paintFooter(g);

    if (dragHover) {
        const juce::Rectangle<float> all(0.0f, 0.0f, (float) designWidth, (float) designHeight);
        g.setColour(kBg.withAlpha(0.93f)); g.fillRect(all);
        g.setColour(kAccent);
        const juce::Rectangle<float> frame = all.reduced(10.0f);
        juce::Path dashes; dashes.addRectangle(frame);
        juce::Path stroked;
        const float pattern[] = {9.0f, 6.0f};
        juce::PathStrokeType(2.0f).createDashedStroke(stroked, dashes, pattern, 2);
        g.fillPath(stroked);
        drawKeyMark(g, all.getCentreX() - 37.0f, 128.0f, kBone, kBg);
        drawText(g, "DROP AUDIO TO UNLOCK", all.withTrimmedTop(140.0f).withHeight(70.0f), 26.0f, true, kBone,
                 juce::Justification::centred, 0.12f);
        drawText(g, "wav  aiff  flac  ogg  mp3", all.withTrimmedTop(212.0f).withHeight(24.0f), 12.0f, false, kBoneDim,
                 juce::Justification::centred, 0.1f);
    }
}

void TonicRecreationAudioProcessorEditor::paintHeader(juce::Graphics& g) {
    drawKeyMark(g, 16.0f, 11.0f, kBone, kBg);
    drawText(g, "SKELL KEY", { 100.0f, 9.0f, 220.0f, 26.0f }, 21.0f, true, kBone,
             juce::Justification::centredLeft, 0.16f);
    drawText(g, "KEY   /   TEMPO   /   TUNING", { 101.0f, 33.0f, 240.0f, 14.0f }, 9.0f, false, kBoneDim,
             juce::Justification::centredLeft, 0.18f);
    // A spine as the rule under the header: a few lit vertebrae, then dim ones.
    const float y = 55.0f;
    const int n = 48;
    const float slot = (float) (designWidth - 28) / (float) n;
    for (int i = 0; i < n; ++i) {
        const bool lit = i < 6;
        g.setColour(lit ? kAccent : kBoneFaint.withAlpha(0.9f));
        g.fillRoundedRectangle(14.0f + slot * (float) i, y, slot - 2.2f, lit ? 3.0f : 2.0f, 1.0f);
    }
}

void TonicRecreationAudioProcessorEditor::paintKeyCard(juce::Graphics& g) {
    const juce::Rectangle<float> card(14.0f, 66.0f, 572.0f, 160.0f);
    drawCard(g, card);
    drawText(g, "DETECTED KEY", { 30.0f, 76.0f, 200.0f, 14.0f }, 9.0f, true, kBoneDim,
             juce::Justification::centredLeft, 0.2f);

    // Big key: tonic in bone, mode in dim bone, so the pitch reads first.
    if (view.root >= 0) {
        const bool flats = processor.getState().getRawParameterValue("preferFlats")->load() > 0.5f;
        const juce::String tonic = (flats ? flatNames : sharpNames)[view.root];
        const auto tonicFont = makeFont(60.0f, true);
        drawText(g, tonic, { 28.0f, 90.0f, 200.0f, 70.0f }, 60.0f, true, kBone);
        const float w = textWidth(tonicFont, tonic);
        drawText(g, view.mode == 0 ? "major" : "minor", { 28.0f + w + 10.0f, 117.0f, 140.0f, 34.0f }, 24.0f,
                 false, kAccent);
    } else {
        drawText(g, view.keyText, { 28.0f, 96.0f, 270.0f, 56.0f }, 32.0f, true, kBoneDim);
    }
    drawText(g, view.keyHint, { 30.0f, 158.0f, 170.0f, 14.0f }, 11.0f, false, kBoneDim);
    drawTeeth(g, { 30.0f, 175.0f, 140.0f, 11.0f }, view.keyConfidence);
    drawText(g, view.candidates, { 30.0f, 193.0f, 270.0f, 14.0f }, 11.0f, false, kBoneDim);
    drawText(g, view.chordText, { 30.0f, 208.0f, 270.0f, 14.0f }, 11.0f, true, kBone);

    // Divider between the readout and the spine.
    g.setColour(kLine);
    g.drawLine(310.0f, 78.0f, 310.0f, 214.0f, 1.0f);
    paintSpine(g, { 326.0f, 76.0f, 248.0f, 146.0f });
}

void TonicRecreationAudioProcessorEditor::paintSpine(juce::Graphics& g, juce::Rectangle<float> area) {
    const float slot = area.getWidth() / 12.0f, w = slot - 4.5f;
    const float baseY = area.getBottom() - 20.0f, maxH = baseY - area.getY() - 6.0f;
    const bool flats = processor.getState().getRawParameterValue("preferFlats")->load() > 0.5f;
    const auto* names = flats ? flatNames : sharpNames;

    g.setColour(kBoneFaint);
    g.fillRect(area.getX(), baseY + 2.0f, area.getWidth(), 1.0f);
    for (int pc = 0; pc < 12; ++pc) {
        const float x = area.getX() + slot * (float) pc + 1.2f;
        const bool tonic = view.root >= 0 && pc == view.root;
        const bool diatonic = view.root >= 0 && inScale(pc, view.root, view.mode);
        const float value = juce::jlimit(0.0f, 1.0f, chromaShown[(size_t) pc]);
        const float h = 5.0f + (maxH - 5.0f) * std::pow(value, 0.85f);

        // Ghost of the vertebra's full height so the spine reads as a structure.
        g.setColour(kPanelHi);
        g.fillRoundedRectangle(x, baseY - maxH, w, maxH, 3.0f);

        juce::Colour c = tonic ? kAccent : (diatonic ? kBone.withAlpha(0.88f) : kBoneFaint);
        if (view.root < 0) c = kBoneFaint;
        if (tonic && tonicPulse > 0.0f) {
            g.setColour(kAccent.withAlpha(0.22f * tonicPulse));
            g.fillRoundedRectangle(x - 3.0f, baseY - h - 3.0f, w + 6.0f, h + 6.0f, 5.0f);
        }
        g.setColour(c);
        g.fillRoundedRectangle(x, baseY - h, w, h, 3.0f);
        // Intervertebral groove and knobs give each bar a bone silhouette.
        if (h > 14.0f) {
            g.setColour(kBg.withAlpha(0.55f));
            g.fillRect(x + 2.0f, baseY - h * 0.5f, w - 4.0f, 1.2f);
        }
        g.setColour(c);
        g.fillEllipse(x - 0.8f, baseY - h - 1.6f, 4.2f, 4.2f);
        g.fillEllipse(x + w - 3.4f, baseY - h - 1.6f, 4.2f, 4.2f);

        drawText(g, names[pc], { area.getX() + slot * (float) pc - 3.0f, baseY + 5.0f, slot + 6.0f, 13.0f }, 9.5f,
                 tonic, tonic ? kAccent : (diatonic ? kBone : kBoneDim), juce::Justification::centred);
    }
}

void TonicRecreationAudioProcessorEditor::paintTempoCard(juce::Graphics& g) {
    const juce::Rectangle<float> r(14.0f, 236.0f, 282.0f, 80.0f);
    drawCard(g, r);
    drawText(g, "TEMPO", { r.getX() + 16.0f, r.getY() + 9.0f, 100.0f, 14.0f }, 9.0f, true, kBoneDim,
             juce::Justification::centredLeft, 0.2f);
    const bool have = view.bpmText != "--";
    drawText(g, view.bpmText, { r.getX() + 16.0f, r.getY() + 21.0f, 150.0f, 40.0f }, 34.0f, true,
             have ? kAccent : kBoneDim);
    if (have) {
        const float w = textWidth(makeFont(34.0f, true), view.bpmText);
        drawText(g, "BPM", { r.getX() + 16.0f + w + 6.0f, r.getY() + 36.0f, 50.0f, 16.0f }, 10.0f, true, kBoneDim,
                 juce::Justification::centredLeft, 0.12f);
    }
    drawText(g, view.bpmHint, { r.getX() + 16.0f, r.getY() + 60.0f, 160.0f, 14.0f }, 10.5f, false, kBoneDim);
    drawTeeth(g, { r.getRight() - 96.0f, r.getY() + 14.0f, 80.0f, 11.0f }, view.bpmConfidence, 8);
}

void TonicRecreationAudioProcessorEditor::paintReferenceCard(juce::Graphics& g) {
    const juce::Rectangle<float> r(304.0f, 236.0f, 282.0f, 80.0f);
    drawCard(g, r);
    drawText(g, "REFERENCE A", { r.getX() + 16.0f, r.getY() + 9.0f, 120.0f, 14.0f }, 9.0f, true, kBoneDim,
             juce::Justification::centredLeft, 0.2f);
    drawText(g, view.refText, { r.getX() + 16.0f, r.getY() + 21.0f, 130.0f, 40.0f }, 34.0f, true,
             view.tuningValid ? kAccent : kBoneDim);
    if (view.tuningValid) {
        const float w = textWidth(makeFont(34.0f, true), view.refText);
        drawText(g, "Hz", { r.getX() + 16.0f + w + 6.0f, r.getY() + 36.0f, 30.0f, 16.0f }, 10.0f, true, kBoneDim);
    }
    drawText(g, view.refHint, { r.getX() + 16.0f, r.getY() + 60.0f, 130.0f, 14.0f }, 10.5f, false, kBoneDim);

    // Cents gauge, -50 .. +50, with a pointer. Bone ticks, accent pointer.
    const float gx = r.getX() + 156.0f, gw = 108.0f, gy = r.getY() + 44.0f;
    g.setColour(kBoneFaint);
    g.fillRect(gx, gy, gw, 1.0f);
    for (int t = -5; t <= 5; ++t) {
        const float x = gx + gw * (float) (t + 5) / 10.0f;
        const float th = (t == 0) ? 10.0f : (t % 5 == 0 ? 7.0f : 4.0f);
        g.setColour(t == 0 ? kBone : kBoneDim);
        g.fillRect(x - 0.5f, gy - th, 1.0f, th);
    }
    drawText(g, "-50", { gx - 14.0f, gy + 3.0f, 28.0f, 11.0f }, 8.5f, false, kBoneDim, juce::Justification::centred);
    drawText(g, "0", { gx + gw * 0.5f - 14.0f, gy + 3.0f, 28.0f, 11.0f }, 8.5f, false, kBoneDim, juce::Justification::centred);
    drawText(g, "+50", { gx + gw - 14.0f, gy + 3.0f, 28.0f, 11.0f }, 8.5f, false, kBoneDim, juce::Justification::centred);
    if (view.tuningValid) {
        const float x = gx + gw * (juce::jlimit(-50.0f, 50.0f, view.cents) + 50.0f) / 100.0f;
        juce::Path tri; tri.addTriangle(x, gy + 1.0f, x - 4.5f, gy - 11.0f, x + 4.5f, gy - 11.0f);
        g.setColour(kAccent); g.fillPath(tri);
    }
}

void TonicRecreationAudioProcessorEditor::paintFooter(juce::Graphics& g) {
    const juce::Rectangle<float> r(14.0f, 326.0f, 572.0f, 40.0f);
    drawCard(g, r);
    if (view.busy) {
        g.setColour(kAccent);
        g.fillRect(r.getX() + 1.0f, r.getY() + 1.0f, (r.getWidth() - 2.0f) * view.progress, 2.0f);
    }
    drawText(g, view.status, { r.getX() + 16.0f, r.getY() + 4.0f, 360.0f, r.getHeight() - 8.0f }, 11.0f, false,
             processor.fileFailed() ? juce::Colour(0xffe58a6b) : kBoneDim);
}

void TonicRecreationAudioProcessorEditor::resized() {
    const float s = scale();
    auto fit = [s](float x, float y, float w, float h) {
        return juce::Rectangle<int>(juce::roundToInt(x * s), juce::roundToInt(y * s),
                                    juce::roundToInt(w * s), juce::roundToInt(h * s));
    };
    pauseButton.setBounds(fit(420.0f, 14.0f, 80.0f, 28.0f));
    resetButton.setBounds(fit(506.0f, 14.0f, 80.0f, 28.0f));
    openFileButton.setBounds(fit(402.0f, 332.0f, 88.0f, 28.0f));
    liveButton.setBounds(fit(496.0f, 332.0f, 84.0f, 28.0f));
}

// --------------------------------------------------------------------------- file drops
bool TonicRecreationAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files) {
    for (const auto& file : files) if (isAudioFile(file)) return true;
    return false;
}
void TonicRecreationAudioProcessorEditor::fileDragEnter(const juce::StringArray&, int, int) {
    dragHover = true;
    for (auto* b : { &pauseButton, &resetButton, &openFileButton, &liveButton }) b->setVisible(false);
    repaint();
}
void TonicRecreationAudioProcessorEditor::fileDragExit(const juce::StringArray&) {
    dragHover = false;
    for (auto* b : { &pauseButton, &resetButton, &openFileButton, &liveButton }) b->setVisible(true);
    repaint();
}
void TonicRecreationAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int) {
    fileDragExit({});
    for (const auto& f : files) if (isAudioFile(f)) { loadFile(juce::File(f)); break; }
}
void TonicRecreationAudioProcessorEditor::loadFile(const juce::File& f) {
    if (!f.existsAsFile()) return;
    if (processor.requestFileAnalysis(f)) {
        pauseButton.setToggleState(false, juce::dontSendNotification);
        pauseButton.setButtonText("Pause");
        timerCallback();
    }
}
