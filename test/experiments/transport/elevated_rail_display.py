"""Append actual whole rail candidates to the exactly bound preceding full scene."""

import argparse
import json
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from cycle_bridge_display import append
from outshine_ref.contracts import Mesh


def run(args):
    if args.output.exists() or not args.output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("Fresh private system-temp display output required")
    report = json.loads((args.study / "report.json").read_text())
    baseline = json.loads((args.baseline_scene.parent / "report.json").read_text())
    scene = json.loads(args.baseline_scene.read_text())
    path = Path(scene["mesh"])
    if (
        digest(args.study / "candidates.npz") != report["candidates_sha256"]
        or digest(path) != scene["mesh_sha256"]
        or baseline["sources_after"] != len(report["old_source_ids"])
        or baseline["local_report_sha256"]
        != report["prior_modules"][-1]["report_sha256"]
    ):
        raise ValueError("Same actual complete preceding transport display required")
    with np.load(path, allow_pickle=False) as saved:
        original = dict(saved)
    with np.load(args.study / "candidates.npz", allow_pickle=False) as saved:
        candidates = dict(saved)
    args.output.mkdir(parents=True)
    whole_scene = dict(scene)
    origin = report["methods"]["flat"]["fit"]["origin_xy_m"]
    z = report["methods"]["flat"]["fit"]["coefficients"][2]
    scene.update(
        target=[*origin, z],
        eye=[origin[0] + 600, origin[1] - 600, z + 600],
        span_m=900,
        detail_view=True,
        profile=[1280, 720],
        samples=16,
    )
    palette = np.array(scene["colours"])
    rows = {}
    for method in (
        "before",
        *(name for name in ("flat", "tilted") if report["methods"][name]["accepted"]),
    ):
        if method == "before":
            mesh_path = path
        else:
            body = Mesh(
                candidates[method + "_vertices"].astype("f4"),
                candidates[method + "_triangles"],
            )
            shown = append(original, [(body, 3)], palette)
            if not all(
                np.array_equal(shown[k][: len(v)], v) for k, v in original.items()
            ):
                raise ValueError("Original full-scene array prefix changed")
            mesh_path = args.output / (method + ".npz")
            np.savez_compressed(mesh_path, **shown)
        for prefix, camera in (("", scene), ("whole-", whole_scene)):
            name = prefix + method
            row = dict(
                camera,
                id="raised-rail-candidate-" + name,
                mesh=str(mesh_path.resolve()),
                mesh_sha256=digest(mesh_path),
                output=str(args.output / (name + ".png")),
            )
            (args.output / (name + "-scene.json")).write_text(
                json.dumps(row, indent=2) + "\n"
            )
            rows[name] = {
                "mesh_sha256": row["mesh_sha256"],
                "scene_sha256": digest(args.output / (name + "-scene.json")),
            }
    (args.output / "display-report.json").write_text(
        json.dumps(
            {
                "study_report_sha256": digest(args.study / "report.json"),
                "baseline_scene_sha256": digest(args.baseline_scene),
                "actual_old_sources": len(report["old_source_ids"]),
                "whole_candidate_sources": len(report["source_ids"]),
                "scenes": rows,
                "scope": "Whole actual unchanged terrain/transport/support prefixes; candidate deck slab only. No new piers/hardware/consumer/world/device admission, no published images.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--study", type=Path, required=True)
    parser.add_argument("--baseline-scene", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
