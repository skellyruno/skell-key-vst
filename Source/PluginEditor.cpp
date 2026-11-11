#include <algorithm>
#include <functional>
#include <regex>
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
// Colours are theme variables. Each editor applies its own theme before it paints
// (applyTheme is cheap and does nothing when the theme is already current).
juce::Colour kBg, kPanel, kPanelHi, kLine, kBone, kBoneDim, kBoneFaint, kAccent,
             kKeyWhiteDia, kKeyWhiteOther, kKeyBlackDia, kKeyBlackOther, kKeyInk, kOnAccent, kBtnHover, kBtnDown, kWarn;
struct ThemeDef { const char* name; unsigned bg, panel, panelHi, line, bone, boneDim, boneFaint, accent,
                  whiteDia, whiteOther, blackDia, blackOther, keyInk, onAccent, btnHover, btnDown; };
const ThemeDef themes[] = {
    // 0: Bone - the original look (main theme)
    {"Bone",      0xff0b0a09, 0xff131210, 0xff1b1915, 0xff2f2b23, 0xffe9e1cc, 0xff9b937d, 0xff4d483c, 0xffb6f56a,
                  0xffd9d2bd, 0xff8c8573, 0xff4b473b, 0xff211f19, 0xff3a362b, 0xff0b0c08, 0xff26231c, 0xff3a4a22},
    // 1: Neon - indigo night with a cyan accent
    {"Neon",      0xff07060f, 0xff0f0c1c, 0xff191330, 0xff33285c, 0xffeae6ff, 0xff9d95c9, 0xff463e75, 0xff2ff3e0,
                  0xffd6d0ff, 0xff7f78ad, 0xff4a3f88, 0xff1c1636, 0xff2a2252, 0xff04130f, 0xff241b45, 0xff0f5a55},
    // 2: Graveyard - moss and mist with a pumpkin accent
    {"Graveyard", 0xff0a0d0b, 0xff111611, 0xff1a211a, 0xff2c382c, 0xffd7dccb, 0xff8e9a86, 0xff424b3f, 0xffff9a3c,
                  0xffc9d0bc, 0xff7b8473, 0xff3f4a3d, 0xff1c231b, 0xff2f372c, 0xff160c02, 0xff222b22, 0xff6b3f12},
    // 3: Clean - light paper and ink
    {"Clean",     0xfff3f0e8, 0xffeae6da, 0xffdcd7c8, 0xffc2bba8, 0xff1f1d18, 0xff6a6556, 0xffb9b2a0, 0xff3a8a1e,
                  0xfffdfbf4, 0xffcfc9b8, 0xff5b574a, 0xff2a2822, 0xff4a4639, 0xfff7f5ee, 0xffe0dbcb, 0xffb9d6a6},
};
constexpr int numThemes = (int) (sizeof(themes) / sizeof(themes[0]));
int appliedTheme = -1;
void applyTheme(int index) {
    index = juce::jlimit(0, numThemes - 1, index);
    if (index == appliedTheme) return;
    appliedTheme = index;
    const auto& t = themes[index];
    kBg = juce::Colour(t.bg); kPanel = juce::Colour(t.panel); kPanelHi = juce::Colour(t.panelHi); kLine = juce::Colour(t.line);
    kBone = juce::Colour(t.bone); kBoneDim = juce::Colour(t.boneDim); kBoneFaint = juce::Colour(t.boneFaint); kAccent = juce::Colour(t.accent);
    kKeyWhiteDia = juce::Colour(t.whiteDia); kKeyWhiteOther = juce::Colour(t.whiteOther);
    kKeyBlackDia = juce::Colour(t.blackDia); kKeyBlackOther = juce::Colour(t.blackOther);
    kKeyInk = juce::Colour(t.keyInk); kOnAccent = juce::Colour(t.onAccent); kBtnHover = juce::Colour(t.btnHover); kBtnDown = juce::Colour(t.btnDown);
    kWarn = juce::Colour(0xffe58a6b);
}

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

// Transparent button laid over the skeleton-and-piano scene; clicking it plays the key.
class SceneHitButton final : public juce::Button {
public:
    SceneHitButton() : juce::Button("Play detected key") { setMouseCursor(juce::MouseCursor::PointingHandCursor); }
    void paintButton(juce::Graphics&, bool, bool) override {}
    void mouseEnter(const juce::MouseEvent& ev) override { juce::Button::mouseEnter(ev); if (onEnter) onEnter(); }
    void mouseDown(const juce::MouseEvent& ev) override {
        if (ev.mods.isPopupMenu()) { if (onRightClick) onRightClick(); return; }   // right-click: next costume
        if (midiAt) {                                                              // a piano key: play it while held
            const int n = midiAt(ev.getPosition());
            if (n >= 0) { heldNote = n; if (noteCb) noteCb(n, true); return; }
        }
        juce::Button::mouseDown(ev);
    }
    void mouseDrag(const juce::MouseEvent& ev) override {
        if (heldNote >= 0) {
            const int n = midiAt ? midiAt(ev.getPosition()) : -1;
            if (n >= 0 && n != heldNote) { if (noteCb) { noteCb(heldNote, false); noteCb(n, true); } heldNote = n; }
            return;
        }
        juce::Button::mouseDrag(ev);
    }
    void mouseUp(const juce::MouseEvent& ev) override {
        if (heldNote >= 0) { if (noteCb) noteCb(heldNote, false); heldNote = -1; return; }
        juce::Button::mouseUp(ev);
    }
    std::function<void()> onRightClick, onEnter;
    std::function<int(juce::Point<int>)> midiAt;     // MIDI note under a point, or -1 when not on a key
    std::function<void(int, bool)> noteCb;
    int heldNote = -1;
};

// Flat, squared bone buttons.
class SkellLookAndFeel final : public juce::LookAndFeel_V4 {
public:
    const int* themeIndex = nullptr;   // the owning editor's theme
    void sync() const { if (themeIndex != nullptr) applyTheme(*themeIndex); }
    void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                              bool highlighted, bool down) override {
        sync();
        if (button.getComponentID() == "keymark") {                // the skull key logo is the look switcher
            if (highlighted || down) {
                g.setColour(kAccent.withAlpha(down ? 0.28f : 0.16f));
                g.fillRoundedRectangle(button.getLocalBounds().toFloat().reduced(1.0f), 5.0f);
            }
            return;
        }
        auto r = button.getLocalBounds().toFloat().reduced(0.5f);
        auto fill = kPanelHi;
        if (down) fill = kBtnDown;
        else if (highlighted) fill = kBtnHover;
        const bool mini = button.getComponentID() == "mini";
        if (mini && button.getToggleState()) fill = kAccent;
        g.setColour(fill); g.fillRect(r);
        const bool focus = button.hasKeyboardFocus(true);
        g.setColour(focus ? kAccent : (highlighted ? kBoneDim : kLine));
        g.drawRect(r, 1.0f);
        if (mini && !button.getToggleState() && (bool) button.getProperties()["hint"]) {   // credible tempo alternative: pulse an outline
            const float pulse = 0.5f + 0.5f * std::sin((float) juce::Time::getMillisecondCounterHiRes() * 0.006f);
            g.setColour(kAccent.withAlpha(0.35f + 0.65f * pulse)); g.drawRect(r, 1.5f);
        }
        if (mini && button.getToggleState()) { g.setColour(kAccent); g.drawRect(r, 1.0f); }
        else if (button.getToggleState()) {
            g.setColour(kAccent);
            g.fillRect(r.getX(), r.getY(), 2.5f, r.getHeight());
        }
    }
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override {
        return makeFont(juce::jmin(12.5f, (float) buttonHeight * 0.46f), true, 0.06f);
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& button, bool, bool) override {
        sync();
        if (button.getComponentID() == "keymark") return;
        g.setColour(button.isEnabled() ? kBone : kBoneDim);
        const bool mini = button.getComponentID() == "mini";
        g.setFont(mini ? makeFont((float) button.getHeight() * 0.64f, true, 0.02f) : getTextButtonFont(button, button.getHeight()));
        if (mini && button.getToggleState()) g.setColour(kOnAccent);
        else if (mini && button.isMouseOver()) g.setColour(kAccent);
        g.drawFittedText(mini ? button.getButtonText() : button.getButtonText().toUpperCase(), button.getLocalBounds().reduced(4, 1),
                         juce::Justification::centred, 1, 0.85f);
    }
};

