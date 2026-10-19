"""Original cluster two-phase selection: old depth guesses, current depth decides."""

import argparse
import hashlib
import json
import resource
import time
import tracemalloc
from pathlib import Path

import numpy as np
from compare_plane_cloud import image_roi
from full_view_reference import load, views
from native_capture_meshes import fields_at, independent_depth, sample
from native_view_clusters import load_groups, selected_faces
from triangle_image_reference import (
    average_samples,
    image_rays,
    intersect_source,
    prepare_source,
    relight,
)
from two_phase_occlusion import classify, project, pyramid


def mesh_subset(source, faces):
    subset = {
        **source,
        "indices": source["indices"][faces],
        "binding": source["binding"][faces],
    }
    for field in ("positions", "uv", "normal", "vertex_normal", "colour", "metres"):
        assert subset[field] is source[field]
    return subset


def render_depth(source, faces, origins, directions, full_index):
    if not len(faces):
        return np.full(len(origins), np.inf), np.full(len(origins), -1, np.int64)
    full = len(faces) == len(source["indices"])
    subset = source if full else mesh_subset(source, faces)
    prepared = full_index if full else prepare_source(subset)
    d, f = intersect_source(subset, origins, directions, prepared=prepared)
    valid = f >= 0
    f[valid] = faces[f[valid]]
    return d, f


def checked_reference(
    source, groups, reference, live, origins, directions, depth, owner
):
    all_faces = selected_faces(groups, live)
    baseline_source = source if live.all() else mesh_subset(source, all_faces)
    baseline_index = reference if live.all() else prepare_source(baseline_source)
    expected, local_owner, fields = sample(
        baseline_source, baseline_index, origins, directions
    )
    expected_owner = local_owner.copy()
    valid = local_owner >= 0
    expected_owner[valid] = all_faces[local_owner[valid]]
    np.testing.assert_array_equal(owner, expected_owner)
    np.testing.assert_array_equal(depth, expected)
    candidate_fields = fields_at(source, origins, directions, depth, owner)
    for key in fields:
        np.testing.assert_array_equal(candidate_fields[key], fields[key])
    covered = np.flatnonzero(np.isfinite(expected))
    selected = np.unique(
        np.r_[
            np.linspace(0, len(origins) - 1, min(16, len(origins))).astype(int),
            covered[
                np.linspace(0, len(covered) - 1, min(16, len(covered))).astype(int)
            ],
        ]
    )
    brute = independent_depth(
        baseline_source["positions"][baseline_source["indices"]],
        origins[selected],
        directions[selected],
    )
    np.testing.assert_array_equal(np.isfinite(brute), np.isfinite(expected[selected]))
    finite = np.isfinite(brute)
    np.testing.assert_allclose(
        brute[finite], expected[selected][finite], atol=1e-8, rtol=1e-10
    )
    return expected, expected_owner, fields, candidate_fields, len(selected)


