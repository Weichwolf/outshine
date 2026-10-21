"""Full unchanged capture cohorts at 1280x720 with 2x2 sample/reference checks."""

import argparse
import hashlib
import json
import platform
import resource
import sys
import tempfile
import time
from pathlib import Path

import numpy as np
from capture_raster_reference import (
    compare_saved_rays,
    controls,
    errors,
    frame,
    input_keys,
    keys_at,
)
from check_building_capture_inputs import check, load
from compare_actual_capture_movement import views
from native_capture_meshes import REPRESENTATIONS, mesh
from triangle_image_reference import prepare_source


def picture(rows, output):
    from PIL import Image, ImageDraw

    canvas = Image.new("RGB", (3 * 640, 3 * 390), "white")
    draw = ImageDraw.Draw(canvas)
    for row, (name, images, records) in enumerate(rows):
        for column, representation in enumerate(REPRESENTATIONS):
            canvas.paste(
                Image.fromarray(images[representation]).resize((640, 360)),
                (column * 640, row * 390 + 30),
            )
            record = records["Full_to_Captured"]
            draw.text(
                (column * 640 + 4, row * 390 + 5),
                f"{name}: {representation}; Full/Captured lost {record['lost_subsamples']}, input changes {record['different_owned_vertex_input_subsamples']}",
                fill="black",
            )
    canvas.save(output)


def image_pixels(depth, reference):
    valid = reference > 0
    low, high = np.percentile(reference[valid], [1, 99])
    grey = np.clip((depth - low) / max(high - low, np.finfo(float).eps), 0, 1)
    covered = depth > 0
    grey = np.where(covered, 60 + 170 * grey, 0).reshape(720, 2, 1280, 2).mean((1, 3))
    alpha = covered.reshape(720, 2, 1280, 2).mean((1, 3))
    # Composite after four-sample averaging, without a full Float64 RGB scratch.
    return (grey[..., None] + (1 - alpha[..., None]) * [16, 18, 22]).astype(np.uint8)


def run(args):
    began = time.perf_counter()
    document = load(args.input)
    check(document)
    calls = [c for r in document["results"] for c in r["projection_calls"]]
    if len(calls) != 1:
        raise ValueError("one explicit original projection required")
    call = calls[0]
    meshes = {name: mesh(call, name) for name in REPRESENTATIONS}
    classes, key_cost = input_keys(meshes)
    prepared = {name: prepare_source(source) for name, source in meshes.items()}
    args.output.mkdir(parents=True, exist_ok=False)
    reports, rows = {}, []
    for name, view in views(document, call, meshes["Fine"]["positions"]).items():
        frames, times, work = {}, {}, {}
        for representation, source in meshes.items():
            start = time.perf_counter()
            work[representation] = {}
            frames[representation] = frame(source, view, counters=work[representation])
            times[representation] = time.perf_counter() - start
        records = {
            f"{a}_to_{b}": errors(frames[a], frames[b], classes[a], classes[b])
            for a, b in (("Fine", "Full"), ("Full", "Captured"), ("Fine", "Captured"))
        }
        unequal = keys_at(frames["Full"][1], classes["Full"]) != keys_at(
            frames["Captured"][1], classes["Captured"]
        )
        unequal |= keys_at(frames["Fine"][1], classes["Fine"]) != keys_at(
            frames["Full"][1], classes["Full"]
        )
        changed = np.flatnonzero(unequal)
        mandatory = np.asarray(
            [
                r["worst_depth_difference_flat_subsample"]
                for r in records.values()
                if r["worst_depth_difference_flat_subsample"] is not None
            ],
            np.int64,
        )
        ray_checks = {
            representation: controls(
                source,
                prepared[representation],
                frames[representation],
                classes[representation],
                view,
                changed,
                mandatory,
            )
            for representation, source in meshes.items()
        }
        report = {
            "width_height": [1280, 720],
            "samples_per_pixel": 4,
            "evaluated_screen_subsamples_per_path": 2560 * 1440,
            "pose": {
                "eye_offset_ecef": view[0].tolist(),
                "basis": view[1].tolist(),
                "focal_px": view[2],
            },
            "errors": records,
            "raster_seconds": times,
            "raster_work": work,
            "ray_controls": ray_checks,
        }
        if args.ray_reference:
            report["all_previous_saved_ray_controls"] = compare_saved_rays(
                args.ray_reference, name, frames, view
            )
        arrays = {}
        for representation, (depth, owner) in frames.items():
            arrays[representation + "_inverse_depth"] = depth
            arrays[representation + "_triangle"] = owner
        np.savez_compressed(args.output / f"{name}.npz", **arrays)
        rows.append(
            (
                name,
                {
                    representation: image_pixels(image[0], frames["Fine"][0])
                    for representation, image in frames.items()
                },
                records,
            )
        )
        reports[name] = report
        del arrays
    np.savez_compressed(
        args.output / "triangle-bindings.npz",
        **{name + "_binding": source["binding"] for name, source in meshes.items()},
        **{name + "_input_key": classes[name] for name in REPRESENTATIONS},
    )
    picture(rows, args.output / "complete-capture-geometry.png")
    report = {
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "native_provenance": document["provenance"],
        "capture_policy": {
            k: call["capture"][k]
            for k in (
                "opaque_for_coverage",
                "runtime_culling",
                "focal_px",
                "error_px",
                "cube_face_pixels",
            )
        },
        "members": call["members"],
        "scope": "Complete cohort, 1280x720/focal unchanged, original follow-up poses, all geometry, 2x2 sample centres. Float64 depth and owned vertex-input packets only, no native PBR image or device admission. Equal raster input classes do not qualify actual per-Piece material bindings; coplanar ray/raster ownership ambiguities are counted. Positive poses do not prove a continuous movement radius.",
        "costs": {
            "triangles": {name: len(m["indices"]) for name, m in meshes.items()},
            "input_array_bytes": {
                name: sum(a.nbytes for a in m.values()) for name, m in meshes.items()
            },
            "resident_frame_array_bytes": sum(
                a.nbytes for pair in frames.values() for a in pair
            ),
            "key_registry": key_cost,
            "platform": platform.platform(),
            "total_seconds": time.perf_counter() - began,
            "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
            * (1024 if sys.platform.startswith("linux") else 1)
            if sys.platform.startswith("linux") or sys.platform == "darwin"
            else None,
            "scope": "Process peak includes JSON, Trimesh/R-tree, raster scratch and output compression. Exact allocation/copy counts and GPU/runtime costs remain unmeasured; explicit byte counts are distinct array extents.",
        },
        "views": reports,
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--ray-reference", type=Path)
    args = parser.parse_args()
    if not args.output.resolve().is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("fresh diagnostic output must stay in system temp")
    result = run(args)
    print(
        json.dumps(
            {
                "output": str(args.output),
                "costs": result["costs"],
                "Full_to_Captured": {
                    name: row["errors"]["Full_to_Captured"]
                    for name, row in result["views"].items()
                },
            }
        )
    )
