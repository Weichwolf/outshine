"""Whole original surface group versus object-space cards at common image rays."""

import argparse
import hashlib
import json
import resource
import time
import tracemalloc
from pathlib import Path

import numpy as np
from check_building_capture_inputs import check, load
from compare_actual_capture_movement import views
from full_view_reference import load as load_full_view
from full_view_reference import views as full_views
from native_capture_meshes import independent_depth, mesh, sample
from object_plane_cloud import mip_texels, partition
from plane_cloud_fields import bake, query
from triangle_image_reference import (
    average_samples,
    depth_difference,
    difference,
    image_rays,
    prepare_source,
    projected_bounds,
    relight,
)


def measured(function):
    tracemalloc.start()
    start, cpu = time.perf_counter(), time.process_time()
    result = function()
    cost = {
        "wall_s": time.perf_counter() - start,
        "cpu_s": time.process_time() - cpu,
        "traced_peak_bytes": tracemalloc.get_traced_memory()[1],
        "allocations": None,
    }
    tracemalloc.stop()
    return result, cost


def stored(baked, fallback, reasons, output):
    charts = [x["chart"] for x in baked]
    arrays = {
        "fallback_original_faces": fallback,
        "original_face_reasons": reasons,
        "face_offsets": np.cumsum(
            [0] + [len(c.faces) for c in charts], dtype=np.uint32
        ),
        "faces": np.concatenate([c.faces for c in charts]),
        "basis": np.array([c.basis for c in charts]),
        "offset": np.array([c.offset for c in charts]),
        "low": np.array([c.low for c in charts]),
        "shape": np.array([c.shape for c in charts], np.uint32),
        "pitch": np.array([c.pitch for c in charts]),
        "texture_offsets": np.cumsum([0] + [c.texels for c in charts], dtype=np.uint32),
        "rgba": np.concatenate([x["rgba"].reshape(-1, 4) for x in baked]),
        "moment": np.concatenate([x["moment"].reshape(-1, 4) for x in baked]),
    }
    np.savez_compressed(output / "charts.npz", **arrays)
    return {
        "stored_array_payload_bytes": sum(a.nbytes for a in arrays.values()),
        "archive_bytes": (output / "charts.npz").stat().st_size,
        "scope": "actual Float64 implicit plane rectangles, base fields and original face indices; no fabricated Float32 quad/GPU layout or complete fallback asset",
    }


def image_roi(source, view):
    eye, basis, focal, width, height = view
    points = (source["positions"].astype(float) - eye) @ basis.T
    x, y, z = points.T
    planes = np.stack(
        (
            z - 0.1,
            z * width / (2 * focal) + x,
            z * width / (2 * focal) - x,
            z * height / (2 * focal) + y,
            z * height / (2 * focal) - y,
        )
    )
    if (planes.max(1) < 0).any():
        return None
    box = projected_bounds(source["positions"], view)
    box = np.stack((np.floor(box[0]), np.ceil(box[1])))
    box = np.maximum(box, [0, 0])
    box = np.minimum(box, [width, height])
    return box if (box[1] > box[0]).all() else None


def select_full_poses(source, poses):
    candidates = []
    for name, view in poses.items():
        if name.startswith("static-"):
            box = image_roi(source, view)
            if box is not None:
                candidates.append(
                    (np.linalg.norm(box.mean(0) - np.array(view[3:]) / 2), name)
                )
    if not candidates:
        raise ValueError(
            "whole native rotation has no visible input; no empty-image success"
        )
    _, chosen = min(candidates)
    frame = int(chosen.rsplit("-", 1)[1])
    frames = sum(name.startswith("moving-") for name in poses)
    names = (chosen, f"moving-{frame:03d}", f"moving-{frame % frames + 1:03d}")
    return {name: poses[name] for name in names}