TonicRecreationAudioProcessorEditor::TonicRecreationAudioProcessorEditor(TonicRecreationAudioProcessor& p)
  : AudioProcessorEditor(&p), processor(p) {
    setTitle("Skell Key");
    // Design space 550 x 450; the default window is 92 % of the previous 495 x 405 default.
    setSize(defaultWidth, defaultHeight);
    setResizable(true, true);
    setResizeLimits(defaultWidth * 4 / 5, defaultHeight * 4 / 5, defaultWidth * 2, defaultHeight * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio((double) designWidth / (double) designHeight);

    handL = sceneArea.getCentreX() - 28.0f; handR = sceneArea.getCentreX() + 28.0f;
    scene = std::make_unique<SceneHitButton>();
    scene->setTooltip("Click: hear the chord loop of the track. Shift-click: hear the key as a scale. Right-click: change costume.");
    scene->onClick = [this] {
        if (view.root < 0) return;
        if (juce::ModifierKeys::currentModifiers.isShiftDown()) processor.playKeyPreview(view.root, view.mode);   // shift: scale + tonic chord
        else processor.playChordLoop();                                                                           // click: the track's chord loop
    };
    scene->onStateChange = [this] { repaint(); };
    scene->onRightClick = [this] { costume = (costume + 1) % 4; repaint(); };
    scene->onEnter = [this] { if (!hasKeyboardFocus(false)) grabKeyboardFocus(); };   // so the computer keyboard can play
    scene->midiAt = [this](juce::Point<int> p) { return pitchAt(p); };
    scene->noteCb = [this](int midi, bool on) { processor.liveNote(midi, on); };
    setWantsKeyboardFocus(true);
    addAndMakeVisible(*scene);
    resized();

    theme = processor.getTheme(); applyTheme(theme);
    look = std::make_unique<SkellLookAndFeel>();
    look->themeIndex = &theme;
    for (auto* b : { &pauseButton, &resetButton, &openFileButton, &liveButton, &halfButton, &doubleButton, &helpButton, &skinButton, &lockButton }) {
        b->setLookAndFeel(look.get());
        addAndMakeVisible(b);
    }
    halfButton.setComponentID("mini"); helpButton.setComponentID("mini");
    helpButton.setWantsKeyboardFocus(false);
    skinButton.setComponentID("keymark"); skinButton.setWantsKeyboardFocus(false);
    lockButton.setComponentID("mini"); lockButton.setWantsKeyboardFocus(false); lockButton.setClickingTogglesState(true);
    lockButton.setTooltip("Scale lock: snap the notes you play to the detected scale");
    lockButton.onClick = [this] { processor.setScaleLock(lockButton.getToggleState()); };
    tagButton.setWantsKeyboardFocus(false);
    tagButton.setTooltip("Add the key and tempo to file names (one file or a whole folder)");
    tagButton.onClick = [this] { showTagMenu(); };
    skinButton.setTooltip("Click the key to change the look  (now: " + juce::String(themes[theme].name) + ")");
    skinButton.onClick = [this] {
        theme = (theme + 1) % numThemes; processor.setTheme(theme); applyTheme(theme);
        skinButton.setButtonText(themes[theme].name);
        skinButton.setTooltip("Click the key to change the look  (now: " + juce::String(themes[theme].name) + ")");
        for (auto* b : { &pauseButton, &resetButton, &openFileButton, &liveButton, &halfButton, &doubleButton, &helpButton, &skinButton, &lockButton }) b->repaint();
        repaint();
    };
    skinButton.setButtonText(themes[theme].name);
    helpButton.setTooltip("What can I click?");
    helpButton.onClick = [this] { showHelp(true); }; doubleButton.setComponentID("mini");
    halfButton.setTooltip("Tempo is double what it should be: halve it");
    doubleButton.setTooltip("Tempo is half what it should be: double it");
    halfButton.setClickingTogglesState(true); doubleButton.setClickingTogglesState(true);
    halfButton.setWantsKeyboardFocus(false); doubleButton.setWantsKeyboardFocus(false);   // no focus ring after a click
    halfButton.onClick = [this] {       // on/off; turning one on turns the other off
        hintDismissed = true;
        if (halfButton.getToggleState()) doubleButton.setToggleState(false, juce::dontSendNotification);
        bpmFactor = halfButton.getToggleState() ? 0.5f : 1.0f; timerCallback(); };
    doubleButton.onClick = [this] {
        hintDismissed = true;
        if (doubleButton.getToggleState()) halfButton.setToggleState(false, juce::dontSendNotification);
        bpmFactor = doubleButton.getToggleState() ? 2.0f : 1.0f; timerCallback(); };
    pauseButton.setButtonText("SEND TO TUNE");
    pauseButton.setClickingTogglesState(true);
    pauseButton.setEnabled(false);
    pauseButton.setTooltip("Send the confirmed Key, Scale and A4 to SKELL tuners and choirs. Works locally on Windows with no Hub, Python or internet. Click again to stop.");
    resetButton.setTooltip("Forget everything heard so far and start again");
    openFileButton.setTooltip("Analyze an audio file. You can also drop one anywhere on this window.");
    liveButton.setTooltip("Leave file mode and analyze the DAW input again");

    pauseButton.onClick = [this] {
        const bool on = pauseButton.getToggleState() && view.root >= 0;
        const bool connected = processor.setWorldSharing(on, view.root, view.mode);
        // Send only the typed SkellCore tuning packet. The old MIDI scale
        // output could trigger sustained notes in other DAW instruments.
        processor.setScaleOutput(false);
        if (on && !connected) {
            pauseButton.setToggleState(false, juce::dontSendNotification);
            processor.setScaleOutput(false);
        }
        // SEND TO TUNE affects only SkellCore sharing; do not replace the
        // user's system clipboard or inject sustained scale MIDI notes.
        timerCallback();
    };
    resetButton.onClick = [this] {
        hintDismissed = false; resetGate(); bpmFactor = 1.0f; halfButton.setToggleState(false, juce::dontSendNotification); doubleButton.setToggleState(false, juce::dontSendNotification);
        processor.clearSavedResult();
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
        hintDismissed = false; resetGate(); bpmFactor = 1.0f; halfButton.setToggleState(false, juce::dontSendNotification); doubleButton.setToggleState(false, juce::dontSendNotification);
        processor.returnToLive();
        // Keep an explicit SEND TO TUNE session active when changing from
        // file analysis to live input. Publishing waits for a confirmed key.
        timerCallback();
    };
    startTimerHz(30);
    timerCallback();
}

TonicRecreationAudioProcessorEditor::~TonicRecreationAudioProcessorEditor() {
    stopTimer();
    for (auto* b : { &pauseButton, &resetButton, &openFileButton, &liveButton, &halfButton, &doubleButton, &helpButton, &skinButton, &lockButton }) b->setLookAndFeel(nullptr);
    scene.reset();
    look.reset();
}

void TonicRecreationAudioProcessorEditor::showHelp(bool on) {
    helpShown = on;
    for (auto* b : { &pauseButton, &resetButton, &openFileButton, &liveButton, &halfButton, &doubleButton, &helpButton, &skinButton, &lockButton }) b->setVisible(!on);
    if (scene != nullptr) scene->setVisible(!on);
    repaint();
}
void TonicRecreationAudioProcessorEditor::mouseDown(const juce::MouseEvent&) { if (helpShown) showHelp(false); }

// ------------------------------------------------------------------ playable piano
int TonicRecreationAudioProcessorEditor::pitchAt(juce::Point<int> p) const {
    const auto& a = sceneArea;
    const float s = scale();
    const float x = a.getX() + (float) p.x / s, y = a.getY() + (float) p.y / s;
    const float kbTop = a.getBottom() - 62.0f, kbH = 58.0f, ww = a.getWidth() / 7.0f, bw = ww * 0.58f, bh = 35.0f;
    if (y < kbTop || y > kbTop + kbH || x < a.getX() || x > a.getRight()) return -1;
    static const int whitePc[7] = {0, 2, 4, 5, 7, 9, 11}, blackPc[5] = {1, 3, 6, 8, 10}, blackSlot[5] = {1, 2, 4, 5, 6};
    if (y < kbTop + bh)
        for (int i = 0; i < 5; ++i) {
            const float cx = a.getX() + ww * (float) blackSlot[i];
            if (x >= cx - bw * 0.5f && x <= cx + bw * 0.5f) return 60 + blackPc[i];
        }
    const int idx = juce::jlimit(0, 6, (int) ((x - a.getX()) / ww));
    return 60 + whitePc[idx];
}

namespace {
struct KeyMap { char c; int semis; };
const KeyMap kKeyMap[] = {{'a',0},{'w',1},{'s',2},{'e',3},{'d',4},{'f',5},{'t',6},{'g',7},{'y',8},{'h',9},{'u',10},{'j',11},
                          {'k',12},{'o',13},{'l',14},{'p',15},{';',16}};
constexpr int kNumKeyMap = (int) (sizeof(kKeyMap) / sizeof(kKeyMap[0]));
bool keyDown(char c) {
    const auto m = juce::ModifierKeys::currentModifiers;
    if (m.isCommandDown() || m.isCtrlDown() || m.isAltDown()) return false;      // leave host shortcuts alone
    return juce::KeyPress::isKeyCurrentlyDown(c) || juce::KeyPress::isKeyCurrentlyDown(std::toupper((unsigned char) c));
}
}

bool TonicRecreationAudioProcessorEditor::keyStateChanged(bool) {
    bool changed = false;
    for (int i = 0; i < kNumKeyMap; ++i) {
        const bool down = keyDown(kKeyMap[i].c);
        if (down != kbHeld[(size_t) i]) { kbHeld[(size_t) i] = down; processor.liveNote(60 + kKeyMap[i].semis, down); changed = true; }
    }
    return changed;
}
bool TonicRecreationAudioProcessorEditor::keyPressed(const juce::KeyPress& k) {
    for (int i = 0; i < kNumKeyMap; ++i) if (k.getTextCharacter() == (juce::juce_wchar) kKeyMap[i].c) return true;   // consume note keys
    return false;
}
void TonicRecreationAudioProcessorEditor::focusLost(FocusChangeType) {
    for (int i = 0; i < kNumKeyMap; ++i) if (kbHeld[(size_t) i]) { kbHeld[(size_t) i] = false; processor.liveNote(60 + kKeyMap[i].semis, false); }
}

// ------------------------------------------------------------------ file tagging
juce::String TonicRecreationAudioProcessorEditor::keyLabel(int root, int mode) const {
    const bool flats = processor.getState().getRawParameterValue("preferFlats")->load() > 0.5f;
    return juce::String((flats ? flatNames : sharpNames)[juce::jlimit(0, 11, root)]) + (mode == 0 ? "maj" : "min");
}
juce::File TonicRecreationAudioProcessorEditor::stampedFile(const juce::File& f, int root, int mode, float bpm) const {
    std::string base = f.getFileNameWithoutExtension().toStdString();
    base = std::regex_replace(base, std::regex("_[A-G][#b]?(maj|min)(_[0-9]+)?$"), "");   // replace an older stamp
    juce::String name = juce::String(base) + "_" + keyLabel(root, mode);
    if (bpm > 0.0f) name += "_" + juce::String(juce::roundToInt(bpm));
    return f.getSiblingFile(name + f.getFileExtension());
}
std::vector<std::pair<juce::File, juce::File>> TonicRecreationAudioProcessorEditor::planRenames(int& noKey, int& blocked) const {
    std::vector<std::pair<juce::File, juce::File>> plan;
    noKey = blocked = 0;
    for (const auto& it : batch) {
        if (it.root < 0) { ++noKey; continue; }
        const auto target = stampedFile(it.file, it.root, it.mode, it.bpm);
        if (target == it.file) continue;                                   // already carries exactly this stamp
        if (target.exists()) { ++blocked; continue; }
        plan.emplace_back(it.file, target);
    }
    return plan;
}
int TonicRecreationAudioProcessorEditor::applyRenames(const std::vector<std::pair<juce::File, juce::File>>& plan) {
    std::vector<std::pair<juce::File, juce::File>> done;
    for (const auto& pr : plan)
        if (pr.first.existsAsFile() && !pr.second.exists() && pr.first.moveFileTo(pr.second)) {
            done.push_back(pr);
            if (juce::File(processor.getFilePath()) == pr.first) processor.retargetFile(pr.second);
        }
    if (!done.empty()) lastRenames = done;
    return (int) done.size();
}
void TonicRecreationAudioProcessorEditor::undoLastTagging() {
    int n = 0;
    for (auto it = lastRenames.rbegin(); it != lastRenames.rend(); ++it)
        if (it->second.existsAsFile() && !it->first.exists() && it->second.moveFileTo(it->first)) {
            ++n;
            if (juce::File(processor.getFilePath()) == it->second) processor.retargetFile(it->first);
        }
    lastRenames.clear();
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Undo tagging", juce::String(n) + " file(s) renamed back.");
}
void TonicRecreationAudioProcessorEditor::showTagMenu() {
    const juce::File current(processor.getFilePath());
    const bool canSingle = processor.isOfflineMode() && !processor.isFileBusy() && view.root >= 0 && current.existsAsFile() && !batchActive;
    juce::PopupMenu m;
    m.addItem(1, "Add key + tempo to this file's name", canSingle);
    m.addItem(2, "Tag every file in a folder...", !batchActive);
    m.addSeparator();
    m.addItem(3, "Undo last tagging", !lastRenames.empty());
    juce::Component::SafePointer<TonicRecreationAudioProcessorEditor> safe(this);
    m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&tagButton), [safe](int choice) {
        if (safe == nullptr) return;
        if (choice == 1) safe->tagThisFile();
        else if (choice == 2) safe->startFolderTag();
        else if (choice == 3) safe->undoLastTagging();
    });
}
void TonicRecreationAudioProcessorEditor::tagThisFile() {
    const juce::File f(processor.getFilePath());
    if (!f.existsAsFile() || view.root < 0) return;
    batch.clear(); batch.push_back({ f, view.root, view.mode, bpmNow });
    int noKey = 0, blocked = 0;
    const auto plan = planRenames(noKey, blocked);
    if (plan.empty()) {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Nothing to do",
            blocked > 0 ? "A file with that name already exists." : "This file already has this key and tempo in its name.");
        return;
    }
    juce::Component::SafePointer<TonicRecreationAudioProcessorEditor> safe(this);
    juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, "Add key and tempo to the file name?",
        plan[0].first.getFileName() + "\n   becomes\n" + plan[0].second.getFileName() + "\n\nYou can undo this from the Tag menu.",
        "Rename", "Cancel", this, juce::ModalCallbackFunction::create([safe, plan](int r) {
            if (r == 1 && safe != nullptr) safe->applyRenames(plan);
        }));
}
void TonicRecreationAudioProcessorEditor::startFolderTag() {
    folderChooser = std::make_unique<juce::FileChooser>("Choose a folder of samples to tag", juce::File{}, "");
    juce::Component::SafePointer<TonicRecreationAudioProcessorEditor> safe(this);
    folderChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
        [safe](const juce::FileChooser& c) { if (safe != nullptr) safe->beginBatch(c.getResult()); });
}
void TonicRecreationAudioProcessorEditor::beginBatch(const juce::File& dir) {
    if (!dir.isDirectory()) return;
    auto files = dir.findChildFiles(juce::File::findFiles, false);
    batch.clear();
    for (const auto& f : files) if (isAudioFile(f.getFullPathName())) batch.push_back({ f, -1, 2, 0.0f });
    std::sort(batch.begin(), batch.end(), [](const BatchItem& a, const BatchItem& b) { return a.file.getFileName().compareNatural(b.file.getFileName()) < 0; });
    if (batch.empty()) { juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "No audio files", "That folder has no wav, aiff, flac, ogg or mp3 files."); return; }
    batchIndex = -1; batchActive = true; batchWaiting = false;
}
void TonicRecreationAudioProcessorEditor::stepBatch() {
    if (!batchActive) return;
    if (batchWaiting) {
        if (!processor.isOfflineMode()) { batchActive = false; return; }               // user left file mode: cancel
        if (processor.isFileBusy()) return;
        auto& it = batch[(size_t) batchIndex];
        if (!processor.fileFailed()) { it.root = processor.getDetectedRoot(); it.mode = processor.getDetectedMode(); it.bpm = processor.getDetectedBpm(); }
        batchWaiting = false;
    }
    while (++batchIndex < (int) batch.size()) {
        hintDismissed = false; resetGate(); bpmFactor = 1.0f;
        halfButton.setToggleState(false, juce::dontSendNotification); doubleButton.setToggleState(false, juce::dontSendNotification);
        if (processor.requestFileAnalysis(batch[(size_t) batchIndex].file)) { batchWaiting = true; return; }
    }
    batchActive = false;
    finishBatch();
}
void TonicRecreationAudioProcessorEditor::finishBatch() {
    int noKey = 0, blocked = 0;
    const auto plan = planRenames(noKey, blocked);
    juce::String msg = "Analysed " + juce::String((int) batch.size()) + " files.\n";
    if (plan.empty()) {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Folder tagging",
            msg + "Nothing to rename" + (noKey ? " (" + juce::String(noKey) + " had no clear key)." : "."));
        return;
    }
    msg += "Add key + tempo to " + juce::String((int) plan.size()) + " file names?\n\n";
    for (size_t i = 0; i < plan.size() && i < 3; ++i) msg += plan[i].first.getFileName() + "  ->  " + plan[i].second.getFileName() + "\n";
    if (plan.size() > 3) msg += "...\n";
    if (noKey || blocked) msg += "\nSkipped: " + juce::String(noKey) + " with no clear key, " + juce::String(blocked) + " name clashes.\n";
    msg += "\nTempos are the detector's reading (half/double time is possible). You can undo from the Tag menu.";
    juce::Component::SafePointer<TonicRecreationAudioProcessorEditor> safe(this);
    juce::AlertWindow::showOkCancelBox(juce::MessageBoxIconType::QuestionIcon, "Tag folder", msg, "Rename", "Cancel", this,
        juce::ModalCallbackFunction::create([safe, plan](int r) {
            if (r != 1 || safe == nullptr) return;
            const int n = safe->applyRenames(plan);
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon, "Folder tagging", juce::String(n) + " file(s) renamed.");
        }));
}

