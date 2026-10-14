"""Fair operator comparison and portable actual bridge/tunnel height-surface pairs."""

import argparse
import hashlib
import importlib.util
import json
import os
import resource
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import vertical_clearance

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]
OUTPUT = ROOT / "build/experiments/transport/height-gap-library"
REFERENCE_COMMIT = "e43a3fd733843f04d48794c18a04b82a52b4f8dc"
REFERENCE_SHA256 = "552158e95aa869c417a20a73e0aa190d1058530cf9eee0f2c9844f458aaf0ae4"


def legacy():
    path = "test/experiments/transport/overpass_clearance.py"
    source = subprocess.check_output(
        ["git", "show", REFERENCE_COMMIT + ":" + path], cwd=ROOT
    )
    digest = hashlib.sha256(source).hexdigest()
    if digest != REFERENCE_SHA256:
        raise ValueError("owned previous operator source changed")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    snapshot = OUTPUT / "previous_clearance.py"
    snapshot.write_bytes(source)
    spec = importlib.util.spec_from_file_location(
        "outshine_previous_clearance", snapshot
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.vertical_clearance, digest


def prepare():
    from bridge_continuation_experiment import inputs as continuation_inputs
    from bridge_continuation_recipe import build as continuation_build
    from overpass_experiment import FIXTURE as PAIRS
    from overpass_recipe import build as pair_build
    from tunnel_portal_recipe import build as portal_build
    from tunnel_portal_showcase import inputs as portal_inputs

    pairs = []
    for i, case in enumerate(json.loads(PAIRS.read_text())["cases"]):
        for method in ("parabolic", "cubic"):
            meshes, report = pair_build(case, method=method)
            if meshes is not None:
                upper, lower = meshes
                pairs.append(
                    (
                        f"overpass-{i}-{method}",
                        lower,
                        Mesh(upper.vertices - [0, 0, 0.35], upper.triangles),
                    )
                )
    captured, approaches, root, old, _used = continuation_inputs()
    for method in ("rebuild", "retain"):
        meshes, report = continuation_build(
            captured["sources"], approaches, root, old, geometry_method=method
        )
        if not report["valid"]:
            raise ValueError(report["reason"])
        upper, lower = meshes
        pairs.append(
            (
                "continuation-" + method,
                lower,
                Mesh(upper.vertices - [0, 0, 0.35], upper.triangles),
            )
        )
    fixture, previous = portal_inputs()
    for method in ("parabolic", "cubic"):
        model, report = portal_build(fixture, previous, method=method)
        if not report["valid"]:
            raise ValueError(report["reason"])
        pairs.extend(
            (
                ("old-floor-forward-" + method, previous[0], model["top"]),
                ("old-floor-backward-" + method, model["top"], previous[0]),
                ("portal-cover-" + method, model["roof"], model["terrain"]),
                (
                    "portal-headroom-" + method,
                    model["floor"],
                    Mesh(
                        model["roof"].vertices - [0, 0, 0.35], model["roof"].triangles
                    ),
                ),
            )
        )
    arrays, names = {}, []
    for i, (name, lower, upper) in enumerate(pairs):
        names.append(name)
        for prefix, mesh in (("lower", lower), ("upper", upper)):
            arrays[f"{prefix}_vertices_{i}"] = mesh.vertices
            arrays[f"{prefix}_faces_{i}"] = mesh.triangles
    OUTPUT.mkdir(parents=True, exist_ok=True)
    np.savez_compressed(OUTPUT / "pairs.npz", **arrays)
    (OUTPUT / "pairs.json").write_text(
        json.dumps({"names": names, "reference_commit": REFERENCE_COMMIT}, indent=2)
        + "\n"
    )
    return pairs


def load_pairs():
    names = json.loads((OUTPUT / "pairs.json").read_text())["names"]
    with np.load(OUTPUT / "pairs.npz") as data:
        return [
            (
                name,
                Mesh(data[f"lower_vertices_{i}"], data[f"lower_faces_{i}"]),
                Mesh(data[f"upper_vertices_{i}"], data[f"upper_faces_{i}"]),
            )
            for i, name in enumerate(names)
        ]


def evaluate(operator, pairs):
    results = []
    for name, lower, upper in pairs:
        result = operator(lower, upper)
        if not result["valid"]:
            raise ValueError((name, result))
        results.append(
            {
                "name": name,
                "minimum_clearance_m": result["minimum_clearance_m"],
                "witness_xy": result["witness_xy"],
                "checks": result["checks"],
            }
        )
    return results


def worker(method):
    resource.setrlimit(resource.RLIMIT_CPU, (15, 15))
    resource.setrlimit(resource.RLIMIT_AS, (2 * 1024**3, 2 * 1024**3))
    pairs = load_pairs()
    operator = legacy()[0] if method == "previous" else vertical_clearance
    expected = evaluate(operator, pairs)
    times = []
    for _ in range(5):
        began = time.perf_counter()
        result = evaluate(operator, pairs)
        times.append((time.perf_counter() - began) * 1000)
        if result != expected:
            raise ValueError("operator result changed between samples")
    print(
        json.dumps(
            {
                "method": method,
                "samples_ms": times,
                "results": expected,
                "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
                * 1024,
            }
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--worker", choices=("previous", "library"))
    args = parser.parse_args()
    if args.worker:
        worker(args.worker)
        return
    pairs = prepare()
    old, reference_sha = legacy()
    before, after = evaluate(old, pairs), evaluate(vertical_clearance, pairs)
    difference = max(
        abs(a["minimum_clearance_m"] - b["minimum_clearance_m"])
        for a, b in zip(before, after)
    )
    if difference > 1e-10:
        raise ValueError("height gap result changed")
    rows = []
    for method in ("previous", "library"):
        fresh = [
            json.loads(
                subprocess.run(
                    [sys.executable, __file__, "--worker", method],
                    cwd=ROOT,
                    env=os.environ
                    | {"OPENBLAS_NUM_THREADS": "1", "OMP_NUM_THREADS": "1"},
                    capture_output=True,
                    text=True,
                    timeout=40,
                    check=True,
                ).stdout
            )
            for _ in range(3)
        ]
        rows.append(
            {
                "method": method,
                "p50_ms": float(np.median([t for r in fresh for t in r["samples_ms"]])),
                "peak_rss_bytes": max(r["peak_rss_bytes"] for r in fresh),
                "workers": fresh,
            }
        )
    commit = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
    ).strip()
    (OUTPUT / "comparison.json").write_text(
        json.dumps(
            {
                "commit": commit,
                "reference_commit": REFERENCE_COMMIT,
                "reference_source_sha256": reference_sha,
                "pairs": len(pairs),
                "maximum_result_difference_m": difference,
                "before": before,
                "after": after,
                "methods": rows,
                "scope": "Offline complete surface queries; inputs/imports outside timings, included in worker peak. Cooperative geometry time with separate hard worker limits; not native/device proof.",
            },
            indent=2,
        )
        + "\n"
    )
    print(
        json.dumps(
            {
                "pairs": len(pairs),
                "maximum_result_difference_m": difference,
                "methods": [
                    {k: r[k] for k in ("method", "p50_ms", "peak_rss_bytes")}
                    for r in rows
                ],
            }
        )
    )


if __name__ == "__main__":
    main()
