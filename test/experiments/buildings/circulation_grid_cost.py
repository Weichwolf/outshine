"""Compare complete real circulation queries and body construction at equal routes."""

import argparse
import gc
import hashlib
import importlib
import importlib.util
import json
import os
import platform
import subprocess
import sys
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
from allocation_measure import allocations
from hip_crown_plan import with_hip
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import plan
from terraced_crown_probe import cases

ROUTES = importlib.import_module("outshine_ref.buildings.interiors.routes")


def reference(path):
    name = "outshine_ref.buildings.interiors._circulation_cost_reference"
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module.graph


def same_routes(left, right):
    for (a, aw), (b, bw) in zip(left, right, strict=True):
        if aw != bw or (a is None) != (b is None):
            raise ValueError("Complete route work or failure changed")
        if a is not None and [p.wkb for p in a] != [p.wkb for p in b]:
            raise ValueError("Complete route points or branch ordering changed")


def same_bodies(left, right):
    a, mesh_a = left
    b, mesh_b = right
    for key in ("portals", "terraces", "replaced_window_ids", "route_work"):
        if getattr(a.body.plan, key) != getattr(b.body.plan, key):
            raise ValueError("Complete terrace/portal/source product changed")
    for ma, mb in (
        (mesh_a, mesh_b),
        (a.body.facade.coarse.mesh, b.body.facade.coarse.mesh),
        (
            {"owners": a.body.facade.coarse.triangle_owners},
            {"owners": b.body.facade.coarse.triangle_owners},
        ),
    ):
        if ma.keys() != mb.keys():
            raise ValueError("Complete body field set changed")
        for key, first in ma.items():
            second = mb[key]
            if (
                first.dtype != second.dtype
                or first.shape != second.shape
                or first.tobytes() != second.tobytes()
            ):
                raise ValueError("Complete body geometry changed")
    for key in ("owner_ids", "window_ids"):
        if getattr(a.body.facade.coarse, key) != getattr(b.body.facade.coarse, key):
            raise ValueError("Facade source ownership changed")


def compare(methods, verify, output, prefix):
    names = tuple(methods)
    baseline = methods[names[0]]()
    report = {name: {"rounds": []} for name in names}
    for iteration in range(9):
        order = names[iteration % len(names) :] + names[: iteration % len(names)]
        for name in order:
            gc.collect()
            cpu, wall = time.process_time(), time.perf_counter()
            value = methods[name]()
            report[name]["rounds"].append(
                {
                    "cpu_s": time.process_time() - cpu,
                    "wall_s": time.perf_counter() - wall,
                }
            )
            verify(baseline, value)
            del value
    for name in names:
        gc.collect()
        value, memory = allocations(output / f"{prefix}-{name}.bin", methods[name])
        verify(baseline, value)
        report[name].update(memory)
        for clock in ("cpu", "wall"):
            report[name][clock + "_median_ms"] = 1000 * float(
                np.median([row[clock + "_s"] for row in report[name]["rounds"]])
            )
        del value
    return report


def collect(request):
    original, requests = ROUTES.graph, []

    def observed(space, anchors):
        requests.append((space, anchors.copy()))
        return original(space, anchors)

    with patch.object(ROUTES, "graph", observed):
        RoofedProducts(request)
    for _space, anchors in requests:
        anchors.setflags(write=False)
    return tuple(requests)


def construct(request, function):
    with patch.object(ROUTES, "graph", function):
        owner = RoofedProducts(request)
        mesh = owner.body.draw(False)
    return owner, mesh


def work(inputs, function):
    covered, original = [], ROUTES.shapely.covers

    def observed(space, geometry):
        result = original(space, geometry)
        covered.append(np.asarray(result).copy())
        return result

    with patch.object(ROUTES.shapely, "covers", observed):
        responses = [function(*row) for row in inputs]
    if len(covered) != 2 * len(inputs):
        raise ValueError("Expected separate grid vertex/edge containment queries")
    candidates = sum(row[1]["candidates"] for row in responses)
    masks = covered[1::2]
    edges = sum(int(np.count_nonzero(mask)) for mask in masks)
    if candidates != sum(len(mask) for mask in masks):
        raise ValueError("Candidate work and observed containment masks differ")
    return {
        "graph_requests": len(inputs),
        "anchors": sum(len(row[1]) for row in inputs),
        "grid_nodes": sum(row[1]["grid_nodes"] for row in responses),
        "candidate_edges": candidates,
        "contained_edges_including_failed_queries": edges,
        "original_edge_norm_calls": edges,
        "batched_edge_norm_calls": sum(bool(len(mask)) for mask in masks),
        "failures": [row[1]["reason"] for row in responses if row[0] is None],
        "copy_scope": "Guaranteed float64 coordinate gathers only; excludes integer materialization, arithmetic, geometry/native internals and anchor collection outside measurements. Separate untimed containment observation includes failed queries.",
        "original_coordinate_copy_lower_bound_bytes": candidates * 32,
        "batched_coordinate_copy_lower_bound_bytes": candidates * 32 + edges * 32,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference-module", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if os.getpriority(os.PRIO_PROCESS, 0) != 0:
        raise ValueError("Run cost comparisons at normal priority")
    if args.output.resolve().is_relative_to(Path(__file__).resolve().parents[3]):
        raise ValueError("Private cost outputs belong in system temp")
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", "lib", "test/experiments/buildings"],
        text=True,
    ):
        raise ValueError("Freeze graph and body producers before recording costs")
    args.output.mkdir(parents=True, exist_ok=False)
    functions = {"original": reference(args.reference_module), "batched": ROUTES.graph}
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "reference_module_sha256": hashlib.sha256(
            args.reference_module.read_bytes()
        ).hexdigest(),
        "python": platform.python_version(),
        "numpy": np.__version__,
        "platform": platform.platform(),
        "priority": os.getpriority(os.PRIO_PROCESS, 0),
        "loadavg_start": os.getloadavg(),
        "total_internal_copied_bytes": None,
        "scope": "Nine rotating complete queries, normal-priority trend; separate Memray/native-stack allocation profiling. Captured geometry is already prepared by the original graph; query construction and input residency excluded from graph costs. Whole cold body/compact facade/terrace construction plus body joining includes new spaces/preparation, patch dispatch, and all query work. Roof/near expansion excluded. Checks outside timing. Scoped incremental heap peaks are not RSS/UMA/native frame/device evidence.",
        "cases": [],
    }
    started = time.process_time()
    sources = cases()
    report["source_loading_cpu_s_single_run"] = time.process_time() - started
    for index, (source, kind) in enumerate(sources):
        started = time.process_time()
        request = with_hip(plan(source, kind))
        preparation_s = time.process_time() - started
        inputs = collect(request)
        row = {
            "case": index,
            "source_id": source["id"],
            "programme": kind,
            "source_plan_preparation_cpu_s_single_run": preparation_s,
            "work": work(inputs, functions["original"]),
            "graphs": compare(
                {
                    name: lambda f=f, inputs=inputs: tuple(f(*row) for row in inputs)
                    for name, f in functions.items()
                },
                same_routes,
                args.output,
                f"graphs-{index}",
            ),
            "body": compare(
                {
                    name: lambda f=f, request=request: construct(request, f)
                    for name, f in functions.items()
                },
                same_bodies,
                args.output,
                f"body-{index}",
            ),
        }
        report["cases"].append(row)
        print(
            json.dumps({"case": index, "state": "all_complete_results_exact"}),
            flush=True,
        )
    report["loadavg_end"] = os.getloadavg()
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
