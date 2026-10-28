"""Source-height hip/contour controls, independent checks, costs and local models."""

import argparse
import hashlib
import json
import os
import resource
import time
from pathlib import Path

import numpy as np
from evidence import revision
from hip_crown_checks import check
from hip_crown_models import export
from hip_crown_plan import with_hip
from roof_access_cost import measured
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import plan
from terraced_crown_probe import cases


def build(current, whole=False):
    owner = RoofedProducts(current)
    owner.draw(True) if whole else owner.physical_roof()
    return owner


def save_roof(buffers, prefix, owner):
    for i, product in enumerate(owner.physical_roof()):
        for key, value in product.mesh.items():
            buffers[f"{prefix}_{i}_{key}"] = value
        buffers[f"{prefix}_{i}_roots"] = product.vertex_roots


def replay(output, repeats):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze the hip crown producer before evidence")
    began = time.process_time()
    sources = cases()
    acquisition = time.process_time() - began
    rows, groups, buffers = [], [], {}
    for index, (row, kind) in enumerate(sources):
        began = time.process_time()
        original = plan(row, kind)
        plans = {
            "flat": original,
            "auto": with_hip(original),
            "contour": with_hip(original, method="contour"),
        }
        planning = time.process_time() - began
        group, results = {}, {}
        for mode, request in plans.items():
            build(request)
            owner, cost = measured(
                lambda p=request: build(p), repeats, output / f"{index}-{mode}.bin"
            )
            group[mode] = owner
            if owner.body.facade.near is not None:
                raise ValueError("Roof-only cost expanded a physical body")
            results[mode] = {
                "cost": cost,
                "effective_kind": owner.roof_crown.kind,
                "failure": owner.roof_failure,
            }
            if mode != "flat":
                results[mode]["checks"] = check(owner)
            save_roof(buffers, f"{index}_{mode}", owner)
        rows.append(
            {
                "id": row["id"],
                "properties": row["properties"],
                "source_wkb_sha256": hashlib.sha256(row["polygon"].wkb).hexdigest(),
                "source_top_m": original.source_top_m,
                "body_top_m": original.body.top_m,
                "storeys": len(original.body.storeys),
                "planning_cpu_s": planning,
                "variants": results,
            }
        )
        groups.append(group)
    whole = {}
    for mode in ("flat", "auto"):
        current = groups[-1][mode].plan
        owner, cost = measured(
            lambda p=current: build(p, True), repeats, output / f"whole-{mode}.bin"
        )
        mesh = owner.draw(True)
        whole[mode] = cost | {
            "vertices": len(mesh["vertices"]),
            "triangles": len(mesh["faces"]),
        }
        for key, value in mesh.items():
            buffers[f"whole_{mode}_{key}"] = value
    np.savez_compressed(output / "geometry.npz", **buffers)
    queue = export(groups, output, proof)
    (output / "render-queue.json").write_text(json.dumps(queue, indent=2) + "\n")
    report = proof | {
        "sources": rows,
        "whole_last_source": whole,
        "acquisition_cpu_s": acquisition,
        "load_average": os.getloadavg(),
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "known_work": {
            "source_boundary_cap": 128,
            "skeleton_node_cap": 1024,
            "python_event_cap": 4096,
            "triangle_facet_candidate_cap": 65536,
            "cooperative_seconds": 1.0,
            "native_call_preemptible": False,
            "cga_to_enu_written_bytes_per_converted_vertex": 24,
            "connected_fan_property_bytes_per_triangle": 120,
            "faces_u64_to_i32_copy_bytes_per_triangle": 12,
            "total_internal_copied_bytes": None,
        },
        "scope": "Fresh owners after a library warmup, compact planning plus roof-only expansion; source preparation, independent validation/export/render excluded. Native/contour roofs compare only when both produce hips; failed contour keeps an explicit original roof. Flat/hip and complete last-source costs are functional increments, not equal-quality speedups. RSS includes retained controls/validation/export. Cooperative native deadline is checked after return; no frame/controller/World/device admission.",
    }
    (output / "report.json").write_text(
        json.dumps(report, indent=2, allow_nan=False) + "\n"
    )
    print(
        json.dumps(
            {
                "commit": proof["commit"],
                "sources": len(rows),
                "auto_hips": sum(
                    r["variants"]["auto"]["effective_kind"] == "hip" for r in rows
                ),
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
