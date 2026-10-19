"""Record the calibration's actual compiler flags without changing an unchanged header."""

import json
import subprocess
import sys
from pathlib import Path


def prepare(flags):
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
    contents = (
        "#ifndef OUTSHINE_CALIBRATION_BUILD_H\n#define OUTSHINE_CALIBRATION_BUILD_H\n"
        "#include <string_view>\n"
        "namespace outshine::Diagnostics::Calibration {\n"
        f"inline constexpr std::string_view kBuildCommit = {json.dumps(commit)};\n"
        f"inline constexpr std::string_view kBuildFlags = {json.dumps(flags)};\n"
        "}\n#endif\n"
    )
    target = root / "build/generated/CalibrationBuild.h"
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists() or target.read_text() != contents:
        pending = target.with_suffix(".h.tmp")
        pending.write_text(contents)
        pending.replace(target)


if __name__ == "__main__":
    prepare(sys.argv[1])
