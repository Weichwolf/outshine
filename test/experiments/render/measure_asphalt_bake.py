"""Measure source generation and scalar bake; allocation tracing is a separate run."""

import argparse
import hashlib
import json
import resource
import statistics
import sys
import tempfile
import time
from contextlib import nullcontext
from pathlib import Path

from asphalt_angular import angular_tile


def measure(method, output, track):
    scope = nullcontext()
    if track:
        import memray

        scope = memray.Tracker(
            str(output / "allocations.bin"), trace_python_allocators=True
        )
    times, hashes = [], []
    with scope:
        for _ in range(1 if track else 3):
            started = time.perf_counter()
            field, work = angular_tile(16, 4, 512, method)
            times.append(time.perf_counter() - started)
            hashes.append(hashlib.sha256(field.tobytes()).hexdigest())
    if len(set(hashes)) != 1:
        raise ValueError("deterministic repeated source/bake required")
    report = {
        "method": method,
        "period_cells": 16,
        "density": 4,
        "size": 512,
        "repeats": len(times),
        "instrumented": track,
        "median_seconds": statistics.median(times),
        "seconds": times,
        "field_sha256": hashes[0],
        "field_bytes": field.nbytes,
        "work_per_bake": work,
        "peak_process_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1 if sys.platform == "darwin" else 1024),
        "scope": "Full deterministic Python source generation and scalar bake only. Hash serialization is inside tracked allocation scope, outside ordinary timing. No mips, npz IO, upload, GPU/renderer or cache-hit time; tracked bytes are allocation traffic, not total copies.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "method": method,
                "instrumented": track,
                "median_seconds": report["median_seconds"],
                "field_sha256": hashes[0],
            }
        ),
        flush=True,
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--method", choices=("area", "ss32-dc"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--track", action="store_true")
    args = parser.parse_args()
    target, temporary = args.output.resolve(), Path(tempfile.gettempdir()).resolve()
    checkout = Path(__file__).resolve().parents[3]
    if (
        target == temporary
        or not target.is_relative_to(temporary)
        or target.is_relative_to(checkout)
        or target.exists()
    ):
        parser.error("fresh explicit system-temp output outside the checkout required")
    target.mkdir(parents=True)
    measure(args.method, target, args.track)
