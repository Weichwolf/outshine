"""Fixed-inventory four-way form experiment and independent whole-source bounds."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from compact_crown import CompactCrown
from connected_leaf import build_connected
from limb_experiment import canonical_wood
from oak_scaffold import tapered_radii
from oak_skin_experiment import seed_continuations
from shoot_form import ShootForm
from spatial_group_source import load_source

TRAITS = {
    "original": {},
    "sizes": {"scale_spread": 0.55},
    "roll": {"leaf_roll_spread": 0.85, "host_roll_spread": 0.3},
    "combined": {
        "scale_spread": 0.55,
        "leaf_roll_spread": 0.85,
        "host_roll_spread": 0.3,
    },
}


def containment(source, form, poses):
    owners = np.repeat(
        np.arange(len(form.scale)), np.diff(form.base.data["offsets"]).astype(int)
    )
    maximum = -np.inf
    checked = 0
    for variant, blade in enumerate(source.blades):
        rows = np.flatnonzero(poses["variants"] == variant)
        source_mesh = blade.mesh(8, 1)
        carrier = build_connected(blade, variant, size=16)["mesh"]
        for begin in range(0, len(rows), 512):
            chosen = rows[begin : begin + 512]
            for mesh in (source_mesh, carrier):
                points = np.einsum(
                    "nij,vj->nvi", poses["frames"][chosen], mesh["vertices"]
                )
                points += poses["origins"][chosen, None]
                low = form.lower[owners[chosen]]
                high = form.upper[owners[chosen]]
                maximum = max(
                    maximum,
                    float((low[:, None] - points).max()),
                    float((points - high[:, None]).max()),
                )
                checked += len(chosen) * len(mesh["vertices"])
    if maximum > 0:
        raise AssertionError(f"form outside its declared envelope:{maximum}")
    return {"transformed_vertex_checks": checked, "maximum_signed_violation_m": maximum}


def prepare(source, inputs, output):
    base = CompactCrown.build(source)
    for name, traits in TRAITS.items():
        ShootForm.build(source, base, **traits).save(output / f"{name}.npz")
    metadata = json.loads((inputs / "comparison.json").read_text())
    crown, recipe = source.crown, metadata["recipe"]
    radii = tapered_radii(
        crown.parents, recipe["root_radius_m"], recipe["terminal_radius_m"]
    )
    seed_count = metadata["products"]["spatial"]["source_seed_nodes"]
    hints = seed_continuations(crown.parents, crown.parents[:seed_count])
    result = canonical_wood(crown, radii=radii, max_nodes=16384, continuations=hints)
    if result is None:
        raise ValueError("original whole C1 wood exceeds declared capacity")
    wood, _ = result
    np.savez_compressed(output / "wood.npz", **wood)
    target_host = int(
        np.argmin(np.linalg.norm(base.data["origins"] - (0, -6, 10), axis=1))
    )
    target = (
        base.data["origins"][target_host] + base.data["frames"][target_host, :, 1] * 0.2
    )
    return {
        "source_identity": source.identity,
        "near": {
            "eye": (target + (0.35, -2.6, 0.6)).tolist(),
            "target": target.tolist(),
            "fov_y_deg": 45,
        },
        "whole": {"eye": [0, -26, 10], "target": [0, 0, 9], "fov_y_deg": 50},
        "target_host_node": int(base.data["hosts"][target_host]),
        "wood_triangles": len(wood["faces"]),
    }


def run(inputs, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under/tmp")
    output.mkdir(parents=True, exist_ok=False)
    source = load_source(inputs)
    start = time.perf_counter()
    with (
        patch("outshine_ref.vegetation.Shoot.detail", side_effect=AssertionError),
        patch("outshine_ref.vegetation.Blade.mesh", side_effect=AssertionError),
    ):
        configuration = prepare(source, inputs, output)
    (output / "scene.json").write_text(json.dumps(configuration, indent=2) + "\n")
    build_s = time.perf_counter() - start
    records = []
    for name in TRAITS:
        form = ShootForm.load(output / f"{name}.npz", expected_source=source.identity)
        before = time.perf_counter()
        poses = form.evaluate(
            np.ones(len(form.scale), bool), max_leaves=100000, anchors=True
        )
        elapsed = time.perf_counter() - before
        np.testing.assert_array_equal(poses["ids"], np.arange(90000, dtype=np.uint64))
        np.testing.assert_array_equal(poses["variants"], source.crown.variants)
        analytic = sum(
            b.area() * int((poses["variants"] == i).sum())
            for i, b in enumerate(source.blades)
        )
        records.append(
            {
                "mode": name,
                "identity": form.identity,
                "leaves": len(poses["ids"]),
                "file_bytes": (output / f"{name}.npz").stat().st_size,
                "evaluate_whole_s": elapsed,
                "analytic_leaf_area_m2": analytic,
                "shoot_scale_min_max_mean": [
                    float(form.scale.min()),
                    float(form.scale.max()),
                    float(form.scale.mean()),
                ],
                "roll_mean_std_rad": [float(form.roll.mean()), float(form.roll.std())],
                **containment(source, form, poses),
            }
        )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "scene": configuration,
        "prepare_s": build_s,
        "cases": records,
        "process_peak_rss_bytes_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * 1024,
        "scope": "Authored source-preserving inventory form controls, new poses/identity; same original C1 wood and procedural material. Independent source/carrier vertex bounds. Shared-host CPU timing, no botanical/native/LOD/shadow/wind/device admission.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    run(args.inputs, args.output)
