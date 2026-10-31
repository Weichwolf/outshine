"""Actual original triangle/source binding: complete and selected exact-word costs."""

import argparse
import hashlib
import json
import statistics
import time
import tracemalloc
from pathlib import Path

import numpy as np
from native_source_triangle_materials import DATA, bound_selection
from source_binding_columns import encode
from source_patch_inputs import read_bound_sources

ROOT = Path(__file__).resolve().parents[3]


def measure(call, repetitions=7):
    times = []
    value = None
    for _ in range(repetitions):
        before = time.process_time_ns()
        value = call()
        times.append((time.process_time_ns() - before) / 1e6)
    tracemalloc.start()
    value = call()
    current, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return value, {
        "cpu_ms_median": statistics.median(times),
        "cpu_ms_max": max(times),
        "repetitions": repetitions,
        "tracemalloc_current_bytes": current,
        "tracemalloc_peak_bytes": peak,
    }


def run(args):
    if args.output.exists():
        raise ValueError("Fresh explicit output required")
    paths = [Path(__file__), Path(__file__).with_name("source_binding_columns.py")]
    hashes = {
        path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in paths
    }
    original, context, provenance = read_bound_sources(
        ROOT / "test/outshine/data/render/building-depth-samples"
    )
    matrix = context["triangle_binding"]
    selected, native = bound_selection(DATA, original)
    if selected.dtype != np.dtype("<i4"):
        raise ValueError("Actual native selected triangle ordinals required")
    query = np.r_[
        selected, selected[::-1], selected[:1], np.array([0, len(matrix) - 1], np.int32)
    ]
    reference = matrix[query]
    variants = {}
    for mode in ("direct", "narrow", "bitpack"):
        construct = (
            (lambda: matrix.copy())
            if mode == "direct"
            else (lambda chosen=mode: encode(matrix, bitpack=chosen == "bitpack"))
        )
        product, build = measure(construct)
        gather = (
            (lambda value=product: value[query])
            if mode == "direct"
            else (lambda value=product: value.gather(query))
        )
        values, query_cost = measure(gather)
        if not np.array_equal(values.view("u4"), reference.view("u4")):
            raise ValueError("Selected native source binding words changed")
        all_rows = (
            product
            if mode == "direct"
            else product.gather(np.arange(len(matrix), dtype=np.int32))
        )
        if not np.array_equal(all_rows.view("u4"), matrix.view("u4")):
            raise ValueError("Complete native source binding words changed")
        variants[mode] = {
            "array_payload_bytes": product.nbytes
            if mode == "direct"
            else product.payload_bytes,
            "column_modes": None if mode == "direct" else product.modes,
            "column_dtypes": None
            if mode == "direct"
            else [c.dtype.str for c in product.columns],
            "build": build,
            "gather": query_cost,
            "complete_and_selected_words_exact": True,
        }
    if hashes != {
        path.name: hashlib.sha256(path.read_bytes()).hexdigest() for path in paths
    }:
        raise ValueError("Measured source changed")
    report = {
        "source_binding_sha256": provenance["source_binding_sha256"],
        "native_selection_sha256": native["samples_sha256"],
        "source_sha256": hashes,
        "original_triangles": len(matrix),
        "actual_native_selected": len(selected),
        "selected_rows_with_reverse_duplicates_and_endpoints": len(query),
        "variants": variants,
        "scope": "Lossless six-column native source binding only. Payload counts NumPy data buffers, not Python objects, descriptors, serialization, native cache or geometry. Separate seven CPU rounds and traced calls include Python/NumPy work but not all native allocations or full allocation traffic. Selection retains true native IDs; all original source parts, including unknown -1, remain unchanged. No physical IO, GPU, runtime World, shadow, p99 or device admission.",
    }
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
