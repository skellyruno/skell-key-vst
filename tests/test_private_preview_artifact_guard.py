from pathlib import Path
import unittest


WORKFLOW_PATH = Path(__file__).resolve().parents[1] / ".github" / "workflows" / "build.yml"


def step_block(workflow: str, name: str) -> str:
    lines = workflow.splitlines()
    marker = f"      - name: {name}"
    try:
        start = lines.index(marker)
    except ValueError as exc:
        raise AssertionError(f"Workflow step not found: {name}") from exc
    end = next(
        (index for index in range(start + 1, len(lines)) if lines[index].startswith("      - name: ")),
        len(lines),
    )
    return "\n".join(lines[start:end])


class PrivatePreviewArtifactGuardTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.workflow = WORKFLOW_PATH.read_text(encoding="utf-8")

    def test_installable_package_upload_requires_private_repository(self):
        block = step_block(self.workflow, "Upload one-level installable VST3 bundle")
        self.assertIn("if: ${{ github.event.repository.private == true }}", block)
        self.assertIn("uses: actions/upload-artifact@v6", block)
        self.assertIn("path: release-stage/**", block)
        self.assertIn("retention-days: 30", block)

    def test_public_repository_run_emits_withholding_notice(self):
        block = step_block(self.workflow, "Withhold installable preview artifact from public repositories")
        self.assertIn("if: ${{ github.event.repository.private != true }}", block)
        self.assertIn("::notice::", block)
        self.assertIn("$env:GITHUB_STEP_SUMMARY", block)
        self.assertIn("installable Skell Key VST3 package", block)

    def test_pluginval_report_remains_diagnostic_only(self):
        block = step_block(self.workflow, "Upload pluginval report")
        self.assertIn("if: always()", block)
        self.assertIn("path: pluginval-reports/", block)
        self.assertNotIn("release-stage", block)

    def test_python_regressions_are_discovered_together(self):
        self.assertIn('python -m unittest discover -s tests -p "test_*.py" -v', self.workflow)
        self.assertEqual(self.workflow.count("path: release-stage/**"), 1)


if __name__ == "__main__":
    unittest.main()