void TonicRecreationAudioProcessorEditor::resetGate() {
    gateRoot = -1; gateMode = 2; gateSince = 0.0; shownRoot = -1; shownMode = 2;
    shownBpm = 0.0f; bpmCand = 0.0f; bpmSince = 0.0; tuneCand = 0.0f; tuneSince = 0.0;
}

void TonicRecreationAudioProcessorEditor::timerCallback() {
    stepBatch();
    const bool offline = processor.isOfflineMode();
    const bool startingFile = offline && processor.isFileBusy() && processor.getFileAnalyzedSeconds() < 0.25f;
    const bool flats = processor.getState().getRawParameterValue("preferFlats")->load() > 0.5f;
    const char* const* names = flats ? flatNames : sharpNames;

    // Confidence gate: live input shows a key/tempo only after it has stayed the same for a few
    // seconds; a file shows its result only once the whole analysis has finished.
    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    const bool fileBusy = offline && processor.isFileBusy();
    const int rawRoot = startingFile ? -1 : processor.getDetectedRoot(), rawMode = processor.getDetectedMode();
    if (rawRoot != gateRoot || rawMode != gateMode) { gateRoot = rawRoot; gateMode = rawMode; gateSince = nowMs; }
    if (fileBusy) { shownRoot = -1; }
    else if (rawRoot >= 0 && (offline || nowMs - gateSince >= keyHoldMs)) { shownRoot = rawRoot; shownMode = rawMode; }
    else if (rawRoot < 0 && (offline || nowMs - gateSince >= 2500.0)) { shownRoot = -1; }
    const bool keyPending = rawRoot >= 0 && shownRoot < 0 && !fileBusy;
    // A reopened project keeps its last confirmed result until fresh audio produces a new one.
    const bool fromSaved = !offline && shownRoot < 0 && processor.getSavedRoot() >= 0;
    const int root = shownRoot >= 0 ? shownRoot : (fromSaved ? processor.getSavedRoot() : -1);
    const int mode = shownRoot >= 0 ? shownMode : (fromSaved ? processor.getSavedMode() : rawMode);
    view.root = root; view.mode = mode;
    if (root < 0) {
        view.keyText = (startingFile || fileBusy) ? "Analyzing..."
                     : (offline && !processor.isFileBusy() ? "No key found" : "Listening...");
        view.keyHint = keyPending ? "Gathering evidence" : (fileBusy ? "Reading the whole file" : "Waiting for musical input");
        view.keyConfidence = 0.0f;
    } else {
        view.keyText = juce::String(names[root]) + (mode == 0 ? " major" : " minor");
        view.keyConfidence = fromSaved ? 0.0f : juce::jlimit(0.0f, 1.0f, processor.getConfidence());
        view.keyHint = fromSaved ? (keyPending ? "Saved result - checking audio" : "Saved result - play audio to refresh")
                                 : "Key evidence  " + juce::String(juce::roundToInt(view.keyConfidence * 100.0f)) + "%";
    }
    juce::String also;
    int shown = 0;
    for (int i = 0; i < 3 && shown < 2 && !startingFile && root >= 0 && !fromSaved; ++i) {
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
    if (root >= 0 && !fromSaved && cr >= 0 && cr < 12 && kind >= 0 && kind < 8) {
        view.chordText = "Chord   " + juce::String(names[cr]) + " " + chordKinds[kind];
        if (bass >= 0 && bass < 12 && bass != cr) view.chordText += "/" + juce::String(names[bass]);
    }

    const float bpmRaw = startingFile ? 0.0f : processor.getDetectedBpm();
    if (fileBusy) shownBpm = 0.0f;
    else if (offline) shownBpm = bpmRaw;
    else if (bpmRaw > 0.0f && shownBpm > 0.0f && std::abs(std::log2(bpmRaw / shownBpm)) < 0.03f) { shownBpm = bpmRaw; bpmSince = nowMs; }
    else {
        if (bpmRaw <= 0.0f || bpmCand <= 0.0f || std::abs(std::log2(bpmRaw / bpmCand)) >= 0.03f) { bpmCand = bpmRaw; bpmSince = nowMs; }
        if (bpmRaw > 0.0f && nowMs - bpmSince >= bpmHoldMs) shownBpm = bpmRaw;
        else if (bpmRaw <= 0.0f && nowMs - bpmSince >= 2500.0) shownBpm = 0.0f;
    }
    const bool bpmPending = bpmRaw > 0.0f && shownBpm <= 0.0f && !fileBusy;
    const bool bpmFromSaved = !offline && shownBpm <= 0.0f && processor.getSavedBpm() > 0.0f;
    const float bpmValue = bpmFromSaved ? processor.getSavedBpm() : juce::jlimit(0.0f, 400.0f, shownBpm * bpmFactor);
    view.bpmConfidence = (startingFile || shownBpm <= 0.0f) ? 0.0f : juce::jlimit(0.0f, 1.0f, processor.getBpmConfidence());
    view.bpmHave = bpmValue > 0.0f;
    const int dots = (int) (nowMs / 350.0) % 4;           // animated "working" marker instead of a dead dash
    const juce::String working = juce::String("...").substring(0, juce::jmax(1, dots));
    view.bpmText = bpmValue > 0 ? juce::String(bpmValue, 1) : ((fileBusy || bpmPending) ? working : "--");
    view.bpmHint = bpmFromSaved ? juce::String("Saved result") : bpmValue > 0 ? "Confidence  " + juce::String(juce::roundToInt(view.bpmConfidence * 100.0f)) + "%"
                                : (bpmPending ? "Gathering evidence" : (fileBusy ? "Analyzing file" : "Waiting for rhythm"));

    const float hz = processor.getDetectedTuningHz();
    const float tConf = startingFile ? 0.0f : processor.getTuningConfidence();
    // Tuning: hidden while a file is still being analysed; live, it must hold steady for 3 s first.
    if (std::abs(hz - tuneCand) > 0.4f) { tuneCand = hz; tuneSince = nowMs; }
    const bool tuneSure = offline ? !fileBusy : (nowMs - tuneSince >= 3000.0);
    view.tuningValid = tConf >= 0.03f && std::isfinite(hz) && hz > 0.0f && tuneSure;
    const bool tuneFromSaved = !offline && !view.tuningValid && processor.getSavedHz() > 0.0f && processor.getSavedTuneConf() >= 0.03f;
    const float shownHz = tuneFromSaved ? processor.getSavedHz() : hz;
    if (tuneFromSaved) view.tuningValid = true;
    resultsHeld = fileBusy;
    view.cents = view.tuningValid ? 1200.0f * std::log2(shownHz / 440.0f) : 0.0f;
    view.refText = view.tuningValid ? juce::String(shownHz, 1) : (fileBusy ? working : "--");
    view.refHint = view.tuningValid ? juce::String(view.cents >= 0 ? "+" : "") + juce::String(view.cents, 1) + " cents"
                                    : (fileBusy ? "Analyzing file" : "Detecting tuning");

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
    {   // Input level (dB-ish) with a fast attack and slow release for the header strip.
        const float l = processor.getInputLevel();
        const float db = l > 1.0e-5f ? 20.0f * std::log10(l) : -100.0f;
        const float target = juce::jlimit(0.0f, 1.0f, (db + 54.0f) / 54.0f);
        level += (target - level) * (target > level ? 0.6f : 0.12f);
    }
    {   // Once clicked, automatic following survives silence and source changes.
        // Do not require the user to toggle sharing again after each phrase.
        const bool haveKey = root >= 0;
        const bool sending = pauseButton.getToggleState();
        pauseButton.setEnabled(haveKey || sending); // can always click OFF
        pauseButton.setButtonText(!sending ? "SEND TO TUNE"
            : (!processor.isWorldOnline() ? "LINK UNAVAILABLE"
                : (haveKey ? "SENDING TO TUNE" : "WAITING FOR KEY")));
    }
    {   // Octave hint: only while no manual correction is active.
        const float alt = processor.getAltTempoRatio();
        const bool have = bpmValue > 0.0f && bpmFactor == 1.0f && !hintDismissed;
        halfButton.getProperties().set("hint", have && alt == 0.5f);
        doubleButton.getProperties().set("hint", have && alt == 2.0f);
    }
    if (shownRoot >= 0) {   // remember the confirmed result in the plugin state (saved with the project)
        const bool tuneFresh = view.tuningValid && !tuneFromSaved;
        processor.saveResult(shownRoot, shownMode, shownBpm > 0.0f ? bpmValue : processor.getSavedBpm(),
                             tuneFresh ? hz : processor.getSavedHz(), tuneFresh ? tConf : processor.getSavedTuneConf());
    }
    previewPc = processor.getPreviewPitchClass();
    liveMask = processor.getLiveMask();
    if (previewPc < 0 && liveMask != 0) previewPc = processor.getLiveLast();
    bpmNow = juce::String(view.bpmText).getFloatValue();
    tonicPulse = juce::jmax(0.0f, tonicPulse - 0.05f);
    if (root != lastRoot) { tonicPulse = 1.0f; lastRoot = root; }
    repaint();
}

// --------------------------------------------------------------------------- painting
void TonicRecreationAudioProcessorEditor::paint(juce::Graphics& g) {
    applyTheme(theme);
    g.fillAll(kBg);
    g.addTransform(juce::AffineTransform::scale(scale()));
    paintHeader(g);
    paintKeyCard(g);
    paintTempoCard(g);
    paintReferenceCard(g);
    paintFooter(g);

    if (helpShown) {
        const juce::Rectangle<float> all(0.0f, 0.0f, (float) designWidth, (float) designHeight);
        g.setColour(kBg); g.fillRect(all);
        drawText(g, "WHAT CAN I CLICK?", { 30.0f, 28.0f, 400.0f, 26.0f }, 18.0f, true, kBone, juce::Justification::centredLeft, 0.14f);
        const char* const rows[][2] = {
            {"Skeleton", "hear the track's chord loop"},
            {"Shift + skeleton", "hear the key as a scale"},
            {"Right-click skeleton", "change its costume"},
            {"/2   x2", "fix a half- or double-time tempo (green = applied)"},
            {"Send to Tune", "send key, scale + A4 to SkellyTune via SkellWorld"},
            {"Open Audio / drop a file", "analyse a whole file"},
            {"Pulsing /2 or x2", "a likely alternative tempo"},
            {"Dots on the keys", "how often each note led the melody"},
            {"The key (top-left)", "cycle the look: Bone (main), Neon, Graveyard, Clean"},
            {"Piano keys", "play them: mouse, keys A-K, or a MIDI keyboard"},
            {"Lock", "snap what you play to the detected scale"} };
        float y = 70.0f;
        for (auto& row : rows) {
            drawText(g, row[0], { 30.0f, y, 190.0f, 20.0f }, 12.5f, true, kAccent, juce::Justification::centredLeft);
            drawText(g, row[1], { 226.0f, y, 310.0f, 20.0f }, 12.5f, false, kBone, juce::Justification::centredLeft);
            y += 26.0f;
        }
        drawText(g, "click anywhere to close", { 30.0f, designHeight - 40.0f, 300.0f, 16.0f }, 11.0f, false, kBoneDim, juce::Justification::centredLeft, 0.1f);
        return;
    }
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
        drawKeyMark(g, all.getCentreX() - 37.0f, all.getCentreY() - 70.0f, kBone, kBg);
        drawText(g, "DROP AUDIO TO UNLOCK", all.withTrimmedTop(all.getCentreY() - 28.0f).withHeight(60.0f), 24.0f, true, kBone,
                 juce::Justification::centred, 0.12f);
        drawText(g, "wav  aiff  flac  ogg  mp3", all.withTrimmedTop(all.getCentreY() + 30.0f).withHeight(24.0f), 12.0f, false, kBoneDim,
                 juce::Justification::centred, 0.1f);
    }
}

