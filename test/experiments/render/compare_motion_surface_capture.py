"""One complete coarse cell versus two-view/two-front-layer surface captures."""

import argparse
import hashlib
import json
import resource
import subprocess
import time
from functools import partial
from pathlib import Path

import numpy as np
from attributed_prisms import native_fields, native_inputs
from compare_plane_cloud import image_roi, select_full_poses
from full_view_reference import load, views
from motion_surface_capture import (
    decoded,
    front_layers,
    intrinsic_cell_box,
    select_groups,
    select_intrinsic_cell,
    shared_mesh,
    source_groups,
)
from native_capture_meshes import independent_depth
from triangle_image_reference import (
    average_samples,
    depth_difference,
    difference,
    image_rays,
    intersect_source,
    panel,
    prepare_source,
    relight,
)

SUNS = ((0.3, -0.4, 0.8), (-0.6, 0.5, 0.2))


def stage(call):
    start = time.perf_counter()
    result = call()
    return result, time.perf_counter() - start


def look_at(eye, source, up, projection):
    target = (source["positions"].min(0) + source["positions"].max(0)) / 2
    forward = target - eye
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, up)
    right /= np.linalg.norm(right)
    return eye, np.array([right, np.cross(right, forward), forward]), *projection


def stored(source, faces, path):
    product, seconds = stage(lambda: shared_mesh(source, faces))
    np.savez_compressed(path, **product)
    with np.load(path, allow_pickle=False) as archive:
        loaded = {key: archive[key] for key in archive.files}
    current = decoded(loaded)
    inputs = native_inputs(current)
    prepared, prepare_seconds = stage(lambda: prepare_source(current))
    lineage = sum(loaded[k].nbytes for k in ("binding", "original_faces"))
    core = sum(value.nbytes for value in loaded.values()) - lineage
    return (current, prepared, inputs), {
        "triangles": len(faces),
        "vertices": len(current["positions"]),
        "sources": len(np.unique(current["binding"][:, 1])),
        "core_array_bytes": core,
        "lineage_bytes": lineage,
        "stored_array_bytes": core + lineage,
        "container_bytes": path.stat().st_size,
        "build_seconds": seconds,
        "diagnostic_index_seconds": prepare_seconds,
        "decode_normal_bytes": current["vertex_normal"].nbytes,
        "vertex_stage_bytes": inputs["uv"].nbytes + inputs["flat_codes"].nbytes,
    }


def field_sample(product, origins, directions):
    source, prepared, inputs = product
    depth, faces = intersect_source(source, origins, directions, prepared=prepared)
    fields = native_fields(inputs, origins, directions, depth, faces)
    sources = np.full(len(faces), -1, np.int32)
    hit = faces >= 0
    sources[hit] = source["binding"][faces[hit], 1]
    return depth, faces, sources, fields


def images(hit, width, height):
    depth, _faces, _sources, fields = hit
    alpha = np.isfinite(depth).astype(float)
    return [
        np.dstack(
            (
                average_samples(
                    relight(fields | {"colour": fields["colour"][:, :3]}, sun, depth),
                    width,
                    height,
                    2,
                ),
                average_samples(alpha, width, height, 2),
            )
        )
        for sun in SUNS
    ]


def compare(original, candidate):
    a, b = np.isfinite(original[0]), np.isfinite(candidate[0])
    common = a & b
    same = common & (original[2] == candidate[2])
    equal_code = original[3]["code"] == candidate[3]["code"]
    report = depth_difference(original[0], candidate[0], common) | {
        "lost_samples": int((a & ~b).sum()),
        "extra_samples": int((~a & b).sum()),
        "changed_source_samples": int((common & ~same).sum()),
        "changed_code_samples": int((common & ~equal_code).sum()),
        "max_normal_unit_distance": float(
            np.linalg.norm(
                original[3]["normal"][common] - candidate[3]["normal"][common], axis=1
            ).max()
        )
        if common.any()
        else None,
    }
    for key in ("coordinates", "metres", "colour"):
        match = same & equal_code
        report[f"max_{key}_component_same_source_code"] = (
            float(abs(original[3][key][match] - candidate[3][key][match]).max())
            if match.any()
            else None
        )
    changed = a != b
    changed[common] |= (
        ~same[common]
        | ~equal_code[common]
        | (abs(original[0][common] - candidate[0][common]) > 1e-6)
    )
    return report, changed


