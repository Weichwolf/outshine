"""Fresh consumers: compact composition, flat NPZ or memory-mapped complete rows."""

import argparse
import hashlib
import json
import resource
import time
import tracemalloc
from pathlib import Path
from unittest.mock import patch

import numpy as np
from attached_crown import AttachedCrown
from demand import Camera
from outshine_ref.vegetation import TwigClusters

FIELDS = ("ids", "variants", "origins", "frames", "nodes", "twig_ids")


def consume(study, method, demand):
    with np.load(study / "demand.npz", allow_pickle=False) as z:
        lower, upper, offsets, source = (
            z["lower"],
            z["upper"],
            z["offsets"],
            str(z["source"]),
        )
    camera = Camera(
        (0, -4, 10), (0, -6, 10), fov_y=50, width=2560, height=1440, near=0.05, far=1000
    )
    selected = (
        camera.visible(lower, upper)
        if demand == "near"
        else np.full(len(lower), demand == "all")
    )
    mask = np.repeat(selected, np.diff(offsets).astype(int))
    if not mask.any():
        return {
            k: np.empty(
                (0, 3, 3)
                if k == "frames"
                else (0, 3)
                if k in ("origins", "nodes")
                else 0,
                np.uint8
                if k == "variants"
                else np.uint64
                if k in ("ids", "twig_ids")
                else float,
            )
            for k in FIELDS
        }
    if method == "compact":
        form = AttachedCrown.load(study / "attached.npz", expected_source=source)
        return form.evaluate(selected, max_leaves=100000, anchors=True)
    if method == "npz":
        with np.load(study / "full.npz", allow_pickle=False) as z:
            rows = {k: z[k] for k in FIELDS}
    else:
        rows = {
            k: np.load(study / "flat" / f"{k}.npy", mmap_mode="r", allow_pickle=False)
            for k in FIELDS
        }
    return rows if mask.all() else {k: v[mask] for k, v in rows.items()}


def peak_rss():
    fields = dict(
        line.split(":", 1)
        for line in Path("/proc/self/status").read_text().splitlines()
        if ":" in line
    )
    return int(fields["VmHWM"].split()[0]) * 1024


def run(study, method, demand, output, trace):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private output must be under /tmp")
    guard_begin = time.perf_counter()
    with (
        patch.object(AttachedCrown, "build", side_effect=AssertionError),
        patch.object(TwigClusters, "twigs", side_effect=AssertionError),
        patch.object(TwigClusters, "prototype", side_effect=AssertionError),
        patch.object(TwigClusters, "shoot", side_effect=AssertionError),
        patch("numpy.random.Generator", side_effect=AssertionError),
    ):
        guard_setup_s = time.perf_counter() - guard_begin
        if trace:
            tracemalloc.start()
        began = time.perf_counter()
        rows = consume(study, method, demand)
        load_query_s = time.perf_counter() - began
    before_consume_rss = peak_rss()
    allocations = {}
    if trace:
        current, peak = tracemalloc.get_traced_memory()
        blocks = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        allocations = {
            "traced_current_bytes": current,
            "traced_peak_bytes": peak,
            "traced_live_blocks": blocks,
        }
        tracemalloc.stop()
    began = time.perf_counter()
    digest = hashlib.sha256()
    for key in FIELDS:
        digest.update(memoryview(rows[key]).cast("B") if rows[key].size else b"")
    consumption_s = time.perf_counter() - began
    report = {
        "method": method,
        "demand": demand,
        "trace": trace,
        "selected_leaves": len(rows["ids"]),
        "pose_sha256": digest.hexdigest(),
        "load_query_s": load_query_s,
        "guard_setup_s": guard_setup_s,
        "consume_s": consumption_s,
        "output_array_bytes": sum(v.nbytes for v in rows.values()),
        "process_peak_before_consume_bytes": before_consume_rss,
        "process_peak_after_consume_bytes": peak_rss(),
        "getrusage_peak_bytes_linux": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        **allocations,
        "scope": "Fresh Python consumer, warm OS file cache. Equal rows; full read consumption is a copy-free hash measured separately and forces mmap pages. Traced blocks are live, not cumulative allocations. No GPU/device or per-frame measurement.",
    }
    output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--study", type=Path, required=True)
    parser.add_argument("--method", choices=("compact", "npz", "mmap"), required=True)
    parser.add_argument("--demand", choices=("all", "near", "none"), required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--trace", action="store_true")
    args = parser.parse_args()
    run(args.study, args.method, args.demand, args.output, args.trace)