void TonicRecreationAudioProcessorEditor::paintHeader(juce::Graphics& g) {
    drawKeyMark(g, 16.0f, 11.0f, kBone, kBg);
    drawText(g, "SKELL KEY", { 100.0f, 9.0f, 220.0f, 26.0f }, 21.0f, true, kBone,
             juce::Justification::centredLeft, 0.16f);
    drawText(g, "KEY   /   TEMPO   /   TUNING", { 101.0f, 33.0f, 240.0f, 14.0f }, 10.0f, false, kBoneDim,
             juce::Justification::centredLeft, 0.18f);
    // Header strip: an input level meter (lit segments) with a tempo playhead that
    // sweeps one bar (4 beats) at the detected BPM.
    const float y = 54.0f;
    const int n = 48;
    const float slot = (float) (designWidth - 28) / (float) n;
    const int lit = juce::roundToInt(level * (float) n);
    int head = -1;
    if (bpmNow > 0.0f) {
        const double beats = juce::Time::getMillisecondCounterHiRes() * 0.001 * (double) bpmNow / 60.0;
        head = (int) (std::fmod(beats, 4.0) / 4.0 * (double) n);
    }
    for (int i = 0; i < n; ++i) {
        const float x = 14.0f + slot * (float) i;
        const bool on = i < lit;
        const int trail = head >= 0 ? ((head - i) + n) % n : 99;
        float h = on ? 3.4f : 2.0f;
        juce::Colour c = on ? kAccent.withAlpha(0.55f + 0.45f * (float) i / (float) n) : kBoneFaint.withAlpha(0.9f);
        if (trail == 0) { c = kBone; h = 6.0f; }
        else if (trail == 1) { c = on ? kBone.withAlpha(0.8f) : kBoneDim; h = 4.6f; }
        else if (trail == 2 && !on) c = kBoneDim.withAlpha(0.7f);
        g.setColour(c);
        g.fillRoundedRectangle(x, y + 3.0f - h * 0.5f, slot - 2.2f, h, 1.0f);
    }
}

