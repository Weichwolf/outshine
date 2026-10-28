"""Real terrace-route extension: retained contacts, equal-geometry sharing and costs."""

import argparse
import hashlib
import json
import os
import resource
import time
from pathlib import Path

import numpy as np
from evidence import revision
from roof_access_cost import measured
from terraced_crown_plan import plan
from terraced_crown_probe import cases
from terraced_mass_admission import inspect
from terraced_route_cost import build, work
from terraced_route_models import export


def replay(output, repeats):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze terrace route implementation before evidence")
    began = time.process_time()
    sources = cases()
    acquisition = time.process_time() - began
    records, buffers, final = [], {}, None
    for index, (row, kind) in enumerate(sources):
        current = plan(row, kind)
        owners, result = {}, {}
        for mode in ("grid", "fresh", "shared"):
            build(current, mode)
            owners[mode], cost = measured(
                lambda m=mode, p=current: build(p, m),
                repeats,
                output / f"{index}-{mode}.bin",
            )
            result[mode] = {"cost": cost, "work": work(owners[mode])}
            mesh = owners[mode].draw(False)
            for key, array in mesh.items():
                buffers[f"{index}_{mode}_coarse_{key}"] = array
        before = {p.identifier: p for p in owners["grid"].body.plan.portals}
        after = {p.identifier: p for p in owners["shared"].body.plan.portals}
        if any(after.get(key) != value for key, value in before.items()):
            raise ValueError("Previously functional portal changed")
        if owners["fresh"].body.plan.portals != owners["shared"].body.plan.portals:
            raise ValueError("Equal-quality fresh/shared routes differ")
        records.append(
            {
                "id": row["id"],
                "source_wkb_sha256": hashlib.sha256(row["polygon"].wkb).hexdigest(),
                "properties": row["properties"],
                "source_top_m": current.source_top_m,
                "body_top_m": current.body.top_m,
                "storeys": len(current.body.storeys),
                "variants": result,
            }
        )
        if index == len(sources) - 1:
            final = current
    whole, owners = {}, {}
    for mode in ("grid", "fresh", "shared"):
        owners[mode], cost = measured(
            lambda m=mode: build(final, m, whole=True),
            repeats,
            output / f"whole-{mode}.bin",
        )
        owner = owners[mode]
        mesh = owner.draw(True)
        whole[mode] = {
            "cost": cost,
            "work": work(owner),
            "triangles": len(mesh["faces"]),
            "vertices": len(mesh["vertices"]),
        }
        for key, array in mesh.items():
            buffers[f"whole_{mode}_{key}"] = array
        floors = owner.body.facade.physical().floors
        for i, product in enumerate(
            (floors.shell, floors.stairs, floors.landing, *owner.physical_roof())
        ):
            for key, array in product.mesh.items():
                buffers[f"{mode}_physical_{i}_{key}"] = array
            buffers[f"{mode}_physical_{i}_roots"] = product.vertex_roots
    for key, array in owners["fresh"].draw(True).items():
        if not np.array_equal(array, owners["shared"].draw(True)[key]):
            raise ValueError("Sharing altered the actual draw")
    checks = inspect(owners["shared"].body)
    np.savez_compressed(output / "geometry.npz", **buffers)
    queue = export(owners, output, proof)
    (output / "render-queue.json").write_text(json.dumps(queue, indent=2) + "\n")
    report = proof | {
        "sources": records,
        "whole_last_source": whole,
        "checks": checks,
        "acquisition_cpu_s": acquisition,
        "load_average": os.getloadavg(),
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "limits": {"vertices": 768, "triangles": 1024, "path_points": 2048},
        "total_internal_copied_bytes": None,
        "scope": "Fresh equivalent wrappers for all modes; compact Owner construction versus complete last-source near construction measured separately, warm libraries. Source preparation/checks/export/render excluded. Fresh/shared have identical portals/meshes; grid/shared adds functionality, no equal-quality speed claim. Source/height/floors/old contacts retained. RSS includes retained controls/checks/export. No general controller/World/cache/native/device acceptance.",
    }
    (output / "report.json").write_text(
        json.dumps(report, indent=2, allow_nan=False) + "\n"
    )
    print(
        json.dumps(
            {
                "commit": proof["commit"],
                "sources": len(records),
                "portals": [
                    r["variants"]["shared"]["work"]["portals"] for r in records
                ],
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
