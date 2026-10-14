"""Pinned pair plans, grounded omissions and fair fresh-process recipe costs."""

import argparse
import collections
import hashlib
import json
import os
import resource
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
from overpass_checks import control
from overpass_products import products
from overpass_recipe import build
from overpass_ribbon import grade_audit, ribbon, source_frame

ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / "build/experiments/transport/overpasses"
FIXTURE = Path(__file__).resolve().parent / "fixtures/overpass-vienna-cases.json"


def digest(meshes):
    return hashlib.sha256(
        b"".join(m.vertices.tobytes() + m.triangles.tobytes() for m in meshes)
    ).hexdigest()


def partial_ground(case):
    source = case["lower"]
    if source["properties"].get("subclass") == "steps":
        return None
    try:
        frame = source_frame(source["xy"], source["width_m"])
        table = np.c_[
            frame["station"],
            np.interp(
                frame["station"], [0.0, frame["station"][-1]], source["end_heights_m"]
            ),
        ]
        mesh = ribbon(frame, table, source["width_m"], source["crossfall"])
        grade_audit(frame, mesh, table, source["grade"])
        return mesh
    except ValueError:
        return None


def write_case(case, meshes, report, method):
    name = case["id"] + "-" + method
    for role, mesh in zip(("upper", "lower"), meshes):
        np.savez_compressed(
            OUTPUT / f"{name}-{role}.npz",
            vertices=mesh.vertices,
            triangles=mesh.triangles,
        )
    plan, binding = products(case, meshes)
    binding.update(case=case, report=report, frame_id=plan.frame.identifier)
    (OUTPUT / f"{name}-products.json").write_text(json.dumps(binding, indent=2) + "\n")


def coverage():
    inputs = json.loads(FIXTURE.read_text())
    results = {}
    for method in ("parabolic", "cubic"):
        rows = []
        for case in inputs["cases"]:
            meshes, report = build(case, method=method)
            ground = meshes[1] if meshes is not None else partial_ground(case)
            report.update(
                case_id=case["id"],
                upper_used=meshes is not None,
                lower_used=ground is not None,
                mesh_sha256=digest(meshes) if meshes is not None else None,
                lower_omission_reason=""
                if ground is not None
                else "unsupported_or_invalid_ground_strip",
                component_scope="Independent hypothetical two-source plan; remaining tile sources retained outside this local experiment, not dropped from the world or certified against this pair.",
            )
            rows.append(report)
            if meshes is not None:
                write_case(case, meshes, report, method)
            elif ground is not None:
                np.savez_compressed(
                    OUTPUT / f"{case['id']}-{method}-partial-ground.npz",
                    vertices=ground.vertices,
                    triangles=ground.triangles,
                )
        results[method] = rows
    return {
        "methods": results,
        "inputs": inputs["inputs"],
        "fixture_sha256": hashlib.sha256(FIXTURE.read_bytes()).hexdigest(),
        "scope": inputs["selection"],
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
    }


def worker(method):
    resource.setrlimit(resource.RLIMIT_CPU, (10, 10))
    resource.setrlimit(resource.RLIMIT_AS, (2 * 1024**3, 2 * 1024**3))
    cases = json.loads(FIXTURE.read_text())["cases"]
    common = [
        case
        for case in cases
        if all(build(case, method=m)[0] is not None for m in ("parabolic", "cubic"))
    ]
    timings, hashes = [], []
    for _ in range(7):
        began = time.perf_counter_ns()
        outputs = [build(case, method=method) for case in common]
        timings.append(time.perf_counter_ns() - began)
        if any(meshes is None for meshes, _ in outputs):
            raise ValueError("common valid source unexpectedly failed")
        hashes.append(
            hashlib.sha256(
                "".join(digest(meshes) for meshes, _ in outputs).encode()
            ).hexdigest()
        )
    if len(set(hashes)) != 1:
        raise ValueError("repeated recipe output changed")
    memory = next(
        line
        for line in Path("/proc/self/status").read_text().splitlines()
        if line.startswith("VmHWM:")
    )
    return {
        "method": method,
        "cases": [c["id"] for c in common],
        "batch_ns": timings,
        "mesh_sha256": hashes[0],
        "current_exec_peak_rss_bytes": int(memory.split()[1]) * 1024,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--worker", choices=("parabolic", "cubic"))
    args = parser.parse_args()
    if args.worker:
        print(json.dumps(worker(args.worker)))
        return
    OUTPUT.mkdir(parents=True, exist_ok=True)
    report = coverage()
    costs = []
    for method in ("parabolic", "cubic"):
        workers = []
        for _ in range(3):
            result = subprocess.run(
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
                env=dict(os.environ, OPENBLAS_NUM_THREADS="1"),
            )
            workers.append(json.loads(result.stdout))
        if len({w["mesh_sha256"] for w in workers}) != 1:
            raise ValueError("fresh recipe worker output changed")
        costs.append(
            {
                "method": method,
                "workers": workers,
                "p50_recipe_ms_per_case": float(
                    np.median([n for w in workers for n in w["batch_ns"]])
                )
                / len(workers[0]["cases"])
                / 1e6,
                "peak_rss_bytes": max(
                    w["current_exec_peak_rss_bytes"] for w in workers
                ),
            }
        )
        print(
            json.dumps(
                {
                    "method": method,
                    "reasons": dict(
                        collections.Counter(
                            r["reason"] or "valid" for r in report["methods"][method]
                        )
                    ),
                }
            ),
            flush=True,
        )
    if costs[0]["workers"][0]["cases"] != costs[1]["workers"][0]["cases"]:
        raise ValueError("cost methods used different source cases")
    report.update(
        costs=costs,
        cost_scope="Three fresh single-BLAS-thread workers/method; seven identical common-pair batches. Complete geometry/preparation/grade/clearance certificate included; JSON loading/warmup/products/serialization/rendering excluded. Whole-exec peak includes imported packages, inputs and both warmup methods; CPU10s, AS2GiB, parent30s. No isolated algorithm/device/frame cost claim.",
    )
    (OUTPUT / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    mesh, note = build(control(240.0))
    write_case(control(240.0), mesh, note, "parabolic")


if __name__ == "__main__":
    main()
