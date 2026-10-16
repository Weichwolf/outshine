"""Frozen source-bound full-scene experiment; no admission before saved-mesh replay."""

import argparse
import json
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from camera_height_inputs import digest
from curb_experiment import save_products
from cycle_bridge_audit import audit
from cycle_bridge_display import scenes
from cycle_bridge_geometry import build
from cycle_bridge_inputs import inputs
from cycle_bridge_problem import problem
from cycle_bridge_profile import solve
from cycle_bridge_supports import supports

HERE = Path(__file__).resolve().parent


def generate(args):
    began = time.perf_counter()
    if (
        args.output.exists()
        or subprocess.check_output(
            ["git", "status", "--porcelain", "--", "lib", str(HERE)], text=True
        ).strip()
    ):
        raise ValueError("fresh output and frozen clean source required")
    args.output.mkdir(parents=True)
    fixture = json.loads(args.fixture.read_text())
    p = problem(fixture)
    methods, heights = {}, None
    for name in ("chain", "lp"):
        result, note = solve(
            *(p[k] for k in ("stations", "lower", "upper", "rate")), method=name
        )
        methods[name] = note
        if result is None:
            raise ValueError("actual complete profile failed: " + str(note))
        if heights is not None and not np.allclose(result, heights, rtol=0, atol=1e-8):
            raise ValueError("equivalent fixed-chain profiles differ")
        if heights is None:
            heights = result
    geometry = {}
    for name in ("slices", "arrangement"):
        try:
            ribbon, note = build(p, heights, method=name)
        except ValueError as error:
            geometry[name] = {"valid": False, "reason": str(error)}
        else:
            geometry[name] = dict(note, valid=True)
    if not geometry["arrangement"]["valid"]:
        raise ValueError("actual noded bridge geometry invalid")
    table = np.array(geometry["arrangement"]["table"])
    proof = audit(fixture, ribbon, table, p["domain"])
    before, _fine, binding = inputs(args, fixture, p, ribbon)
    piers, pier_note = supports(fixture, p, ribbon)
    save_products(args.output / "piers.npz", piers)
    np.savez_compressed(
        args.output / "bridge.npz",
        body_vertices=ribbon.body.vertices,
        body_faces=ribbon.body.triangles,
        lower_vertices=ribbon.lower.vertices,
        upper_vertices=ribbon.upper.vertices,
        cap_faces=ribbon.upper.triangles,
        table=table,
    )
    np.savez_compressed(
        args.output / "profile-input.npz",
        **{k: p[k] for k in ("stations", "lower", "upper", "rate")},
    )
    display = scenes(
        args, before, [(ribbon.body, 3), *[(x.body, 10) for x in piers]], p
    )
    usage = json.loads((args.network / "comparison.json").read_text())["source_usage"]
    changed = {
        r["source_id"]
        for r in fixture["sources"]
        if r["role"] in ("bridge", "approach")
    }
    for row in usage:
        if row["source_id"] in changed:
            row.update(
                status="used", reason="", height_owner="cycle_bridge_station_profile"
            )
    (args.output / "source-usage.json").write_text(json.dumps(usage, indent=2) + "\n")
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "dirty": False,
        "bindings": binding,
        "profile_methods": methods,
        "geometry_methods": geometry,
        "construction_proof": proof,
        "supports": pier_note,
        "bridge_sha256": digest(args.output / "bridge.npz"),
        "piers_sha256": digest(args.output / "piers.npz"),
        "presentation": display,
        "source_usage_sha256": digest(args.output / "source-usage.json"),
        "profile_input_sha256": digest(args.output / "profile-input.npz"),
        "source_profiles": [
            dict(r, height_owner="affine_ground_plus_station_table")
            for r in fixture["sources"]
            if r["role"] in ("bridge", "approach")
        ],
        "public_travel": p["public_travel"].tolist(),
        "access_notes": p["access_notes"],
        "status": "requires_independent_saved_mesh_verification",
        "seconds": time.perf_counter() - began,
        "peak_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
    }
    (args.output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {k: report[k] for k in ("commit", "status", "seconds", "peak_rss_kib")}
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--fixture", type=Path, default=HERE / "fixtures/cycle-bridge-vienna.json"
    )
    for name in ("network", "baseline", "supports", "reference-root", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    resource.setrlimit(resource.RLIMIT_AS, (4 * 1024**3, 4 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (120, 120))
    generate(parser.parse_args())
