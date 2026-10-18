"""Equivalent attached-roof union methods, full resident reuse and scoped memory."""

import argparse
import json
import os
from pathlib import Path

import numpy as np
from dormer_assets import prepare
from dormer_geometry import union
from dormer_plan import plan
from dormer_present import bound
from dormer_sources import sources
from elevation_costs import allocations
from evidence import revision
from provider_contract import attributes
from rooflight_lamina_probe import measured
from structural_shell_geometry import native
from tile_roof_envelopes import choose


def equivalent(a, b):
    first, second = native(a.mesh, a.vertex_roots), native(b.mesh, b.vertex_roots)
    if (first - second).volume() + (second - first).volume() > 1e-7:
        raise ValueError("Union methods changed occupied volume")
    np.testing.assert_allclose(
        [a.volume_m3, a.area_m2], [b.volume_m3, b.area_m2], rtol=0, atol=1e-7
    )
    for owner in range(len(a.source_ids)):

        def area(p):
            xyz = p.mesh["vertices"][p.mesh["faces"][p.source_indices == owner]]
            return (
                np.linalg.norm(
                    np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0]), axis=1
                ).sum()
                / 2
            )

        if abs(area(a) - area(b)) > 1e-7:
            raise ValueError("Union methods changed component surface provenance")


def costs(allocation_output=None):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze attached-roof costs before measurement")
    records = []
    for index, (row, _neighbors) in enumerate(sources()):
        params = attributes(row["properties"])
        choice = choose(row, params)
        asset = prepare(row, params, choice)
        selected, selection = measured(lambda: plan(row, asset.original, choice))
        if selected[0] != asset.dormers:
            raise ValueError(
                "Measured scalar planning changed the actual roof programme"
            )
        batch = union(asset.original, asset.dormers)
        sequential = union(asset.original, asset.dormers, method="sequential")
        equivalent(batch, sequential)
        products = []
        for method in ("batch", "sequential"):
            call = lambda: union(asset.original, asset.dormers, method=method)
            value, timing = measured(call)
            memory = None
            if allocation_output is not None:
                allocation_output.mkdir(parents=True, exist_ok=True)
                _value, memory = allocations(
                    allocation_output / f"{index}-{method}.bin", call
                )
            products.append(
                timing
                | {
                    "method": method,
                    "memory": memory,
                    "output_triangles": len(value.mesh["faces"]),
                    "output_array_count": len(value.mesh),
                    "output_array_bytes": sum(v.nbytes for v in value.mesh.values()),
                    "input_roof_triangles": len(asset.original.mesh["faces"]),
                    "input_prism_triangles": len(asset.dormers) * 12,
                    "boolean_api_calls": 1 if method == "batch" else len(asset.dormers),
                }
            )
        near = asset.near_products()

        def rebuild():
            fresh = prepare(row, params, choice)
            return fresh.near_products()

        _value, rebuild_cost = measured(rebuild)
        reused, reuse_cost = measured(asset.near_products)
        if reused is not near:
            raise ValueError("Resident near request rebuilt its physical product")
        _draw, binding_cost = measured(lambda: bound(near.skin, 12, 32768))
        records.append(
            {
                "source_id": row["id"],
                "case": index,
                "dormers": len(asset.dormers),
                "selection": selection,
                "unions": products,
                "rebuild_complete_near": rebuild_cost,
                "resident_near": reuse_cost,
                "bind_resident_skin": binding_cost,
                "resident_mesh_copies": 0,
                "resident_mesh_copied_bytes": 0,
                "bound_material_array_copy_bytes": near.skin["materials"].nbytes,
                "near_details_triangles": len(near.details["faces"]),
                "scope": "Roof setup excluded from union-only rows; complete-near rebuild includes source roof, planning, union, pockets, matching split and frame/glass details. Reuse returns the same resident object; binding copies material IDs only.",
            }
        )
    return proof | {
        "loadavg": os.getloadavg(),
        "logical_cpus": os.cpu_count(),
        "records": records,
        "scope": "Same physical occupied surfaces and component provenance for union alternatives. Shared-load host timings and scoped allocations, not process/native/device frame or persistent-cache evidence.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--allocations", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path("/tmp")):
        parser.error("--output requires a file under /tmp")
    output.parent.mkdir(parents=True, exist_ok=True)
    memory = (
        output.with_name(output.stem + "-allocations") if args.allocations else None
    )
    output.write_text(json.dumps(costs(memory), indent=2) + "\n")


if __name__ == "__main__":
    main()
