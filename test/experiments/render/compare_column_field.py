"""Same native camera/image rays against a common field, plus full-parent costs."""

import argparse
import json
import resource
import tempfile
import time
from pathlib import Path

import numpy as np
from column_field_bake import cook, store_native_fallback
from column_field_query import load_product, trace
from compare_plane_cloud import image_roi
from full_view_reference import load, views
from parent_surface_inputs import open_parent
from PIL import Image
from triangle_image_reference import (
    average_samples,
    depth_difference,
    image_rays,
    intersect_source,
    panel,
    prepare_source,
)


def compare(source, prepared, product, view, output):
    box = image_roi(source, view)
    if box is None:
        return {"offscreen": True}
    width, height = (box[1] - box[0]).astype(int)
    origins, directions = image_rays(view, box, width, height, 2)
    depth, faces = intersect_source(source, origins, directions, prepared=prepared)
    start = time.perf_counter()
    candidate, normal, owners, work = trace(product, origins, directions)
    elapsed = time.perf_counter() - start
    a, b = np.isfinite(depth), np.isfinite(candidate)
    original_normal = np.zeros_like(normal)
    original_normal[a] = source["vertex_normal"][source["indices"][faces[a], 0]]
    sun = product["basis"][2] + 0.5 * product["basis"][0]
    sun /= np.linalg.norm(sun)
    pictures = []
    for n, covered in ((original_normal, a), (normal, b)):
        intensity = 0.2 + 0.8 * np.maximum(n @ sun, 0)
        rgb = np.where(covered[:, None], np.repeat(intensity[:, None], 3, axis=1), 0)
        pictures.append(
            np.dstack(
                (
                    average_samples(rgb, width, height, 2),
                    average_samples(covered.astype(float), width, height, 2),
                )
            )
        )
    error = np.zeros((len(a), 3))
    error[a & ~b] = [1, 0, 0]
    error[~a & b] = [0, 0.7, 1]
    error[a & b] = [0.25, 0.25, 0.25]
    pictures.append(
        np.dstack(
            (
                average_samples(error, width, height, 2),
                average_samples((a | b).astype(float), width, height, 2),
            )
        )
    )
    panel(
        output.with_suffix(".png"),
        [
            dict(
                zip(
                    ("native", "columns", "lost red, added cyan"), pictures, strict=True
                )
            )
        ],
        "Lambert geometry",
    )
    np.savez_compressed(
        output.with_suffix(".npz"),
        original_depth=depth,
        candidate_depth=candidate,
        original_face=faces,
        candidate_interval=owners,
        original_normal=original_normal,
        candidate_normal=normal,
        roi=box,
        width=width,
        height=height,
    )
    output.with_suffix(".json").write_text(
        json.dumps(
            {
                "view": [view[0].tolist(), view[1].tolist(), *view[2:]],
                "roi": box.tolist(),
                "width": int(width),
                "height": int(height),
            },
            indent=2,
        )
        + "\n"
    )
    return {
        "roi": box.tolist(),
        "subsamples": len(a),
        "original_hits": int(a.sum()),
        "field_hits": int(b.sum()),
        "lost_samples": int((a & ~b).sum()),
        "added_samples": int((~a & b).sum()),
        "depth": depth_difference(depth, candidate, np.ones(len(a), bool)),
        "query_s": elapsed,
        "work": work,
        "scope": "geometry/axial-normal diagnostic only; no material identity, UV, RGBA, PBR or full-scene acceptance",
    }


def parent_picture(product, view, output):
    columns, spans = product["columns"], product["spans"]
    pitch = product["header"]["pitch_m"]
    low = np.r_[columns.min(0) * pitch, spans[:, 0].min()]
    high = np.r_[(columns.max(0) + 1) * pitch, spans[:, 1].max()]
    corners = (
        np.array(
            [
                [x, y, z]
                for x in (low[0], high[0])
                for y in (low[1], high[1])
                for z in (low[2], high[2])
            ]
        )
        @ product["basis"]
    )
    box = image_roi({"positions": corners}, view)
    width, height = (box[1] - box[0]).astype(int)
    origins, directions = image_rays(view, box, width, height, 2)
    start = time.perf_counter()
    depth, normal, _, work = trace(product, origins, directions)
    elapsed = time.perf_counter() - start
    sun = product["basis"][2] + 0.5 * product["basis"][0]
    sun /= np.linalg.norm(sun)
    intensity = 0.2 + 0.8 * np.maximum(normal @ sun, 0)
    rgb = np.where(
        np.isfinite(depth)[:, None],
        np.repeat(intensity[:, None], 3, axis=1),
        [0.45, 0.65, 0.85],
    )
    image = np.broadcast_to([0.45, 0.65, 0.85], (view[4], view[3], 3)).copy()
    x, y = box[0].astype(int)
    image[y : y + height, x : x + width] = average_samples(rgb, width, height, 2)
    Image.fromarray(np.uint8(np.clip(image, 0, 1) * 255 + 0.5)).save(output)
    return {
        "roi": box.tolist(),
        "rays": len(depth),
        "hits": int(np.isfinite(depth).sum()),
        "query_s": elapsed,
        "work": work,
        "scope": "candidate-only parent geometry diagnostic with named unresolved local needs; no original full-parent render/PBR/world proof",
    }


