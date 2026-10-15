"""CPU sweep comparison on fixed source-derived leaf plans, excluding source acquisition/layout."""

import argparse
import json
import time
from pathlib import Path

import numpy as np
from door_support_plan import choices
from evidence import revision
from office_support_plan import refine
from outshine_ref.buildings.interiors.doors import pose, sweep
from room_programme import programme
from room_programme_plan import CONTROLS, source_plan


def run(output):
    records = []
    for site, identifier in CONTROLS:
        start = time.perf_counter()
        _row, original, _receipt = source_plan(site, identifier)
        plan = refine(original)
        product = programme(plan, "jittered-anchors")
        candidates = choices(plan, product)
        preparation_ms = (time.perf_counter() - start) * 1000
        methods = []
        for method in ("single-hull", "segmented"):
            times = []
            for repeat in range(10):
                start = time.perf_counter()
                results = [sweep(row["leaf"], method=method) for row in candidates]
                elapsed_ms = (time.perf_counter() - start) * 1000
                if repeat:
                    times.append(elapsed_ms)
            # Same 1001-angle containment requirement, outside the timed region.
            for candidate, (envelope, _proof) in zip(candidates, results, strict=True):
                for angle in np.linspace(0, 90, 1001):
                    if not envelope.covers(pose(candidate["leaf"], float(angle))):
                        raise AssertionError(
                            "Sweep comparison violates dense containment"
                        )
            methods.append(
                {
                    "method": method,
                    "repeats": len(times),
                    "warmups": 1,
                    "candidate_leaves": len(candidates),
                    "p50_ms_per_source_candidate_set": float(np.quantile(times, 0.5)),
                    "p95_ms_per_source_candidate_set": float(np.quantile(times, 0.95)),
                    "samples_ms": times,
                    "envelope_area_m2": sum(shape.area for shape, _ in results),
                    "segment_counts": [proof["segments"] for _, proof in results],
                    "maximum_point_chord_deviation_m": max(
                        proof["maximum_point_chord_deviation_m"] for _, proof in results
                    ),
                    "dense_pose_checks": 1001 * len(candidates),
                }
            )
        records.append(
            {
                "site": site,
                "id": identifier,
                "source_and_layout_preparation_ms": preparation_ms,
                "methods": methods,
            }
        )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(
            revision()
            | {
                "cases": records,
                "scope": "Single-process Linux CPU sweep generation only; source decode, room/furniture layout, dense verification, rendering and native runtime excluded from timed region. Both methods conservatively contain the same motion; the refined method additionally limits per-interval point/chord error to 1 mm. No global optimality or GPU/device cost claim.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args().output)
