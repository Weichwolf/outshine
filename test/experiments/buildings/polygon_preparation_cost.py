"""Compare polygon preparation and actual body construction with a frozen reference."""

import argparse
import gc
import hashlib
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
from outshine_ref.buildings.modelling import faces
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import plan
from terraced_crown_probe import cases


def reference(path):
    name = "outshine_ref.buildings.modelling._polygon_preparation_reference"
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    sys.modules[name] = module
    spec.loader.exec_module(module)
    return module.plane


def same_array(left, right):
    if (
        left.dtype != right.dtype
        or left.shape != right.shape
        or left.tobytes() != right.tobytes()
    ):
        raise ValueError("Prepared polygon or complete body array changed")


def same_planes(left, right):
    for a, b in zip(left, right, strict=True):
        same_array(a[0], b[0])
        if a[1].wkb != b[1].wkb or a[3] != b[3]:
            raise ValueError("Complete polygon or immutable face metadata changed")
        for x, y in zip(a[2], b[2], strict=True):
            same_array(x, y)
        for x, y in zip(a[3].uvs, b[3].uvs, strict=True):
            same_array(np.asarray(x), np.asarray(y))


def same_bodies(left, right):
    a, a_mesh = left
    b, b_mesh = right
    for first, second in (
        (a_mesh, b_mesh),
        (a.body.facade.coarse.mesh, b.body.facade.coarse.mesh),
    ):
        if first.keys() != second.keys():
            raise ValueError("Complete body field set changed")
        for key in first:
            same_array(first[key], second[key])
    same_array(
        a.body.facade.coarse.triangle_owners, b.body.facade.coarse.triangle_owners
    )
    for key in ("owner_ids", "window_ids"):
        if getattr(a.body.facade.coarse, key) != getattr(b.body.facade.coarse, key):
            raise ValueError("Complete body source ownership changed")
    if any(
        owner.body.facade.near is not None or owner.roof_components is not None
        for owner in (a, b)
    ):
        raise ValueError("Measured body preparation expanded roof or near geometry")


def compare(methods, verify, output, prefix):
    names = tuple(methods)
    reference_value = methods[names[0]]()
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
            verify(reference_value, value)
            del value
    for name in names:
        gc.collect()
        value, memory = allocations(output / f"{prefix}-{name}.bin", methods[name])
        verify(reference_value, value)
        report[name].update(memory)
        report[name]["cpu_median_ms"] = 1000 * float(
            np.median([row["cpu_s"] for row in report[name]["rounds"]])
        )
        report[name]["wall_median_ms"] = 1000 * float(
            np.median([row["wall_s"] for row in report[name]["rounds"]])
        )
        del value
    return report


def collect(request):
    original = faces.plane
    captured = []

    def observed(vertices, face, allow_warped=False):
        captured.append((vertices, face, allow_warped))
        return original(vertices, face, allow_warped)

    with patch.object(faces, "plane", observed):
        RoofedProducts(request)
    for vertices, _face, _warped in captured:
        vertices.setflags(write=False)
    return tuple(captured)


def construct(request, function):
    with patch.object(faces, "plane", function):
        owner = RoofedProducts(request)
        mesh = owner.body.draw(False)
    return owner, mesh


def logical_work(inputs):
    rings, points, old_points = 0, 0, 0
    buffers = {}
    for vertices, face, warped in inputs:
        buffers[id(vertices)] = vertices
        count = sum(len(ring) for ring in (face.outer, *face.holes))
        rings += 1 + len(face.holes)
        points += count
        old_points += len(face.outer) + count * (1 if warped else 2)
    return {
        "planes": len(inputs),
        "rings": rings,
        "ring_points": points,
        "shared_input_arrays": len(buffers),
        "input_array_bytes": sum(value.nbytes for value in buffers.values()),
        "original_known_fancy_index_copy_bytes": old_points * 3 * 8,
        "reused_known_fancy_index_copy_bytes": points * 3 * 8,
        "copy_scope": "Only guaranteed float64 XYZ fancy-index gathers; arithmetic temporaries, metadata and all internal/native copying remain separate.",
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
        raise ValueError("Freeze polygon and body producers before recording costs")
    args.output.mkdir(parents=True, exist_ok=False)
    functions = {"original": reference(args.reference_module), "reused": faces.plane}
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
        "cases": [],
        "total_internal_copied_bytes": None,
        "scope": "Nine rotating complete CPU queries, normal-priority trend; separate Memray/native-stack allocations. Existing inputs excluded from scoped peaks, not RSS/UMA. Original source/plan preparation, polygon-only evaluation and complete body/compact facade/terrace construction are separate. Body joining and patch dispatch included; signatures/quality checks outside timing. No physical roof/near expansion, native frame/device claim or whole-world cache measurement.",
    }
    started = time.process_time()
    source_cases = cases()
    report["source_loading_cpu_s_single_run"] = time.process_time() - started
    for index, (source, kind) in enumerate(source_cases):
        started = time.process_time()
        request = with_hip(plan(source, kind))
        preparation_s = time.process_time() - started
        inputs = collect(request)
        plane_methods = {
            name: (
                lambda function=function, inputs=inputs: tuple(
                    function(*row) for row in inputs
                )
            )
            for name, function in functions.items()
        }
        body_methods = {
            name: (
                lambda function=function, request=request: construct(request, function)
            )
            for name, function in functions.items()
        }
        row = {
            "case": index,
            "source_id": source["id"],
            "programme": kind,
            "source_plan_preparation_cpu_s_single_run": preparation_s,
            "work": logical_work(inputs),
            "planes": compare(
                plane_methods, same_planes, args.output, f"planes-{index}"
            ),
            "body": compare(body_methods, same_bodies, args.output, f"body-{index}"),
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
