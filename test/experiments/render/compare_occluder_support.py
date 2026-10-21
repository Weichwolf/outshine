"""Current triangle seeds versus old geometric supports on unchanged real cell geometry."""

import argparse
import gzip
import hashlib
import json
import os
import resource
import subprocess
import time
import tracemalloc
from pathlib import Path

import numpy as np
from current_visibility import box_bounds, mesh_boxes, occluded, project, raster
from depth_reprojection import depth_errors
from occluder_control_scene import scene as analytic_scene
from supported_occluders import (
    raster_faces,
    raster_patches,
    selected_faces,
    tile_patches,
)


def measured(call, trace):
    if trace:
        tracemalloc.start()
    start = time.perf_counter()
    result = call()
    elapsed = time.perf_counter() - start
    peak = tracemalloc.get_traced_memory()[1] if trace else None
    if trace:
        tracemalloc.stop()
    return result, {"seconds": elapsed, "traced_peak_bytes": peak}


def load_cell(path):
    manifest = json.loads((path.parent / "manifest.json").read_text())
    entry = manifest[path.name]
    raw = path.read_bytes()
    if len(raw) != entry["bytes"] or hashlib.sha256(raw).hexdigest() != entry["sha256"]:
        raise ValueError("Original cell differs from its versioned manifest")
    document = json.loads(gzip.decompress(raw))
    result = document["results"][0]
    geometry = result["geometry"]
    vertices, triangles = [], []
    offset = 0
    for name in ["walls", "roofs"]:
        points = np.asarray(geometry[name]["positions"], dtype=np.float32).astype(float)
        faces = np.asarray(geometry[name]["triangles"], dtype=np.int64)
        vertices.append(points)
        triangles.append(faces + offset)
        offset += len(points)
    points, faces = np.concatenate(vertices), np.concatenate(triangles)
    source_ids = [source["index"] for source in document["sources"]]
    source_slot = {value: index for index, value in enumerate(source_ids)}
    owners = np.full(len(faces), -1, dtype=np.int64)
    for call in result["mesh_calls"]:
        for name, begin in [("wall", 0), ("roof", len(triangles[0]))]:
            first, count = call[name + "_index_range"]
            a, b = begin + first // 3, begin + (first + count) // 3
            if first % 3 or count % 3 or (owners[a:b] >= 0).any():
                raise ValueError("Invalid original source index partition")
            owners[a:b] = source_slot[call["source"]]
    if (
        (owners < 0).any()
        or faces.max() >= len(points)
        or not np.isfinite(points).all()
    ):
        raise ValueError("Incomplete real-cell geometry/source binding")
    return points, faces, owners, document, hashlib.sha256(raw).hexdigest()


def camera(document, points):
    up = np.array(document["anchor_ecef"], float)
    up /= np.linalg.norm(up)
    target = (points.min(axis=0) + points.max(axis=0)) / 2
    native_eye = np.array(document["view"]["eye_ecef"]) - document["anchor_ecef"]
    delta = native_eye - target
    eye = target + delta / np.linalg.norm(delta) * 500
    forward = (target - eye) / np.linalg.norm(target - eye)
    right = np.cross(forward, up)
    right /= np.linalg.norm(right)
    basis = np.stack((right, np.cross(right, forward), forward))
    return eye, basis, document["view"]["focal_px"], 1280, 720


def scene_variants(points, faces, owners, old_view, old_winners):
    eye, basis, focal, width, height = old_view
    valid = old_winners >= 0
    changed = int(np.bincount(owners[old_winners[valid]]).argmax())
    angle = np.deg2rad(6)
    c, s = np.cos(angle), np.sin(angle)
    rotation = np.array([[c, 0, -s], [0, 1, 0], [s, 0, c]])
    turned = rotation @ basis
    all_live = np.ones(len(faces), bool)
    history_valid = owners != changed
    moved = points.copy()
    changed_vertices = np.unique(faces[~history_valid])
    if np.intersect1d(changed_vertices, faces[history_valid]).size:
        raise ValueError(
            "Dynamic source test requires independent original vertex ownership"
        )
    moved[changed_vertices] += 20 * basis[0]
    rows = [
        ("rotation", points, all_live, all_live, (eye, turned, focal, width, height)),
        (
            "translation",
            points,
            all_live,
            all_live,
            (eye + 6 * basis[0], basis, focal, width, height),
        ),
        (
            "combined",
            points,
            all_live,
            all_live,
            (eye + 6 * basis[0], turned, focal, width, height),
        ),
        ("source_removed", points, history_valid, history_valid, old_view),
        ("source_moved", moved, all_live, history_valid, old_view),
        (
            "projection",
            points,
            all_live,
            all_live,
            (eye, basis, focal * 1.05, width, height),
        ),
    ]
    return rows, changed


