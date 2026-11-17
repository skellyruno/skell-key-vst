from __future__ import annotations

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tools.normalize_vst3_moduleinfo import (
    ModuleInfoError,
    normalize_bundle,
    normalize_moduleinfo_bytes,
)


PRODUCT = "Skell Key"
VENDOR = "Skell Audio"
PROCESSOR_CID = "ABCDEF019182FAEB496E5263546E5263"
CONTROLLER_CID = "ABCDEF011234ABCD496E5263546E5263"


def metadata_object() -> dict:
    return {
        "Name": PRODUCT,
        "Version": "0.1.0",
        "Description": 'quoted ,} text and an escaped "quote".',
        "Factory Info": {
            "Vendor": VENDOR,
            "URL": "",
            "E-Mail": "",
            "Flags": {
                "Unicode": True,
                "Classes Discardable": False,
                "Component Non Discardable": False,
            },
        },
        "Classes": [
            {
                "CID": PROCESSOR_CID,
                "Category": "Audio Module Class",
                "Name": PRODUCT,
                "Vendor": VENDOR,
                "Version": "0.1.0",
                "Sub Categories": ["Fx"],
            },
            {
                "CID": CONTROLLER_CID,
                "Category": "Component Controller Class",
                "Name": PRODUCT,
                "Vendor": VENDOR,
                "Version": "0.1.0",
                "Sub Categories": ["Fx"],
            },
        ],
    }


def add_trailing_commas(text: str) -> str:
    output: list[str] = []
    in_string = False
    escaped = False
    for char in text:
        if in_string:
            output.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
            output.append(char)
        elif char in "}]":
            index = len(output) - 1
            while index >= 0 and output[index].isspace():
                index -= 1
            if index >= 0 and output[index] not in "{[,":
                output.insert(index + 1, ",")
            output.append(char)
        else:
            output.append(char)
    return "".join(output)


def metadata_bytes(trailing: bool = True, crlf: bool = True) -> bytes:
    text = json.dumps(metadata_object(), indent=2)
    if trailing:
        text = add_trailing_commas(text)
    if crlf:
        text = text.replace("\n", "\r\n")
    return text.encode("utf-8")


