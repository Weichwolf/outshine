"""Whole-contact costs on the actual four-vertex terrain diagonal neighbourhood."""

import argparse
import gc
import json
import os
import platform
import time
from pathlib import Path
from unittest.mock import patch

import numpy as np
import shapely
import stair_blend_audit
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler, vertical_clearance
from stair_blend_audit import ground_subdivision
from stair_ground_overlay import contact_parts
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


def extrema_parts(xyz, owners, source, polygons, gradients, radius, height_tolerance):
    if len(owners) > 64:
        raise ValueError("bounded complete contact parents required")
    shape = shapely.Polygon(xyz[:, :2])
    parts, vertices = [], 0
    for owner in owners:
        part = shape.intersection(polygons[owner])
        if part.area > 0:
            vertices += len(shapely.get_coordinates(part))
            parts.append((int(owner), part))
    if vertices > 1024 or not parts:
        raise ValueError("bounded complete contact parts required")
    if (
        not shapely.union_all([p for _, p in parts])
        .buffer(radius, quad_segs=32)
        .covers(shape)
    ):
        raise ValueError("contact parts lack complete original coverage")
    triangles = source[owners]
    ground = Mesh(
        triangles.reshape(-1, 3), np.arange(triangles.size // 3).reshape(-1, 3)
    )
    proof = vertical_clearance(
        Mesh(xyz, np.array([[0, 1, 2]])),
        ground,
        max_faces=64,
        max_pairs=64,
        seconds=2,
        include_maximum=True,
    )
    if not proof["valid"]:
        raise ValueError("affine contact parts unresolved")
    error = max(abs(proof["minimum_clearance_m"]), abs(proof["maximum_clearance_m"]))
    if error > height_tolerance:
        raise ValueError("affine contact parts exceed the original height bound")
    return parts, error, vertices


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
    captured = []

    def capture(*arguments):
        captured.append(arguments)
        return contact_parts(*arguments)

    with patch.object(stair_blend_audit, "contact_parts", capture):
        ground_subdivision(contact, source, 1.0)
    if len(captured) != 2:
        raise ValueError("two actual piecewise children required")

    def parts(call):
        values = [call(*arguments) for arguments in captured]
        if sum(len(v[0]) for v in values) != 4 or sum(v[2] for v in values) != 16:
            raise ValueError("same complete contact parts and vertices required")
        return max(v[1] for v in values)

    calls = {
        "piecewise_overlay": lambda: ground_subdivision(contact, source, 1.0)[
            "maximum_parent_height_error_m"
        ],
        "whole_affine_extrema": lambda: extrema_contact(contact, source),
        "parts_overlay": lambda: parts(contact_parts),
        "parts_affine_extrema": lambda: parts(extrema_parts),
    }
    required = {k: call() for k, call in calls.items()}
    if max(required.values()) > 1e-7 or np.ptp(list(required.values())) > 1e-12:
        raise ValueError("same actual complete contact predicate required")
    args.output.mkdir()
    costs = {name: {"rounds": []} for name in calls}
    names, load = tuple(calls), os.getloadavg()
    for turn in range(9):
        for name in names[turn % len(names) :] + names[: turn % len(names)]:
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
        "scope": "Same actual four-vertex Double contact neighbourhood, unchanged 1e-7m height and existing XY tolerance. Whole overlay also derives its Lipschitz envelope and parent accounting; whole extrema is a smaller independent predicate control. Parts methods compare identical two prepared child contexts, four parent pieces and sixteen intersection vertices, including coverage and height checks. Only this crop is timed; no whole-network speed ratio, IO/fit/Q/device/UMA claim or complete internal-copy count.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"commit": commit, "output": str(args.output)}), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
