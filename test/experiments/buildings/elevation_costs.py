"""Equivalent repeated wall compilation versus immutable warm-product reuse."""

import argparse
import json
import os
from pathlib import Path

import numpy as np
from elevation_choice import choose
from elevation_cut import walls
from elevation_mesh import compiled
from elevation_sources import sources
from evidence import revision
from provider_contract import attributes
from rooflight_lamina_probe import measured
from tile_elevations import ElevationFacades
from tile_facades import recipe_for
from tile_roof_crowns import CrownFacades
from verify_elevations import build
from verify_facade_frames import view_for


def allocations(path, call):
    import memray

    with memray.Tracker(path, trace_python_allocators=True, native_traces=True):
        value = call()
    reader = memray.FileReader(path)
    frees = {
        memray.AllocatorType.FREE,
        memray.AllocatorType.PYMALLOC_FREE,
        memray.AllocatorType.MUNMAP,
    }
    count, requested = 0, 0
    for event in reader.get_allocation_records():
        if event.allocator not in frees and event.size > 0:
            count += event.n_allocations
            requested += event.size
    return value, {
        "allocation_events": count,
        "requested_bytes": requested,
        "high_watermark_bytes": sum(
            r.size for r in reader.get_high_watermark_allocation_records()
        ),
        "profiler": "memray",
        "version": memray.__version__,
        "original": str(path),
    }


def costs(allocation_output=None):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze elevation cost producer first")
    records = []
    for index, (row, neighbors) in enumerate(sources()[:4]):
        _mesh, data, after = build(ElevationFacades, row, neighbors, view_for(row))
        plans = after.elevations[row["id"]]
        owner = CrownFacades([row, *neighbors], view_for(row))
        row = owner.rows[0]
        params = attributes(row["properties"])
        raw, info = owner.base(row, params, 12)
        rhythm = recipe_for(row, params)["parameters"]
        mesh, frames, _material = owner.surface(raw, row, params | info, rhythm, 12)
        original = after.source_elevations[row["id"]][0]
        entry = after.staged_entries[row["id"]]
        selected, choice_cost = measured(
            lambda: choose(
                row, params, original, data["facade_recipe"], entry, after.entrances
            )
        )
        if selected[0] != plans:
            raise ValueError("Scalar selection differs from actual saved programme")
        compile_call = lambda: compiled(mesh, frames, plans, owner.binding, 12)
        result, build_cost = measured(compile_call)
        product, scopes = result
        for value in product.values():
            value.flags.writeable = False
        immutable_frames = tuple(scopes)
        reused, reuse_cost = measured(lambda: (product, immutable_frames))
        repeated = compiled(mesh, frames, plans, owner.binding, 12)
        for k in product:
            np.testing.assert_array_equal(repeated[0][k], reused[0][k])
        if repeated[1] != list(reused[1]):
            raise ValueError("Reused scope metadata differs from repeated construction")
        memory = {}
        if allocation_output is not None:
            allocation_output.mkdir(parents=True, exist_ok=True)
            for name, call in (
                ("compile", compile_call),
                ("reuse", lambda: (product, immutable_frames)),
            ):
                _value, memory[name] = allocations(
                    allocation_output / f"{index}-{name}.bin", call
                )
        wall_source, _owners, _remove, _maximum = walls(
            mesh, frames, plans[0].scopes[0].base_m
        )
        records.append(
            {
                "source_id": row["id"],
                "case": index,
                "selection": choice_cost,
                "compile": build_cost,
                "reuse": reuse_cost,
                "memory": memory,
                "input_triangles": len(mesh["faces"]),
                "output_triangles": len(product["faces"]),
                "input_array_count": len(mesh),
                "input_array_bytes": sum(a.nbytes for a in mesh.values()),
                "output_array_count": len(product),
                "output_array_bytes": sum(a.nbytes for a in product.values()),
                "cut_passes": 3,
                "owner_plane_triangle_tests": len(frames) * len(mesh["faces"]),
                "slab_triangle_visits": len(wall_source.triangles) * 3,
                "facade_scopes": sum(len(p.scopes) for p in plans),
                "logical_openings": sum(f["opening_count"] for f in scopes),
                "reuse_mesh_copies": 0,
                "reuse_copied_mesh_bytes": 0,
                "compile_output_array_write_bytes_lower_bound": sum(
                    a.nbytes for a in product.values()
                ),
            }
        )
    return proof | {
        "loadavg": os.getloadavg(),
        "logical_cpus": os.cpu_count(),
        "records": records,
        "scope": "Same pre-opening bound wall products; source setup/roof generation, recesses, renderer, persistent caches and native/device frames excluded. Warm reuse returns already resident immutable arrays with matching bindings. Tracemalloc peak is scoped, not total allocation traffic or native peak.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--allocations",
        action="store_true",
        help="Also retain scoped memray originals under the /tmp output parent",
    )
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path("/tmp")):
        parser.error("--output requires a file under /tmp")
    output.parent.mkdir(parents=True, exist_ok=True)
    allocation_output = (
        output.with_suffix("").with_name(output.stem + "-allocations")
        if args.allocations
        else None
    )
    output.write_text(json.dumps(costs(allocation_output), indent=2) + "\n")


if __name__ == "__main__":
    main()
