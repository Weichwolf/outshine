"""Hold the whole accepted through body; compare existing terrain representations."""

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
from stair_local_cost import result_hash
from stair_module_inputs import read_arrays
from stair_module_soil import prepare
from stair_patch_inputs import fresh
from stair_root_contact_study import held
from stair_terrain_experiment import frozen


def preparation(context, mode):
    try:
        plan, selected, note = prepare(
            context["state"],
            context["data"],
            context["ground"],
            dict(context["recipe"], terrain_layout_mode=mode),
            context["sample"],
        )
    except ValueError as error:
        if mode != "legacy" or str(error) != "local terrain lost original coverage":
            raise
        return {"valid": False, "reason": str(error)}
    return {
        "valid": True,
        "array_sha256": result_hash({"layout": plan, "selection": {"faces": selected}}),
        "work": note,
        "owned_layout_bytes": sum(v.nbytes for v in plan.values()),
    }


def run(args):
    commit = frozen()
    fresh(args.output)
    if os.getpriority(os.PRIO_PROCESS, 0) != 0:
        raise ValueError("Measure at normal priority")
    context = held(args)
    expected = {mode: preparation(context, mode) for mode in ("legacy", "free_region")}
    stored = read_arrays(args.local / "local-layout.npz")
    selection = read_arrays(args.local / "selection.npz")
    if expected["free_region"]["array_sha256"] != result_hash(
        {"layout": stored, "selection": {"faces": selection["old_selected_faces"]}}
    ):
        raise ValueError(
            "Fresh terrain must exactly reproduce the accepted saved product"
        )
    args.output.mkdir()
    load = os.getloadavg()
    costs = {name: {"rounds": []} for name in expected}
    names = tuple(costs)
    for turn in range(9):
        for name in names[turn % 2 :] + names[: turn % 2]:
            gc.collect()
            cpu, wall = time.process_time(), time.perf_counter()
            outcome = preparation(context, name)
            costs[name]["rounds"].append(
                {
                    "cpu_s": time.process_time() - cpu,
                    "wall_s": time.perf_counter() - wall,
                }
            )
            if outcome != expected[name]:
                raise ValueError("Terrain outcome changed between repetitions")
    for name in names:
        gc.collect()
        result, memory = allocations(
            args.output / (name + ".bin"), lambda name=name: preparation(context, name)
        )
        if result != expected[name]:
            raise ValueError("Profiled terrain outcome changed")
        costs[name].update(memory)
        for clock in ("cpu", "wall"):
            costs[name][clock + "_median_ms"] = 1000 * float(
                np.median([r[clock + "_s"] for r in costs[name]["rounds"]])
            )
    chosen = int(selection["old_selected_faces"].sum())
    report = {
        "commit": commit,
        "python": platform.python_version(),
        "numpy": np.__version__,
        "load_before": load,
        "load_after": os.getloadavg(),
        "fixture_sha256": digest(args.fixture),
        "saved_reader_sha256": digest(args.local / "verification.json"),
        "outcomes": expected,
        "costs": costs,
        "held_body_array_bytes": sum(v.nbytes for v in context["data"].values()),
        "old_polygon_difference_inputs": chosen,
        "free_region_polygon_difference_inputs": 1,
        "scope": "Identical actual old field, complete new body, source/frame/pins and coverage requirements. Legacy is rejected; free_region exactly reproduces accepted saved arrays. Different validity outcomes, no equal-result speed claim. Python prepare scopes exclude body construction, field solve, IO, total copies, process/UMA peak and native device/frame time.",
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
    for name in ("resident", "local", "fixture", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--prior-module", type=Path, action="append", default=[])
    run(parser.parse_args())
