"""Equal far caps requested directly or viewed from complete prepared physical data."""

import argparse
import json
import os
from pathlib import Path

from evidence import revision
from industrial_relief_mesh import cap_prefix
from industrial_relief_products import ArticulatedDetails
from industrial_relief_signal import direct_caps
from industrial_rooflight_probe import storage_bytes
from prepared_hall_tile import prepare_halls
from prepared_hall_tile_probe import source_tile
from rooflight_lamina_probe import measured
from verify_recipe_halls import mesh_equal


def costs(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze cap implementation before measurement")
    rows, _receipt, _context, view = source_tile()
    owner = ArticulatedDetails(prepare_halls(rows), view)
    representatives = {}
    parts = [p for values in owner.reliefs.values() for p in values]
    for part in parts:
        representatives.setdefault(part.plan.kind, part)
    retained = storage_bytes([m for p in parts for m in (p.full, p.outer)])
    with_caps = storage_bytes(
        [m for p in parts for m in (p.full, p.outer, cap_prefix(p))]
    )
    if retained != with_caps:
        raise ValueError("Prepared cap views unexpectedly duplicate geometry storage")
    cases, load_start = [], os.getloadavg()
    for kind, part in sorted(representatives.items()):
        mesh_equal(cap_prefix(part), direct_caps(part.plan))
        entries = []
        for name, call in (
            ("prepared-prefix", lambda p=part: cap_prefix(p)),
            ("direct-caps-only", lambda p=part: direct_caps(p.plan)),
        ):
            mesh, timing = measured(call)
            entries.append(
                timing
                | {
                    "method": name,
                    "triangles": len(mesh["faces"]),
                    "logical_output_array_span_bytes": sum(
                        a.nbytes for a in mesh.values()
                    ),
                    "referenced_unique_array_storage_bytes": storage_bytes([mesh]),
                    "new_geometry_buffer_count": 0
                    if name == "prepared-prefix"
                    else len(mesh),
                    "new_geometry_buffer_bytes": 0
                    if name == "prepared-prefix"
                    else storage_bytes([mesh]),
                }
            )
        cases.append({"facade_id": part.plan.id, "kind": kind, "entries": entries})
    output.mkdir(parents=True, exist_ok=True)
    (output / "costs.json").write_text(
        json.dumps(
            proof
            | {
                "cases": cases,
                "logical_cpus": os.cpu_count(),
                "load_start": load_start,
                "load_end": os.getloadavg(),
                "prepared_full_outer_bytes": retained,
                "prepared_full_outer_plus_caps_bytes": with_caps,
                "all_outer_triangles": sum(len(p.outer["faces"]) for p in parts),
                "all_cap_triangles": sum(len(cap_prefix(p)["faces"]) for p in parts),
                "scope": "Shared-host normal-priority first/21 subsequent calls and separate traced peak. Equal arrays/dtypes/material/UV/normals: cap prefix of already prepared physical parts versus direct cap-only request. Prefix views keep complete physical storage alive; no far-only retained-memory reduction is claimed. Alias creates ndarray/Python view metadata but zero new geometry buffers or copied geometry bytes. Direct method builds no sides/body; closed asset/source/CSG preparation excluded. Known final bytes and scoped traced peak, not all native/Python allocations/traffic or GPU/frame/device costs.",
            },
            indent=2,
            allow_nan=False,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    costs(args.output)
