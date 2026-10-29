"""Held-input work/quality comparison of existing finite height corrections."""

import argparse
import gc
import json
import os
import platform
import time
from pathlib import Path

import numpy as np
from allocation_measure import allocations
from camera_height_inputs import digest
from outshine_ref.contracts import Mesh
from outshine_ref.transport.level_heights._rounding import repair as adjacent
from stair_height_contraction import MAX_REPAIR_M, normalized_grade
from stair_local_replay import verified
from stair_module_inputs import inputs, read_arrays
from stair_module_roads import face_frames
from stair_patch_inputs import fresh
from stair_seed_height_rounding import repair
from stair_terrain_experiment import frozen


def assess(result, raw, faces, frames):
    value, proof = result
    if value is None:
        return {"valid": False, "proof": proof}
    vertices = value.vertices if isinstance(value, Mesh) else value
    np.testing.assert_array_equal(vertices[:, :2], raw[:, :2])
    if abs(vertices[:, 2] - raw[:, 2]).max() > MAX_REPAIR_M:
        raise ValueError("Correction exceeds the same five millimetre budget")
    grades = [normalized_grade(vertices, faces, frames, cast) for cast in (False, True)]
    if not all(np.isfinite(g) and g <= 1 for g in grades):
        raise ValueError("A correction violates unchanged physical profile bounds")
    return {"valid": True, "maximum_normalized_grades": grades, "proof": proof}


def run(args):
    commit = frozen()
    fresh(args.output)
    if os.getpriority(os.PRIO_PROCESS, 0) != 0:
        raise ValueError("Measure at normal process priority")
    verified(args.local)
    saved = json.loads((args.local / "comparison.json").read_text())
    if saved["fixture_sha256"] != digest(args.fixture) or saved["artifacts"][
        "module.npz"
    ] != digest(args.local / "module.npz"):
        raise ValueError("Exact saved source input bytes required")
    _, rows, _, _, _, _ = inputs(args.fixture)
    data = read_arrays(args.local / "module.npz")
    raw = data["surface_vertices"].copy()
    raw[:, 2] = data["unrounded_seed_heights"]
    faces = data["surface_faces"]
    frames = face_frames(rows, raw[:, :2], faces, data["owners"])
    source = Mesh(raw, faces)
    methods = {
        "contraction": lambda: repair(raw, faces, frames, seconds=8),
        "adjacent": lambda: adjacent(source, frames, (), (), seconds=8),
    }
    expected = {
        name: assess(call(), raw, faces, frames) for name, call in methods.items()
    }
    args.output.mkdir()
    load = os.getloadavg()
    costs = {name: {"rounds": []} for name in methods}
    names = tuple(methods)
    for turn in range(9):
        for name in names[turn % 2 :] + names[: turn % 2]:
            gc.collect()
            cpu, wall = time.process_time(), time.perf_counter()
            result = methods[name]()
            costs[name]["rounds"].append(
                {
                    "cpu_s": time.process_time() - cpu,
                    "wall_s": time.perf_counter() - wall,
                }
            )
            if assess(result, raw, faces, frames) != expected[name]:
                raise ValueError("Finite correction outcome changed between rounds")
            del result
    for name, call in methods.items():
        gc.collect()
        result, memory = allocations(args.output / (name + ".bin"), call)
        if assess(result, raw, faces, frames) != expected[name]:
            raise ValueError("Profiled correction outcome changed")
        costs[name].update(memory)
        for clock in ("cpu", "wall"):
            costs[name][clock + "_median_ms"] = 1000 * float(
                np.median([r[clock + "_s"] for r in costs[name]["rounds"]])
            )
    report = {
        "commit": commit,
        "python": platform.python_version(),
        "numpy": np.__version__,
        "load_before": load,
        "load_after": os.getloadavg(),
        "fixture_sha256": digest(args.fixture),
        "outcomes": expected,
        "costs": costs,
        "held_raw_vertex_bytes": raw.nbytes,
        "held_face_bytes": faces.nbytes,
        "new_saved_witness_bytes": raw[:, 2].nbytes,
        "scope": "Same physical limits and five millimetre correction budget, different success outcomes; no equal-result speed comparison or global optimality claim. Held Python scopes exclude IO/continuous fitting, total copies, process/UMA and native device/frame time.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "commit": commit,
                "valid": {k: v["valid"] for k, v in expected.items()},
                "output": str(args.output),
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("local", "fixture", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    run(parser.parse_args())
