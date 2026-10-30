"""Held actual terrain work and controls for exact versus floating cover decisions."""

import argparse
import gc
import hashlib
import importlib.util
import json
import os
import platform
import re
import subprocess
import time
from pathlib import Path

import numpy as np
import shapely
from allocation_measure import allocations
from camera_height_inputs import digest
from stair_module_inputs import read_arrays
from stair_patch_inputs import fresh
from stair_terrain_experiment import frozen
from stair_terrain_overlap import remove_covered_duplicates
from stair_terrain_overlap_checks import square_case


def floating_cover(xy, faces):
    _, first, inverse, counts = np.unique(
        np.sort(faces, axis=1),
        axis=0,
        return_index=True,
        return_inverse=True,
        return_counts=True,
    )
    groups = np.flatnonzero(counts > 1)
    if not len(groups):
        return faces, {}
    duplicate = counts[inverse] > 1
    polygons = shapely.polygons(xy[faces])
    tree = shapely.STRtree(polygons)
    keep = np.ones(len(faces), bool)
    for group in groups:
        index = first[group]
        candidates = tree.query(polygons[index])
        neighbours = candidates[~duplicate[candidates]]
        if shapely.covers(shapely.union_all(polygons[neighbours]), polygons[index]):
            keep[inverse == group] = False
    return faces[keep], {}


def area_filter(xy, faces):
    return faces[shapely.area(shapely.polygons(xy[faces])) > 1e-12], {}


def outcome(value, original, expected):
    faces, _ = value
    return {
        "face_index_sha256": hashlib.sha256(faces.tobytes()).hexdigest(),
        "removed_facets": len(original) - len(faces),
        "matches_required_face_indices": bool(np.array_equal(faces, expected)),
    }


def reference(args):
    if not re.fullmatch(r"[0-9a-f]{40}", args.reference_commit):
        raise ValueError("Full frozen coverage reference commit required")
    code = subprocess.check_output(
        [
            "git",
            "show",
            args.reference_commit
            + ":test/experiments/transport/stair_terrain_overlap.py",
        ]
    )
    path = args.output / "historical_overlap.py"
    path.write_bytes(code)
    spec = importlib.util.spec_from_file_location("historical_overlap", path)
    if spec is None or spec.loader is None:
        raise ValueError("Frozen coverage reference cannot be loaded")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.remove_covered_duplicates, {
        "commit": args.reference_commit,
        "source_sha256": hashlib.sha256(code).hexdigest(),
    }


def run(args):
    commit = frozen()
    fresh(args.output)
    if os.getpriority(os.PRIO_PROCESS, 0) != 0:
        raise ValueError("Measure at normal process priority")
    study = json.loads((args.study / "report.json").read_text())
    raw_path = args.study / "pre-cleanup.npz"
    if (
        study["pre_cleanup_sha256"] != digest(raw_path)
        or not study["removed_overlap"]["removed_facets"]
    ):
        raise ValueError("Actual independently verified network study required")
    raw = read_arrays(raw_path)
    expected, proof = remove_covered_duplicates(raw["xy"], raw["faces"])
    cases = {
        "repair": (raw["xy"], raw["faces"], expected),
        "already_valid": (raw["xy"], expected, expected),
    }
    methods = {
        "exact": remove_covered_duplicates,
        "geos_union": floating_cover,
        "area_threshold": area_filter,
    }
    args.output.mkdir()
    historical = None
    if args.reference_commit:
        methods["clip_reference"], historical = reference(args)
    required = {
        case: {
            name: outcome(call(xy, faces), faces, wanted)
            for name, call in methods.items()
        }
        for case, (xy, faces, wanted) in cases.items()
    }
    controls = {}
    for name in ("one_ulp_gap", "tiny_island"):
        xy, faces = square_case()
        if name == "one_ulp_gap":
            xy[-1, 1] = np.nextafter(2.0, np.inf)
        else:
            xy[-3:] = [[1, 2 + 2**-30], [1 + 2**-24, 2 + 2**-30], [1, 2 + 2**-29]]
        controls[name] = {
            key: outcome(call(xy, faces), faces, faces) for key, call in methods.items()
        }
    load = os.getloadavg()
    costs = {case: {name: {"rounds": []} for name in methods} for case in cases}
    names = tuple(methods)
    for turn in range(9):
        for case, (xy, faces, wanted) in cases.items():
            for name in names[turn % len(names) :] + names[: turn % len(names)]:
                gc.collect()
                cpu, wall = time.process_time(), time.perf_counter()
                result = methods[name](xy, faces)
                costs[case][name]["rounds"].append(
                    {
                        "cpu_s": time.process_time() - cpu,
                        "wall_s": time.perf_counter() - wall,
                    }
                )
                if outcome(result, faces, wanted) != required[case][name]:
                    raise ValueError("Repeated terrain result changed")
                del result
    for case, (xy, faces, wanted) in cases.items():
        for name, call in methods.items():
            gc.collect()
            result, memory = allocations(
                args.output / (case + "-" + name + ".bin"),
                lambda call=call, xy=xy, faces=faces: call(xy, faces),
            )
            if outcome(result, faces, wanted) != required[case][name]:
                raise ValueError("Profiled terrain result changed")
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
        "reference": historical,
        "study_sha256": digest(args.study / "report.json"),
        "pre_cleanup_sha256": digest(raw_path),
        "proof": proof,
        "actual_outcomes": required,
        "controls": controls,
        "costs": costs,
        "input_xy_bytes": raw["xy"].nbytes,
        "input_index_bytes": raw["faces"].nbytes,
        "indexed_polygon_xy_copy_bytes": raw["xy"][raw["faces"]].nbytes,
        "scope": "Actual same repair and already-valid inputs; exact outputs preserved. GEOS is a floating control, area threshold fails an actual positive-area island control. Discovery/IO/whole terrain construction/heights/readers, total copies, process/UMA/native times excluded; exact method alone supplies a rational coverage proof.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"commit": commit, "output": str(args.output)}), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("study", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--reference-commit")
    run(parser.parse_args())
