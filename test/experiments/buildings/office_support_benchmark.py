"""Equal compact-room constraints on two bounded subdivision searches, timed before furniture."""

import argparse
import hashlib
import json
import platform
import resource
import time
from pathlib import Path

import numpy as np
from building_vertical import level
from evidence import revision
from office_support_plan import refine
from office_support_verification import admission
from room_programme_plan import CONTROLS, source_plan


def signature(plan):
    digest = hashlib.sha256()
    for floor in range(plan["storeys"]):
        for identifier, room in zip(
            plan["storey_room_ids"][floor], level(plan, floor)["rooms"], strict=True
        ):
            digest.update(identifier.encode())
            digest.update(room.wkb)
        for door in level(plan, floor)["doors"]:
            digest.update(door["id"].encode())
            digest.update(door["polygon"].wkb)
    return digest.hexdigest()


def compare(plan, method, repeats):
    previous = refine(plan, method)
    admission(plan, previous)
    expected = signature(previous)
    samples = []
    for _ in range(repeats):
        began = time.perf_counter()
        result = refine(plan, method)
        samples.append(1000.0 * (time.perf_counter() - began))
        if signature(result) != expected:
            raise ValueError("Compact support room plan changed under replay")
    admission(plan, result)
    proof = result["support_refinement"]
    return {
        "method": method,
        "status": proof["status"],
        "support_rooms": len(proof["spaces"]),
        "missing": proof["unresolved"],
        "spaces": proof["spaces"],
        "p50_ms": float(np.quantile(samples, 0.5)),
        "p95_ms": float(np.quantile(samples, 0.95)),
        "samples_ms": samples,
        "plan_sha256": expected,
    }


def run(output, repeats):
    if not 3 <= repeats <= 21:
        raise ValueError("Compact support comparison requires 3..21 repeats")
    output.mkdir(parents=True, exist_ok=True)
    results = []
    for site, identifier in CONTROLS:
        began = time.perf_counter()
        _row, plan, receipt = source_plan(site, identifier)
        prepare_s = time.perf_counter() - began
        rows = [
            compare(plan, method, repeats) for method in ("wall-ends", "sliding-wall")
        ]
        results.append(
            {
                "id": identifier,
                "receipt": receipt,
                "preparation_s": prepare_s,
                "alternatives": rows,
            }
        )
        print(
            site,
            [(r["method"], r["status"], round(r["p50_ms"], 1)) for r in rows],
            flush=True,
        )
    record = revision() | {
        "status": "pass",
        "cases": results,
        "repeats": repeats,
        "warmups": 1,
        "platform": platform.platform(),
        "python": platform.python_version(),
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "rss_scope": "Fresh Linux process including imports, source preparation and all alternatives; not an object/GPU/app allocation budget",
        "scope": "Same original rooms, windows, approaches, compact-room areas and independent corridor-door constraints. CPU subdivision only; no furniture, tracing, native frame/device or optimality claim.",
    }
    (output / "office-support-benchmark.json").write_text(
        json.dumps(record, indent=2) + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--output",
        type=Path,
        default=Path("build/experiments/buildings/office-space-plan"),
    )
    parser.add_argument("--repeats", type=int, default=7)
    args = parser.parse_args()
    run(args.output, args.repeats)
