"""Equal-output retained references versus repeated assembly; scoped CPU/array costs."""

import argparse
import json
import os
import resource
import time
from dataclasses import replace
from pathlib import Path

from evidence import revision
from facade_parts import append
from industrial_rooflight_probe import storage_bytes
from prepared_hall import HallInstance, prepare_asset
from recipe_hall_operators import inputs_for
from recipe_rooflight_operators import prepare
from rooflight_lamina import assemble
from rooflight_lamina_probe import measured
from verify_industrial_halls import sources
from verify_prepared_hall import view_for_factor
from verify_recipe_halls import mesh_equal


def sequence(row, kind):
    near, far = (view_for_factor(row, factor) for factor in (1, 8))
    if kind == "same":
        return ((far, 0.0, 0.0),)
    if kind == "camera":
        turn = replace(far, target=far.target + [25, -25, 0], movement_m=2)
        return ((near, 0.0, 0.0), (far, 0.0, 0.0), (turn, 0.0, 0.0))
    return ((far, 0.0, 0.0), (far, 0.5, 90.0), (far, 1.0, 90.0))


def calls(asset, row, kind, method):
    states = sequence(row, kind)
    actor = HallInstance(asset)
    index = 0

    def call():
        nonlocal index
        view, opened, angle = states[index % len(states)]
        index += 1
        if method == "assemble":
            return assemble(asset.run, view, opened=opened, staff_angle=angle)[0]
        draw = actor.update(view, opened=opened, staff_angle=angle)
        return append(draw.body, draw.render_parts) if method == "export" else draw

    return call


def check(asset, row, kind):
    actor = HallInstance(asset)
    for view, opened, angle in sequence(row, kind):
        expected = assemble(asset.run, view, opened=opened, staff_angle=angle)[0]
        draw = actor.update(view, opened=opened, staff_angle=angle)
        mesh_equal(append(draw.body, draw.render_parts), expected)


def costs(output):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze retained hall implementation before measurement")
    cases = []
    load_start = os.getloadavg()
    for row, neighbors in sources()[:3]:
        start = time.perf_counter()
        run = prepare().evaluate(inputs_for(row, neighbors))
        body_ms = (time.perf_counter() - start) * 1000
        start = time.perf_counter()
        asset = prepare_asset(run)
        parts_ms = (time.perf_counter() - start) * 1000
        retained = (
            asset.body,
            *asset.fixed,
            asset.closed_door,
            *asset.closed_gate_full,
            *asset.closed_gate_flat,
            *asset.full_panels,
            *asset.sheet_panels,
        )
        entries = []
        for kind in ("same", "camera", "state"):
            check(asset, row, kind)
            for method in ("assemble", "references", "export"):
                _value, timing = measured(calls(asset, row, kind, method))
                entries.append(timing | {"kind": kind, "method": method})
        draw = HallInstance(asset).update(view_for_factor(row, 8))
        cases.append(
            {
                "source_id": row["id"],
                "fields": len(asset.field_bounds),
                "graph_and_full_body_ms": body_ms,
                "additional_asset_parts_ms": parts_ms,
                "retained_geometry_unique_array_bytes": storage_bytes(retained),
                "bound_array_bytes": asset.field_bounds.nbytes
                + asset.gate_bounds.nbytes,
                "body_array_bytes": storage_bytes([asset.body]),
                "render_parts": len(draw.render_parts),
                "diagnostic_export_output_bytes": storage_bytes(
                    [append(draw.body, draw.render_parts)]
                ),
                "camera_only_new_geometry_arrays": 0,
                "camera_only_geometry_bytes_copied": 0,
                "camera_bound_points_per_update": len(asset.field_bounds) * 8
                + len(asset.gate_bounds),
                "measurements": entries,
            }
        )
    result = proof | {
        "cases": cases,
        "logical_cpus": os.cpu_count(),
        "load_start": load_start,
        "load_end": os.getloadavg(),
        "process_max_rss_kib": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,
        "scope": "Shared-host normal-priority CPU study. First and 21 subsequent calls after one prepared asset; peak traced in a separate following call. Same/source-camera/state output arrays and dtypes checked outside timing. References return existing geometry; diagnostic export and repeated assembly copy complete bodies. Retained bytes count unique geometry arrays, excluding graph/source/solid backend/templates/Python objects. No total allocation, CPU traffic, native cache, GPU/frame/device or full-body LOD admission.",
    }
    output.mkdir(parents=True, exist_ok=True)
    (output / "costs.json").write_text(
        json.dumps(result, indent=2, allow_nan=False) + "\n"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    costs(args.output)