class ModuleInfoNormalizerTests(unittest.TestCase):
    def test_normalizes_juce_trailing_commas_and_preserves_quoted_text(self):
        raw = metadata_bytes(trailing=True)
        parsed, normalized, manifest = normalize_moduleinfo_bytes(raw, PRODUCT, VENDOR)
        normalized_text = normalized.decode("utf-8")
        strict = json.loads(normalized_text)
        self.assertEqual(strict["Description"], 'quoted ,} text and an escaped "quote".')
        self.assertTrue(manifest["trailing_commas_removed"])
        self.assertTrue(manifest["rewritten"])
        self.assertEqual(manifest["source_moduleinfo_sha256"], hashlib.sha256(raw).hexdigest())
        self.assertEqual(manifest["staged_moduleinfo_sha256"], hashlib.sha256(normalized).hexdigest())
        self.assertEqual(len(parsed["Classes"]), 2)
        self.assertNotRegex(normalized_text, r",\s*[}\]]")

    def test_accepts_strict_json_without_trailing_commas(self):
        raw = metadata_bytes(trailing=False)
        _, normalized, manifest = normalize_moduleinfo_bytes(raw, PRODUCT, VENDOR)
        self.assertEqual(json.loads(normalized)["Name"], PRODUCT)
        self.assertFalse(manifest["trailing_commas_removed"])

    def test_rejects_unterminated_string(self):
        raw = b'{"Name":"Skell Key","Version":"0.1.0","broken":"unterminated}'
        with self.assertRaisesRegex(ModuleInfoError, "unterminated"):
            normalize_moduleinfo_bytes(raw, PRODUCT, VENDOR)

    def test_rejects_duplicate_json_keys(self):
        text = json.dumps(metadata_object(), indent=2)
        text = text.replace(
            '  "Name": "Skell Key",',
            '  "Name": "Skell Key",\n  "Name": "Skell Key",',
            1,
        )
        with self.assertRaisesRegex(ModuleInfoError, "duplicate key"):
            normalize_moduleinfo_bytes(text.encode(), PRODUCT, VENDOR)

    def test_rejects_non_finite_numbers(self):
        text = json.dumps(metadata_object(), indent=2)
        text = text.replace(
            '  "Version": "0.1.0",',
            '  "Version": "0.1.0",\n  "Unexpected": NaN,',
            1,
        )
        with self.assertRaisesRegex(ModuleInfoError, "non-standard numeric"):
            normalize_moduleinfo_bytes(text.encode(), PRODUCT, VENDOR)

    def test_rejects_wrong_product_identity(self):
        data = metadata_object()
        data["Name"] = "Other Plugin"
        with self.assertRaisesRegex(ModuleInfoError, "Expected product Name"):
            normalize_moduleinfo_bytes(json.dumps(data).encode(), PRODUCT, VENDOR)

    def test_rejects_wrong_vendor(self):
        data = metadata_object()
        data["Factory Info"]["Vendor"] = "Unknown"
        with self.assertRaisesRegex(ModuleInfoError, "Expected Factory Info Vendor"):
            normalize_moduleinfo_bytes(json.dumps(data).encode(), PRODUCT, VENDOR)

    def test_rejects_duplicate_class_ids(self):
        data = metadata_object()
        data["Classes"][1]["CID"] = PROCESSOR_CID
        with self.assertRaisesRegex(ModuleInfoError, "Duplicate VST3 class CID"):
            normalize_moduleinfo_bytes(json.dumps(data).encode(), PRODUCT, VENDOR)

    def test_requires_processor_and_controller_classes(self):
        data = metadata_object()
        data["Classes"][1]["Category"] = "Other"
        with self.assertRaisesRegex(ModuleInfoError, "Component Controller Class"):
            normalize_moduleinfo_bytes(json.dumps(data).encode(), PRODUCT, VENDOR)

    def test_preserves_exact_source_hash_for_bom_and_crlf(self):
        raw = b"\xef\xbb\xbf" + metadata_bytes(trailing=True, crlf=True)
        _, normalized, manifest = normalize_moduleinfo_bytes(raw, PRODUCT, VENDOR)
        self.assertEqual(manifest["source_moduleinfo_sha256"], hashlib.sha256(raw).hexdigest())
        self.assertEqual(json.loads(normalized)["Name"], PRODUCT)

    def test_normalizes_staged_copy_without_changing_build_bundle(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "build" / "Skell Key.vst3"
            staged = root / "release-stage" / "Skell Key.vst3"
            for bundle in (source, staged):
                info = bundle / "Contents" / "Resources" / "moduleinfo.json"
                info.parent.mkdir(parents=True, exist_ok=True)
                info.write_bytes(metadata_bytes(trailing=True))
            original = (source / "Contents/Resources/moduleinfo.json").read_bytes()
            manifest_path = root / "release-stage" / "moduleinfo-verification.json"
            manifest = normalize_bundle(staged, PRODUCT, VENDOR, manifest_path)
            self.assertEqual((source / "Contents/Resources/moduleinfo.json").read_bytes(), original)
            self.assertNotEqual((staged / "Contents/Resources/moduleinfo.json").read_bytes(), original)
            self.assertTrue(manifest_path.is_file())
            self.assertEqual(json.loads(manifest_path.read_text())["version"], "0.1.0")
            self.assertTrue(manifest["trailing_commas_removed"])

    def test_refuses_to_overwrite_existing_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            bundle = root / "Skell Key.vst3"
            info = bundle / "Contents" / "Resources" / "moduleinfo.json"
            info.parent.mkdir(parents=True)
            info.write_bytes(metadata_bytes(trailing=True))
            manifest_path = root / "existing.json"
            manifest_path.write_text("keep me", encoding="utf-8")
            with self.assertRaisesRegex(ModuleInfoError, "Refusing to overwrite"):
                normalize_bundle(bundle, PRODUCT, VENDOR, manifest_path)
            self.assertEqual(manifest_path.read_text(), "keep me")


if __name__ == "__main__":
    unittest.main()
