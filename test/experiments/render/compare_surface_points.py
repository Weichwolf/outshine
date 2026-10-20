"""Whole original cell versus a bounded geometric sphere-cut preflight (#952)."""

import argparse
import hashlib
import json
import time
import tracemalloc
from pathlib import Path

import numpy as np
from compare_visible_normals import inputs
from surface_point_hierarchy import build, cut, load, verify_partition
from triangle_image_reference import (
    image_rays,
    intersect_source,
    prepare_source,
    projected_bounds,
)


def circles(screen, box, supersample=4):
    width, height = map(int, box[1] - box[0])
    mask = np.zeros((height * supersample, width * supersample), bool)
    tests = 0
    for x, y, radius in screen:
        if not np.isfinite([x, y, radius]).all():
            raise ValueError("unprojectable leaf remains local unresolved geometry")
        low = np.maximum(
            np.floor((np.array([x, y]) - radius - box[0]) * supersample), 0
        ).astype(int)
        high = np.minimum(
            np.ceil((np.array([x, y]) + radius - box[0]) * supersample),
            np.array([width, height]) * supersample,
        ).astype(int)
        if np.any(high <= low):
            continue
        xx = box[0, 0] + (np.arange(low[0], high[0]) + 0.5) / supersample
        yy = box[0, 1] + (np.arange(low[1], high[1]) + 0.5) / supersample
        inside = (xx[None] - x) ** 2 + (yy[:, None] - y) ** 2 <= radius**2
        mask[low[1] : high[1], low[0] : high[0]] |= inside
        tests += int(inside.size)
    return mask.reshape(height, supersample, width, supersample).mean((1, 3)), tests


def compare_mask(source, prepared, rows, view, path):
    bounds = projected_bounds(source["positions"], view)
    screen = rows["screen"]
    if len(screen):
        bounds = np.stack(
            [
                np.minimum(bounds[0], (screen[:, :2] - screen[:, 2:]).min(0)),
                np.maximum(bounds[1], (screen[:, :2] + screen[:, 2:]).max(0)),
            ]
        )
    box = np.stack(
        [
            np.maximum(np.floor(bounds[0]), [0, 0]),
            np.minimum(np.ceil(bounds[1]), view[3:]),
        ]
    ).astype(int)
    width, height = map(int, box[1] - box[0])
    if min(width, height) <= 0 or width * height > 262144:
        raise ValueError("joint same-resolution diagnostic ROI budget")
    origins, directions = image_rays(view, box, width, height, 4)
    depth, _face = intersect_source(source, origins, directions, prepared=prepared)
    reference = np.isfinite(depth).reshape(height, 4, width, 4).mean((1, 3))
    all_leaves, tests = circles(screen, box)
    admitted, smaller = circles(screen[2 * screen[:, 2] <= 1], box)
    np.savez_compressed(
        path,
        reference=reference,
        leaf_stop=all_leaves,
        admissible_only=admitted,
        box=box,
    )

    def errors(candidate):
        return {
            "alpha_sum_px": float(candidate.sum()),
            "lost_alpha_px": float(np.maximum(reference - candidate, 0).sum()),
            "extra_alpha_px": float(np.maximum(candidate - reference, 0).sum()),
        }

    return {
        "roi": box.tolist(),
        "reference_alpha_px": float(reference.sum()),
        "leaf_stop_unadmitted": errors(all_leaves),
        "admissible_only_incomplete": errors(admitted),
        "reference_subpixel_rays": len(depth),
        "circle_sample_tests": tests + smaller,
    }


def save_view(view):
    return [np.asarray(view[0]).tolist(), np.asarray(view[1]).tolist(), *view[2:]]