def reference(points, faces, live, view):
    ids = np.flatnonzero(live)
    p, z = project(points, view)
    depth, winners = raster(p, z, faces[ids], view[3], view[4], with_faces=True)
    winner_ids = np.full(winners.shape, -1, dtype=np.int64)
    valid = winners >= 0
    winner_ids[valid] = ids[winners[valid]]
    return depth, winner_ids


def evaluate(points, faces, owners, live, view, seed_depth, full, winners):
    boxes = mesh_boxes(points, faces[live], owners[live], int(owners.max()) + 1)
    present = np.isfinite(boxes[0]).all(axis=1)
    # Removed sources have no bound and must not enter numeric projection.
    compact = tuple(b[present] for b in boxes)
    lo, hi, nearest, safe, visible, _ = box_bounds(compact, view)
    hidden = occluded(lo, hi, nearest, safe, seed_depth, 4)
    keep = np.zeros(len(present), bool)
    keep[present] = visible & ~hidden
    accepted = live & keep[owners]
    image, ids = reference(points, faces, accepted, view)
    changed = ids != winners
    return {
        "visible_sources": int(np.count_nonzero(visible)),
        "hidden_sources": int(np.count_nonzero(hidden & visible)),
        "late_colour_triangles": int(np.count_nonzero(accepted)),
        "changed_original_face_pixels": int(np.count_nonzero(changed)),
        "maximum_final_depth_error": float(np.abs(image - full).max()),
        "seed_depth_errors": depth_errors(seed_depth, full),
    }, keep


def compare_pose(
    output,
    variant,
    faces,
    owners,
    old_points,
    old_seeds,
    quads,
    patch_owners,
    budget,
    trace,
):
    name, points, live, valid_history, view = variant
    output.mkdir()
    (full, winners), ref_cost = measured(
        lambda: reference(points, faces, live, view), trace
    )
    methods, depths = {}, {}
    calls = {
        "current_seeds": lambda: raster_faces(
            points, faces, selected_faces(points, faces, view, live, budget), view
        ),
        "history_triangles": lambda: raster_faces(
            old_points, faces, old_seeds[valid_history[old_seeds]], view
        ),
        "history_patches": lambda: raster_patches(
            quads, patch_owners, view, valid_history
        ),
    }
    for name, call in calls.items():
        (depth, work), cost = measured(call, trace)
        result, keep = evaluate(points, faces, owners, live, view, depth, full, winners)
        methods[name] = result | {
            "cost": cost,
            "raster_work": work,
            "kept_source_ordinals": np.flatnonzero(keep).tolist(),
        }
        depths[name] = depth
        if (
            result["changed_original_face_pixels"]
            or result["maximum_final_depth_error"] != 0
        ):
            raise ValueError("Candidate lost actual current original geometry")
    potential, _ = evaluate(points, faces, owners, live, view, full, full, winners)
    np.savez_compressed(
        output / "depths.npz", reference=full, original_face=winners, **depths
    )
    return {
        "pose": variant[0],
        "eye": view[0].tolist(),
        "basis": view[1].tolist(),
        "focal": view[2],
        "reference_cost": ref_cost,
        "full_depth_diagnostic": potential,
        "methods": methods,
    }


def input_scene(args):
    if args.control:
        points, faces, owners, document, old_view = analytic_scene()
        source_hash = hashlib.sha256(
            points.tobytes() + faces.tobytes() + owners.tobytes()
        ).hexdigest()
        return points, faces, owners, document, old_view, source_hash
    points, faces, owners, document, source_hash = load_cell(args.input)
    return points, faces, owners, document, camera(document, points), source_hash


