"""Shade a checked cached Place mesh; preserve geometry and actual camera provenance."""

import argparse
import hashlib
import json
import subprocess
import sys
import time
from pathlib import Path

from evidence import revision
from place_materials import default_finish


def render(source_report, output, samples_per_pass=None):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError(
            "Cached Place rendering requires a clean frozen implementation"
        )
    original_bytes = source_report.read_bytes()
    original = json.loads(original_bytes)
    if (
        original.get("representation") != "python-place-buildings"
        or original["experiment_tree_dirty"]
    ):
        raise ValueError("Cached input needs its own clean Python Place report")
    name = original["place"]
    config = json.loads(source_report.with_name(name + "-scene.json").read_text())
    if config["mesh_sha256"] != original["renderer"]["mesh_sha256"]:
        raise ValueError("Cached geometry differs from its original renderer proof")
    output.mkdir(parents=True, exist_ok=True)
    started = time.perf_counter()
    config.update(
        facade_finishes=default_finish(),
        output=str((output / (name + ".png")).resolve()),
    )
    if samples_per_pass is not None:
        config["samples_per_pass"] = samples_per_pass
    scene = output / (name + "-scene.json")
    scene.write_text(json.dumps(config, indent=2) + "\n")
    with (output / (name + "-render.log")).open("w") as stream:
        subprocess.run(
            [
                sys.executable,
                str(Path(__file__).with_name("mitsuba_interior.py")),
                str(scene),
            ],
            stdout=stream,
            stderr=subprocess.STDOUT,
            timeout=900,
            check=True,
        )
    renderer = json.loads((output / (name + ".renderer.json")).read_text())
    if (
        renderer["projection"] != original["renderer"]["projection"]
        or renderer["included_source_triangles"] != original["triangles"]
    ):
        raise ValueError("Cached rendering changed the camera or omitted geometry")
    report = (
        original
        | proof
        | {
            "geometry_commit": original.get("geometry_commit", original["commit"]),
            "geometry_mesh_sha256": renderer["mesh_sha256"],
            "geometry_report_sha256": hashlib.sha256(original_bytes).hexdigest(),
            "scene_sha256": hashlib.sha256(scene.read_bytes()).hexdigest(),
            "facade_finishes": config["facade_finishes"],
            "renderer": renderer,
            "image_sha256": renderer["image_sha256"],
            "total_cached_render_seconds": time.perf_counter() - started,
            "cache_scope": "Checked whole-scene experiment mesh reused; no provider decode or geometry generation. Native per-asset cache/LOD admission unproved.",
        }
    )
    report.pop("total_build_and_render_seconds", None)
    (output / (name + "-report.json")).write_text(json.dumps(report, indent=2) + "\n")
    print(name, "cached render complete", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source_report", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--samples-per-pass", type=int)
    args = parser.parse_args()
    render(args.source_report, args.output, args.samples_per_pass)
