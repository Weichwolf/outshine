"""Fresh complete bake or ready query costs, with optional own immutable reference."""

import argparse
import hashlib
import importlib.util
import json
import resource
import sys
import time
import tracemalloc
from pathlib import Path

import numpy as np


def array_digest(arrays):
    digest = hashlib.sha256()
    for key, value in sorted(arrays.items()):
        digest.update(key.encode())
        digest.update(str((value.dtype, value.shape)).encode())
        digest.update(value.tobytes())
    return digest.hexdigest()


def run(args):
    if args.trace:
        tracemalloc.start()
    if args.phase == "bake":
        from coarse_city_domains import load_snapshot
        from urban_patch_bake import bake

        if args.reference is not None:
            specification = importlib.util.spec_from_file_location(
                "urban_cost_reference", args.reference
            )
            module = importlib.util.module_from_spec(specification)
            specification.loader.exec_module(module)
            bake = module.bake
        started = time.perf_counter()
        document = load_snapshot(args.input)
        input_seconds = time.perf_counter() - started
        started = time.perf_counter()
        product, metadata = bake(document)
        operation_seconds = time.perf_counter() - started
        arrays = product.arrays()
        report = {
            "input_seconds": input_seconds,
            "patches": len(product.chunks),
            "own_render_masses": int((product.tiers > 0).sum()),
            "work": {
                key: metadata[key]
                for key in [
                    "cells_considered",
                    "footprint_geometries_tested",
                    "patch_domain_tests",
                    "lots_accepted_without_footprint_geometry",
                ]
                if key in metadata
            },
        }
    else:
        from urban_patch_trace import trace
        from urban_patches import load

        started = time.perf_counter()
        product = load(args.product)
        input_seconds = time.perf_counter() - started
        with np.load(args.rays, allow_pickle=False) as archive:
            origins, directions, chunks = (
                archive["origins"],
                archive["directions"],
                archive["chunks"],
            )
        started = time.perf_counter()
        result = trace(product, origins, directions, mode=args.mode, chunks=chunks)
        operation_seconds = time.perf_counter() - started
        arrays = {
            key: value
            for key, value in zip(
                ["depth", "owner", "face", "normal"], result[:4], strict=True
            )
        }
        report = {
            "input_seconds": input_seconds,
            "rays": len(origins),
            "hits": int(np.isfinite(result[0]).sum()),
            "work": result[-1],
        }
    current, peak = tracemalloc.get_traced_memory() if args.trace else (None, None)
    rss = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    report.update(
        {
            "phase": args.phase,
            "mode": args.mode,
            "reference": str(args.reference) if args.reference else None,
            "operation_seconds": operation_seconds,
            "array_bytes": sum(a.nbytes for a in arrays.values()),
            "arrays_sha256": array_digest(arrays),
            "traced_current_bytes": current,
            "traced_peak_bytes": peak,
            "process_raw_peak_rss_bytes": rss
            if sys.platform == "darwin"
            else rss * 1024,
            "scope": "Normalized source snapshot acquisition/MVT decode excluded; ready loading/source work separate. Counts/array payloads are not all allocations/copy/DRAM traffic. No GPU/native/frame inference.",
        }
    )
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--phase", choices=["bake", "query"], required=True)
    parser.add_argument("--input", type=Path)
    parser.add_argument("--product", type=Path)
    parser.add_argument("--rays", type=Path)
    parser.add_argument("--mode", choices=["boxes", "masks"], default="boxes")
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--trace", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