def render(source, original_index, baked, fallback, fallback_index, view, suns):
    box = image_roi(source, view)
    if box is None:
        raise ValueError("selected test pose has no on-screen source extent")
    width, height = (box[1] - box[0]).astype(int)
    if width * height * 4 > 4194304:
        raise ValueError("complete same-resolution ROI exceeds diagnostic budget")
    origins, directions = image_rays(view, box, width, height, 2)
    original_depth, _, fields = sample(source, original_index, origins, directions)
    source_alpha = np.isfinite(original_depth).astype(float)
    ref = [
        np.dstack(
            (
                average_samples(relight(fields, sun, original_depth), width, height, 2),
                average_samples(source_alpha, width, height, 2),
            )
        )
        for sun in suns
    ]
    depths, alpha, colours = [], [], []
    work = {"ray_chart_plane_tests": 0, "rectangle_hits": 0, "nonzero_alpha_hits": 0}
    for first in range(0, len(origins), 512):
        d, a, rgb, w = query(
            baked,
            fallback,
            fallback_index,
            origins[first : first + 512],
            directions[first : first + 512],
            suns,
        )
        depths.append(d)
        alpha.append(a)
        colours.append(rgb)
        for key in work:
            work[key] += w[key]
    cloud_depth, cloud_alpha = np.concatenate(depths), np.concatenate(alpha)
    cloud_rgb = np.concatenate(colours, axis=1)
    cloud = [
        np.dstack(
            (
                average_samples(rgb, width, height, 2),
                average_samples(cloud_alpha, width, height, 2),
            )
        )
        for rgb in cloud_rgb
    ]
    selected = np.linspace(0, len(origins) - 1, min(64, len(origins))).astype(int)
    brute = independent_depth(
        source["positions"][source["indices"]], origins[selected], directions[selected]
    )
    np.testing.assert_array_equal(
        np.isfinite(brute), np.isfinite(original_depth[selected])
    )
    finite = np.isfinite(brute)
    np.testing.assert_allclose(
        brute[finite], original_depth[selected][finite], atol=1e-8, rtol=1e-10
    )
    common = np.isfinite(original_depth) & np.isfinite(cloud_depth)
    report = {
        "roi_pixels": [int(width), int(height)],
        "screen_roi": box.tolist(),
        "evaluated_subsamples": len(origins),
        "reference_covered_subsamples": int(source_alpha.sum()),
        "work": work,
        "independent_original_rays": len(selected),
        "errors_per_light": [difference(a, b) for a, b in zip(ref, cloud, strict=True)],
        "depth": depth_difference(original_depth, cloud_depth, common),
    }
    arrays = {
        "original_depth": original_depth,
        "cloud_depth": cloud_depth,
        "original_alpha": source_alpha,
        "cloud_alpha": cloud_alpha,
        "original_images": np.array(ref),
        "cloud_images": np.array(cloud),
    }
    return report, arrays


