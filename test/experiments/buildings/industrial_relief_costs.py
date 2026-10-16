"""Equal-solid direct versus Shape relief work, storage and scoped CPU/peak costs."""

import argparse
import json
import os
import time
from pathlib import Path

import shapely
from evidence import revision
from industrial_relief_mesh import direct, generic, products
from industrial_relief_products import ArticulatedDetails
from industrial_rooflight_probe import storage_bytes
from prepared_hall_tile import prepare_halls
from prepared_hall_tile_probe import source_tile
from rooflight_lamina_probe import measured


def costs(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze industrial relief before measurement")
    rows, _receipt, _context, view = source_tile()
    start = time.perf_counter()
    assets = prepare_halls(rows)
    asset_ms = (time.perf_counter() - start) * 1000
    start = time.perf_counter()
    owner = ArticulatedDetails(assets, view)
    additional_ms = (time.perf_counter() - start) * 1000
    representatives = {}
    for parts in owner.reliefs.values():
        for part in parts:
            if part.plan.kind not in representatives:
                representatives[part.plan.kind] = part
    cases, load_start = [], os.getloadavg()
    for kind, part in sorted(representatives.items()):
        measurements = []
        for name, method in (("direct", lambda p: direct(p)[0]), ("shape", generic)):
            mesh, timing = measured(lambda method=method, plan=part.plan: method(plan))
            measurements.append(
                timing
                | {
                    "method": name,
                    "triangles": len(mesh["faces"]),
                    "output_unique_array_bytes": storage_bytes([mesh]),
                    "known_final_vertex_normal_bytes": mesh["vertices"].nbytes
                    + mesh["normals"].nbytes,
                }
            )
        full, outer = products(part.plan)
        cases.append(
            {
                "facade_id": part.plan.id,
                "kind": kind,
                "ring_vertices": len(shapely.get_coordinates(part.plan.region)),
                "region_area_m2": part.plan.region.area,
                "depth_m": part.plan.depth_m,
                "full_triangles": len(full["faces"]),
                "outer_triangles": len(outer["faces"]),
                "full_outer_shared_unique_array_bytes": storage_bytes([full, outer]),
                "measurements": measurements,
            }
        )
    report = proof | {
        "prepare_all_eight_complete_halls_ms": asset_ms,
        "additional_all_relief_assets_ms": additional_ms,
        "prepared_relief_facades": sum(map(len, owner.reliefs.values())),
        "all_full_outer_geometry_unique_array_bytes": storage_bytes(
            [
                m
                for parts in owner.reliefs.values()
                for p in parts
                for m in (p.full, p.outer)
            ]
        ),
        "cases": cases,
        "logical_cpus": os.cpu_count(),
        "load_start": load_start,
        "load_end": os.getloadavg(),
        "scope": "Shared-host normal-priority CPU probe, first/21 subsequent calls and separate traced peak. Direct versus existing general Shape extrusion on the same snapped local regions, closed solid and metric UV contract; independent equal-volume/area/bidirectional solid differences and actual function in verify_industrial_relief. Full/outer products share vertex/attribute buffers; source hall body preparation is separate. Known unique final arrays and bytes only; total native heap, all transient copies/allocations, GPU/frame/device admission unproved.",
    }
    output.mkdir(parents=True, exist_ok=True)
    (output / "costs.json").write_text(
        json.dumps(report, indent=2, allow_nan=False) + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    costs(args.output)
