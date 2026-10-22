"""Consume native roof events with the unchanged Python plan and pin checker."""

import argparse
import ctypes
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from outshine_ref.buildings.modelling import roof_partition
from outshine_ref.buildings.modelling.skeleton_plan import SkeletonBudgetExceeded
from shapely.errors import GEOSException
from verify_hip_fixture import verify


class NativeMotion:
    def __init__(self, library):
        self.library = ctypes.CDLL(str(library.resolve()))
        self.solve = self.library.OutshineRoofCollision
        self.solve.argtypes = [ctypes.c_char_p]
        self.solve.restype = ctypes.c_char_p
        self.calls = self.pairs = self.maximum_events = self.maximum_vertices = 0
        self.bridge_seconds = 0.0

    def __call__(self, loops, motions, normals, intercepts, now, tolerance):
        row = {
            "loops": [{"ids": ids.tolist(), "points": p.tolist()} for ids, p in loops],
            "normals": normals.tolist(),
            "intercepts": intercepts.tolist(),
            "now": now,
            "tolerance": tolerance,
        }
        began = time.perf_counter()
        raw = self.solve(json.dumps(row, separators=(",", ":")).encode())
        if raw is None:
            raise ValueError("native prototype refused the input")
        result = json.loads(raw)
        self.bridge_seconds += time.perf_counter() - began
        if not result["valid"]:
            raise ValueError("native prototype found no forward event")
        if not np.array_equal(np.concatenate(motions), np.asarray(result["motions"])):
            raise ValueError("native and reference velocities differ")
        self.calls += 1
        self.pairs += result["pair_count"]
        self.maximum_events = max(self.maximum_events, len(result["events"]))
        self.maximum_vertices = max(self.maximum_vertices, len(result["motions"]))
        return (
            result["advance_m"],
            result["pair_count"],
            [(e["participants"], np.asarray(e["point"])) for e in result["events"]],
        )


def check(fixture, library):
    data = json.loads(fixture.read_text())
    if data["schema"] != "outshine.hip-plan-reference/1" or len(data["records"]) != 48:
        raise ValueError("the complete existing 48-original fixture is required")
    if len({row["id"] for row in data["records"]}) != 48:
        raise ValueError("distinct original inputs are required")
    motion = NativeMotion(library)
    original = roof_partition.collision
    roof_partition.collision = motion
    results = []
    try:
        for row in data["records"]:
            began = time.perf_counter()
            before = motion.calls
            try:
                plan = verify(row, data["comparison"], producer=roof_partition.prepare)
                result = {"id": row["id"], "valid": True, "facets": len(plan.facets)}
            except (
                AssertionError,
                ValueError,
                SkeletonBudgetExceeded,
                GEOSException,
            ) as error:
                result = {
                    "id": row["id"],
                    "valid": False,
                    "error": f"{type(error).__name__}: {error}",
                }
            result.update(
                seconds_including_pin_check=time.perf_counter() - began,
                native_calls=motion.calls - before,
            )
            results.append(result)
    finally:
        roof_partition.collision = original
    return {
        "status": int(not all(row["valid"] for row in results)),
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "fixture_sha256": hashlib.sha256(fixture.read_bytes()).hexdigest(),
        "library_sha256": hashlib.sha256(library.read_bytes()).hexdigest(),
        "cases": results,
        "native_calls": motion.calls,
        "vertex_edge_pairs_per_pass": motion.pairs,
        "native_vertex_edge_pair_visits": 2 * motion.pairs,
        "maximum_front_vertices": motion.maximum_vertices,
        "maximum_contact_events": motion.maximum_events,
        "bridge_seconds_including_json": motion.bridge_seconds,
        "scope": "Native motion/events consumed by unchanged Python topology/facets/finish and all original pins. Existing 2s/geometry guards unchanged; JSON bridge is diagnostic glue, not the runtime interface. No complete native roof, cache, texture, world or GPU admission.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument(
        "--fixture",
        type=Path,
        default=Path(__file__).with_name("reference") / "hip-plans.json",
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = check(args.fixture, args.library)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    passed = sum(row["valid"] for row in result["cases"])
    print(
        f"{passed}/48 original roof pins, {result['native_calls']} native motion calls"
    )
    raise SystemExit(result["status"])


if __name__ == "__main__":
    main()