def controls(products, hits, origins, directions, changed):
    selected = np.unique(
        np.r_[
            np.flatnonzero(changed),
            np.linspace(0, len(origins) - 1, min(16, len(origins))).astype(int),
        ]
    )
    pairs = []
    for product, hit in zip(products, hits, strict=True):
        source = product[0]
        count = len(selected) * len(source["indices"])
        if count > 8388608:
            raise ValueError("Independent all-critical-ray control budget exhausted")
        depth = independent_depth(
            source["positions"][source["indices"]],
            origins[selected],
            directions[selected],
        )
        finite = np.isfinite(depth)
        np.testing.assert_array_equal(finite, np.isfinite(hit[0][selected]))
        np.testing.assert_allclose(
            depth[finite], hit[0][selected][finite], atol=1e-8, rtol=1e-10
        )
        pairs.append(count)
    return {"rays": len(selected), "triangle_ray_pairs": pairs}


def capture_views(source, document, poses):
    up = np.asarray(document["anchor_ecef"], float)
    up /= np.linalg.norm(up)
    return [
        look_at(poses[name][0], source, up, poses[name][2:])
        for name in ("static-001", "moving-060")
    ]


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    (source, document, context, receipt), decode_seconds = stage(
        lambda: load(args.input)
    )
    poses = views(document, context)
    original = source, prepare_source(source), native_inputs(source)
    captures, kept = [], []
    capture_poses = capture_views(source, document, poses)
    for number, view in enumerate(capture_poses):
        box = image_roi(source, view)
        width, height = (box[1] - box[0]).astype(int)
        origins, directions = image_rays(view, box, width, height, 2)
        (faces, records, work), seconds = stage(
            partial(front_layers, source, original[1], origins, directions)
        )
        if not len(faces):
            raise ValueError("Empty capture is not a complete-view comparison")
        kept.append(faces)
        np.savez_compressed(
            args.output / f"capture{number}.npz",
            records=records,
            faces=faces,
            eye=view[0],
            basis=view[1],
            box=box,
        )
        captures.append(
            work
            | {
                "seconds": seconds,
                "faces": len(faces),
                "diagnostic_record_bytes": records.nbytes,
                "eye": view[0].tolist(),
            }
        )
    faces = np.unique(np.concatenate(kept))
    owners = np.unique(source["binding"][faces, 1])
    closure = np.flatnonzero(np.isin(source["binding"][:, 1], owners))
    products, payload = {"original": original}, {}
    for name, selected in (
        ("coarse", np.arange(len(source["indices"]))),
        ("captured-faces", faces),
        ("captured-sources", closure),
    ):
        products[name], payload[name] = stored(
            source, selected, args.output / f"{name}.npz"
        )
    (bounds, groups), group_seconds = stage(lambda: source_groups(source))
    (cell_axes, cell_bounds), cell_seconds = stage(
        partial(intrinsic_cell_box, source["positions"], document["anchor_ecef"])
    )
    np.savez_compressed(
        args.output / "intrinsic-cell.npz", axes=cell_axes, bounds=cell_bounds
    )
    group_offsets = np.cumsum([0] + [len(group) for group in groups], dtype=np.uint32)
    group_faces = np.concatenate(groups)
    np.savez_compressed(
        args.output / "groups.npz",
        bounds=bounds,
        offsets=group_offsets,
        faces=group_faces,
    )
    for delta in (-30.0, 6.0, 30.0):
        first = capture_poses[0]
        poses[f"side{delta:+g}m"] = (first[0] + delta * first[1][0], *first[1:])
    selected_panels = set(select_full_poses(source, poses))
    reports, presentations, previous = {}, {}, set()
    for name, view in poses.items():
        selected, selection_seconds = stage(partial(select_groups, bounds, view))
        active = set(map(int, selected))
        cell_visible, cell_selection_seconds = stage(
            partial(select_intrinsic_cell, cell_axes, cell_bounds, view)
        )
        if cell_visible != bool(active):
            raise ValueError(
                "Single intrinsic cell and grouped control draw sets differ"
            )
        group_report = {
            "active_groups": sorted(active),
            "entering_groups": sorted(active - previous),
            "departing_groups": sorted(previous - active),
            "metadata_box_tests": len(bounds),
            "selection_seconds": selection_seconds,
            "selected_triangles": sum(len(groups[i]) for i in active),
            "intrinsic_cell_visible": cell_visible,
            "intrinsic_cell_box_tests": 1,
            "intrinsic_cell_selection_seconds": cell_selection_seconds,
        }
        previous = active
        box = image_roi(source, view)
        if box is None:
            reports[name] = {"offscreen": True, "groups": group_report}
            continue
        width, height = (box[1] - box[0]).astype(int)
        if width * height * 4 > 32768:
            raise ValueError("Finite full-resolution image ROI exceeded")
        origins, directions = image_rays(view, box, width, height, 2)
        hits, query_seconds = {}, {}
        for path, product in products.items():
            hits[path], query_seconds[path] = stage(
                partial(field_sample, product, origins, directions)
            )
        a = hits["original"]
        active_faces = (
            np.concatenate([groups[i] for i in active]) if active else np.empty(0, int)
        )
        if not np.isin(a[1][a[1] >= 0], active_faces).all():
            raise ValueError("Coarse group culling lost a visible original surface")
        errors, changed = {}, np.zeros(len(origins), bool)
        pictures = {path: images(hit, width, height) for path, hit in hits.items()}
        for path in ("coarse", "captured-faces", "captured-sources"):
            errors[path], differences = compare(a, hits[path])
            changed |= differences
            errors[path]["image_lights"] = [
                difference(x, y)
                for x, y in zip(pictures["original"], pictures[path], strict=True)
            ]
        verification = controls(
            [products[p] for p in products],
            [hits[p] for p in products],
            origins,
            directions,
            changed,
        )
        arrays = {"origins": origins, "directions": directions}
        for path, hit in hits.items():
            for key, value in zip(("depth", "face", "source"), hit[:3], strict=True):
                arrays[f"{path}_{key}"] = value
            for key, value in hit[3].items():
                arrays[f"{path}_{key}"] = value
        np.savez_compressed(args.output / f"{name}.npz", **arrays)
        reports[name] = {
            "offscreen": False,
            "groups": group_report,
            "box": box.tolist(),
            "rays": len(origins),
            "query_seconds": query_seconds,
            "errors": errors,
            "independent": verification,
            "diagnostic_output_array_bytes": sum(a.nbytes for a in arrays.values()),
        }
        presentations[name] = pictures
        print(
            name,
            "loss",
            errors["captured-faces"]["lost_samples"],
            errors["captured-sources"]["lost_samples"],
            flush=True,
        )
    for path in ("captured-faces", "captured-sources"):
        for metric in ("lost_samples", "changed_code_samples"):
            selected_panels.add(
                max(presentations, key=lambda n: reports[n]["errors"][path][metric])
            )
    for name in selected_panels:
        panel(
            args.output / f"{name}.png",
            [
                {path: pictures[row] for path, pictures in presentations[name].items()}
                for row in range(2)
            ],
            name,
        )
    report = {
        "commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "receipt": receipt,
        "uncaptured_source_ids": np.setdiff1d(
            np.unique(source["binding"][:, 1]), owners
        ).tolist(),
        "uncaptured_reason": "No triangle selected by either projector's two front layers; every source remains in the full coarse product.",
        "decode_verify_seconds": decode_seconds,
        "captures": captures,
        "payload": payload,
        "groups": {
            "count": len(groups),
            "build_seconds": group_seconds,
            "array_bytes": bounds.nbytes + group_offsets.nbytes + group_faces.nbytes,
        },
        "intrinsic_cell": {
            "build_seconds": cell_seconds,
            "metadata_bytes": cell_axes.nbytes + cell_bounds.nbytes,
            "same_grouped_primitive_sets_at_every_pose": True,
        },
        "poses": reports,
        "panels": sorted(selected_panels),
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "scope": "Linux Python opaque-control only. Whole original triangles give generous support before resampling. Captures are not LDI textures and do not establish an eye radius. All intrinsic coarse arrays remain resident; captures add bytes if kept with them. Hierarchy frustum demand is checked against every original sampled first hit; CPU diagnostic ray readers use full stored coarse arrays, not a native draw implementation. No source regeneration during the frozen sequence; group draw demand changes. Source IDs/material codes/normal/coordinate/metre/RGBA fields compared. Two Lambert lights are diagnostics, not PBR/opacity/shadow/GPU/frame qualification. Build and query intervals exclude imports/output; complete allocation/copy traffic is unknown.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists() or not args.output.resolve().is_relative_to(Path("/tmp")):
        parser.error("A fresh explicit system-temp directory is required")
    run(args)


if __name__ == "__main__":
    main()