def picture(rows, output):
    from PIL import Image, ImageDraw

    canvas = Image.new("RGB", (1280, 720), "white")
    draw = ImageDraw.Draw(canvas)
    for row, (name, arrays) in enumerate(rows):
        images = [
            arrays["original_images"][0],
            arrays["cloud_images"][0],
            arrays["original_images"][1],
            arrays["cloud_images"][1],
        ]
        for column, (label, data) in enumerate(
            zip(
                (
                    "Original / light A",
                    "Planes / light A",
                    "Original / light B",
                    "Planes / light B",
                ),
                images,
                strict=True,
            )
        ):
            image = Image.fromarray(
                np.rint(np.clip(data[:, :, :3], 0, 1) * 255).astype(np.uint8)
            )
            scale = min(310 / image.width, 180 / image.height)
            image = image.resize(
                (
                    max(1, round(image.width * scale)),
                    max(1, round(image.height * scale)),
                ),
                Image.Resampling.NEAREST,
            )
            x, y = 320 * column, 240 * row
            draw.text((x + 5, y + 3), name + " | " + label, fill="black")
            draw.text(
                (x + 5, y + 205),
                f"Original crop {data.shape[1]}x{data.shape[0]} px; nearest display",
                fill="black",
            )
            canvas.paste(image, (x + (320 - image.width) // 2, y + 28))
    canvas.save(output / "comparison.png")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        type=Path,
        default=Path(
            "test/outshine/data/render/building-capture-movement/native-cell.json.gz"
        ),
    )
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--epsilon", type=float, default=1.0)
    parser.add_argument("--pitch", type=float, default=1.0)
    args = parser.parse_args()
    document = load(args.input)
    if "view_key" in document:
        source, document, context, input_receipt = load_full_view(args.input)
        available_poses = full_views(document, context)
        poses = select_full_poses(source, available_poses)
        pose_scope = "3 selected actual native benchmark poses from 120 supplied states; no full-sequence or continuous admission"
    else:
        check(document)
        calls = [c for r in document["results"] for c in r["projection_calls"]]
        if len(calls) != 1:
            raise ValueError("one explicitly recorded complete input group required")
        source = mesh(calls[0], "Full")
        poses = views(document, calls[0], source["positions"])
        input_receipt = {
            "scope": "existing full known native surface group; opaque control"
        }
        pose_scope = "3 original declared cell-centred diagnostic poses"
    (charts, fallback, reasons, work), partition_cost = measured(
        lambda: partition(
            source["positions"],
            source["indices"],
            epsilon=args.epsilon,
            pitch=args.pitch,
        )
    )
    if not charts:
        raise ValueError("no candidate charts; original geometry is retained")
    baked, bake_cost = measured(lambda: [bake(source, c) for c in charts])
    original_index = prepare_source(source)
    fallback_source = {
        **source,
        "indices": source["indices"][fallback],
        "binding": source["binding"][fallback],
    }
    fallback_index = prepare_source(fallback_source) if len(fallback) else None
    args.output.mkdir(parents=True, exist_ok=False)
    storage = stored(baked, fallback, reasons, args.output)
    up = np.array(document["anchor_ecef"], float)
    up /= np.linalg.norm(up)
    east = np.cross([0.0, 0.0, 1.0], up)
    east /= np.linalg.norm(east)
    north = np.cross(up, east)
    suns = np.stack((up + 0.8 * east + 0.2 * north, up - 0.7 * east + 0.4 * north))
    rows, reports = [], {}
    for name, view in poses.items():
        (report, arrays), cost = measured(
            lambda view=view: render(
                source,
                original_index,
                baked,
                fallback_source,
                fallback_index,
                view,
                suns,
            )
        )
        report.update(
            cost=cost,
            eye=view[0].tolist(),
            basis=view[1].tolist(),
            focal_px=view[2],
            presentation_width_height=list(view[3:]),
        )
        reports[name] = report
        rows.append((name, arrays))
        np.savez_compressed(args.output / (name + ".npz"), **arrays)
        print(name, json.dumps(report), flush=True)
    picture(rows, args.output)
    result = {
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "input_receipt": input_receipt,
        "pose_scope": pose_scope,
        "source_paths_sha256": {
            name: hashlib.sha256(
                (Path(__file__).parent / name).read_bytes()
            ).hexdigest()
            for name in (
                "object_plane_cloud.py",
                "plane_cloud_fields.py",
                "compare_plane_cloud.py",
                "full_view_reference.py",
            )
        },
        "epsilon_m": args.epsilon,
        "synthetic_above_horizon_light_directions": suns.tolist(),
        "pitch_m": args.pitch,
        "partition_work": work,
        "partition_cost": partition_cost,
        "bake_cost": bake_cost,
        "bake_work": [x["work"] for x in baked],
        "storage": storage,
        "declared_full_mip_texture_bytes": sum(
            12 * mip_texels(c.shape) for c in charts
        ),
        "original_array_bytes": {key: value.nbytes for key, value in source.items()},
        "peak_rss_bytes_linux": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "views": reports,
        "scope": "complete known original surface group, 3 explicit poses, same native focal/1280x720/2x2 samples; colour-factor Lambert and opaque control only; native PBR/shadow/whole-city/full-sequence/continuous/device admission open; all originals retained by diagnostic",
    }
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")


if __name__ == "__main__":
    main()
