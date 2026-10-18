"""Exhaustive actual-source containment and bounded-pose demand comparison."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from connected_leaf import build_connected
from crown_demand import CrownDemand
from demand import Camera
from spatial_group_source import load_source


def containment(source, plans, templates):
    originals = [b.mesh(8, 1) for b in source.blades]
    worst = {method: -np.inf for method in plans}
    vertices = 0
    for i, node in enumerate(source.crown.hosts):
        source.crown._groups.clear()
        poses = source.crown.group(int(node), 128)
        for v in np.unique(poses["variants"]):
            selected = poses["variants"] == v
            for mesh in (originals[int(v)], templates[int(v)]["mesh"]):
                world = np.einsum(
                    "nij,vj->nvi", poses["frames"][selected], mesh["vertices"]
                )
                world += poses["origins"][selected, None]
                low, high = world.min((0, 1)), world.max((0, 1))
                vertices += world.shape[0] * world.shape[1]
                for method, p in plans.items():
                    worst[method] = max(
                        worst[method],
                        float(np.max(p.lower[i] - low)),
                        float(np.max(high - p.upper[i])),
                    )
    if any(v > 0 for v in worst.values()):
        raise AssertionError(f"actual source outside analytic envelope:{worst}")
    return {
        "transformed_vertex_checks": vertices,
        "maximum_signed_violation_m": worst,
        "leaves": len(source.crown.variants),
        "retained_groups": len(source.crown._groups),
    }


def cameras():
    return {
        "whole": Camera((0, -35, 12), (0, 0, 10)),
        "inside": Camera((0, 0, 12), (2, 0, 12)),
        "edge": Camera((5, -4, 12), (5, -1, 12)),
        "away": Camera((0, -35, 12), (0, -50, 12)),
    }


def study(inputs, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under/tmp")
    output.mkdir(parents=True, exist_ok=False)
    source = load_source(inputs)
    plans = {}
    build_times = {}
    with (
        patch(
            "outshine_ref.vegetation.Shoot.detail",
            side_effect=AssertionError("poses before need"),
        ),
        patch(
            "outshine_ref.vegetation.Blade.mesh",
            side_effect=AssertionError("mesh before need"),
        ),
    ):
        for method in ("root", "arc"):
            start = time.perf_counter()
            plans[method] = CrownDemand(source, method=method)
            build_times[method] = time.perf_counter() - start
    templates = {i: build_connected(b, i, size=16) for i, b in enumerate(source.blades)}
    proof = containment(source, plans, templates)
    # This full reference table exists only in the exhaustive comparator.
    _, reference = plans["root"].prepare((cameras()["whole"],), max_leaves=100000)
    np.testing.assert_array_equal(reference["ids"], np.arange(90000, dtype=np.uint64))
    np.savez_compressed(output / "reference-poses.npz", **reference)
    records = []
    for name, camera in cameras().items():
        for method, plan in plans.items():
            trials = []
            for _ in range(5):
                before = time.perf_counter()
                query = plan.query((camera,), max_leaves=100000)
                trials.append(time.perf_counter() - before)
            before = time.perf_counter()
            with patch(
                "crown_shoots.CrownShoots.tables",
                side_effect=AssertionError("whole table"),
            ):
                query, poses = plan.prepare((camera,), max_leaves=100000)
            pose_s = time.perf_counter() - before
            for key, values in poses.items():
                np.testing.assert_array_equal(values, reference[key][poses["ids"]])
            if name != "whole":
                np.savez_compressed(output / f"{name}-{method}-selection.npz", **query)
            records.append(
                {
                    "view": name,
                    "method": method,
                    "hosts": int(query["detail"].sum()),
                    "leaf_poses": query["requested_leaf_poses"],
                    "median_query_s": float(np.median(trials)),
                    "prepare_s": pose_s,
                    "output_pose_array_bytes": sum(v.nbytes for v in poses.values()),
                    "bounds_bytes": plan.lower.nbytes + plan.upper.nbytes,
                }
            )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_identity": source.identity,
        "bounds_build_s": build_times,
        "containment": proof,
        "cases": records,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Analytic whole-shoot bounds, source coarse8x1 and largest-padding connected16 carrier vertices on all90000 original poses. Camera-only selection, no shadow/occlusion/whole-forest/LOD/GPU admission. Full reference poses retained solely for this exhaustive comparison; production prepare only requests selected groups and preallocates final pose buffers. Actual normal-priority shared-host timings, not device evidence or cumulative allocation/traffic.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--inputs", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    a = p.parse_args()
    study(a.inputs, a.output)
