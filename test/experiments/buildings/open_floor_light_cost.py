"""Scoped ceiling-plan work/time/retained-data measurement, not a frame budget."""

import argparse
import json
import resource
import time
import tracemalloc
from pathlib import Path

import numpy as np
import shapely
from evidence import revision
from open_floor_lights import fixture_plan
from terraced_crown_plan import plan
from terraced_mass_frame import local_plan
from terraced_mass_rooms import cavities
from terraced_mass_sources import sources


def run(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the fixture-cost producer first")
    began = time.perf_counter()
    mass = plan(sources()[0], "cascade").body
    setup = time.perf_counter() - began
    local = local_plan(mass)
    rooms = [c for c in cavities(local) if c.identifier.endswith("/open-floor")]
    keepout = shapely.from_wkb(local.core.footprint_wkb).buffer(0.20)
    candidates = 0
    for room in rooms:
        domain = room.polygon.difference(keepout)
        lo = np.floor(np.array(domain.bounds[:2]) / 8)
        hi = np.ceil(np.array(domain.bounds[2:]) / 8)
        candidates += int(np.prod(hi - lo))
    times = []
    for _ in range(16):
        began = time.perf_counter()
        records = fixture_plan(mass)
        times.append(time.perf_counter() - began)
    tracemalloc.start()
    traced = fixture_plan(mass)
    current, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    if records != traced:
        raise ValueError("Cost instrumentation changed fixture contents")
    result = proof | {
        "source_id": mass.identifier,
        "input_mass_plan_setup_seconds": setup,
        "fixture_seconds_16_calls": times,
        "fixture_median_seconds": float(np.median(times)),
        "fixture_p95_seconds": float(np.percentile(times, 95)),
        "open_floors": len(rooms),
        "derived_grid_candidate_cells": candidates,
        "accepted_fixtures": len(records),
        "ceiling_plan_json_bytes": len(json.dumps(records).encode()),
        "tracemalloc_retained_bytes": current,
        "tracemalloc_peak_bytes": peak,
        "process_peak_rss_kib_linux": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss,
        "scope": "One actual existing mass plan,8m grid. Input setup separate, no body mesh build/render. Cell count derived from the exact same domains, not native operation telemetry. Tracemalloc sees tracked allocations only; total allocation churn, Shapely-native peaks/copied bytes and device/runtime costs unmeasured. Shared-host capture, not isolated scaling proof.",
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: result[k]
                for k in (
                    "fixture_median_seconds",
                    "derived_grid_candidate_cells",
                    "accepted_fixtures",
                    "ceiling_plan_json_bytes",
                    "tracemalloc_peak_bytes",
                )
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path("/tmp")) or output == Path("/tmp"):
        parser.error("--output requires an explicit file beneath /tmp")
    run(output)
