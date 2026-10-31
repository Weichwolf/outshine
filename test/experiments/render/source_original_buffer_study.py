"""Compare copied and shared immutable buffers on the bound original cohort."""

import argparse
import gc
import json
import os
import statistics
import time
import tracemalloc
from pathlib import Path

import numpy as np
from native_source_triangle_materials import DATA, bound_selection
from source_original_range_parts import compact_part_inputs
from source_original_ranges import (
    decode_indexed,
    digest,
    original_rows,
    read_ranges,
    write_ranges,
)
from source_patch_inputs import read_bound_sources
from source_triangle_materials import triangle_parts

ROOT = Path(__file__).resolve().parents[3]


def copied_ranges(directory, index):
    blob = (directory / "rows.bin").read_bytes()
    offset = 0
    result = []
    for record in index:
        start, length = int(record["offset"]), int(record["bytes"])
        if start != offset:
            raise ValueError("Complete contiguous reference ranges required")
        raw = blob[start : start + length]
        if len(raw) != length or digest(raw) != record["digest"].tobytes():
            raise ValueError("Changed original source range")
        triangles, vertices = decode_indexed(raw, compact=True)
        if np.any(triangles["binding"][:, 1] != record["source"]):
            raise ValueError("Original source identity changed")
        result.append((triangles, vertices))
        offset += length
    if offset != len(blob):
        raise ValueError("Complete original source buffer required")
    return tuple(result)


def same_chunks(a, b):
    if len(a) != len(b) or any(
        x.tobytes() != u.tobytes() or y.tobytes() != v.tobytes()
        for (x, y), (u, v) in zip(a, b, strict=True)
    ):
        raise ValueError("Original source chunks differ")


def run(args):
    if args.output.exists():
        raise ValueError("Fresh output required")
    args.output.mkdir(parents=True)
    source, context, provenance = read_bound_sources(
        ROOT / "test/outshine/data/render/building-depth-samples"
    )
    bound_selection(DATA, source)
    directory = args.output / "ranges"
    index = write_ranges(
        directory, original_rows(source, context["triangle_binding"]), indexed=True
    )

    def old():
        return copied_ranges(directory, index)

    def new():
        chunks, work = read_ranges(
            directory, index, index["source"], indexed=True, compact=True
        )
        if work != {
            "read_calls": 1,
            "read_bytes": (directory / "rows.bin").stat().st_size,
        }:
            raise ValueError("One identical complete source read required")
        return chunks

    elapsed = {"copied": [], "shared": []}
    for _ in range(7):
        for name, call in (("copied", old), ("shared", new)):
            start = time.process_time_ns()
            chunks = call()
            elapsed[name].append((time.process_time_ns() - start) / 1e6)
            if name == "copied":
                reference = chunks
            else:
                same_chunks(reference, chunks)
    parts = compact_part_inputs(chunks, source["parts"])
    expected = triangle_parts(
        source,
        np.arange(len(context["triangle_binding"]), dtype="<u4"),
        referenced_vertices=True,
    )
    offset = 0
    for (metadata, arrays, binding), (old_metadata, old_arrays) in zip(
        parts, expected, strict=True
    ):
        if metadata != old_metadata or any(
            arrays[k].tobytes() != v.tobytes() for k, v in old_arrays.items()
        ):
            raise ValueError("Original native material input changed")
        if not np.array_equal(
            binding, context["triangle_binding"][offset : offset + len(binding)]
        ):
            raise ValueError("Original source binding changed")
        offset += len(binding)
    if offset != len(context["triangle_binding"]):
        raise ValueError("Incomplete original source bindings")
    del parts, expected, chunks, reference
    costs = {}
    for name, call in (("copied", old), ("shared", new)):
        gc.collect()
        tracemalloc.start()
        held = call()
        current, peak = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        costs[name] = {
            "cpu_ms_median": statistics.median(elapsed[name]),
            "cpu_ms_max": max(elapsed[name]),
            "retained_traced_bytes": current,
            "peak_traced_bytes": peak,
            "chunks": len(held),
        }
        del held
    report = {
        "source_binding_sha256": provenance["source_binding_sha256"],
        "sources": len(index),
        "triangles": len(context["triangle_binding"]),
        "range_bytes": (directory / "rows.bin").stat().st_size,
        "index_bytes": (directory / "index.npy").stat().st_size,
        "costs": costs,
        "loadavg": list(os.getloadavg()),
        "scope": "Bound complete original cohort with one warm contiguous application read. The copied reference retains per-source immutable copies; the shared path retains the original immutable read buffer. Traced incremental memory excludes held inputs and native/OS allocations. Partial/gapped ranges, movement, live World, physical IO, GPU and device acceptance require separate evidence.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
