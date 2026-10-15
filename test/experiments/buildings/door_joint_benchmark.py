"""Equal-geometry bounded furniture beam comparison after feasible door-aware partitioning."""

import argparse
import json
import resource
import time
import tracemalloc
from pathlib import Path

import numpy as np
from door_furnishing import joint
from evidence import revision
from office_support_plan import refine
from room_programme_plan import CONTROLS, source_plan


def measure(plan, width):
    samples = []
    for index in range(10):
        started = time.perf_counter()
        product, selection = joint(plan, width)
        elapsed = time.perf_counter() - started
        if selection["unresolved"] or len(selection["selected"]) != 2:
            raise ValueError("Candidate lost a required complete support door")
        if index:
            samples.append(elapsed)
    support = {row["room_id"] for row in selection["selected"]}
    length = sum(
        path.length
        for room in product["rooms"]
        if room["room_id"] in support
        for path in room["result"]["proof"]["paths"]
    )
    tracemalloc.start()
    joint(plan, width)
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return {
        "beam_width": width,
        "samples_seconds": samples,
        "p50_seconds": float(np.median(samples)),
        "p95_seconds": float(np.percentile(samples, 95)),
        "placement_candidates_tested": sum(r["tested"] for r in selection["attempts"]),
        "hinge_trials": len(selection["attempts"]),
        "support_activity_path_length_m": length,
        "python_traced_peak_bytes_separate_run": peak,
        "scope": "Same required full-size objects, movement exclusion, parked leaf and actor clearance. Python traced peak excludes untracked GEOS/native storage and runs outside timing samples.",
    }


def run(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Cost comparison requires a clean frozen implementation")
    cases = []
    for site, identifier in CONTROLS:
        _, original, _ = source_plan(site, identifier)
        started = time.perf_counter()
        plan = refine(original, door_aware=True)
        partition_seconds = time.perf_counter() - started
        methods = [measure(plan, width) for width in (1, 4)]
        cases.append(
            {
                "site": site,
                "id": identifier,
                "partition_seconds": partition_seconds,
                "maximum_partition_door_trials_per_step": 24,
                "methods": methods,
            }
        )
        print(site, "bounded beam comparison complete", flush=True)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(
            proof
            | {
                "cases": cases,
                "max_process_rss_kib": resource.getrusage(
                    resource.RUSAGE_SELF
                ).ru_maxrss,
                "scope": "Whole CPU reference process peak and warm finite timings under concurrent host work. Neither isolated per-method native allocation totals nor device/frame budgets; one warmup plus nine timing samples per case/method.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    run(parser.parse_args().output)
