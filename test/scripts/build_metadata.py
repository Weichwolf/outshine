"""Record benchmark compiler flags without rewriting unchanged generated headers."""

import json
import subprocess
import sys
from pathlib import Path


def prepare(flags, component):
    root = Path(__file__).resolve().parents[2]
    commit = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=root, text=True
    ).strip()
    dirty = subprocess.check_output(
        ["git", "status", "--porcelain", "--untracked-files=normal"],
        cwd=root,
        text=True,
    )
    if dirty:
        commit += "+dirty"
    stem, namespace = {
        'calibration': ('Calibration', 'outshine::Diagnostics::Calibration'),
        'client': ('Client', 'outshine::Client'),
    }[component]
    guard = f'OUTSHINE_{stem.upper()}_BUILD_H'
    contents = (
        f"#ifndef {guard}\n#define {guard}\n"
        "#include <string_view>\n"
        f"namespace {namespace} {{\n"
        f"inline constexpr std::string_view kBuildCommit = {json.dumps(commit)};\n"
        f"inline constexpr std::string_view kBuildFlags = {json.dumps(flags)};\n"
        "}\n#endif\n"
    )
    target = root / f"build/generated/{stem}Build.h"
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists() or target.read_text() != contents:
        pending = target.with_suffix(".h.tmp")
        pending.write_text(contents)
        pending.replace(target)


if __name__ == "__main__":
    prepare(sys.argv[1], sys.argv[2])