def fallback_policy(tree, rows, source, view):
    """Offline conservative policy cost, not a metadata-only runtime selector."""
    need = np.zeros(len(tree.order), bool)
    bad = rows["selected"][2 * rows["screen"][:, 2] > 1]
    for node in np.r_[bad, rows["unresolved"]]:
        first, count = tree.first[node], tree.count[node]
        need[tree.order[first : first + count]] = True
    faces = np.any(need[source["indices"]], axis=1)
    triangle = source["positions"].astype(float)[source["indices"]]
    normal = np.cross(triangle[:, 1] - triangle[:, 0], triangle[:, 2] - triangle[:, 0])
    front = np.sum(normal * (triangle[:, 0] - view[0]), axis=1) < 0
    return {
        "faces": int(faces.sum()),
        "source_ordinals": np.unique(source["binding"][faces, 1]).tolist(),
        "front_faces": int((faces & front).sum()),
        "front_source_ordinals": np.unique(
            source["binding"][faces & front, 1]
        ).tolist(),
        "scope": "any unsupported vertex requests all its incident original faces; a conservative policy, no optimal hybrid lower bound",
    }


def run(path, output):
    output.mkdir(parents=True, exist_ok=True)
    began = time.perf_counter()
    source, document, _context, receipt, poses, selected, *_ = inputs(path)
    source_seconds = time.perf_counter() - began
    began = time.perf_counter()
    tree, report = build(source["positions"], source["indices"])
    report["build_seconds"] = time.perf_counter() - began
    tracemalloc.start()
    repeated, _ = build(source["positions"], source["indices"])
    report["build_python_traced_peak_bytes"] = tracemalloc.get_traced_memory()[1]
    report["build_retained_traced_allocation_blocks"] = sum(
        s.count for s in tracemalloc.take_snapshot().statistics("filename")
    )
    tracemalloc.stop()
    for key, value in tree.arrays().items():
        np.testing.assert_array_equal(value, repeated.arrays()[key])
    del repeated
    np.savez_compressed(output / "hierarchy.npz", **tree.arrays())
    packet = load(output / "hierarchy.npz")
    pose_json = {name: save_view(view) for name, view in poses.items()}
    (output / "views.json").write_text(json.dumps(pose_json))
    report.update(
        source_prepare_seconds=source_seconds,
        scope="geometry/density only; no normal/material/depth/PBR/opaque-occluder admission",
        input_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        receipt=receipt,
        original_native_built_bytes=document["results"][0]["built_bytes"],
        original_neutral_uint32_array_bytes=sum(
            a.nbytes for k, a in source.items() if k not in ("vertex_normal", "indices")
        )
        + source["indices"].size * 4,
        source_input_file_bytes=path.stat().st_size,
        profile={
            "width": next(iter(poses.values()))[3],
            "height": next(iter(poses.values()))[4],
            "focal_px": next(iter(poses.values()))[2],
            "diameter_px": 1.0,
            "supersample": 4,
        },
        poses={},
    )
    cuts = {}
    began = time.perf_counter()
    for name, view in poses.items():
        rows, work = cut(packet, view)
        work["verified_original_vertex_addresses"] = verify_partition(packet, rows)
        report["poses"][name] = work
        for key, value in rows.items():
            cuts[f"{name}_{key}"] = value
    report["all_query_and_partition_seconds"] = time.perf_counter() - began
    np.savez_compressed(output / "cuts.npz", **cuts)
    report["all_cut_arrays_bytes"] = sum(a.nbytes for a in cuts.values())
    prepared = prepare_source(source)
    report["images"] = {}
    for name, view in selected.items():
        rows, _ = cut(packet, view)
        report["images"][name] = compare_mask(
            source, prepared, rows, view, output / f"{name}.npz"
        )
        report["images"][name]["conservative_original_fallback"] = fallback_policy(
            packet, rows, source, view
        )
        print(name, report["images"][name], flush=True)
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


def replay(path, views, output):
    tree = load(path)
    cameras = json.loads(views.read_text())
    arrays = {}
    for name, view in cameras.items():
        rows, _ = cut(tree, view)
        verify_partition(tree, rows)
        arrays.update({f"{name}_{key}": value for key, value in rows.items()})
    np.savez_compressed(output, **arrays)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        type=Path,
        default=Path(__file__).resolve().parents[2]
        / "outshine/data/render/building-full-view/cell.json.gz",
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--hierarchy", type=Path)
    parser.add_argument("--views", type=Path)
    args = parser.parse_args()
    if args.hierarchy:
        replay(args.hierarchy, args.views, args.output)
    else:
        run(args.input, args.output)
