"""Finite image controls: all closed windows, fixed radius and camera-bound candidates."""

import argparse
import hashlib
import itertools
import json
import resource
import time
import tracemalloc
from dataclasses import replace
from pathlib import Path

import numpy as np
from compare_window_detail_demand import fingerprint, requests, stage
from evidence import revision
from industrial_relief_probe import camera
from podium_probe import write
from terraced_facade_probe import materials
from test_window_detail_demand import cases
from window_detail_demand import MAX_WINDOWS, WindowDetails, buffer_bytes, materialize
from window_extent_plan import compile_bounds
from window_extent_query import WindowRange

CORNERS = np.asarray(list(itertools.product((0, 1), repeat=3)))


def camera_candidates(metadata, view):
    """Conservative finite control only; no opaque coverage or shadow-demand claim."""
    corners = np.where(
        CORNERS[None], metadata.boxes[:, 1, None], metadata.boxes[:, 0, None]
    )
    local = (corners - view.eye) @ view.basis.T
    planes = view.planes
    distances = local @ planes[:, :3].T + planes[:, 3]
    magnitude = np.abs(local) @ np.abs(planes[:, :3]).T + np.abs(planes[:, 3])
    guard = 64 * np.finfo(float).eps * np.maximum(magnitude, 1)
    needed = np.all(np.max(distances + guard, axis=1) >= 0, axis=1)
    return tuple(
        i for i, keep in zip(metadata.identifiers, needed, strict=True) if keep
    )


def views(prepared):
    first = replace(requests(prepared)[0][0], width=1280, height=720)
    return {
        "near": first,
        "far": replace(first, eye=first.target + 4 * (first.eye - first.target)),
    }


def choices(metadata, view):
    return {
        "full": metadata.identifiers,
        "radius": WindowRange(metadata).query(view.eye, 5.0).identifiers,
        "camera_bounds": camera_candidates(metadata, view),
    }


def export(output, mesh, prepared, view, ground_span, proof):
    output.mkdir(parents=True, exist_ok=False)
    write(
        output,
        "scene",
        mesh,
        {
            "camera": camera(view),
            "source_id": prepared.mass.identifier,
            "source": {"material_definitions": materials()},
            "facade_materials": {},
            "ground_span_m": ground_span,
            "scope": "Complete coarse building plus selected existing closed windows. All closed windows are the finite image reference; no whole physical building/interior, general visibility/shadow/radiance/LOD or native acceptance.",
        },
        proof,
    )
    return str((output / "scene-geometry.json").resolve())


def source(output, name, prepared, traced, proof):
    if traced:
        tracemalloc.start()
    metadata, bounds_cost = stage(lambda: compile_bounds(prepared), traced)
    poses = views(prepared)
    needed, selection_costs = {}, {}
    # All requests are resolved from plans/metadata before any detail construction.
    for pose, view in poses.items():
        needed[pose], selection_costs[pose] = stage(
            lambda view=view: choices(metadata, view), traced
        )
    products, coarse_costs, coarse_hashes = {}, {}, {}
    for method in ("full", "radius", "camera_bounds"):
        products[method], coarse_costs[method] = stage(
            lambda: WindowDetails(prepared, max_windows=MAX_WINDOWS), traced
        )
        coarse_hashes[method] = fingerprint(products[method].coarse.mesh)
    assert all(value == coarse_hashes["full"] for value in coarse_hashes.values())
    full_draw, full_cost = stage(
        lambda: products["full"].update(metadata.identifiers), traced
    )
    assert full_draw.detailed_ids == metadata.identifiers
    assert not full_draw.fallback_ids
    full_mesh = materialize(full_draw)
    del full_draw
    span = 2.5 * np.ptp(full_mesh["vertices"][:, :2], axis=0).max()
    rows, jobs = [], []
    for pose_index, (pose, view) in enumerate(poses.items()):
        for method, ids in needed[pose].items():
            if method == "full":
                mesh = full_mesh
                cost = (
                    full_cost
                    if pose_index == 0
                    else {
                        "seconds": 0.0,
                        "additional_traced_peak_bytes": 0 if traced else None,
                    }
                )
                generated = list(ids) if pose_index == 0 else []
                released, detail_bytes = (
                    [],
                    sum(
                        buffer_bytes(p.mesh) for p in products[method].resident.values()
                    ),
                )
            else:
                draw, cost = stage(
                    lambda method=method, ids=ids: products[method].update(ids), traced
                )
                assert draw.detailed_ids == ids and not draw.fallback_ids
                generated, released = list(draw.generated_ids), list(draw.released_ids)
                detail_bytes = draw.resident_detail_bytes
                mesh = materialize(draw)
                del draw
            geometry = export(
                output / name / pose / method, mesh, prepared, view, float(span), proof
            )
            jobs.append(
                {"case": name, "pose": pose, "method": method, "geometry": geometry}
            )
            rows.append(
                {
                    "pose": pose,
                    "method": method,
                    "selected_ids": list(ids),
                    "generated_ids": generated,
                    "released_ids": released,
                    "detail_cost": cost,
                    "detail_bytes": detail_bytes,
                    "triangles": len(mesh["faces"]),
                    "draw_bytes": buffer_bytes(mesh),
                    "draw_hashes": fingerprint(mesh),
                }
            )
    if traced:
        tracemalloc.stop()
    return {
        "case": name,
        "source_id": prepared.mass.identifier,
        "kind": prepared.mass.kind,
        "windows": len(metadata.identifiers),
        "metadata_bytes": metadata.boxes.nbytes,
        "metadata_cost": bounds_cost,
        "selection_costs": selection_costs,
        "coarse_costs": coarse_costs,
        "coarse_bytes": buffer_bytes(products["full"].coarse.mesh),
        "coarse_hashes": coarse_hashes["full"],
        "rows": rows,
    }, jobs


def run(args):
    proof = revision()
    if proof["experiment_tree_dirty"]:
        raise ValueError("Freeze image-control sources before running")
    args.output.mkdir(parents=True, exist_ok=False)
    start = time.perf_counter()
    plans = cases()
    preparation = time.perf_counter() - start
    rows, jobs = [], []
    for name, i in (("courts", 0), ("concave", 5), ("compact", 8)):
        row, work = source(args.output, name, plans[i], args.trace, proof)
        rows.append(row)
        jobs.extend(work)
    report = proof | {
        "input_sha256": {
            str(path): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in (
                Path("test/experiments/buildings/data/stepped-profiles.json"),
                Path(
                    "test/experiments/buildings/data/terraced-admission-controls.json"
                ),
            )
        },
        "trace": args.trace,
        "plan_preparation_seconds": preparation,
        "cases": rows,
        "jobs": jobs,
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Finite static Python controls at 1280x720, original explicit 5m radius versus metadata camera-plane candidates versus all closed windows in the same complete coarse hull. No occlusion, independent shadow/reflection demand, general LOD/radiance or native/device admission. Full reference generated once per source; its far row reuses that mesh without new construction. Existing selected-owner replacement semantics, including paired-wing owned accents, are unchanged. Diagnostic materialization/hash/export excluded from stage timers; process RSS includes them and all imports/plans/products. Trace omits some native heaps.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {"producer": proof["commit"], "jobs": len(jobs), "output": str(args.output)}
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--trace", action="store_true")
    run(parser.parse_args())
