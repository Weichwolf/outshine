"""Append actual whole-contact piers to a verified unchanged stair/terrain scene."""

import argparse
import json
import resource
import time
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from cycle_bridge_display import append
from stair_cycle_inputs import FIXTURE
from stair_support_audit import quantized
from stair_support_plan import inputs, plan
from stair_terrain_experiment import frozen
from wien_support_experiment import packed


def display(output, baseline, products):
    original = dict(np.load(baseline / "after-display.npz"))
    scene = json.loads((baseline / "after-overview-scene.json").read_text())
    palette = np.array(scene["colours"])
    shown = append(original, [(quantized(p.body), 6) for p in products], palette)
    np.savez_compressed(output / "after-display.npz", **shown)
    (output / "before-display.npz").symlink_to(
        (baseline / "after-display.npz").resolve()
    )
    points = np.vstack([p.body.vertices for p in products])
    centre = (points.min(0) + points.max(0)) / 2
    for view in ("overview", "junction"):
        camera = dict(scene)
        if view == "junction":
            camera.update(
                target=centre.tolist(),
                eye=(centre + [240, -320, 160]).tolist(),
                span_m=420,
                detail_view=True,
            )
        for phase in ("before", "after"):
            source = output / (phase + "-display.npz")
            row = dict(
                camera,
                id="Wien-stair-support-" + phase + "-" + view,
                mesh=str(source),
                mesh_sha256=digest(source),
                output=str(output / (phase + "-" + view + ".png")),
            )
            (output / (phase + "-" + view + "-scene.json")).write_text(
                json.dumps(row, indent=2) + "\n"
            )
    return {
        "after_display_sha256": digest(output / "after-display.npz"),
        "baseline_array_bytes": sum(a.nbytes for a in original.values()),
        "after_array_bytes": sum(a.nbytes for a in shown.values()),
    }


def generate(args):
    commit = frozen()
    if args.output.exists():
        raise ValueError("fresh explicit stair support output required")
    began = time.perf_counter()
    rows, road, lower, context, artifacts = inputs(args.products, args.fixture)
    args.output.mkdir(parents=True)
    methods, selected, chosen = {}, None, None
    for method in ("fixed", "shifted"):
        products, note = plan(rows, road, lower, method=method)
        body = packed(
            [p.body for p in products],
            [(p.lower_contact, p.upper_contact) for p in products],
        )
        target = args.output / (method + ".npz")
        np.savez_compressed(target, **body)
        note.update(
            mesh_sha256=digest(target),
            owned_output_array_bytes=sum(a.nbytes for a in body.values()),
        )
        methods[method] = note
        if selected is None or len(products) > len(selected):
            selected, chosen = products, method
        print(
            json.dumps({key: note[key] for key in ("method", "accepted", "seconds")}),
            flush=True,
        )
    if not selected:
        raise ValueError("no admitted piers; unchanged baseline remains available")
    report = {
        "commit": commit,
        "fixture_path": str(args.fixture.resolve()),
        "fixture_sha256": digest(args.fixture),
        "baseline": str(args.products.resolve()),
        "baseline_report_sha256": digest(args.products / "comparison.json"),
        "baseline_artifacts": artifacts,
        "origin_lat_lon": context["origin_lat_lon"],
        "frame": "Shared retained local ENU XY/Z metres; decoded Mapterhorn height datum unchanged, no geodetic conversion or measured deck claim.",
        "methods": methods,
        "selected": chosen,
        "display": display(args.output, args.products, selected),
        "seconds": time.perf_counter() - began,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "admitted_source_ids": [],
        "scope": "Geometric procedural piers on unchanged selected products, conservative other-source exclusion in both precisions. Explicit omissions remain local; no structural calculation, complete source admission, foundation/soil capacity or native integration.",
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")


def arguments():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument(
        "--fixture",
        type=Path,
        default=FIXTURE.with_name("stair-cycle-south-vienna.json"),
    )
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    resource.setrlimit(resource.RLIMIT_AS, (3 * 1024**3,) * 2)
    generate(arguments())
