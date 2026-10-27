#!/usr/bin/env python3
"""Check documented v0.6.1 release metadata against tracked plugin source.

Static repository validation only: does not compile Unreal Engine, verify GPU
inference, verify uploaded binary hashes, or grant third-party distribution rights.
"""

from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PLUGIN = ROOT / "Plugins" / "DLSS5ForUE5"
ERRORS: list[str] = []


def check(condition: bool, message: str) -> None:
    if not condition:
        ERRORS.append(message)


def read(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8-sig")
    except (OSError, UnicodeDecodeError) as exc:
        ERRORS.append(f"{path.relative_to(ROOT)}: {exc}")
        return ""


descriptor = json.loads(read(PLUGIN / "DLSS5ForUE5.uplugin"))
version = descriptor.get("Version")
name = descriptor.get("VersionName", "")
check(version == 601, "Expected plugin Version 601 for v0.6.1.")
check(name.startswith("0.6.1"), "VersionName must match the release.")
for module in descriptor.get("Modules", []):
    module_name = module.get("Name", "")
    check(bool(module_name and (PLUGIN / "Source" / module_name).is_dir()),
          f"Module source missing: {module_name}")

readme = read(ROOT / "README.md")
notes = read(ROOT / "RELEASE_NOTES_v0.6.1.md")
commands = read(ROOT / "docs" / "COMMANDS.md")
changelog = read(ROOT / "CHANGELOG.md")
third_party = read(ROOT / "THIRD_PARTY_NOTICES.md")
editor = read(PLUGIN / "Source/DLSS5ForUE5Editor/Private/DLSS5ForUE5EditorModule.cpp")
check("2026-10-27" in editor, "Maintainer-verified Editor build date missing.")
check("# DLSS5ForUE5" in readme and "v0.6.1" in readme, "README release identity missing.")
check(notes.startswith("# DLSS5ForUE5 v0.6.1"), "Release notes title/version mismatch.")
check("## v0.6.1" in changelog, "Changelog missing current release.")
check("THIRD_PARTY_NOTICES.md" in readme, "README missing third-party reference.")
check("community compatibility" in third_party.lower(), "NR runtime provenance missing.")

highlights = notes.split("## Highlights", 1)[-1].split("\n## ", 1)[0]
check("build date" not in highlights.lower() and "Rebuilt for" not in highlights,
      "Internal build/date information does not belong in release highlights.")

for match in re.finditer(r"!\[[^\]]*\]\(([^)]+)\)", readme):
    target = match.group(1).split("#", 1)[0].strip()
    if not target.startswith(("http://", "https://", "data:")):
        check((ROOT / target).is_file(), f"Missing README image: {target}")

# Every registered main r.DLSS5 CVar should appear in the published reference.
cvars = read(PLUGIN / "Source/DLSS5ForUE5_nvngx/Private/DLSS5NRCVars.cpp")
registered = set(re.findall(r'TEXT\("(r\.DLSS5\.[^"]+)"\)', cvars))
for cvar in sorted(registered):
    check(cvar in commands, f"Documented CVar missing: {cvar}")

# All shader registrations should point to a shipped .usf in the plugin.
for shader in (PLUGIN / "Source").rglob("*.cpp"):
    code = read(shader)
    for virtual in re.findall(r'"/DLSS5NR/([^"]+\.usf)"', code):
        check((PLUGIN / "Shaders" / virtual).is_file(),
              f"Registered shader not found: {virtual}")

checksums = read(ROOT / "SHA256SUMS_v0.6.1.txt")
zip_records: dict[str, str] = {}
for line in checksums.splitlines():
    if not line.strip():
        continue
    m = re.fullmatch(r"([0-9a-f]{64})\s{2}(\S+\.zip)", line)
    check(bool(m), f"Malformed checksum: {line[:90]}")
    if m:
        check(m.group(2) not in zip_records, f"Duplicate checksum filename: {m.group(2)}")
        zip_records[m.group(2)] = m.group(1)

for ue in ("5.5", "5.6", "5.7", "5.8"):
    expected = f"DLSS5ForUE5.v0.6.1.-UE{ue}.zip"
    check(expected in zip_records, f"Release checksum missing: UE {ue}")
    check(expected in readme or expected in notes, f"Release archive not documented: {expected}")
check("DLSS5ForUE5-v0.6.1-SOURCE.zip" in zip_records,
      "Source archive checksum missing.")

if ERRORS:
    for item in ERRORS:
        print(f"ERROR: {item}", file=sys.stderr)
    print(f"Release contract failed: {len(ERRORS)} finding(s).", file=sys.stderr)
    raise SystemExit(1)

print(f"Release contract OK: v0.6.1, {len(registered)} CVars, "
      "all referenced screenshots and four engine ZIP records.")
