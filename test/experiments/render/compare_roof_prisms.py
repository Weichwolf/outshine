"""Complete native city cell versus grouped procedural solids, geometry only."""

import argparse
import hashlib
import itertools
import json
import resource
import time
import tracemalloc
from pathlib import Path

import numpy as np
from compare_plane_cloud import image_roi
from full_view_reference import load, views
from native_capture_meshes import independent_depth
from primitive_bvh import Hierarchy, build, trace
from roof_prism_control import explicit_mesh
from roof_prisms import Prisms, compile_prisms
from triangle_image_reference import (
    average_samples,
    depth_difference,
    difference,
    image_rays,
    intersect_source,
    prepare_source,
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
        "physical_copied_bytes": None,
    }
    tracemalloc.stop()
    return result, cost


def subset(source, faces):
    indices = source["indices"][faces]
    unique, inverse = np.unique(indices, return_inverse=True)
    return {
        "positions": source["positions"][unique],
        "indices": inverse.reshape(-1, 3).astype(np.uint32),
    }


def flat_fields(source, faces):
    normal = np.zeros((len(faces), 3))
    valid = faces >= 0
    xyz = source["positions"][source["indices"][faces[valid]]].astype(float)
    n = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
    normal[valid] = n / np.linalg.norm(n, axis=1)[:, None]
    return {"normal": normal, "colour": np.ones_like(normal)}


def render(
    source, reference, fallback, fallback_index, prisms, hierarchy, view, suns, control
):
    # Include conservative primitive corners so approximation growth cannot hide outside the ROI.
    corners = np.array(
        [
            np.array(list(itertools.product(*zip(box[0], box[1], strict=True))))
            for box in prisms.bounds
        ]
    ).reshape(-1, 3)
    extent = {"positions": np.concatenate((source["positions"], corners))}
    box = image_roi(extent, view)
    if box is None:
        return {"offscreen": True}, None
    width, height = (box[1] - box[0]).astype(int)
    if width * height * 4 > 4194304:
        raise ValueError("full same-resolution ROI exceeds diagnostic ray budget")
    origins, directions = image_rays(view, box, width, height, 2)
    ref_depth, ref_face = intersect_source(
        source, origins, directions, prepared=reference
    )
    query_start = time.perf_counter()
    (depth, owner, face, work), cost = measured(
        lambda: trace(prisms, hierarchy, origins, directions)
    )
    fields = {"normal": np.zeros((len(depth), 3)), "colour": np.ones((len(depth), 3))}
    hit = owner >= 0
    fields["normal"][hit] = prisms.planes[face[hit], :3]
    if len(fallback["indices"]):
        t, f = intersect_source(fallback, origins, directions, prepared=fallback_index)
        use = t < depth
        fields["normal"][use] = flat_fields(fallback, f)["normal"][use]
        depth[use] = t[use]
    cost["candidate_with_fallback_wall_s"] = time.perf_counter() - query_start
    work["logical_box_plane_operand_bytes"] = (
        48 * work["node_ray_tests"] + 32 * work["plane_ray_tests"]
    )
    ref_fields = flat_fields(source, ref_face)
    original, candidate = [], []
    for sun in suns:
        for images, d, attrs in (
            (original, ref_depth, ref_fields),
            (candidate, depth, fields),
        ):
            images.append(
                np.dstack(
                    (
                        average_samples(relight(attrs, sun, d), width, height, 2),
                        average_samples(np.isfinite(d).astype(float), width, height, 2),
                    )
                )
            )
    covered = np.flatnonzero(np.isfinite(ref_depth))
    selected = np.unique(
        np.r_[
            np.linspace(0, len(origins) - 1, min(16, len(origins))).astype(int),
            covered[
                np.linspace(0, len(covered) - 1, min(16, len(covered))).astype(int)
            ],
        ]
    )
    brute = independent_depth(
        source["positions"][source["indices"]], origins[selected], directions[selected]
    )
    np.testing.assert_array_equal(np.isfinite(brute), np.isfinite(ref_depth[selected]))
    finite = np.isfinite(brute)
    np.testing.assert_allclose(
        brute[finite], ref_depth[selected][finite], atol=1e-8, rtol=1e-10
    )
    # Independent all-triangle control for the *represented analytic solids*, not native parity.
    solid_brute = independent_depth(control, origins[selected], directions[selected])
    solid_depth, _, _, _ = trace(
        prisms, hierarchy, origins[selected], directions[selected]
    )
    np.testing.assert_array_equal(np.isfinite(solid_brute), np.isfinite(solid_depth))
    finite = np.isfinite(solid_brute)
    np.testing.assert_allclose(
        solid_depth[finite], solid_brute[finite], atol=1e-6, rtol=1e-9
    )
    report = {
        "screen_roi": box.tolist(),
        "roi_pixels": [int(width), int(height)],
        "subsamples": len(origins),
        "covered_original": int(np.isfinite(ref_depth).sum()),
        "covered_candidate": int(np.isfinite(depth).sum()),
        "independent_original_rays": len(selected),
        "independent_solid_rays": len(selected),
        "cost": cost,
        "work": work,
        "flat_primitive_ray_tests": len(origins) * len(prisms.binding),
        "errors_per_light": [
            difference(a, b) for a, b in zip(original, candidate, strict=True)
        ],
        "depth": depth_difference(ref_depth, depth, np.ones(len(depth), bool)),
    }
    arrays = {
        "original_depth": ref_depth,
        "prism_depth": depth,
        "original_images": np.array(original),
        "prism_images": np.array(candidate),
    }
    return report, arrays


