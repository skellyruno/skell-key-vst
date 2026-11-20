#!/usr/bin/env python3
"""Validate and normalize a staged Windows VST3 moduleinfo.json.

The source/build bundle must be copied into a release staging directory before
this script is called. Only the staged metadata file is rewritten; the VST3
binary and original build-tree bundle are never modified.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
import tempfile
from pathlib import Path
from typing import Any


class ModuleInfoError(ValueError):
    """Raised when VST3 module metadata is malformed or has the wrong identity."""


def _strip_trailing_commas(text: str) -> tuple[str, bool]:
    """Remove commas immediately before JSON object/array closers, outside strings."""
    output: list[str] = []
    in_string = False
    escaped = False
    removed = False

    for index, char in enumerate(text):
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
        elif char == ",":
            lookahead = index + 1
            while lookahead < len(text) and text[lookahead].isspace():
                lookahead += 1
            if lookahead < len(text) and text[lookahead] in "}]":
                removed = True
                continue
            output.append(char)
        else:
            output.append(char)

    if in_string or escaped:
        raise ModuleInfoError("moduleinfo.json contains an unterminated JSON string")

    return "".join(output), removed


def _reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ModuleInfoError(f"moduleinfo.json contains duplicate key: {key}")
        result[key] = value
    return result


def _reject_non_finite(value: str) -> None:
    raise ModuleInfoError(f"moduleinfo.json contains non-standard numeric constant: {value}")


def normalize_moduleinfo_bytes(
    raw: bytes, expected_product: str, expected_vendor: str
) -> tuple[dict[str, Any], bytes, dict[str, Any]]:
    """Validate metadata and return (parsed object, strict JSON bytes, manifest)."""
    source_sha256 = hashlib.sha256(raw).hexdigest()
    try:
        source_text = raw.decode("utf-8-sig")
    except UnicodeDecodeError as exc:
        raise ModuleInfoError(f"moduleinfo.json is not valid UTF-8: {exc}") from exc

    cleaned_text, trailing_commas_removed = _strip_trailing_commas(source_text)
    try:
        metadata = json.loads(
            cleaned_text,
            object_pairs_hook=_reject_duplicate_keys,
            parse_constant=_reject_non_finite,
        )
    except ModuleInfoError:
        raise
    except json.JSONDecodeError as exc:
        raise ModuleInfoError(
            f"moduleinfo.json is invalid after trailing-comma normalization: {exc}"
        ) from exc

    if not isinstance(metadata, dict):
        raise ModuleInfoError("moduleinfo.json root must be an object")

    name = metadata.get("Name")
    version = metadata.get("Version")
    if name != expected_product:
        raise ModuleInfoError(
            f"Expected product Name {expected_product!r}, found {name!r}"
        )
    if not isinstance(version, str) or not version.strip():
        raise ModuleInfoError("moduleinfo.json must contain a non-empty string Version")

    factory = metadata.get("Factory Info")
    if not isinstance(factory, dict):
        raise ModuleInfoError("moduleinfo.json Factory Info must be an object")
    vendor = factory.get("Vendor")
    if vendor != expected_vendor:
        raise ModuleInfoError(
            f"Expected Factory Info Vendor {expected_vendor!r}, found {vendor!r}"
        )

    classes = metadata.get("Classes")
    if not isinstance(classes, list) or not classes:
        raise ModuleInfoError("moduleinfo.json Classes must be a non-empty array")

    class_ids: list[str] = []
    categories: list[str] = []
    for index, item in enumerate(classes):
        if not isinstance(item, dict):
            raise ModuleInfoError(f"Classes[{index}] must be an object")
        cid = item.get("CID")
        if not isinstance(cid, str) or re.fullmatch(r"[0-9A-Fa-f]{32}", cid) is None:
            raise ModuleInfoError(f"Classes[{index}] has an invalid 32-hex CID")
        if cid.casefold() in {value.casefold() for value in class_ids}:
            raise ModuleInfoError(f"Duplicate VST3 class CID: {cid}")
        class_ids.append(cid)

        category = item.get("Category")
        if not isinstance(category, str) or not category.strip():
            raise ModuleInfoError(f"Classes[{index}] has no valid Category")
        categories.append(category)

        if item.get("Name") != expected_product:
            raise ModuleInfoError(
                f"Classes[{index}] Name does not match {expected_product!r}"
            )
        if item.get("Vendor") != expected_vendor:
            raise ModuleInfoError(
                f"Classes[{index}] Vendor does not match {expected_vendor!r}"
            )
        if item.get("Version") != version:
            raise ModuleInfoError(
                f"Classes[{index}] Version does not match top-level Version {version!r}"
            )

    for required in ("Audio Module Class", "Component Controller Class"):
        if categories.count(required) != 1:
            raise ModuleInfoError(
                f"Expected exactly one {required!r}; found {categories.count(required)}"
            )

    normalized_text = json.dumps(
        metadata, ensure_ascii=False, indent=2, allow_nan=False
    ) + "\n"
    normalized_bytes = normalized_text.encode("utf-8")
    # Reparse the exact bytes that will be staged. This is the strict-JSON gate.
    try:
        json.loads(
            normalized_bytes.decode("utf-8"),
            object_pairs_hook=_reject_duplicate_keys,
            parse_constant=_reject_non_finite,
        )
    except (json.JSONDecodeError, ModuleInfoError) as exc:
        raise ModuleInfoError(f"Normalized moduleinfo.json is not strict JSON: {exc}") from exc

    staged_sha256 = hashlib.sha256(normalized_bytes).hexdigest()
    contact = {
        "URL": factory.get("URL"),
        "E-Mail": factory.get("E-Mail"),
    }
    missing_contact_fields = [
        key for key, value in contact.items()
        if not isinstance(value, str) or not value.strip()
    ]
    manifest = {
        "product": expected_product,
        "vendor": expected_vendor,
        "version": version,
        "source_moduleinfo_sha256": source_sha256,
        "staged_moduleinfo_sha256": staged_sha256,
        "staged_moduleinfo_bytes": len(normalized_bytes),
        "trailing_commas_removed": trailing_commas_removed,
        "rewritten": normalized_bytes != raw,
        "class_ids": class_ids,
        "class_categories": categories,
        "missing_contact_fields": missing_contact_fields,
    }
    return metadata, normalized_bytes, manifest


def normalize_bundle(
    bundle: Path,
    expected_product: str,
    expected_vendor: str,
    manifest_path: Path | None = None,
) -> dict[str, Any]:
    moduleinfo_path = bundle / "Contents" / "Resources" / "moduleinfo.json"
    if not moduleinfo_path.is_file():
        raise ModuleInfoError(f"Missing VST3 moduleinfo.json: {moduleinfo_path}")
    if manifest_path is not None and manifest_path.exists():
        raise ModuleInfoError(
            f"Refusing to overwrite existing moduleinfo manifest: {manifest_path}"
        )

    raw = moduleinfo_path.read_bytes()
    _, normalized_bytes, manifest = normalize_moduleinfo_bytes(
        raw, expected_product, expected_vendor
    )

    if normalized_bytes != raw:
        with tempfile.NamedTemporaryFile(
            mode="wb", prefix=".moduleinfo.", suffix=".tmp",
            dir=moduleinfo_path.parent, delete=False
        ) as temporary:
            temporary.write(normalized_bytes)
            temporary_path = Path(temporary.name)
        temporary_path.replace(moduleinfo_path)

    if manifest_path is not None:
        manifest_path.parent.mkdir(parents=True, exist_ok=True)
        manifest_path.write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

    return manifest


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bundle", type=Path, required=True)
    parser.add_argument("--product", required=True)
    parser.add_argument("--vendor", required=True)
    parser.add_argument("--manifest-out", type=Path, required=True)
    args = parser.parse_args()

    try:
        manifest = normalize_bundle(
            args.bundle, args.product, args.vendor, args.manifest_out
        )
    except (ModuleInfoError, OSError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    print(json.dumps(manifest, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
