"""Equal frame regions and closed bodies; open render work is counted separately."""

import argparse
import gc
import json
import time
import tracemalloc
from pathlib import Path

import numpy as np
import shapely
from evidence import revision
from facade_frame_programme import placement
from outshine_ref.buildings.facade_frames import FacadeFrame, frame_surface, region
from outshine_ref.buildings.modelling import PrismVolume, union_prisms
from structural_shell_geometry import native
from verify_facade_frames import build, sources, view_for


def rectangles(plan):
    result = []
    for i, centre in enumerate(plan.vertical):
        lo = 0 if i == 0 else centre - plan.pier_m / 2
        hi = plan.width_m if i == len(plan.vertical) - 1 else centre + plan.pier_m / 2
        result.append((lo, 0, hi, plan.height_m))
    for i, centre in enumerate(plan.horizontal):
        lo = 0 if i == 0 else centre - plan.beam_m / 2
        hi = (
            plan.height_m if i == len(plan.horizontal) - 1 else centre + plan.beam_m / 2
        )
        result.append((0, lo, plan.width_m, hi))
    return np.asarray(result)


def vector_region(plan):
    u, v = np.asarray(plan.vertical), np.asarray(plan.horizontal)
    x0, y0 = np.meshgrid(
        u[:-1] + plan.pier_m / 2, v[:-1] + plan.beam_m / 2, indexing="ij"
    )
    x1, y1 = np.meshgrid(
        u[1:] - plan.pier_m / 2, v[1:] - plan.beam_m / 2, indexing="ij"
    )
    holes = np.stack(
        (
            np.stack((x0, y0), -1),
            np.stack((x0, y1), -1),
            np.stack((x1, y1), -1),
            np.stack((x1, y0), -1),
        ),
        -2,
    ).reshape(-1, 4, 2)
    outer = np.array(
        [[0, 0], [plan.width_m, 0], [plan.width_m, plan.height_m], [0, plan.height_m]]
    )
    return shapely.polygons(outer, holes=holes)


def union_region(plan):
    r = rectangles(plan)
    return shapely.union_all(shapely.box(r[:, 0], r[:, 1], r[:, 2], r[:, 3])).simplify(
        0
    )


def union_body(plan):
    r = rectangles(plan)
    parts = tuple(
        PrismVolume(f"frame/bar-{i}", shapely.box(*box), 0, plan.depth_m)
        for i, box in enumerate(r)
    )
    return union_prisms(parts)


def observations(method, plan, repeats):
    samples = []
    for i in range(repeats + 2):
        began = time.perf_counter()
        result = method(plan)
        if i >= 2:
            samples.append((time.perf_counter() - began) * 1000)
    gc.collect()
    tracemalloc.start()
    result = method(plan)
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    return result, {
        "runs_ms": samples,
        "p50_ms": float(np.median(samples)),
        "p95_ms": float(np.percentile(samples, 95)),
        "traced_peak_bytes_separate_run": peak,
    }


def plans():
    values = [("analytic-one-aperture", FacadeFrame(4, 3, (0.12, 3.88), (0.11, 2.89)))]
    for row, neighbors in sources():
        _, info, owner = build(row, neighbors, view_for(row))
        r = max(
            owner.frames.completed,
            key=lambda r: (
                (len(r["frame"]["vertical"]) - 1) * (len(r["frame"]["horizontal"]) - 1)
            ),
        )
        f = next(f for f in info["facade_plan"] if f["id"] == r["facade_id"])
        values.append((row["id"], placement(f, 0, info["facade_recipe"]).frame))
    return values


def compare(plan, repeats):
    expected = region(plan)
    regions = {}
    for name, method in (
        ("broadcast-apertures", region),
        ("vector-apertures", vector_region),
        ("GEOS-bar-union", union_region),
    ):
        value, stats = observations(method, plan, repeats)
        error = value.symmetric_difference(expected).area
        if error > 1e-9 or len(value.interiors) != plan.apertures:
            raise ValueError("Frame region alternatives disagree on apertures")
        regions[name] = stats | {
            "difference_m2": error,
            "coordinate_count": int(shapely.get_num_coordinates(value)),
        }
    mesh, direct = observations(frame_surface, plan, repeats)
    union, united = observations(union_body, plan, repeats)
    a, b = native(mesh), native(union.mesh, union.vertex_roots)
    xor = abs((a - b).volume()) + abs((b - a).volume())
    if xor > 1e-7 or abs(a.surface_area() - b.surface_area()) > 1e-7:
        raise ValueError("Frame reference and bar union disagree on exterior solid")
    direct.update(
        triangles=len(mesh["faces"]),
        output_array_bytes=sum(v.nbytes for v in mesh.values()),
    )
    united.update(
        triangles=len(union.mesh["faces"]),
        output_array_bytes=sum(v.nbytes for v in union.mesh.values()),
        other_provenance_bytes=sum(
            getattr(union, field).nbytes
            for field in ("vertex_roots", "source_indices", "source_triangles")
        ),
    )
    outer = frame_surface(plan, back_covered=True)
    return {
        "apertures": plan.apertures,
        "bars": len(plan.vertical) + len(plan.horizontal),
        "region_methods": regions,
        "direct_closed": direct,
        "union_closed": united,
        "symmetric_volume_error_m3": xor,
        "outer_triangles": len(outer["faces"]),
        "outer_array_bytes": sum(v.nbytes for v in outer.values()),
        "back_triangles_not_materialized": len(mesh["faces"]) - len(outer["faces"]),
    }


def run(output, repeats):
    provenance = revision()
    if provenance["experiment_tree_dirty"]:
        raise ValueError("Freeze source before frame work comparisons")
    records = [compare(plan, repeats) | {"source_id": name} for name, plan in plans()]
    record = provenance | {
        "cases": records,
        "scope": "Equal metrical regions and independently verified closed external solids, two warmups and scoped normal-priority nonisolated timing. Region work separately compared to full construction. Generic 3D bar union additionally returns per-bar root/provenance fields unused by the frame contract; no global optimum claim. Tracing excludes native/GEOS allocations and cumulative/internal copies. Owned final arrays/omitted-back counts are scoped, not total memory. Native/device/frame/image-LOD admission remains open.",
    }
    output.write_text(json.dumps(record, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=21)
    args = parser.parse_args()
    if not 1 <= args.repeats <= 21:
        parser.error("Expected 1..21 measured repetitions")
    run(args.output, args.repeats)