def picture(rows, path):
    from PIL import Image, ImageDraw

    canvas = Image.new("RGB", (1280, 720), "white")
    draw = ImageDraw.Draw(canvas)
    for row, (name, data) in enumerate(rows):
        for column, (label, array) in enumerate(
            (
                ("Original A", data["original_images"][0]),
                ("Prisms A", data["prism_images"][0]),
                ("Original B", data["original_images"][1]),
                ("Prisms B", data["prism_images"][1]),
            )
        ):
            image = Image.fromarray(
                np.rint(np.clip(array[:, :, :3], 0, 1) * 255).astype(np.uint8)
            )
            scale = min(310 / image.width, 175 / image.height)
            image = image.resize(
                (
                    max(1, round(scale * image.width)),
                    max(1, round(scale * image.height)),
                ),
                Image.Resampling.NEAREST,
            )
            x, y = column * 320, row * 240
            draw.text((x + 4, y + 4), name + " / " + label, fill="black")
            canvas.paste(image, (x + 4, y + 28))
            draw.text(
                (x + 4, y + 207),
                f"{array.shape[1]}x{array.shape[0]} px; geometry only",
                fill="black",
            )
    canvas.save(path)


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=False)
    source, document, context, receipt = load(args.input)
    (prisms, reasons), assembly = measured(lambda: compile_prisms(document, source))
    hierarchy, build_cost = measured(lambda: build(prisms.bounds))
    fallback = subset(source, prisms.fallback_faces)
    represented = set(prisms.binding[:, 0].tolist())
    if represented & set(reasons) or represented | set(reasons) != {
        s["index"] for s in document["sources"]
    }:
        raise ValueError("original source partition incomplete")
    arrays = vars(prisms) | {
        "node_bounds": hierarchy.bounds,
        "node_children": hierarchy.children,
        "fallback_positions": fallback["positions"],
        "fallback_indices": fallback["indices"],
    }
    np.savez_compressed(args.output / "representation.npz", **arrays)
    payload = sum(a.nbytes for a in arrays.values())
    # Query the stored representation, not a separate in-memory geometry recipe.
    with np.load(args.output / "representation.npz") as stored:
        prisms = Prisms(**{k: stored[k] for k in vars(prisms)})
        hierarchy = Hierarchy(stored["node_bounds"], stored["node_children"])
        fallback = {
            "positions": stored["fallback_positions"],
            "indices": stored["fallback_indices"],
        }
    control = np.concatenate(
        [
            explicit_mesh(prisms.planes[a:b], box.mean(0))
            for a, b, box in zip(
                prisms.offsets[:-1], prisms.offsets[1:], prisms.bounds, strict=True
            )
        ]
    )
    reference = prepare_source(source)
    fallback_index = prepare_source(fallback) if len(fallback["indices"]) else None
    suns = [
        np.array(context["scene_frame"]["y_ecef"])
        + 0.7 * np.array(context["scene_frame"]["x_ecef"]),
        np.array(context["scene_frame"]["y_ecef"])
        - 0.8 * np.array(context["scene_frame"]["z_ecef"]),
    ]
    results, images = {}, []
    for name, view in views(document, context).items():
        result, raw = render(
            source,
            reference,
            fallback,
            fallback_index,
            prisms,
            hierarchy,
            view,
            suns,
            control,
        )
        results[name] = result
        if raw is not None:
            np.savez_compressed(args.output / (name + ".npz"), **raw)
            if name in ("static-020", "moving-020", "moving-021"):
                images.append((name, raw))
    if not images:
        raise ValueError("no visible control images")
    picture(images, args.output / "comparison.png")
    report = {
        "input": str(args.input),
        "input_sha256": hashlib.sha256(args.input.read_bytes()).hexdigest(),
        "source_sha256": {
            name: hashlib.sha256(
                (Path(__file__).parent / name).read_bytes()
            ).hexdigest()
            for name in (
                "roof_prisms.py",
                "primitive_bvh.py",
                "roof_prism_control.py",
                "compare_roof_prisms.py",
                "full_view_reference.py",
                "triangle_image_reference.py",
                "native_capture_meshes.py",
                "compare_plane_cloud.py",
            )
        },
        "input_receipt": receipt,
        "scope": "whole delivered cell; all 120 benchmark poses; opaque geometric front faces and flat Lambert only; no UV/PBR/shadow/native admission",
        "sources": len(document["sources"]),
        "represented_sources": len(represented),
        "primitives": len(prisms.binding),
        "planes": len(prisms.planes),
        "fallback_source_reasons": reasons,
        "fallback_triangles": len(prisms.fallback_faces),
        "actual_array_bytes": {k: a.nbytes for k, a in arrays.items()},
        "actual_payload_bytes": payload,
        "archive_bytes": (args.output / "representation.npz").stat().st_size,
        "original_geometry_bytes": source["positions"].nbytes
        + source["indices"].size * 4,
        "original_vertices": len(source["positions"]),
        "original_triangles": len(source["indices"]),
        "assembly": assembly,
        "bvh_build": build_cost,
        "peak_process_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "explicit_solid_control_triangles": len(control),
        "poses": results,
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                k: v
                for k, v in report.items()
                if k
                not in (
                    "poses",
                    "input_receipt",
                    "fallback_source_reasons",
                    "actual_array_bytes",
                )
            }
        )
    )


if __name__ == "__main__":
    main()
