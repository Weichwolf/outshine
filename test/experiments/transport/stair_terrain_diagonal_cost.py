"""Equal actual diagonal choices: rational local certificate versus GEOS pair checks."""

import argparse
import gc
import hashlib
import json
import os
import platform
import time
from pathlib import Path

import numpy as np
import shapely
from camera_height_inputs import digest
from stair_module_inputs import read_arrays
from stair_module_topology import check_surface
from stair_patch_inputs import fresh
from stair_terrain_diagonals import (
    alternative,
    areas,
    checked_coordinates,
    repair_diagonals,
)
from stair_terrain_experiment import frozen


def geos_diagonals(xy, faces, classes, *, max_swaps=64, max_tests=192):
    quantized, bad = checked_coordinates(xy, faces, classes, max_swaps, max_tests)
    if not len(bad):
        return faces, {}
    result, swaps, tests = faces.copy(), 0, 0
    for face_id in bad:
        if areas(quantized, result[[face_id]])[0] > 0:
            continue
        if swaps >= max_swaps:
            raise ValueError("terrain diagonal swap budget exhausted")
        neighbours = np.flatnonzero(np.isin(result, result[face_id]).sum(1) == 2)
        for other in neighbours:
            tests += 1
            if tests > max_tests:
                raise ValueError("terrain diagonal candidate budget exhausted")
            if classes[face_id] != classes[other] or classes[face_id] == 2:
                continue
            pair = alternative(result[face_id], result[other])
            if pair is None or any(
                np.any(areas(c, pair) <= 0) for c in (xy, quantized)
            ):
                continue
            before = shapely.union_all(shapely.polygons(xy[result[[face_id, other]]]))
            after = shapely.union_all(shapely.polygons(xy[pair]))
            if not before.equals(after) or any(
                not shapely.coverage_is_valid(shapely.polygons(c[pair]))
                for c in (xy, quantized)
            ):
                continue
            u, v = int(pair[0, 0]), int(pair[0, 2])
            if np.any(np.any(result == u, axis=1) & np.any(result == v, axis=1)):
                continue
            result[[face_id, other]] = pair
            swaps += 1
            break
        else:
            raise ValueError("no D/Q-positive terrain diagonal within its class")
    if np.any(areas(quantized, result) <= 0):
        raise ValueError("terrain diagonal choices leave an invalid Float32 facet")
    return result, {"swaps": swaps, "candidate_tests": tests}


def outcome(value, expected):
    faces, _ = value
    if not np.array_equal(faces, expected):
        raise ValueError("complete actual terrain diagonal result changed")
    return hashlib.sha256(faces.tobytes()).hexdigest()


def run(args):
    from allocation_measure import allocations

    commit = frozen()
    fresh(args.output)
    if os.getpriority(os.PRIO_PROCESS, 0) != 0:
        raise ValueError("measure at normal process priority")
    raw = read_arrays(args.study / "pre-diagonal.npz")
    xy, faces, classes = (raw[k] for k in ("xy", "faces", "classes"))
    expected, proof = repair_diagonals(xy, faces, classes)
    if not proof.get("swaps"):
        raise ValueError("actual whole network must require a terrain diagonal change")
    check_surface(xy, expected)
    cases = {"repair": faces, "already_valid": expected}
    methods = {"rational": repair_diagonals, "geos_pairs": geos_diagonals}
    costs = {case: {name: {"rounds": []} for name in methods} for case in cases}
    args.output.mkdir()
    load = os.getloadavg()
    names = tuple(methods)
    for turn in range(9):
        for case, indices in cases.items():
            for name in names[turn % 2 :] + names[: turn % 2]:
                gc.collect()
                cpu, wall = time.process_time(), time.perf_counter()
                value = methods[name](xy, indices, classes)
                costs[case][name]["rounds"].append(
                    {
                        "cpu_s": time.process_time() - cpu,
                        "wall_s": time.perf_counter() - wall,
                    }
                )
                outcome(value, expected)
                del value
    for case, indices in cases.items():
        for name, call in methods.items():
            gc.collect()
            value, memory = allocations(
                args.output / (case + "-" + name + ".bin"),
                lambda call=call, indices=indices: call(xy, indices, classes),
            )
            outcome(value, expected)
            costs[case][name].update(memory)
            for clock in ("cpu", "wall"):
                costs[case][name][clock + "_median_ms"] = 1000 * float(
                    np.median([r[clock + "_s"] for r in costs[case][name]["rounds"]])
                )
    report = {
        "commit": commit,
        "python": platform.python_version(),
        "numpy": np.__version__,
        "load_before": load,
        "load_after": os.getloadavg(),
        "study_sha256": digest(args.study / "report.json"),
        "pre_diagonal_sha256": digest(args.study / "pre-diagonal.npz"),
        "proof": proof,
        "face_index_sha256": outcome((expected, proof), expected),
        "input_xy_bytes": xy.nbytes,
        "input_index_bytes": faces.nbytes,
        "input_class_bytes": classes.nbytes,
        "costs": costs,
        "scope": "Same actual held pre-diagonal and already-valid inputs, all resulting indices identical. Rational local orientation certificate versus GEOS pair coverage/domain checks, common finite/capacity preconditions. Full D/Q validation outside timing. Excludes IO/whole terrain construction/height fitting/piecewise contact overlay/readers, total internal copies, process/UMA/native times. No global optimality or device claim.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"commit": commit, "output": str(args.output)}), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("study", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    run(parser.parse_args())
