"""Equal roof-section outputs from batched versus scalar GEOS clip/CDT dispatch."""

import argparse
import gc
import json
import time
import tracemalloc
from pathlib import Path

import numpy as np
import shapely
from evidence import revision
from industrial_roof_choice import section_for
from outshine_ref.buildings.modelling import heightfield_envelope
from outshine_ref.buildings.section_roofs import pack, prepare, section_surface
from provider_contract import attributes
from tile_roof_envelopes import choose
from verify_industrial_roofs import sources
from verify_roof_envelopes import measures


def scalar_surface(polygon, section):
    local, knots = prepare(polygon, section, 65536)
    first, low, last, high = local.bounds
    result, owners = [], []
    for i in range(len(knots) - 1):
        left = min(first, knots[i, 0]) - 1 if i == 0 else knots[i, 0]
        right = (
            max(last, knots[i + 1, 0]) + 1 if i == len(knots) - 2 else knots[i + 1, 0]
        )
        cell = local.intersection(shapely.box(left, low - 1, right, high + 1))
        parts = shapely.get_parts(shapely.constrained_delaunay_triangles(cell))
        points = shapely.get_coordinates(parts).reshape(-1, 4, 2)[:, :3]
        result.extend(points)
        owners.extend([i] * len(points))
    return pack(np.asarray(result), owners, section, 9, 65536)


def measure(method, polygon, section, repeats):
    samples = []
    for i in range(repeats + 2):
        start = time.perf_counter()
        value = method(polygon, section)
        if i >= 2:
            samples.append((time.perf_counter() - start) * 1000)
    gc.collect()
    tracemalloc.start()
    value = method(polygon, section)
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return value, {
        "runs_ms": samples,
        "p50_ms": float(np.median(samples)),
        "p95_ms": float(np.percentile(samples, 95)),
        "separate_traced_peak_bytes": peak,
        "final_array_bytes": sum(v.nbytes for v in value.values()),
        "triangles": len(value["faces"]),
    }


def compare(row, repeats):
    section = section_for(row, choose(row, attributes(row["properties"])))
    first, a = measure(section_surface, row["polygon"], section, repeats)
    second, b = measure(scalar_surface, row["polygon"], section, repeats)
    for key in first:
        if first[key].dtype != second[key].dtype or not np.array_equal(
            first[key], second[key]
        ):
            raise ValueError("Batched and scalar roof arrays differ: " + key)
    envelope = heightfield_envelope(first, 0)
    measures(envelope)
    count = len(section.knots) - 1
    n = sum(
        len(r.coords) - 1 for r in (row["polygon"].exterior, *row["polygon"].interiors)
    )
    return {
        "source_id": row["id"],
        "segments": count,
        "batched": a,
        "scalar": b,
        "arrays_and_dtypes_exact": True,
        "projection_area_m2": row["polygon"].area,
        "closed_volume_m3": envelope.volume_m3,
        "work": {
            "polygons_intersected_and_triangulated_each": count,
            "GEOS_dispatches_batch": 2,
            "GEOS_dispatches_scalar": 2 * count,
            "native_GEos_work_saving_claimed": False,
            "local_xy_copy_plus_final_xyz_write_bytes_each": 5
            * len(first["vertices"])
            * 8,
            "source_boundary_vertices": n,
        },
    }


def run(output, repeats):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the roof implementation before measuring")
    cases = [compare(row, repeats) for row, _ in sources()]
    result = proof | {
        "cases": cases,
        "scope": "Identical full array/attribute outputs; 2 warmups, normal-priority nonisolated runs. Dispatch counts are Python-to-GEOS calls, not fewer clipped elements. Local XY copy plus final packed XYZ allocation writes counted from actual stream size; excludes other temporary writes, allocator/internal copies and native/GEOS heap. Tracemalloc peak is not total process memory. Native/device/image LOD remains unproved.",
    }
    output.write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=21)
    args = parser.parse_args()
    if not 1 <= args.repeats <= 21:
        parser.error("Expected 1..21 measured runs")
    run(args.output, args.repeats)
