from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "Source" / "PluginEditor.cpp").read_text(encoding="utf-8")

CONTROL_LIST = "{ &pauseButton, &resetButton, &openFileButton, &liveButton, &halfButton, &doubleButton, &helpButton, &skinButton, &tagButton, &lockButton }"


def simple_function(signature: str) -> str:
    start = SOURCE.index(signature)
    end = SOURCE.index("\n}", start) + 2
    return SOURCE[start:end]


class TagButtonLifecycleTests(unittest.TestCase):
    def test_tag_button_is_registered_with_the_editor_look_and_feel(self):
        start = SOURCE.index("TonicRecreationAudioProcessorEditor::TonicRecreationAudioProcessorEditor")
        end = SOURCE.index("TonicRecreationAudioProcessorEditor::~TonicRecreationAudioProcessorEditor")
        constructor = SOURCE[start:end]
        self.assertIn(CONTROL_LIST, constructor)
        self.assertIn('tagButton.setComponentID("mini")', constructor)
        self.assertIn("tagButton.onClick = [this] { showTagMenu(); };", constructor)

    def test_tag_button_is_detached_before_look_and_feel_destruction(self):
        destructor = simple_function("TonicRecreationAudioProcessorEditor::~TonicRecreationAudioProcessorEditor")
        self.assertIn(CONTROL_LIST, destructor)
        self.assertIn("b->setLookAndFeel(nullptr)", destructor)

    def test_tag_button_follows_help_and_drag_overlay_visibility(self):
        for signature, expected in (
            ("void TonicRecreationAudioProcessorEditor::showHelp(bool on)", "setVisible(!on)"),
            ("void TonicRecreationAudioProcessorEditor::fileDragEnter(", "setVisible(false)"),
            ("void TonicRecreationAudioProcessorEditor::fileDragExit(", "setVisible(true)"),
        ):
            body = simple_function(signature)
            self.assertIn(CONTROL_LIST, body, signature)
            self.assertIn(expected, body, signature)

    def test_tag_button_bounds_remain_inside_design_space(self):
        self.assertIn("tagButton.setBounds(fit(276.0f, 16.0f, 50.0f, 24.0f))", SOURCE)
        self.assertEqual(SOURCE.count(CONTROL_LIST), 6)
        self.assertLessEqual(276.0 + 50.0, 550.0)
        self.assertLessEqual(16.0 + 24.0, 450.0)


if __name__ == "__main__":
    unittest.main()
