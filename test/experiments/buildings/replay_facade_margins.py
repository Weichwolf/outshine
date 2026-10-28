"""Exact original facade products and bounded local wall-margin reuse costs."""

import argparse
import gc
import hashlib
import json
import os
import platform
import subprocess
import time
from dataclasses import replace
from pathlib import Path
from unittest.mock import patch

import numpy as np
from allocation_measure import allocations
from facade_margin_controls import BASELINE, margin_work, procedures
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import plan
from terraced_crown_probe import cases
from terraced_mass_frame import local_plan


def measure(call, rounds, trace):
    runs = []
    for _ in range(rounds):
        gc.collect()
        cpu, wall = time.process_time(), time.perf_counter()
        value = call()
        runs.append(
            {"cpu_s": time.process_time() - cpu, "wall_s": time.perf_counter() - wall}
        )
        del value
    value, memory = allocations(trace, call)
    return value, {"runs": runs, "allocations": memory}


def owner_stage(plans, procedure):
    with patch("terraced_facade_prepare.candidates", procedure):
        return tuple(RoofedProducts(p) for p in plans)


def replay(output, rounds):
    root = Path(__file__).resolve().parents[3]
    dirty = subprocess.check_output(
        ["git", "status", "--porcelain", "--", "test/experiments/buildings", "lib"],
        cwd=root,
        text=True,
    )
    if dirty:
        raise ValueError("Freeze facade implementation and controls before evidence")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    originals = cases()
    plans = tuple(plan(row, kind) for row, kind in originals)
    masses = tuple(local_plan(p.body) for p in plans)
    controls = procedures()
    expected = tuple(controls["baseline"](mass) for mass in masses)
    low = tuple(
        replace(m, storeys=tuple(replace(s, top_m=s.bottom_m + 1.0) for s in m.storeys))
        for m in masses
    )
    low_expected = tuple(controls["baseline"](mass) for mass in low)
    records = {}
    for name, procedure in controls.items():
        call = lambda procedure=procedure: tuple(procedure(mass) for mass in masses)
        if call() != expected:
            raise ValueError("Window/rejection/cutter/ID changed")
        value, cost = measure(call, rounds, output / f"{name}-facade.bin")
        if value != expected:
            raise ValueError("Profiled facade changed")
        counted, work = margin_work(call)
        if counted != expected:
            raise ValueError("Counted facade changed")
        short, low_work = margin_work(
            lambda procedure=procedure: tuple(procedure(mass) for mass in low)
        )
        if short != low_expected:
            raise ValueError("Low-height omission reasons changed")
        records[name] = cost | {"work": work, "low_height_work": low_work}
    owners, owner_cost, captured = {}, {}, {}
    for name in ("baseline", "storey"):
        call = lambda name=name: owner_stage(plans, controls[name])
        call()
        owners[name], owner_cost[name] = measure(call, 3, output / f"{name}-owners.bin")
        for i, owner in enumerate(owners[name]):
            for key, array in owner.draw(False).items():
                captured[f"{name}_{i}_{key}"] = array
    for key, array in captured.items():
        if key.startswith("baseline_") and not np.array_equal(
            array, captured["storey_" + key.removeprefix("baseline_")]
        ):
            raise ValueError("Complete compact owner draw changed")
    np.savez_compressed(output / "geometry.npz", **captured)
    report = {
        "commit": commit,
        "baseline": BASELINE,
        "python": platform.python_version(),
        "load_average": os.getloadavg(),
        "process_nice": os.getpriority(os.PRIO_PROCESS, 0),
        "threads": {
            k: os.environ.get(k) for k in ("OPENBLAS_NUM_THREADS", "OMP_NUM_THREADS")
        },
        "sources": [
            {
                "id": row["id"],
                "windows": len(f.windows),
                "rejected": len(f.rejected),
                "source_wkb_sha256": hashlib.sha256(row["polygon"].wkb).hexdigest(),
            }
            for (row, _kind), f in zip(originals, expected, strict=True)
        ],
        "facade_methods": records,
        "compact_owner_methods": owner_cost,
        "saved_array_fields": len(captured),
        "total_internal_copied_bytes": None,
        "scope": "All exact original Facade dataclasses including every rejection/ID/cutter. Facade-only versus complete compact Owner stages separate; source/index preparation, independent equality/work checks and export outside measured calls. Allocation runs separate. Historical VCS control; no alternate product path or global cache. No device/native/p99 acceptance.",
    }
    (output / "report.json").write_text(
        json.dumps(report, indent=2, allow_nan=False) + "\n"
    )
    print(
        json.dumps(
            {
                "commit": commit,
                "windows": sum(len(f.windows) for f in expected),
                "rejections": sum(len(f.rejected) for f in expected),
                "saved_array_fields": len(captured),
            }
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=7)
    args = parser.parse_args()
    output = args.output.resolve()
    if (
        output == Path("/tmp")
        or not output.is_relative_to(Path("/tmp"))
        or not 1 <= args.rounds <= 11
    ):
        parser.error("Fresh explicit /tmp subdirectory and1..11 rounds required")
    output.mkdir(parents=True, exist_ok=False)
    replay(output, args.rounds)


if __name__ == "__main__":
    main()
