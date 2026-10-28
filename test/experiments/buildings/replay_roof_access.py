"""Frozen source roof access, independent checks, construction cost and local models."""

import argparse
import hashlib
import json
import os
import resource
import time
from pathlib import Path

import numpy as np
from evidence import revision
from roof_access_checks import check
from roof_access_cost import headhouse, measured
from roof_access_models import export
from roof_access_plan import with_access
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import plan
from terraced_crown_probe import cases


def build(current):
    owner = RoofedProducts(current)
    owner.draw(True)
    return owner


def replay(output, repeats):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze roof-access implementation before evidence")
    began = time.process_time()
    sources = cases()
    acquisition = time.process_time() - began
    rows, owners, costs, buffers = [], {}, {}, {}
    for row, kind in sources:
        began = time.process_time()
        old = plan(row, kind)
        current = with_access(old)
        rows.append(
            {
                "id": row["id"],
                "programme": kind,
                "source_footprint_sha256": hashlib.sha256(
                    row["polygon"].wkb
                ).hexdigest(),
                "source_properties": row["properties"],
                "source_top_m": current.source_top_m,
                "body_top_m": current.body.top_m,
                "storeys": len(current.body.storeys),
                "admitted": current.access is not None,
                "reason": current.reason,
                "plan_cpu_s": time.process_time() - began,
            }
        )
        if current.access is None:
            if current.body is not old.body or current.crown is not old.crown:
                raise ValueError("Refused roof access changed the original product")
            continue
        if owners:
            raise ValueError(
                "This fixed three-source replay expects one admitted source"
            )
        for mode, value in (("original", old), ("access", current)):
            owners[mode], costs[mode] = measured(
                lambda p=value: build(p), repeats, output / (mode + ".bin")
            )
            owner = owners[mode]
            mesh = owner.draw(True)
            costs[mode]["draw_vertices"] = len(mesh["vertices"])
            costs[mode]["draw_triangles"] = len(mesh["faces"])
            for key, array in mesh.items():
                buffers[mode + "_draw_" + key] = array
            floors = owner.body.facade.physical().floors
            for i, product in enumerate(
                (floors.shell, floors.stairs, floors.landing, *owner.physical_roof())
            ):
                for key, array in product.mesh.items():
                    buffers[f"{mode}_physical_{i}_{key}"] = array
                buffers[f"{mode}_physical_{i}_roots"] = product.vertex_roots
        rows[-1]["checks"] = check(owners["access"])
    if not owners:
        raise ValueError("Actual highest-roof positive source was not served")
    comparison = headhouse(owners["access"].plan, output, repeats)
    np.savez_compressed(output / "geometry.npz", **buffers)
    queue = export(owners, output, proof)
    (output / "render-queue.json").write_text(json.dumps(queue, indent=2) + "\n")
    report = proof | {
        "cases": rows,
        "construction": costs,
        "headhouse_comparison": comparison,
        "acquisition_cpu_s": acquisition,
        "load_average": os.getloadavg(),
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "known_work": {
            "added_flights": 2,
            "added_flight_guards": 2,
            "added_mid_landings": 1,
            "added_top_landing_guards": 1,
            "added_highest_ceiling_cuts": 1,
            "headhouse_source_prisms": 3,
            "source_vertex_transforms_in_access_planner": 0,
            "source_geometry_rewritten": False,
            "total_internal_copied_bytes": None,
        },
        "scope": "Fresh Python complete near construction with precomputed plans; source preparation, independent validation, exports and rendering excluded from measured calls. Original/access differ in functionality: incremental cost, not equal-quality speedup. Equivalent closed headhouse alternatives measured separately. Process RSS includes inputs, retained outputs, validation and export. No native/cache/World/controller/device admission.",
    }
    (output / "report.json").write_text(
        json.dumps(report, indent=2, allow_nan=False) + "\n"
    )
    print(
        json.dumps(
            {
                "commit": proof["commit"],
                "sources": len(rows),
                "admitted": sum(r["admitted"] for r in rows),
                "saved_array_fields": len(buffers),
            }
        )
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=3)
    args = parser.parse_args()
    output = args.output.resolve()
    if (
        output == Path("/tmp")
        or not output.is_relative_to(Path("/tmp"))
        or not 1 <= args.repeats <= 7
    ):
        parser.error("Use an explicit /tmp subdirectory and1..7 repeats")
    output.mkdir(parents=True, exist_ok=True)
    replay(output, args.repeats)


if __name__ == "__main__":
    main()
