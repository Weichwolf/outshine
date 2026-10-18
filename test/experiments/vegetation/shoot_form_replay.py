"""Fresh finished-form replay; no original source input, randomness or plan build."""

import argparse
import json
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

from compact_crown import CompactCrown
from demand import Camera
from shoot_form import ShootForm
from shoot_form_metrics import pose_digest
from shoot_form_probe import TRAITS


def replay(study, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private outputs must be under/tmp")
    config = json.loads((study / "scene.json").read_text())
    records = []
    with (
        patch.object(ShootForm, "build", side_effect=AssertionError("form build")),
        patch.object(
            CompactCrown, "build", side_effect=AssertionError("source plan build")
        ),
        patch(
            "spatial_group_source.load_source",
            side_effect=AssertionError("source read"),
        ),
        patch("numpy.random.Generator", side_effect=AssertionError("randomness")),
        patch(
            "crown_shoots.CrownShoots.group",
            side_effect=AssertionError("original poses"),
        ),
    ):
        for mode in TRAITS:
            before = time.perf_counter()
            form = ShootForm.load(
                study / f"{mode}.npz", expected_source=config["source_identity"]
            )
            load_s = time.perf_counter() - before
            for view in ("whole", "near"):
                c = config[view]
                camera = Camera(
                    c["eye"],
                    c["target"],
                    fov_y=c["fov_y_deg"],
                    width=2560,
                    height=1440,
                    near=0.05,
                    far=1000,
                )
                selected = camera.visible(form.lower, form.upper)
                before = time.perf_counter()
                poses = form.evaluate(selected, max_leaves=100000, anchors=True)
                elapsed = time.perf_counter() - before
                records.append(
                    {
                        "mode": mode,
                        "view": view,
                        "form_identity": form.identity,
                        "file_bytes": (study / f"{mode}.npz").stat().st_size,
                        "load_s": load_s,
                        "evaluate_s": elapsed,
                        "leaf_rows": len(poses["ids"]),
                        "pose_sha256": pose_digest(poses),
                    }
                )
    result = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "cases": records,
        "proc_self_high_water_rss_bytes_linux": next(
            int(line.split()[1]) * 1024
            for line in Path("/proc/self/status").read_text().splitlines()
            if line.startswith("VmHWM:")
        ),
        "scope": "Fresh process, only finished coefficients and saved camera declarations. Original source/build/RNG/group callbacks forbidden; no new plant growth or source acquisition. Reference CPU poses, not a GPU/native cache format or budget claim.",
    }
    output.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--study", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    replay(args.study, args.output)
