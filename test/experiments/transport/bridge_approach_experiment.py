"""Pinned real approaches, same-quality profile/mesh costs and complete usage."""

import argparse
import copy
import hashlib
import json
import os
import resource
import subprocess
import sys
import time
from pathlib import Path

import numpy as np
from bridge_approach_checks import FIXTURE, ROOT_FIXTURE
from bridge_approach_recipe import build
from shared_bridge_products import products
from shared_bridge_recipe import build as build_root

ROOT = Path(__file__).resolve().parents[3]
OUTPUT = ROOT / "build/experiments/transport/bridge-approaches"


def inputs():
    captured = json.loads(FIXTURE.read_text())
    sources = copy.deepcopy(json.loads(ROOT_FIXTURE.read_text())["sources"])
    for source in sources:
        if source["role"] == "deferred":
            source["omission_reason"] = "buildings_floor_surface_not_planned"
    base, report = build_root(sources)
    if not report["valid"]:
        raise ValueError("current shared bridge reference is not valid")
    return captured, sources, base, report


def originals(captured, sources):
    return (
        sources
        + [s for c in captured["approaches"] for s in (c["bridge"], c["ground"])]
        + captured["conflicts"]
    )


def usage(captured, sources, report):
    selected = {s["source_id"]: s for s in originals(captured, sources)}
    rows = []
    for tile in captured["inputs"]:
        for identity in tile["all_axis_ids"]:
            source = selected.get(identity)
            reason = (
                "outside_selected_approach_plan"
                if source is None
                else source.get("omission_reason", source.get("reason", ""))
            )
            if (
                source is not None
                and source["role"].startswith("approach")
                and not report["valid"]
            ):
                reason = report["reason"]
            rows.append(
                {
                    "source_id": identity,
                    "status": "not_used" if reason else "used",
                    "reason": reason,
                }
            )
        rows.extend(
            {
                "source_id": identity,
                "status": "not_used",
                "reason": "polygon_source_has_no_axis_recipe",
            }
            for identity in tile["polygon_ids"]
        )
    return rows


def mesh_hash(mesh):
    return hashlib.sha256(
        mesh.vertices.tobytes() + mesh.triangles.tobytes()
    ).hexdigest()


def worker(method, geometry):
    resource.setrlimit(resource.RLIMIT_CPU, (10, 10))
    resource.setrlimit(resource.RLIMIT_AS, (2 * 1024**3, 2 * 1024**3))
    captured, _sources, base, _root_report = inputs()
    if not build(
        captured["approaches"], *base, method=method, geometry_method=geometry
    )[1]["valid"]:
        raise ValueError("warmup has no same-quality approach plan")
    timings, hashes, geometries = [], [], []
    for _ in range(5):
        began = time.perf_counter_ns()
        mesh, report = build(
            captured["approaches"], *base, method=method, geometry_method=geometry
        )
        timings.append(time.perf_counter_ns() - began)
        if not report["valid"]:
            raise ValueError("benchmark returned an unchecked approach plan")
        hashes.append(mesh_hash(mesh))
        geometries.append(report["geometry"])
    if len(set(hashes)) != 1:
        raise ValueError("repeated source plan changed")
    peak = next(
        row
        for row in Path("/proc/self/status").read_text().splitlines()
        if row.startswith("VmHWM:")
    )
    return {
        "profile": method,
        "geometry": geometry,
        "repetitions_ns": timings,
        "mesh_sha256": hashes[0],
        "current_exec_peak_rss_bytes": int(peak.split()[1]) * 1024,
        "geometry_reports": geometries,
    }


def write_plans(captured, sources, base):
    rows = []
    for method in ("parabolic", "cubic"):
        for geometry in ("slices", "midpoints"):
            mesh, report = build(
                captured["approaches"], *base, method=method, geometry_method=geometry
            )
            report.update(
                usage=usage(captured, sources, report),
                unresolved_contacts=[
                    {
                        "source_id": s["source_id"],
                        "reason": s.get("omission_reason", s.get("reason", "")),
                    }
                    for s in originals(captured, sources)
                    if s["role"] in ("deferred", "unresolved")
                ],
                all_source_contacts_resolved=False,
            )
            rows.append(report)
            top = mesh if mesh is not None else base[0]
            roles = (
                ("deck", "approach_bridge", "approach_ground")
                if mesh is not None
                else ("deck",)
            )
            plan, binding = products(
                originals(captured, sources),
                (top, base[1]),
                report,
                top_roles=roles,
                top_kind="joined_transport",
            )
            binding["additional_unresolved_contacts"] = captured["conflicts"]
            binding["buildings_response"] = (
                "https://github.com/Weichwolf/outshine/issues/111#issuecomment-6168650515"
            )
            name = method + "-" + geometry
            np.savez_compressed(
                OUTPUT / f"{name}.npz", vertices=top.vertices, triangles=top.triangles
            )
            (OUTPUT / f"{name}-products.json").write_text(
                json.dumps(binding, indent=2) + "\n"
            )
            report["mesh_sha256"] = mesh_hash(top)
            report["product_meshes"] = len(plan.meshes)
    return rows


def costs():
    result = []
    environment = dict(os.environ, OPENBLAS_NUM_THREADS="1", OMP_NUM_THREADS="1")
    for method in ("parabolic", "cubic"):
        for geometry in ("slices", "midpoints"):
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
                        geometry,
                    ],
                    capture_output=True,
                    text=True,
                    check=True,
                    timeout=30,
                    env=environment,
                )
                workers.append(json.loads(run.stdout))
            if len({w["mesh_sha256"] for w in workers}) != 1:
                raise ValueError("fresh workers disagree")
            result.append(
                {
                    "profile": method,
                    "geometry": geometry,
                    "workers": workers,
                    "p50_approach_ms": float(
                        np.median(
                            [v / 1e6 for w in workers for v in w["repetitions_ns"]]
                        )
                    ),
                    "peak_rss_bytes": max(
                        w["current_exec_peak_rss_bytes"] for w in workers
                    ),
                }
            )
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--worker", nargs=2)
    args = parser.parse_args()
    if args.worker:
        print(json.dumps(worker(*args.worker)))
        return
    OUTPUT.mkdir(parents=True, exist_ok=True)
    captured, sources, base, root_report = inputs()
    reports = write_plans(captured, sources, base)
    results = costs()
    document = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "fixture_sha256": hashlib.sha256(FIXTURE.read_bytes()).hexdigest(),
        "root_fixture_sha256": hashlib.sha256(ROOT_FIXTURE.read_bytes()).hexdigest(),
        "methods": reports,
        "costs": results,
        "root_report": root_report,
        "inputs": captured["inputs"],
        "cost_scope": "Same prepared root/known-lower meshes and source continuations; C1 field, 0.1m far-port adjustment, 0.01m whole-face error, 0.025 gradient, root heights retained and4.5m complete known clearance. Three fresh workers per profile/geometry x five whole approach builds after one warmup. Input/old-root preparation/imports/warmup/products/render excluded from time but included in current-exec VmHWM. CPU10s/AS2GiB/parent30s, one BLAS thread; cooperative GEOS phases can overrun inside a step.",
    }
    (OUTPUT / "comparison.json").write_text(json.dumps(document, indent=2) + "\n")
    print(
        json.dumps(
            {
                "valid": [r["valid"] for r in reports],
                "costs": [
                    {
                        k: c[k]
                        for k in (
                            "profile",
                            "geometry",
                            "p50_approach_ms",
                            "peak_rss_bytes",
                        )
                    }
                    for c in results
                ],
            }
        )
    )


if __name__ == "__main__":
    main()
