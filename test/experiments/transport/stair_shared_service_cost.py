"""Equal-body-check costs: borrow one cap per precision or rebuild per direction."""

import argparse
import gc
import json
import os
import platform
import time
from pathlib import Path

import numpy as np
from allocation_measure import allocations
from stair_local_replay import verified
from stair_module_inputs import inputs, read_arrays
from stair_patch_inputs import fresh
from stair_service_vehicle import inspect_service
from stair_shared_service_reference import equal_checks, separate_caps
from stair_terrain_experiment import frozen


def run(args):
    commit = frozen()
    fresh(args.output)
    if os.getpriority(os.PRIO_PROCESS, 0) != 0:
        raise ValueError("Measure at normal process priority")
    verified(args.local)
    saved = json.loads((args.local / "comparison.json").read_text())
    from camera_height_inputs import digest

    if not saved["valid_local_module"] or saved["fixture_sha256"] != digest(
        args.fixture
    ):
        raise ValueError(
            "Accepted complete product bound to the original fixture required"
        )
    _, rows, _, _, _, _ = inputs(args.fixture)
    if len(rows) != 1:
        raise ValueError("One complete straight source required for this cost control")
    data = read_arrays(args.local / "module.npz")
    proof = inspect_service(data, rows)
    vehicle = proof["curves"]["vehicle"]
    methods = {
        "shared_caps": lambda: inspect_service(data, rows, vehicle=vehicle)["checks"],
        "separate_caps": lambda: separate_caps(data, rows[0], vehicle),
    }
    expected = methods["shared_caps"]()
    equal_checks(expected, methods["separate_caps"]())
    args.output.mkdir()
    load = os.getloadavg()
    records = {name: {"rounds": []} for name in methods}
    names = tuple(methods)
    for turn in range(9):
        for name in names[turn % 2 :] + names[: turn % 2]:
            gc.collect()
            cpu, wall = time.process_time(), time.perf_counter()
            value = methods[name]()
            records[name]["rounds"].append(
                {
                    "cpu_s": time.process_time() - cpu,
                    "wall_s": time.perf_counter() - wall,
                }
            )
            equal_checks(expected, value)
            del value
    for name, call in methods.items():
        gc.collect()
        value, memory = allocations(args.output / (name + ".bin"), call)
        equal_checks(expected, value)
        records[name].update(memory)
        for clock in ("cpu", "wall"):
            records[name][clock + "_median_ms"] = 1000 * float(
                np.median([r[clock + "_s"] for r in records[name]["rounds"]])
            )
    count = len(data["surface_vertices"])
    result = {
        "commit": commit,
        "python": platform.python_version(),
        "numpy": np.__version__,
        "load_before": load,
        "load_after": os.getloadavg(),
        "fixture_sha256": digest(args.fixture),
        "same_complete_body_checks": True,
        "complete_body_checks": len(expected),
        "held_array_bytes": sum(v.nbytes for v in data.values()),
        "methods": records,
        "cap_unions": {"shared_caps": 2, "separate_caps": 4},
        "known_float32_roundtrip_copy_bytes": {
            "shared_caps": 36 * count,
            "separate_caps": 72 * count,
        },
        "scope": "Held straight-source Python body-check scopes. No field generation, source IO, total copies, process/UMA peak, native time, dynamics or device acceptance.",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {"commit": commit, "equal_body_checks": True, "output": str(args.output)}
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("local", "fixture", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    run(parser.parse_args())
