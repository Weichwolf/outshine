"""Pinned complete field, retained-plan failures and equal-quality fresh worker costs."""

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
from bridge_approach_experiment import inputs as previous_inputs
from bridge_approach_recipe import build as previous_build
from bridge_continuation_recipe import TOP_ROLES, build
from capture_bridge_continuations import FIXTURE, PIN
from shared_bridge_products import products

ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / "build/experiments/transport/bridge-continuations"


def inputs():
    if hashlib.sha256(FIXTURE.read_bytes()).hexdigest() != PIN:
        raise ValueError("pinned continuation fixture changed")
    captured = json.loads(FIXTURE.read_text())
    approaches, old_sources, root, root_report = previous_inputs()
    top, report = previous_build(approaches["approaches"], *root)
    if not root_report["valid"] or not report["valid"]:
        raise ValueError("previous independent plan is not valid")
    used = {s["source_id"] for s in old_sources if s["role"] != "deferred"}
    used |= {
        s["source_id"]
        for c in approaches["approaches"]
        for s in (c["bridge"], c["ground"])
    }
    return captured, approaches["approaches"], root, (top, root[1]), used


def usage(captured, report, old_used):
    selected = {s["source_id"]: s for s in captured["sources"]}
    used = set(report["used_source_ids"]) if report["valid"] else old_used
    rows = []
    for entry in captured["inputs"]:
        for identity in entry["all_axis_ids"]:
            source = selected.get(identity)
            reason = (
                ""
                if identity in used
                else source["omission_reason"] or report["reason"]
                if source
                else "outside_connected_three_tile_bridge_field"
            )
            rows.append(
                {
                    "source_id": identity,
                    "status": "used" if identity in used else "not_used",
                    "reason": reason,
                }
            )
        rows.extend(
            {
                "source_id": identity,
                "status": "not_used",
                "reason": "polygon_source_has_no_axis_recipe",
            }
            for identity in entry["polygon_ids"]
        )
    if len({r["source_id"] for r in rows}) != len(rows) or any(
        r["status"] == "not_used" and not r["reason"] for r in rows
    ):
        raise ValueError("complete unique explained source usage required")
    return rows


def mesh_hash(meshes):
    digest = hashlib.sha256()
    for mesh in meshes:
        digest.update(mesh.vertices.tobytes())
        digest.update(mesh.triangles.tobytes())
    return digest.hexdigest()


def worker(method):
    resource.setrlimit(resource.RLIMIT_CPU, (15, 15))
    resource.setrlimit(resource.RLIMIT_AS, (2 * 1024**3, 2 * 1024**3))
    captured, approaches, root, old, _used = inputs()
    if not build(captured["sources"], approaches, root, old, geometry_method=method)[1][
        "valid"
    ]:
        raise ValueError("no same-quality warmup field")
    timings, hashes = [], []
    for _ in range(5):
        began = time.perf_counter_ns()
        meshes, report = build(
            captured["sources"], approaches, root, old, geometry_method=method
        )
        timings.append(time.perf_counter_ns() - began)
        if not report["valid"]:
            raise ValueError("worker omitted requested field")
        hashes.append(mesh_hash(meshes))
    if len(set(hashes)) != 1:
        raise ValueError("fixed source field changed between samples")
    peak = next(
        int(line.split()[1]) * 1024
        for line in Path("/proc/self/status").read_text().splitlines()
        if line.startswith("VmHWM:")
    )
    return {
        "method": method,
        "samples_ns": timings,
        "vmhwm_bytes": peak,
        "mesh_sha256": hashes[0],
    }


def benchmark(method):
    rows = []
    environment = dict(
        os.environ, OPENBLAS_NUM_THREADS="1", OMP_NUM_THREADS="1", MKL_NUM_THREADS="1"
    )
    for _ in range(3):
        completed = subprocess.run(
            [sys.executable, __file__, "--worker", method],
            cwd=ROOT,
            env=environment,
            capture_output=True,
            text=True,
            check=True,
            timeout=40,
        )
        rows.append(json.loads(completed.stdout))
    if len({r["mesh_sha256"] for r in rows}) != 1:
        raise ValueError("fresh workers produced different plans")
    samples = [value for r in rows for value in r["samples_ns"]]
    return {
        "workers": rows,
        "p50_ms": float(np.median(samples) / 1e6),
        "maximum_vmhwm_bytes": max(r["vmhwm_bytes"] for r in rows),
        "scope": "One complete checked extension; old root/approach and source preparation outside time, inside memory; three fresh children, five samples each after warmup. CPU15s/AS2GiB, parent40s, BLAS1. GEOS/solver limits cooperative; no frame/device measurement.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--worker", choices=("rebuild", "retain"))
    args = parser.parse_args()
    if args.worker:
        print(json.dumps(worker(args.worker)))
        return
    OUTPUT.mkdir(parents=True, exist_ok=True)
    captured, approaches, root, old, old_used = inputs()
    comparison = {}
    for method in ("rebuild", "retain"):
        meshes, report = build(
            captured["sources"], approaches, root, old, geometry_method=method
        )
        if not report["valid"]:
            raise ValueError(report["reason"])
        rows = usage(captured, report, old_used)
        report["usage"] = rows
        _plan, metadata = products(
            captured["sources"],
            meshes,
            report,
            top_roles=TOP_ROLES,
            top_kind="joined_transport",
        )
        metadata["unresolved_transport_contacts"] = [
            s for s in captured["sources"] if s["role"] == "unresolved"
        ]
        for kind, mesh in zip(("top", "ground"), meshes):
            np.savez(
                OUTPUT / (method + "-" + kind + ".npz"),
                vertices=mesh.vertices,
                triangles=mesh.triangles,
            )
        (OUTPUT / (method + "-plan.json")).write_text(
            json.dumps(report, indent=2) + "\n"
        )
        (OUTPUT / (method + "-products.json")).write_text(
            json.dumps(metadata, indent=2) + "\n"
        )
        comparison[method] = benchmark(method)
    (OUTPUT / "comparison.json").write_text(json.dumps(comparison, indent=2) + "\n")
    print(
        json.dumps(
            {
                method: {k: v for k, v in data.items() if k != "workers"}
                for method, data in comparison.items()
            }
        )
    )


if __name__ == "__main__":
    main()
