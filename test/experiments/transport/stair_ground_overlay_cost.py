"""Whole-contact costs on the actual four-vertex terrain diagonal neighbourhood."""

import argparse
import gc
import json
import os
import platform
import time
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler, vertical_clearance
from stair_blend_audit import ground_subdivision
from stair_patch_inputs import fresh
from stair_terrain_diagonal_checks import fixture
from stair_terrain_diagonals import repair_diagonals
from stair_terrain_experiment import frozen


def extrema_contact(contact, source):
    domains = [
        shapely.union_all(shapely.polygons(m.vertices[m.triangles, :2]))
        for m in (contact, source)
    ]
    radius = (
        SurfaceSampler(source).xy_tolerance_m
        + 64 * np.finfo(float).eps * max(1, float(abs(source.vertices).max()))
    ) / np.cos(np.pi / 128)
    if any(
        not a.buffer(radius, quad_segs=32).covers(b)
        for a, b in (domains, domains[::-1])
    ):
        raise ValueError("whole extrema contact lacks complete source coverage")
    proof = vertical_clearance(
        contact, source, max_faces=4, max_pairs=4, seconds=2, include_maximum=True
    )
    if not proof["valid"]:
        raise ValueError("whole extrema contact unresolved")
    error = max(abs(proof["minimum_clearance_m"]), abs(proof["maximum_clearance_m"]))
    if error > SurfaceSampler(source).height_tolerance_m:
        raise ValueError("whole extrema contact exceeds the unchanged height bound")
    return error


def run(args):
    from allocation_measure import allocations

    commit = frozen()
    fresh(args.output)
    if os.getpriority(os.PRIO_PROCESS, 0) != 0:
        raise ValueError("measure at normal process priority")
    xy, faces, saved = fixture()
    fixed, _ = repair_diagonals(xy, faces[:2], np.ones(2, int))
    xyz = np.c_[xy[:4], saved["quad_heights_m"]]
    contact, source = Mesh(xyz, fixed), Mesh(xyz, faces[:2])
    calls = {
        "piecewise_overlay": lambda: ground_subdivision(contact, source, 1.0)[
            "maximum_parent_height_error_m"
        ],
        "whole_affine_extrema": lambda: extrema_contact(contact, source),
    }
    required = {k: call() for k, call in calls.items()}
    if max(required.values()) > 1e-7 or np.ptp(list(required.values())) > 1e-12:
        raise ValueError("same actual complete contact predicate required")
    args.output.mkdir()
    costs = {name: {"rounds": []} for name in calls}
    names, load = tuple(calls), os.getloadavg()
    for turn in range(9):
        for name in names[turn % 2 :] + names[: turn % 2]:
            gc.collect()
            cpu, wall = time.process_time(), time.perf_counter()
            value = calls[name]()
            costs[name]["rounds"].append(
                {
                    "cpu_s": time.process_time() - cpu,
                    "wall_s": time.perf_counter() - wall,
                }
            )
            if value != required[name]:
                raise ValueError("repeated whole contact changed")
    for name, call in calls.items():
        gc.collect()
        value, memory = allocations(args.output / (name + ".bin"), call)
        if value != required[name]:
            raise ValueError("profiled complete contact changed")
        costs[name].update(memory)
        for clock in ("cpu", "wall"):
            costs[name][clock + "_median_ms"] = 1000 * float(
                np.median([r[clock + "_s"] for r in costs[name]["rounds"]])
            )
    path = (
        Path(__file__).parents[2]
        / "outshine/data/transport/terrain-diagonal-contact.json"
    )
    report = {
        "commit": commit,
        "python": platform.python_version(),
        "numpy": np.__version__,
        "fixture_sha256": digest(path),
        "load_before": load,
        "load_after": os.getloadavg(),
        "maximum_errors_m": required,
        "costs": costs,
        "retained_coordinate_bytes": xyz.nbytes,
        "retained_face_bytes": contact.triangles.nbytes + source.triangles.nbytes,
        "scope": "Same actual four-vertex Double contact neighbourhood, unchanged 1e-7m height and existing XY tolerance, full bidirectional domain coverage. Overlay also derives its Lipschitz envelope; extrema is an independent existing affine-kernel control. Only this crop is timed; no whole-network speed ratio, IO/fit/Q/device/UMA claim or complete internal-copy count.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"commit": commit, "output": str(args.output)}), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