def execute(source, groups, reference, view, previous, *, live=None):
    box = image_roi(source, view)
    if box is None:
        return {"offscreen": True}, None, None
    width, height = (box[1] - box[0]).astype(int)
    origins, directions = image_rays(view, box, width, height, 2)
    start = time.perf_counter()
    live = np.ones(len(groups.bounds), bool) if live is None else live
    active = np.array([project(b, view)[0] for b in groups.bounds]) & live
    deferred, old_work = classify(groups.bounds, previous, eligible=active)
    first = active & ~deferred
    first_faces = selected_faces(groups, first)
    first_depth, first_owner = render_depth(
        source, first_faces, origins, directions, reference
    )
    current = pyramid(first_depth.reshape(height * 2, width * 2), view, box)
    rejected, current_work = classify(groups.bounds, current, eligible=deferred)
    restored = deferred & ~rejected
    second_faces = selected_faces(groups, restored)
    second_depth, second_owner = render_depth(
        source, second_faces, origins, directions, reference
    )
    use = (second_depth < first_depth) | (
        (second_depth == first_depth)
        & (second_owner >= 0)
        & (second_owner < first_owner)
    )
    depth = np.where(use, second_depth, first_depth)
    owner = np.where(use, second_owner, first_owner)
    next_history = pyramid(depth.reshape(height * 2, width * 2), view, box)
    selection_wall = time.perf_counter() - start
    expected, expected_owner, fields, candidate_fields, independent_rays = (
        checked_reference(
            source, groups, reference, live, origins, directions, depth, owner
        )
    )
    previous_bytes = 0 if previous is None else sum(x.nbytes for x in previous.levels)
    wrong = first_owner != expected_owner
    counts = np.diff(groups.offsets)
    report = {
        "roi": box.tolist(),
        "roi_pixels": [int(width), int(height)],
        "samples": len(origins),
        "current_frustum_groups": int(active.sum()),
        "baseline_triangles": int(counts[active].sum()),
        "first_groups": int(first.sum()),
        "first_triangles": len(first_faces),
        "deferred_groups": int(deferred.sum()),
        "restored_groups": int(restored.sum()),
        "second_triangles": len(second_faces),
        "rejected_groups": int(rejected.sum()),
        "submitted_triangles": len(first_faces) + len(second_faces),
        "separate_final_colour_triangles_if_required": len(first_faces)
        + len(second_faces),
        "stale_only_wrong_samples": int(wrong.sum()),
        "wrong_final_samples": 0,
        "independent_reference_rays": independent_rays,
        "old_queries": old_work,
        "current_queries": current_work,
        "pyramid_reduced_values": current.reduced_values + next_history.reduced_values,
        "pyramid_payload_bytes": sum(
            x.nbytes for x in current.levels + next_history.levels
        ),
        "additional_pyramid_reduced_values": current.reduced_values,
        "baseline_pyramid_reduced_values": next_history.reduced_values,
        "previous_pyramid_payload_bytes": previous_bytes,
        "candidate_selection_wall_s": selection_wall,
        "field_equal_keys": list(fields),
    }
    sun = np.array([0.3, 0.8, 0.5])
    original = np.dstack(
        (
            average_samples(relight(fields, sun, expected), width, height, 2),
            average_samples(np.isfinite(expected).astype(float), width, height, 2),
        )
    )
    first_fields = fields_at(source, origins, directions, first_depth, first_owner)
    first_rgb = relight(first_fields, sun, first_depth)
    prediction = np.dstack(
        (
            average_samples(first_rgb, width, height, 2),
            average_samples(np.isfinite(first_depth).astype(float), width, height, 2),
        )
    )
    final = np.dstack(
        (
            average_samples(relight(candidate_fields, sun, depth), width, height, 2),
            average_samples(np.isfinite(depth).astype(float), width, height, 2),
        )
    )
    arrays = {
        "original_depth": expected,
        "original_owner": expected_owner,
        "first_depth": first_depth,
        "first_owner": first_owner,
        "final_depth": depth,
        "final_owner": owner,
        "first_groups": first,
        "restored_groups": restored,
        "rejected_groups": rejected,
        "original_image": original,
        "first_image": prediction,
        "final_image": final,
    }
    return report, arrays, next_history