void TonicRecreationAudioProcessorEditor::paintKeyCard(juce::Graphics& g) {
    const juce::Rectangle<float> card(14.0f, 66.0f, 522.0f, 200.0f);
    drawCard(g, card);
    drawText(g, "DETECTED KEY", { 30.0f, 76.0f, 200.0f, 14.0f }, 10.5f, true, kBoneDim,
             juce::Justification::centredLeft, 0.2f);

    // Big key: tonic in bone, mode in dim bone, so the pitch reads first.
    if (view.root >= 0) {
        const bool flats = processor.getState().getRawParameterValue("preferFlats")->load() > 0.5f;
        const juce::String tonic = (flats ? flatNames : sharpNames)[view.root];
        const auto tonicFont = makeFont(60.0f, true);
        drawText(g, tonic, { 28.0f, 94.0f, 200.0f, 70.0f }, 60.0f, true, kBone);
        const float w = textWidth(tonicFont, tonic);
        drawText(g, view.mode == 0 ? "major" : "minor", { 28.0f + w + 10.0f, 121.0f, 140.0f, 34.0f }, 24.0f,
                 false, kAccent);
    } else {
        drawText(g, view.keyText, { 28.0f, 100.0f, 230.0f, 56.0f }, 30.0f, true, kBoneDim);
    }
    drawText(g, view.keyHint, { 30.0f, 171.0f, 220.0f, 16.0f }, 12.5f, false, kBoneDim);
    drawTeeth(g, { 30.0f, 190.0f, 150.0f, 12.0f }, view.keyConfidence);
    drawText(g, view.candidates, { 30.0f, 211.0f, 228.0f, 16.0f }, 12.5f, false, kBoneDim);
    drawText(g, view.chordText, { 30.0f, 231.0f, 228.0f, 16.0f }, 12.5f, true, kBone);

    // Divider between the readout and the spine.
    g.setColour(kLine);
    g.drawLine(262.0f, 78.0f, 262.0f, 254.0f, 1.0f);
    paintSpine(g, sceneArea);
}

