"""Render existing synthetic surface cases, retaining their measured report and origin."""

import hashlib
import json
from pathlib import Path
import subprocess
import sys


HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
OUTPUT = ROOT / "build/experiments/transport/showcase/fixtures"


def render_fixtures():
    subprocess.run(
        [sys.executable, "-W", "error", str(HERE / "transport_flat_surface_network.py"),
         "--small", "--junction-mode", "cuts", "--output", str(OUTPUT),
         "--shots", str(OUTPUT)],
        cwd=ROOT, check=True,
    )
    records = {}
    for path in OUTPUT.glob("*-flat-surfaces.json"):
        image = path.with_suffix(".png")
        records[path.stem] = {
            "report_sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "image_sha256": hashlib.sha256(image.read_bytes()).hexdigest(),
        }
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    dirty = subprocess.check_output(
        ["git", "status", "--porcelain", "--", "test/experiments/transport", "lib"],
        cwd=ROOT, text=True,
    ).strip()
    origin = {"commit": commit, "working_tree_dirty": bool(dirty), "records": records}
    path = OUTPUT / "origin.json"
    temporary = path.with_suffix(".tmp")
    temporary.write_text(json.dumps(origin, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)
