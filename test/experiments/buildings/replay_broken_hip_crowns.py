"""Actual functional two-slope crowns, equal profile work and matched whole images."""

import argparse
import gc
import hashlib
import json
import os
import platform
import time
from dataclasses import asdict
from pathlib import Path

import numpy as np
from allocation_measure import allocations
from broken_hip_crown_checks import check
from broken_hip_crown_geometry import profile
from broken_hip_crown_plan import RECIPE, with_broken_hip
from evidence import revision
from hip_crown_geometry import build as hip_build
from hip_crown_models import export
from hip_crown_plan import with_hip
from replay_hip_crowns import save_roof
from terraced_crown_cost import payload
from terraced_crown_geometry import RoofedProducts
from terraced_crown_plan import plan
from terraced_crown_probe import cases


def signature(value):
    products, skin = value
    digest = hashlib.sha256()
    for index, product in enumerate(products):
        fields = dict(product.mesh, roots=product.vertex_roots)
        if hasattr(product, "source_triangle_ids"):
            fields["source_triangles"] = product.source_triangle_ids
        for key, array in sorted(fields.items()):
            digest.update(str((index, key, array.dtype, array.shape)).encode())
            digest.update(array.tobytes())
    for key, array in sorted(skin.items()):
        digest.update(str((key, array.dtype, array.shape)).encode())
        digest.update(array.tobytes())
    return digest.hexdigest()


def measure_profiles(base, requests, output, index):
    signatures = {}
    for name, request in requests.items():
        value = profile(base, request)
        signatures[name] = signature(value)
        del value
    report = {name: {"runs": []} for name in requests}
    for turn in range(7):
        order = list(requests) if turn % 2 == 0 else list(requests)[::-1]
        for name in order:
            gc.collect()
            cpu, wall = time.process_time(), time.perf_counter()
            result = profile(base, requests[name])
            report[name]["runs"].append(
                {
                    "cpu_s": time.process_time() - cpu,
                    "wall_s": time.perf_counter() - wall,
                }
            )
            if signature(result) != signatures[name]:
                raise ValueError("Repeated complete profile geometry changed")
            del result
    for name, request in requests.items():
        trace = output / f"{index}-{name}.bin"
        result, memory = allocations(
            trace, lambda request=request: profile(base, request)
        )
        if signature(result) != signatures[name]:
            raise ValueError("Allocation replay changed complete profile geometry")
        (product,), _skin = result
        report[name].update(
            median_cpu_s=float(np.median([r["cpu_s"] for r in report[name]["runs"]])),
            allocations=memory
            | {
                "scope": "Profile and closed cap only, preexisting source/base hip excluded."
            },
            work=dict(product.profile_work),
            referenced=payload(result),
            total_internal_copied_bytes=None,
            output_signature=signatures[name],
        )
    return report


def case(row, kind, index, output, buffers, *, costs=True):
    started = time.process_time()
    original = plan(row, kind)
    prepared_s = time.process_time() - started
    plans = {
        "hip": with_hip(original),
        "direct": with_broken_hip(original),
        "logical": with_broken_hip(original, profile_method="logical"),
    }
    owners, results = {}, {}
    for name, request in plans.items():
        cpu, wall = time.process_time(), time.perf_counter()
        owner = RoofedProducts(request)
        owner.physical_roof()
        construction = {
            "cpu_s": time.process_time() - cpu,
            "wall_s": time.perf_counter() - wall,
        }
        if owner.body.facade.near is not None:
            raise ValueError("Roof-only construction expanded a functional body")
        if name != "hip" and owner.roof_crown.kind != "broken-hip":
            raise ValueError(
                "Actual two-slope roof retained old product:" + owner.roof_failure
            )
        results[name] = {
            "single_full_crown_construction": construction,
            "effective_kind": owner.roof_crown.kind,
            "failure": owner.roof_failure,
        }
        if name != "hip":
            results[name]["checks"] = check(owner)
            crown = request.crown
            results[name]["request"] = {
                "recipe": RECIPE,
                "identifier": crown.identifier,
                "base_hip_identifier": crown.base_hip.identifier,
                "run_fraction": crown.run_fraction,
                "height_fraction": crown.height_fraction,
                "profile_method": crown.profile_method,
                "maximum_triangles": crown.maximum_triangles,
                "deck_m": crown.deck_m,
                "top_m": crown.top_m,
                "limits": asdict(crown.limits),
                "profile_frame": "Unplaced core-local ENU of the bound base hip",
            }
        save_roof(buffers, f"{index}_{name}", owner)
        if name != "hip":
            buffers[f"{index}_{name}_source_triangles"] = owner.physical_roof()[
                0
            ].source_triangle_ids
        owners[name] = owner
    base_preparation_s = None
    if costs:
        began = time.process_time()
        (local_base,), _ = hip_build(plans["hip"].crown)
        base_preparation_s = time.process_time() - began
        measured = measure_profiles(
            local_base,
            {name: plans[name].crown for name in ("direct", "logical")},
            output,
            index,
        )
        for name in ("direct", "logical"):
            results[name]["profile_cost"] = measured[name]
    for name in ("hip", "direct"):
        # Both variants retain the same original functional body; image export
        # shares its prepared owner outside all measured profile/crown work.
        owners[name].body = owners["hip"].body
        for key, value in owners[name].draw(True).items():
            buffers[f"{index}_{name}_whole_{key}"] = value
    return owners, {
        "source_id": row["id"],
        "source_properties": row["properties"],
        "source_wkb_sha256": hashlib.sha256(row["polygon"].wkb).hexdigest(),
        "source_top_m": original.source_top_m,
        "body_top_m": original.body.top_m,
        "body_plan_cpu_s": prepared_s,
        "shared_unplaced_base_hip_cpu_s": base_preparation_s,
        "variants": results,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--costs", action=argparse.BooleanOptionalAction, default=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == Path("/tmp") or not output.is_relative_to(Path("/tmp")):
        parser.error("Fresh explicit system-temp output required")
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError(
            "Freeze crown/functional body producers before recording evidence"
        )
    output.mkdir(parents=True, exist_ok=False)
    groups, rows, buffers = [], [], {}
    for index, (source, kind) in enumerate(cases()):
        owners, record = case(source, kind, index, output, buffers, costs=args.costs)
        groups.append(owners)
        rows.append(record)
        print(
            json.dumps({"case": index, "source": source["id"], "state": "constructed"}),
            flush=True,
        )
    np.savez_compressed(output / "geometry.npz", **buffers)
    queue = export(
        groups,
        output,
        proof,
        variants=(("hip", "hip"), ("direct", "broken")),
    )
    (output / "render-queue.json").write_text(json.dumps(queue, indent=2) + "\n")
    report = proof | {
        "platform": platform.platform(),
        "python": platform.python_version(),
        "numpy": np.__version__,
        "process_nice": os.getpriority(os.PRIO_PROCESS, 0),
        "load_average": os.getloadavg(),
        "cases": rows,
        "saved_array_fields": len(buffers),
        "costs_recorded": args.costs,
        "scope": "Actual closed two-slope source-height crowns and unchanged functional bodies. When costs_recorded is true: seven alternating equal-profile rounds, separate native/Python allocation traces; base hip/input/body/independent checks/exports/render outside profile costs. Owner initialization and crown construction reported separately as single observations. No native World/Source/radiance/UMA/p99 acceptance.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