void TonicRecreationAudioProcessorEditor::paintSpine(juce::Graphics& g, juce::Rectangle<float> area) {
    // --- keyboard: one octave, C..B. White keys glow with the live chroma of that
    // pitch class; keys in the detected key are bright, the tonic is accent green.
    const float kbTop = area.getBottom() - 62.0f, kbH = 58.0f;
    const float ww = area.getWidth() / 7.0f, bw = ww * 0.58f, bh = 35.0f;
    static const int whitePc[7] = {0, 2, 4, 5, 7, 9, 11};
    static const int blackPc[5] = {1, 3, 6, 8, 10};
    static const int blackSlot[5] = {1, 2, 4, 5, 6};  // boundary after white key index-1
    const bool flats = processor.getState().getRawParameterValue("preferFlats")->load() > 0.5f;
    const auto* names = flats ? flatNames : sharpNames;
    const bool hover = scene != nullptr && scene->isOver();

    auto keyX = [&](int pc) -> float {          // centre x of a pitch class' key
        for (int i = 0; i < 7; ++i) if (whitePc[i] == pc) return area.getX() + ww * ((float) i + 0.5f);
        for (int i = 0; i < 5; ++i) if (blackPc[i] == pc) return area.getX() + ww * (float) blackSlot[i];
        return area.getCentreX();
    };

    // --- skeleton (behind and above the keys) -------------------------------------
    const float cx = area.getCentreX();
    const juce::Colour bone = kBone.withAlpha(0.92f), shade = kBoneDim.withAlpha(0.8f);
    // targets for the hands: left on the tonic, right on the fifth (or the sounding note)
    float tl = cx - 28.0f, tr = cx + 28.0f;
    bool pressL = false, pressR = false;
    if (view.root >= 0) { tl = keyX(view.root); tr = keyX((view.root + 7) % 12); }
    if (previewPc >= 0) { tr = keyX(previewPc); pressR = true; }
    if (tl > tr) std::swap(tl, tr);   // hands never cross
    handL += (juce::jlimit(area.getX() + 8.0f, area.getRight() - 8.0f, tl) - handL) * 0.18f;
    handR += (juce::jlimit(area.getX() + 8.0f, area.getRight() - 8.0f, tr) - handR) * 0.28f;
    if (view.root >= 0 && previewPc == view.root) pressL = true;
    (void) pressL;
    // The skeleton is authored in a 92-unit-tall space above the keys and scaled to fit.
    const float sk = (kbTop - area.getY()) / 92.0f;
    const float handY = area.getY() + 85.0f;   // in skeleton space
    auto toSk = [&](float x) { return cx + (x - cx) / sk; };
    g.saveState();
    g.addTransform(juce::AffineTransform::scale(sk, sk, cx, area.getY()));

    g.setColour(shade);                                   // spine, vertebra by vertebra
    for (int i = 0; i < 9; ++i)
        g.fillRoundedRectangle(cx - 2.6f, area.getY() + 31.0f + 6.4f * (float) i, 5.2f, 4.2f, 1.6f);
    g.setColour(bone);                                    // ribcage
    for (int i = 0; i < 4; ++i) {
        const float y = area.getY() + 42.0f + 7.0f * (float) i, w = 30.0f - 3.0f * (float) i;
        juce::Path rib;
        rib.startNewSubPath(cx - 1.5f, y);
        rib.quadraticTo(cx - w * 0.7f, y - 4.0f, cx - w, y + 4.5f);
        rib.startNewSubPath(cx + 1.5f, y);
        rib.quadraticTo(cx + w * 0.7f, y - 4.0f, cx + w, y + 4.5f);
        g.strokePath(rib, juce::PathStrokeType(1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    const float shY = area.getY() + 36.0f;                // clavicles
    g.drawLine(cx - 27.0f, shY, cx + 27.0f, shY, 2.2f);
    // skull
    float nod = 0.0f;                                     // skull nods on every beat of the detected tempo
    if (bpmNow > 0.0f) {
        const double beats = juce::Time::getMillisecondCounterHiRes() * 0.001 * (double) bpmNow / 60.0;
        nod = 2.4f * std::exp(-5.0f * (float) std::fmod(beats, 1.0));
    }
    const float skY = area.getY() + 4.0f + nod;
    g.setColour(bone);
    g.fillEllipse(cx - 13.0f, skY, 26.0f, 24.0f);
    g.fillRoundedRectangle(cx - 8.0f, skY + 19.0f, 16.0f, 9.0f, 2.5f);
    g.setColour(kPanel);
    g.fillEllipse(cx - 9.0f, skY + 9.0f, 7.0f, 7.5f);
    g.fillEllipse(cx + 2.0f, skY + 9.0f, 7.0f, 7.5f);
    { juce::Path nose; nose.addTriangle(cx, skY + 16.5f, cx - 2.2f, skY + 21.0f, cx + 2.2f, skY + 21.0f); g.fillPath(nose); }
    for (int i = 0; i < 4; ++i) g.fillRect(cx - 6.0f + 3.4f * (float) i, skY + 23.5f, 0.9f, 5.0f);
    const juce::Colour dark(0xff0b0c08);
    if (costume == 1) {                                   // headphones
        juce::Path band; band.startNewSubPath(cx - 14.0f, skY + 12.0f); band.quadraticTo(cx, skY - 13.0f, cx + 14.0f, skY + 12.0f);
        g.setColour(kAccent); g.strokePath(band, juce::PathStrokeType(2.6f));
        g.fillRoundedRectangle(cx - 18.0f, skY + 8.0f, 6.0f, 11.0f, 2.0f);
        g.fillRoundedRectangle(cx + 12.0f, skY + 8.0f, 6.0f, 11.0f, 2.0f);
    } else if (costume == 2) {                            // top hat
        g.setColour(dark); g.fillRoundedRectangle(cx - 9.0f, skY - 14.0f, 18.0f, 15.0f, 1.5f);
        g.setColour(kAccent); g.fillRect(cx - 9.0f, skY - 3.0f, 18.0f, 2.6f);
        g.setColour(dark); g.fillRoundedRectangle(cx - 16.0f, skY - 0.5f, 32.0f, 3.2f, 1.2f);
        g.setColour(kBoneDim); g.drawRoundedRectangle(cx - 16.0f, skY - 0.5f, 32.0f, 3.2f, 1.2f, 0.7f);
    } else if (costume == 3) {                            // shades
        g.setColour(dark);
        g.fillRoundedRectangle(cx - 11.5f, skY + 8.0f, 10.5f, 8.0f, 2.5f);
        g.fillRoundedRectangle(cx + 1.0f, skY + 8.0f, 10.5f, 8.0f, 2.5f);
        g.fillRect(cx - 1.5f, skY + 10.0f, 3.0f, 1.6f);
    }
    // arms: shoulder -> elbow -> hand, with joint knobs and a little hand
    auto arm = [&](float sx, float hx, bool pressed) {
        const float hy = handY + (pressed ? 4.0f : 0.0f);
        const float ex = sx + (hx - sx) * 0.35f + (sx < cx ? -9.0f : 9.0f), ey = shY + (hy - shY) * 0.52f;
        g.setColour(bone);
        juce::Path p; p.startNewSubPath(sx, shY); p.lineTo(ex, ey); p.lineTo(hx, hy - 4.0f);
        g.strokePath(p, juce::PathStrokeType(2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.fillEllipse(sx - 3.0f, shY - 3.0f, 6.0f, 6.0f);
        g.fillEllipse(ex - 3.0f, ey - 3.0f, 6.0f, 6.0f);
        g.fillRoundedRectangle(hx - 5.5f, hy - 5.0f, 11.0f, 5.0f, 2.0f);          // palm
        for (int f = 0; f < 4; ++f) g.fillRect(hx - 5.0f + 3.0f * (float) f, hy - 0.5f, 1.7f, 4.0f);   // fingers
    };
    arm(cx - 27.0f, toSk(handL), pressL);
    arm(cx + 27.0f, toSk(handR), pressR);
    g.restoreState();

    // --- keys (drawn after the arms so fingers rest on top of the keyboard) -----------
    auto glow = [&](int pc) { return juce::jlimit(0.0f, 1.0f, chromaShown[(size_t) pc]); };
    for (int i = 0; i < 7; ++i) {
        const int pc = whitePc[i];
        const bool tonic = view.root >= 0 && pc == view.root;
        const bool diatonic = view.root >= 0 && inScale(pc, view.root, view.mode);
        const bool sounding = previewPc == pc || ((liveMask >> pc) & 1u) != 0;
        const juce::Rectangle<float> k(area.getX() + ww * (float) i + 0.8f, kbTop, ww - 1.6f, kbH);
        g.setColour(diatonic ? kKeyWhiteDia : kKeyWhiteOther);
        g.fillRoundedRectangle(k, 2.0f);
        g.setColour(kAccent.withAlpha((tonic ? 0.30f : 0.0f) + (sounding ? 0.55f : 0.0f) + 0.55f * glow(pc)));
        g.fillRoundedRectangle(k, 2.0f);
        if (sounding || tonic) { g.setColour(kAccent); g.drawRoundedRectangle(k.reduced(0.5f), 2.0f, 1.4f); }
        drawText(g, names[pc], juce::Rectangle<float>(k.getX(), k.getBottom() - 14.0f, k.getWidth(), 14.0f), 9.0f, tonic, tonic ? kOnAccent : kKeyInk,
                 juce::Justification::centred);
    }
    for (int i = 0; i < 5; ++i) {
        const int pc = blackPc[i];
        const bool tonic = view.root >= 0 && pc == view.root;
        const bool diatonic = view.root >= 0 && inScale(pc, view.root, view.mode);
        const bool sounding = previewPc == pc || ((liveMask >> pc) & 1u) != 0;
        const juce::Rectangle<float> k(area.getX() + ww * (float) blackSlot[i] - bw * 0.5f, kbTop, bw, bh);
        g.setColour(diatonic ? kKeyBlackDia : kKeyBlackOther);
        g.fillRoundedRectangle(k, 2.0f);
        g.setColour(kAccent.withAlpha((tonic ? 0.35f : 0.0f) + (sounding ? 0.6f : 0.0f) + 0.6f * glow(pc)));
        g.fillRoundedRectangle(k, 2.0f);
        if (sounding || tonic) { g.setColour(kAccent); g.drawRoundedRectangle(k.reduced(0.5f), 2.0f, 1.4f); }
    }
    // Lead melody: dots on the keys sized by how often that pitch class was the lead note, current note lit.
    {
        const int now = processor.getMelodyNow();
        std::array<float,12> use {}; float mx = 0.0f;
        for (int pc = 0; pc < 12 && !resultsHeld; ++pc) { use[(size_t) pc] = processor.getMelodyUse(pc); mx = juce::jmax(mx, use[(size_t) pc]); }
        if (mx > 0.0f) {
            for (int pc = 0; pc < 12; ++pc) {
                const bool black = !(pc == 0 || pc == 2 || pc == 4 || pc == 5 || pc == 7 || pc == 9 || pc == 11);
                const float x = keyX(pc), y = black ? kbTop + 7.0f : kbTop + 8.0f;
                const float r = 1.4f + 3.2f * use[(size_t) pc];
                if (use[(size_t) pc] < 0.05f && pc != now) continue;
                g.setColour(pc == now ? kAccent : (black ? kBone.withAlpha(0.85f) : kKeyInk));
                g.fillEllipse(x - r, y - r, 2.0f * r, 2.0f * r);
                if (pc == now) { g.setColour(kBone); g.drawEllipse(x - r - 1.5f, y - r - 1.5f, 2.0f * r + 3.0f, 2.0f * r + 3.0f, 1.0f); }
            }
            std::array<int,12> order; for (int i = 0; i < 12; ++i) order[(size_t) i] = i;
            std::sort(order.begin(), order.end(), [&](int a, int b) { return use[(size_t) a] > use[(size_t) b]; });
            juce::String melody;
            for (int i = 0; i < 3; ++i) if (use[(size_t) order[(size_t) i]] >= 0.35f) melody += "  " + juce::String(names[order[(size_t) i]]);
            if (melody.isNotEmpty() && !hover)
                drawText(g, "MELODY" + melody, { area.getX(), area.getY() - 1.0f, 130.0f, 12.0f }, 9.5f, true, kBoneDim,
                         juce::Justification::topLeft, 0.12f);
        }
    }
    // Hint only while the pointer is over the scene (it replaces the melody label), so the top row stays quiet.
    if (hover && view.root >= 0)
        drawText(g, "CLICK: CHORD LOOP   SHIFT: SCALE", { area.getX(), area.getY() - 1.0f, area.getWidth() - 50.0f, 12.0f },
                 9.5f, true, kAccent, juce::Justification::topLeft, 0.1f);
}

void TonicRecreationAudioProcessorEditor::paintTempoCard(juce::Graphics& g) {
    const juce::Rectangle<float> r(14.0f, 276.0f, 257.0f, 100.0f);
    drawCard(g, r);
    drawText(g, "TEMPO", { r.getX() + 16.0f, r.getY() + 9.0f, 100.0f, 14.0f }, 10.5f, true, kBoneDim,
             juce::Justification::centredLeft, 0.2f);
    const bool have = view.bpmHave;
    drawText(g, view.bpmText, { r.getX() + 16.0f, r.getY() + 28.0f, 150.0f, 44.0f }, 38.0f, true,
             have ? kAccent : kBoneDim);
    if (have) {
        const float w = textWidth(makeFont(38.0f, true), view.bpmText);
        drawText(g, "BPM", { r.getX() + 16.0f + w + 6.0f, r.getY() + 46.0f, 50.0f, 16.0f }, 10.5f, true, kBoneDim,
                 juce::Justification::centredLeft, 0.12f);
    }
    drawText(g, view.bpmHint, { r.getX() + 16.0f, r.getY() + 73.0f, 170.0f, 16.0f }, 12.5f, false, kBoneDim);
    drawTeeth(g, { r.getRight() - 100.0f, r.getY() + 16.0f, 84.0f, 12.0f }, view.bpmConfidence, 8);
}

void TonicRecreationAudioProcessorEditor::paintReferenceCard(juce::Graphics& g) {
    const juce::Rectangle<float> r(279.0f, 276.0f, 257.0f, 100.0f);
    drawCard(g, r);
    drawText(g, "REFERENCE A", { r.getX() + 16.0f, r.getY() + 9.0f, 120.0f, 14.0f }, 10.5f, true, kBoneDim,
             juce::Justification::centredLeft, 0.2f);
    drawText(g, view.refText, { r.getX() + 16.0f, r.getY() + 28.0f, 130.0f, 44.0f }, 38.0f, true,
             view.tuningValid ? kAccent : kBoneDim);
    if (view.tuningValid) {
        const float w = textWidth(makeFont(38.0f, true), view.refText);
        drawText(g, "Hz", { r.getX() + 16.0f + w + 6.0f, r.getY() + 46.0f, 30.0f, 16.0f }, 10.5f, true, kBoneDim);
    }
    drawText(g, view.refHint, { r.getX() + 16.0f, r.getY() + 73.0f, 140.0f, 16.0f }, 12.5f, false, kBoneDim);

    // Cents gauge, -50 .. +50, with a pointer. Bone ticks, accent pointer.
    const float gx = r.getX() + 164.0f, gw = 76.0f, gy = r.getY() + 62.0f;
    g.setColour(kBoneFaint);
    g.fillRect(gx, gy, gw, 1.0f);
    for (int t = -5; t <= 5; ++t) {
        const float x = gx + gw * (float) (t + 5) / 10.0f;
        const float th = (t == 0) ? 10.0f : (t % 5 == 0 ? 7.0f : 4.0f);
        g.setColour(t == 0 ? kBone : kBoneDim);
        g.fillRect(x - 0.5f, gy - th, 1.0f, th);
    }
    drawText(g, "-50", { gx - 14.0f, gy + 3.0f, 28.0f, 11.0f }, 10.0f, false, kBoneDim, juce::Justification::centred);
    drawText(g, "0", { gx + gw * 0.5f - 14.0f, gy + 3.0f, 28.0f, 11.0f }, 10.0f, false, kBoneDim, juce::Justification::centred);
    drawText(g, "+50", { gx + gw - 14.0f, gy + 3.0f, 28.0f, 11.0f }, 10.0f, false, kBoneDim, juce::Justification::centred);
    if (view.tuningValid) {
        const float x = gx + gw * (juce::jlimit(-50.0f, 50.0f, view.cents) + 50.0f) / 100.0f;
        juce::Path tri; tri.addTriangle(x, gy + 1.0f, x - 4.5f, gy - 11.0f, x + 4.5f, gy - 11.0f);
        g.setColour(kAccent); g.fillPath(tri);
    }
}

void TonicRecreationAudioProcessorEditor::paintFooter(juce::Graphics& g) {
    const juce::Rectangle<float> r(14.0f, 386.0f, 522.0f, 52.0f);
    drawCard(g, r);
    if (view.busy) {
        g.setColour(kAccent);
        g.fillRect(r.getX() + 1.0f, r.getY() + 1.0f, (r.getWidth() - 2.0f) * view.progress, 2.0f);
    }
    drawText(g, view.status, { r.getX() + 16.0f, r.getY() + 4.0f, 290.0f, r.getHeight() - 8.0f }, 12.5f, false,
             processor.fileFailed() ? kWarn : kBoneDim);
}

void TonicRecreationAudioProcessorEditor::resized() {
    const float s = scale();
    auto fit = [s](float x, float y, float w, float h) {
        return juce::Rectangle<int>(juce::roundToInt(x * s), juce::roundToInt(y * s),
                                    juce::roundToInt(w * s), juce::roundToInt(h * s));
    };
    pauseButton.setBounds(fit(332.0f, 16.0f, 134.0f, 24.0f));
    resetButton.setBounds(fit(472.0f, 16.0f, 62.0f, 24.0f));
    if (scene != nullptr) scene->setBounds(fit(sceneArea.getX(), sceneArea.getY(), sceneArea.getWidth(), sceneArea.getHeight()));
    openFileButton.setBounds(fit(350.0f, 398.0f, 90.0f, 28.0f));
    liveButton.setBounds(fit(444.0f, 398.0f, 84.0f, 28.0f));
    helpButton.setBounds(fit(234.0f, 74.0f, 20.0f, 20.0f));
    skinButton.setBounds(fit(10.0f, 6.0f, 84.0f, 38.0f));
    lockButton.setBounds(fit(sceneArea.getRight() - 44.0f, 74.0f, 44.0f, 16.0f));
    tagButton.setBounds(fit(276.0f, 16.0f, 50.0f, 24.0f));
    halfButton.setBounds(fit(187.0f, 312.0f, 24.0f, 16.0f));
    doubleButton.setBounds(fit(215.0f, 312.0f, 24.0f, 16.0f));
}

// --------------------------------------------------------------------------- file drops
bool TonicRecreationAudioProcessorEditor::isInterestedInFileDrag(const juce::StringArray& files) {
    for (const auto& file : files) if (isAudioFile(file)) return true;
    return false;
}
void TonicRecreationAudioProcessorEditor::fileDragEnter(const juce::StringArray&, int, int) {
    dragHover = true;
    for (auto* b : { &pauseButton, &resetButton, &openFileButton, &liveButton, &halfButton, &doubleButton, &helpButton, &skinButton, &lockButton }) b->setVisible(false);
    repaint();
}
void TonicRecreationAudioProcessorEditor::fileDragExit(const juce::StringArray&) {
    dragHover = false;
    for (auto* b : { &pauseButton, &resetButton, &openFileButton, &liveButton, &halfButton, &doubleButton, &helpButton, &skinButton, &lockButton }) b->setVisible(true);
    repaint();
}
void TonicRecreationAudioProcessorEditor::filesDropped(const juce::StringArray& files, int, int) {
    fileDragExit({});
    for (const auto& f : files) if (isAudioFile(f)) { loadFile(juce::File(f)); break; }
}
void TonicRecreationAudioProcessorEditor::loadFile(const juce::File& f) {
    if (!f.existsAsFile()) return;
    if (processor.requestFileAnalysis(f)) {
        hintDismissed = false; resetGate(); bpmFactor = 1.0f; halfButton.setToggleState(false, juce::dontSendNotification); doubleButton.setToggleState(false, juce::dontSendNotification);
        pauseButton.setToggleState(false, juce::dontSendNotification);
        processor.setScaleOutput(false);
        processor.setWorldSharing(false);
        timerCallback();
    }
}
