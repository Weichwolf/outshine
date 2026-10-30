"""Matched actual whole-world and shifted-pier detail views from verified outputs."""

import argparse
import json
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from cycle_bridge_display import append
from stair_support_audit import quantized
from wien_support_verify import unpack


def run(args):
    if args.output.exists() or not args.output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("Fresh private system-temp display output required")
    report = json.loads((args.study / "report.json").read_text())
    verified = json.loads((args.study / "verification.json").read_text())
    baseline = json.loads(args.baseline_scene.read_text())
    binding = json.loads(
        (args.baseline_scene.parent / "display-report.json").read_text()
    )
    mesh = Path(baseline["mesh"])
    if (
        verified["report_sha256"] != digest(args.study / "report.json")
        or not verified["valid_whole_support_candidate"]
        or binding["study_report_sha256"] != report["deck_report_sha256"]
        or digest(mesh) != baseline["mesh_sha256"]
    ):
        raise ValueError(
            "Verified whole support and matching preceding deck scene required"
        )
    selected = report["selected"]
    path = args.study / (selected + ".npz")
    if digest(path) != report["methods"][selected]["mesh_sha256"]:
        raise ValueError("Actual saved support body bytes changed")
    bodies, _contacts, _data = unpack(path)
    with np.load(mesh, allow_pickle=False) as saved:
        before = dict(saved)
    after = append(
        before, [(quantized(body), 6) for body in bodies], np.array(baseline["colours"])
    )
    if not all(np.array_equal(after[k][: len(v)], v) for k, v in before.items()):
        raise ValueError(
            "Original complete terrain/roads/deck/pier array prefix changed"
        )
    args.output.mkdir(parents=True)
    np.savez_compressed(args.output / "after.npz", **after)
    rows = report["methods"][selected]["stations"]
    example = next(
        (r for r in rows if "product" in r and r["shift_m"]),
        next(r for r in rows if "product" in r),
    )
    vertices = bodies[example["product"]].vertices
    centre = (vertices.min(0) + vertices.max(0)) / 2
    cameras = {
        "whole": dict(baseline),
        "detail": dict(
            baseline,
            target=centre.tolist(),
            eye=(centre + [90, -130, 60]).tolist(),
            span_m=150,
            detail_view=True,
        ),
    }
    outputs = {}
    for view, camera in cameras.items():
        for phase, path in (("before", mesh), ("after", args.output / "after.npz")):
            name = phase + "-" + view
            scene = dict(
                camera,
                id="whole-raised-rail-support-" + name,
                mesh=str(path.resolve()),
                mesh_sha256=digest(path),
                output=str(args.output / (name + ".png")),
                profile=[1280, 720],
                samples=16,
            )
            target = args.output / (name + "-scene.json")
            target.write_text(json.dumps(scene, indent=2) + "\n")
            outputs[name] = {
                "scene_sha256": digest(target),
                "mesh_sha256": scene["mesh_sha256"],
            }
    (args.output / "display-report.json").write_text(
        json.dumps(
            {
                "study_report_sha256": digest(args.study / "report.json"),
                "verification_sha256": digest(args.study / "verification.json"),
                "baseline_scene_sha256": digest(args.baseline_scene),
                "source_example": {
                    k: example[k]
                    for k in ("source_id", "nominal_m", "shift_m", "product")
                },
                "old_scene_arrays_exact": True,
                "support_bodies": len(bodies),
                "scenes": outputs,
                "scope": "Actual complete preceding terrain/transport/deck/pier prefixes and verified new whole piers. Private 1280x720 diagnostic, no native/World/PBR/structural/device admission.",
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
