"""Pinned shared bridge plans, complete usage ledger and bounded same-quality costs."""

import argparse
import hashlib
import json
import os
import resource
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
from shared_bridge_checks import FIXTURE
from shared_bridge_products import products
from shared_bridge_recipe import build

ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / "build/experiments/transport/shared-bridge"


def mesh_hash(meshes):
    return hashlib.sha256(
        b"".join(
            m.vertices.tobytes() + m.triangles.tobytes()
            for m in meshes
            if m is not None
        )
    ).hexdigest()


def source_usage(inputs, meshes, report):
    selected = {s["source_id"]: s for s in inputs["sources"]}
    rows = []
    for identity in inputs["all_axis_source_ids"]:
        source = selected.get(identity)
        if source is None:
            status, reason = "not_used", inputs["unselected_reason"]
        elif source["role"] == "deferred":
            status, reason = "not_used", source["omission_reason"]
        else:
            used = meshes[0 if source["role"] == "deck" else 1] is not None
            status, reason = ("used", "") if used else ("not_used", report["reason"])
        rows.append({"source_id": identity, "status": status, "reason": reason})
    rows.extend(
        {"source_id": s, "status": "not_used", "reason": inputs["deferred_reason"]}
        for s in inputs["deferred_source_ids"]
    )
    return rows


def worker(method):
    resource.setrlimit(resource.RLIMIT_CPU, (10, 10))
    resource.setrlimit(resource.RLIMIT_AS, (2 * 1024**3, 2 * 1024**3))
    sources = json.loads(FIXTURE.read_text())["sources"]
    if not build(sources, method=method)[1]["valid"]:
        raise ValueError("benchmark warmup has no valid result")
    timings, hashes, reports = [], [], []
    for _ in range(5):
        start = time.perf_counter_ns()
        meshes, report = build(sources, method=method)
        timings.append(time.perf_counter_ns() - start)
        if not report["valid"]:
            raise ValueError("same-quality benchmark returned invalid geometry")
        hashes.append(mesh_hash(meshes))
        reports.append(
            {k: report[k] for k in ("deck_height_m", "maximum_deck_port_lift_m")}
        )
    if len(set(hashes)) != 1:
        raise ValueError("repeated component plans differ")
    peak = next(
        row
        for row in Path("/proc/self/status").read_text().splitlines()
        if row.startswith("VmHWM:")
    )
    return {
        "method": method,
        "repetitions_ns": timings,
        "mesh_sha256": hashes[0],
        "current_exec_peak_rss_bytes": int(peak.split()[1]) * 1024,
        "reports": reports,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--worker", choices=("cone", "polygon"))
    args = parser.parse_args()
    if args.worker:
        print(json.dumps(worker(args.worker)))
        return
    OUTPUT.mkdir(parents=True, exist_ok=True)
    inputs = json.loads(FIXTURE.read_text())
    comparisons = []
    for method, lift in (("cone", 0.0), ("cone", 8.0), ("polygon", 8.0)):
        meshes, report = build(inputs["sources"], method=method, port_lift_m=lift)
        name = f"{method}-lift-{lift:g}"
        plan, binding = products(inputs["sources"], meshes, report)
        report.update(
            usage=source_usage(inputs, meshes, report),
            mesh_sha256=mesh_hash(meshes),
            product_meshes=len(plan.meshes),
        )
        comparisons.append(report)
        for role, mesh in zip(("deck", "ground"), meshes):
            if mesh is not None:
                np.savez_compressed(
                    OUTPUT / f"{name}-{role}.npz",
                    vertices=mesh.vertices,
                    triangles=mesh.triangles,
                )
        (OUTPUT / f"{name}-products.json").write_text(
            json.dumps(binding, indent=2) + "\n"
        )
    costs = []
    environment = dict(os.environ, OPENBLAS_NUM_THREADS="1", OMP_NUM_THREADS="1")
    for method in ("cone", "polygon"):
        workers = []
        for _ in range(3):
            run = subprocess.run(
                [
                    sys.executable,
                    "-W",
                    "error",
                    str(Path(__file__)),
                    "--worker",
                    method,
                ],
                capture_output=True,
                text=True,
                check=True,
                timeout=30,
                env=environment,
            )
            workers.append(json.loads(run.stdout))
        if len({r["mesh_sha256"] for r in workers}) != 1:
            raise ValueError("fresh component workers disagree")
        costs.append(
            {
                "method": method,
                "workers": workers,
                "p50_component_ms": float(
                    np.median([v / 1e6 for w in workers for v in w["repetitions_ns"]])
                ),
                "peak_rss_bytes": max(
                    w["current_exec_peak_rss_bytes"] for w in workers
                ),
            }
        )
    document = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "fixture_sha256": hashlib.sha256(FIXTURE.read_bytes()).hexdigest(),
        "inputs": inputs,
        "methods": comparisons,
        "costs": costs,
        "cost_scope": "Same sources, priors, isotropic 0.025 grade, 2 m ground change, 8 m deck port lift, 4.5 m clearance and 2 s solver limit. LP uses an inscribed 16-sided grade polygon; its feasible set is smaller. Five whole component builds per fresh worker after one warmup, three workers per method. Geometry, source targets, fitting, complete clearance and port contacts included; fixture JSON/imports/warmup/product serialization/render excluded from timings but included in current-exec VmHWM. CPU10s/AS2GiB/parent30s, one BLAS thread; GEOS/solver steps may exceed cooperative phase limits.",
    }
    (OUTPUT / "comparison.json").write_text(json.dumps(document, indent=2) + "\n")
    print(
        json.dumps(
            {
                "valid": [r["valid"] for r in comparisons],
                "costs": [
                    {k: c[k] for k in ("method", "p50_component_ms", "peak_rss_bytes")}
                    for c in costs
                ],
            }
        )
    )


if __name__ == "__main__":
    main()