def contact(rows, path):
    from PIL import Image, ImageDraw

    canvas = Image.new("RGB", (1280, 720), "white")
    draw = ImageDraw.Draw(canvas)
    for row, (name, arrays) in enumerate(rows):
        for column, (label, key) in enumerate(
            (
                ("Full reference", "original_image"),
                ("First phase only", "first_image"),
                ("Both phases", "final_image"),
            )
        ):
            data = arrays[key]
            image = Image.fromarray(
                np.rint(np.clip(data[:, :, :3], 0, 1) * 255).astype(np.uint8)
            )
            scale = min(410 / image.width, 170 / image.height)
            image = image.resize(
                (
                    max(1, round(scale * image.width)),
                    max(1, round(scale * image.height)),
                ),
                Image.Resampling.NEAREST,
            )
            x, y = column * 426, row * 240
            canvas.paste(image, (x + 4, y + 30))
            draw.text((x + 4, y + 4), name + " / " + label, fill="black")
            draw.text(
                (x + 4, y + 207),
                f"{data.shape[1]}x{data.shape[0]} px; stored-normal Lambert control",
                fill="black",
            )
    canvas.save(path)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--trace-allocations", action="store_true")
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    source, document, context, receipt = load(args.input)
    groups = load_groups(args.input, document, source)
    np.savez_compressed(args.output / "groups.npz", **vars(groups))
    reference = prepare_source(source)
    poses = views(document, context)
    reports, rows = {}, []
    if args.trace_allocations:
        tracemalloc.start()
    start = time.perf_counter()
    for track in ("static", "moving"):
        previous = None
        for name, view in poses.items():
            if not name.startswith(track + "-"):
                continue
            report, arrays, previous = execute(
                source, groups, reference, view, previous
            )
            reports[name] = report
            if arrays is not None:
                np.savez_compressed(args.output / (name + ".npz"), **arrays)
                if name in ("static-020", "moving-021"):
                    rows.append((name, arrays))
    # Deliberately wrong historical occluder: never a valid final rejection.
    view = poses["static-020"]
    box = image_roi(source, view)
    wide_box = box + np.array([[-100, -100], [100, 100]])
    extent = np.rint((wide_box[1] - wide_box[0]) * 2).astype(int)
    false_history = pyramid(np.full(tuple(extent[::-1]), 0.5), view, wide_box)
    report, arrays, _ = execute(source, groups, reference, view, false_history)
    if report["stale_only_wrong_samples"] == 0 or report["restored_groups"] == 0:
        raise ValueError("stale-depth negative control failed to become red")
    reports["poisoned-history"] = report
    rows.append(("poisoned-history", arrays))
    np.savez_compressed(args.output / "poisoned-history.npz", **arrays)
    # Actual topology change: remove the cluster covering most visible samples.
    ids = arrays["original_owner"]
    which = np.empty(len(source["indices"]), int)
    for i, (a, b) in enumerate(
        zip(groups.offsets[:-1], groups.offsets[1:], strict=True)
    ):
        which[groups.faces[a:b]] = i
    removed = int(
        np.bincount(which[ids[ids >= 0]], minlength=len(groups.bounds)).argmax()
    )
    live = np.ones(len(groups.bounds), bool)
    live[removed] = False
    old = pyramid(
        arrays["original_depth"].reshape(
            report["roi_pixels"][1] * 2, report["roi_pixels"][0] * 2
        ),
        view,
        box,
    )
    report, changed, _ = execute(source, groups, reference, view, old, live=live)
    report["removed_original_cluster"] = removed
    reports["removed-cluster"] = report
    np.savez_compressed(args.output / "removed-cluster.npz", **changed)
    elapsed = time.perf_counter() - start
    peak = tracemalloc.get_traced_memory()[1] if args.trace_allocations else None
    if args.trace_allocations:
        tracemalloc.stop()
    contact(rows, args.output / "comparison.png")
    visible = [
        v
        for k, v in reports.items()
        if k.startswith(("static-", "moving-")) and not v.get("offscreen")
    ]
    result = {
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "source_hashes": {
            n: hashlib.sha256((Path(__file__).parent / n).read_bytes()).hexdigest()
            for n in (
                "native_view_clusters.py",
                "two_phase_occlusion.py",
                "compare_two_phase.py",
                "full_view_reference.py",
                "native_capture_meshes.py",
                "triangle_image_reference.py",
            )
        },
        "input_receipt": receipt,
        "scope": "all original level-zero cluster membership; derived AABBs, finite 2x2 rays, opaque assumption; not native sphere/Float32/raster/PBR/device admission",
        "original_clusters": len(groups.bounds),
        "original_triangles": len(source["indices"]),
        "group_array_bytes": sum(a.nbytes for a in vars(groups).values()),
        "main_poses": len(poses),
        "visible_poses": len(visible),
        "baseline_submitted_triangles": sum(v["baseline_triangles"] for v in visible),
        "two_phase_submitted_triangles": sum(v["submitted_triangles"] for v in visible),
        "complete_diagnostic_wall_s": elapsed,
        "comparison_cost_scope": "all diagnostics incl references/controls; selection includes both group-classification phases, index rebuilds, both depth passes and both pyramids; baseline/field validation excluded; not native time",
        "complete_diagnostic_traced_peak_bytes": peak,
        "allocation_tracing": args.trace_allocations,
        "peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024,
        "allocation_count": None,
        "physical_copied_bytes": None,
        "poses": reports,
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: v
                for k, v in result.items()
                if k not in ("poses", "input_receipt", "source_hashes")
            }
        )
    )


if __name__ == "__main__":
    main()