def run(args):
    here = Path(__file__).resolve().parent
    sources = [
        here / name
        for name in [
            Path(__file__).name,
            "supported_occluders.py",
            "current_visibility.py",
            "depth_reprojection.py",
            "occluder_control_scene.py",
        ]
    ]
    if subprocess.check_output(
        [
            "git",
            "status",
            "--porcelain",
            "--untracked-files=all",
            "--",
            *map(str, sources),
        ],
        text=True,
    ).strip():
        raise ValueError("Freeze the measured geometry-control sources first")
    args.output.mkdir(parents=True, exist_ok=False)
    points, faces, owners, document, old_view, source_hash = input_scene(args)
    (_, old_winners), old_cost = measured(
        lambda: reference(points, faces, np.ones(len(faces), bool), old_view),
        args.trace,
    )
    old_seeds, selection_cost = measured(
        lambda: selected_faces(
            points, faces, old_view, np.ones(len(faces), bool), args.seeds
        ),
        args.trace,
    )
    (quads, patch_owners, work), support_cost = measured(
        lambda: tile_patches(points, faces, old_seeds, old_view, args.block), args.trace
    )
    variants, changed_source = scene_variants(
        points, faces, owners, old_view, old_winners
    )
    rows = [
        compare_pose(
            args.output / row[0],
            row,
            faces,
            owners,
            points,
            old_seeds,
            quads,
            patch_owners,
            args.seeds,
            args.trace,
        )
        for row in variants
    ]
    np.savez_compressed(
        args.output / "support.npz",
        quads=quads,
        owners=patch_owners,
        source_owner=owners,
        old_seed_faces=old_seeds,
    )
    report = {
        "producer_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "input_sha256": source_hash,
        "input_kind": "analytic layered opaque control"
        if args.control
        else "original real-cell opaque control",
        "implementation_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in sources
        },
        "source_registry": [
            {"index": s["index"], "id": s["id"]} for s in document["sources"]
        ],
        "original_position_array_bytes": points.nbytes,
        "original_triangle_array_bytes": faces.nbytes,
        "source_owner_array_bytes": owners.nbytes,
        "source_count": len(document["sources"]),
        "original_triangles": len(faces),
        "old_view": {
            "eye": old_view[0].tolist(),
            "basis": old_view[1].tolist(),
            "focal": old_view[2],
        },
        "seed_budget": args.seeds,
        "block_pixels": args.block,
        "support_work": work,
        "support_array_bytes": quads.nbytes + patch_owners.nbytes,
        "old_full_diagnostic_cost": old_cost,
        "old_selection_cost": selection_cost,
        "support_construction_cost": support_cost,
        "changed_source_ordinal": changed_source,
        "poses": rows,
        "trace": args.trace,
        "host_load_average": os.getloadavg(),
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Either complete original real-cell geometry under an explicit all-opaque CPU assumption, or separate analytic layered opaque geometry; native material bindings remain unqualified. Real-cell control uses a 500m eye; analytic control uses its explicit metre geometry. Both 1280x720, not native recorded poses or complete world. Original 3D edges/planes certify geometric support with finite Double guards; no all-input Float32 coverage guarantee. Unknown/changed source support released. Point-depth alone never supplies these tiles. Old seed selection uses old geometry/projection, not full current depth or reference winners. Extra prepass submits seed triangles; late colour submits original retained triangles again. Full current depth is a diagnostic potential control only, never candidate selection/repair. Python times omit native GPU passes/fences/IO. Source IDs/ranges retained; dynamic changes are diagnostic source edits, not a completed runtime feature.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {"producer": report["producer_commit"], "poses": len(rows), "support": work}
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    inputs = parser.add_mutually_exclusive_group(required=True)
    inputs.add_argument("--input", type=Path)
    inputs.add_argument("--control", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--seeds", type=int, default=64)
    parser.add_argument("--block", type=int, default=16)
    parser.add_argument("--trace", action="store_true")
    run(parser.parse_args())