def main():
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    temp = Path(tempfile.gettempdir()).resolve()
    repo = Path(__file__).resolve().parents[3]
    if not args.output.resolve().is_relative_to(
        temp
    ) or args.output.resolve().is_relative_to(repo):
        parser.error("private output belongs in system temp")
    args.output.mkdir(parents=True, exist_ok=False)
    source, document, context, receipt = load(args.input)
    positions, mapping = np.unique(
        source["positions"].view(np.uint32), axis=0, return_inverse=True
    )
    indices = mapping[source["indices"]].astype(
        np.uint16 if len(positions) <= 65536 else np.uint32
    )
    np.testing.assert_array_equal(
        positions[indices], source["positions"][source["indices"]].view(np.uint32)
    )
    control = {"positions": positions, "indices": indices}
    np.savez_compressed(args.output / "exact-geometry-control.npz", **control)
    poses = views(document, context)
    probes = {k: poses[k] for k in ("static-020", "moving-020", "moving-021")}
    # This is a declared first geometry probe, not a relaxed image-error contract.
    # Dyadic pitch bounds projected XY-cell diagonal at the nearest supplied view.
    nearest = min(
        float(((source["positions"] - eye) @ camera[2]).min())
        for eye, camera, *_ in probes.values()
    )
    focal = next(iter(probes.values()))[2]
    pitch = float(2 ** np.floor(np.log2(nearest / focal / np.sqrt(2))))
    archive = args.input.parent / "original-inputs.tar.gz"
    report = {
        "native_reference": receipt,
        "profile": context["profile"],
        "pitch_m": pitch,
        "pitch_policy": "largest dyadic XY-cell diagonal <= 1px at the nearest original probe depth; not a total shape/depth error bound",
        "nearest_probe_camera_depth_m": nearest,
        "material_scope": "not evaluated; geometry failure/cost screen precedes attributes and full120 poses",
        "exact_geometry_control": {
            "arrays": {k: a.nbytes for k, a in control.items()},
            "bytes": sum(a.nbytes for a in control.values()),
            "scope": "actual bit-exact Float32 unique XYZ plus smallest fitting integer corner indices; no normals, materials or lineage, not a native ABI/frame claim",
        },
    }
    prepared = prepare_source(source)
    with open_parent(archive) as parent:
        ordinals = [r["index"] for r in document["sources"]]
        cells = np.unique(parent.cells[ordinals])
        if len(cells) != 1:
            raise ValueError("this original view requires one delivered source cell")
        np.testing.assert_array_equal(
            np.flatnonzero(parent.cells == cells[0]), ordinals
        )
        start = time.perf_counter()
        report["cell"] = cook(parent, pitch, args.output / "cell", cells=cells)
        store_native_fallback(args.output / "cell", source, report["cell"])
        report["cell"]["bake_s"] = time.perf_counter() - start
        start = time.perf_counter()
        product = load_product(args.output / "cell")
        report["cell"]["cache_load_s"] = time.perf_counter() - start
        report["cell"]["derived_index_bytes"] = product["derived_index_bytes"]
        report["poses"] = {
            name: compare(source, prepared, product, view, args.output / name)
            for name, view in probes.items()
        }
        start = time.perf_counter()
        report["parent"] = cook(parent, pitch, args.output / "parent")
        report["parent"]["bake_s"] = time.perf_counter() - start
        product = load_product(args.output / "parent")
        report["parent"]["derived_index_bytes"] = product["derived_index_bytes"]
        report["parent_picture"] = parent_picture(
            product, probes["static-020"], args.output / "parent.png"
        )
        report["input_form_reads_including_cell_probe"] = parent.form_reads
        report["input_form_bytes_including_cell_probe"] = parent.form_bytes
    report["process_peak_rss_bytes"] = (
        resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "pitch_m": pitch,
                "cell_bytes": report["cell"]["stored_array_bytes"],
                "parent_bytes": report["parent"]["stored_array_bytes"],
                "poses": {
                    k: {j: r[j] for j in ("lost_samples", "added_samples", "depth")}
                    for k, r in report["poses"].items()
                },
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
