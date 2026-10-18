"""Equal front streams built directly, filtered from all sides or reused in memory."""

import argparse
import json
import os
from pathlib import Path
from unittest.mock import patch

import numpy as np
from clad_facade_mesh import build, surface
from clad_facade_plan import placement
from evidence import revision
from facade_frame_geometry import placed
from outshine_ref.buildings.facade_frames import sides
from rooflight_lamina_probe import measured
from verify_facade_frames import sources, view_for


def filtered(plan):
    full = surface(plan, "outer")
    selected = np.all(full["vertices"][full["faces"], 2] == plan.depth_m, axis=1)
    indices = full["faces"][selected].ravel()
    return {
        "vertices": full["vertices"][indices],
        "normals": full["normals"][indices],
        "uv": full["uv"][indices],
        "facade_extent": full["facade_extent"][indices],
        "materials": full["materials"][selected],
        "faces": np.arange(len(indices), dtype=np.uint32).reshape(-1, 3),
    }


def equal(a, b):
    for key in a:
        np.testing.assert_array_equal(a[key], b[key])


def costs(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze clad cost source")
    load = os.getloadavg()
    records = []
    for index, (row, neighbors) in enumerate(sources()):
        _mesh, info, owner = build(row, neighbors, view_for(row))
        facade = next(
            f
            for f in info["facade_plan"]
            if f["id"] == owner.frames.completed[0]["facade_id"]
        )
        for kind in ("broad-piers", "broad-spandrels"):
            plan = placement(facade, 0, info["facade_recipe"], kind)
            cap, separate = surface(plan, "front"), filtered(plan)
            equal(cap, separate)
            entries = []
            for method, operation in (
                ("direct-front", lambda p=plan: surface(p, "front")),
                ("filter-full-outer", lambda p=plan: filtered(p)),
            ):
                with patch("clad_facade_mesh.sides", wraps=sides) as counted:
                    operation()
                    calls = counted.call_count
                result, timing = measured(operation)
                entries.append(
                    timing
                    | {
                        "method": method,
                        "output_triangles": len(result["faces"]),
                        "materialized_triangle_work": plan.triangles("front")
                        if method == "direct-front"
                        else plan.triangles("outer") + plan.triangles("front"),
                        "output_array_bytes": sum(v.nbytes for v in result.values()),
                        "boundary_side_calls_separate_counter_probe": calls,
                    }
                )
            for method, operation in (
                ("reuse-cap-place", lambda p=plan, c=cap: placed(c, p, 10)),
                (
                    "rebuild-cap-place",
                    lambda p=plan: placed(surface(p, "front"), p, 10),
                ),
            ):
                result, timing = measured(operation)
                equal(result, placed(cap, plan, 10))
                entries.append(
                    timing
                    | {
                        "method": method,
                        "output_triangles": len(result["faces"]),
                        "output_array_bytes": sum(v.nbytes for v in result.values()),
                        "new_output_array_bytes": sum(
                            v.nbytes
                            for k, v in result.items()
                            if k not in ("faces", "uv")
                        )
                        if method == "reuse-cap-place"
                        else sum(v.nbytes for v in result.values()),
                    }
                )
            records.append(
                {
                    "index": index,
                    "source_id": row["id"],
                    "kind": kind,
                    "source_region_area_m2": plan.region.area,
                    "closed_reference_triangles": plan.triangles("closed"),
                    "entries": entries,
                }
            )
    output.mkdir(parents=True, exist_ok=True)
    (output / "costs.json").write_text(
        json.dumps(
            proof
            | {
                "cases": records,
                "load_start": load,
                "load_end": os.getloadavg(),
                "logical_cpus": os.cpu_count(),
                "scope": "Exact complete front positions/faces/normals/UV/material streams. First/21 subsequent normal-priority shared-host calls and separate traced peaks. Source/body/plan/contact preparation and renderer/export excluded. Work is explicitly materialized triangle count, not all GEOS/compiler operations. Output bytes and explicit new output allocations exclude hidden temporaries/native heap and total memory traffic. Reuse still transforms/copies placed positions/normals/materials and allocates facade extents; no persistent cache or native frame/device claim.",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    costs(args.output)
