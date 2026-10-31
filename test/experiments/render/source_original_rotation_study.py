"""Measure actual original-source reads through recorded in-place rotations.

This fixed-cohort reference retains every source during rotation. It does not
define eviction, live World revisions, GPU residency or shadow/simulation demand.
"""

import argparse
import gc
import hashlib
import json
import time
import tracemalloc
from pathlib import Path

import numpy as np
from native_source_clip_controls import read as read_controls
from native_source_triangle_materials import DATA, bound_selection
from source_domain_prepared_view import select_sources
from source_original_range_parts import compact_part_inputs
from source_original_ranges import read_ranges
from source_patch_inputs import read_bound_sources
from source_triangle_materials import triangle_parts

ROOT = Path(__file__).resolve().parents[3]


def verify_whole_original(source, binding, chunks):
    parts = compact_part_inputs(chunks, source["parts"])
    expected = triangle_parts(
        source, np.arange(len(binding), dtype="<u4"), referenced_vertices=True
    )
    offset = 0
    for (metadata, arrays, labels), (old_metadata, old_arrays) in zip(
        parts, expected, strict=True
    ):
        if metadata != old_metadata or any(
            arrays[key].tobytes() != value.tobytes()
            for key, value in old_arrays.items()
        ):
            raise ValueError("Whole original material inputs changed")
        if not np.array_equal(labels, binding[offset : offset + len(labels)]):
            raise ValueError("Whole original source binding changed")
        offset += len(labels)
    if offset != len(binding):
        raise ValueError("Whole original source binding missing")


def by_source(chunks):
    result = {}
    for triangles, vertices in chunks:
        identity = int(triangles["binding"][0, 1])
        if identity in result or np.any(triangles["binding"][:, 1] != identity):
            raise ValueError("One complete original chunk per source required")
        result[identity] = triangles, vertices
    return result


def run(args):
    if args.output.exists():
        raise ValueError("Fresh output required")
    source, context, provenance = read_bound_sources(
        ROOT / "test/outshine/data/render/building-depth-samples"
    )
    bound_selection(DATA, source)
    index_raw = (args.ranges / "index.npy").read_bytes()
    ranges_report = json.loads((args.ranges / "report.json").read_text())
    if (
        ranges_report["source_binding_sha256"] != provenance["source_binding_sha256"]
        or ranges_report["index_sha256"] != hashlib.sha256(index_raw).hexdigest()
    ):
        raise ValueError("Bound original source and range index required")
    index = np.load(args.ranges / "index.npy", allow_pickle=False)
    if not np.array_equal(index["source"], context["source_ordinals"]):
        raise ValueError("Complete fixed source cohort required")
    bounds_raw = args.bounds.read_bytes()
    view = json.loads(args.view.read_text())
    if hashlib.sha256(bounds_raw).hexdigest() != view["bounds_sha256"]:
        raise ValueError("Bound source bounds required")
    with np.load(args.bounds, allow_pickle=False) as z:
        prepared = {key: z[key] for key in ("source_ids", "bounds", "normal")}
        if z["source_binding"].tobytes().hex() != provenance["source_binding_sha256"]:
            raise ValueError("Bounds must use the same original source binding")
    with np.load(DATA / "native-visible-triangles.npz", allow_pickle=False) as z:
        views, _ = read_controls(z["near-direction/view_words"])
        placements = z["placements/words"].copy()
    tracemalloc.start()
    start = time.process_time_ns()
    chunks, initial_io = read_ranges(
        args.ranges,
        index,
        index["source"],
        indexed=True,
        compact=True,
        share_contiguous=True,
    )
    cache = by_source(chunks)
    initial_cpu_ms = (time.process_time_ns() - start) / 1e6
    held_current, initial_peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    if set(cache) != set(map(int, index["source"])):
        raise ValueError("Every original source must be retained")
    verify_whole_original(source, context["triangle_binding"], chunks)
    del source, context, chunks
    gc.collect()
    results = []
    for recorded in view["cases"]:
        angle = recorded["angle_degrees"]
        selected = select_sources(prepared, views[angle], placements)
        if set(selected) != set(recorded["selected_source_ids"]):
            raise ValueError("Recorded source demand changed")
        start = time.process_time_ns()
        direct, io = read_ranges(
            args.ranges, index, selected, indexed=True, compact=True
        )
        direct_cpu_ms = (time.process_time_ns() - start) / 1e6
        direct_by_source = by_source(direct)
        if set(direct_by_source) != set(selected):
            raise ValueError("Repeated demand lost a whole original source")
        for identity, (triangles, vertices) in direct_by_source.items():
            old_triangles, old_vertices = cache[identity]
            if (
                triangles.tobytes() != old_triangles.tobytes()
                or vertices.tobytes() != old_vertices.tobytes()
            ):
                raise ValueError("Retained original source words changed")
        results.append(
            {
                "angle_degrees": angle,
                "selected_sources": len(selected),
                "repeat_read_calls": io["read_calls"],
                "repeat_read_bytes": io["read_bytes"],
                "repeat_cpu_ms": direct_cpu_ms,
                "retained_additional_reads": 0,
                "retained_additional_bytes": 0,
            }
        )
    result = {
        "source_binding_sha256": provenance["source_binding_sha256"],
        "index_sha256": ranges_report["index_sha256"],
        "bounds_sha256": view["bounds_sha256"],
        "sources": len(cache),
        "initial_load": initial_io
        | {
            "cpu_ms": initial_cpu_ms,
            "traced_current_bytes": held_current,
            "traced_peak_bytes": initial_peak,
        },
        "held_chunk_array_bytes": sum(t.nbytes + v.nbytes for t, v in cache.values()),
        "index_bytes": len(index_raw),
        "repeat_total_read_calls": sum(r["repeat_read_calls"] for r in results),
        "repeat_total_read_bytes": sum(r["repeat_read_bytes"] for r in results),
        "cases": results,
        "scope": "Actual warm application reads of an immutable fixed 1558-source cohort over 13 recorded in-place camera rotations. Retention starts with the complete cohort; no source is evicted on rotation. Repeated reads are a no-retention comparison, not a production policy. Metadata/source inputs were resident; traced memory excludes held inputs and native/OS allocations. No movement, spatial revision, source acquisition, physical IO, GPU upload/lifetime, shadow/simulation selection, World or device acceptance.",
    }
    args.output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("ranges", "bounds", "view", "output"):
        parser.add_argument("--" + name, type=Path, required=True)
    run(parser.parse_args())
