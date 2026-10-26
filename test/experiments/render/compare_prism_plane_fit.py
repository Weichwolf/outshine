"""Compare an untuned native-vertex plane fit with the full saved original view."""

import argparse
import hashlib
import itertools
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from attributed_prisms import (
    compile_product,
    load_product,
    native_fields,
    native_inputs,
    render_product,
)
from compare_attributed_prisms import field_errors
from compare_plane_cloud import image_roi
from fit_prism_planes import fit_product, geometry_arrays, render_source_shading
from full_view_reference import load, views
from packed_normals import pack
from prepared_surface_recipes import load_recipes
from process_memory import peak_rss_bytes
from triangle_image_reference import image_rays, intersect_source, prepare_source


def metrics(source, faces, original_depth, original, candidate):
    depth, fields, identities, _part, _owner, _plane, _fallback, work = candidate
    a, b = np.isfinite(original_depth), np.isfinite(depth)
    common = a & b
    same = np.zeros(len(a), bool)
    same[common] = source["binding"][faces[common], 1] == identities[common]
    return {
        "original_hits": int(a.sum()),
        "candidate_hits": int(b.sum()),
        "lost": int((a & ~b).sum()),
        "added": int((~a & b).sum()),
        "fields": field_errors(original, fields, common, same),
        "work": work,
    }


def save_rays(path, source, face, original_depth, original, candidate, size):
    depth, fields, identity, part, owner, plane, fallback, _ = candidate
    original_source = np.full(len(face), -1, np.int32)
    good = face >= 0
    original_source[good] = source["binding"][face[good], 1]
    arrays = {
        "original_depth": original_depth,
        "original_face": face,
        "original_source": original_source,
        "candidate_depth": depth,
        "candidate_source": identity,
        "candidate_part": part,
        "candidate_primitive": owner,
        "candidate_plane": plane,
        "fallback_original_face": fallback,
        "width": size[0],
        "height": size[1],
    }
    arrays.update({"original_" + key: value for key, value in original.items()})
    arrays.update({"candidate_" + key: value for key, value in fields.items()})
    np.savez_compressed(path, **arrays)


def compare_views(source, document, context, baseline, fitted, normal_words, output):
    reference, inputs = prepare_source(source), native_inputs(source)
    bounds = np.concatenate((baseline[0].bounds, fitted[0].bounds))
    corners = np.array(
        [list(itertools.product(*zip(box[0], box[1], strict=True))) for box in bounds]
    ).reshape(-1, 3)
    extent = {"positions": np.concatenate((source["positions"], corners))}
    report = []
    for name, view in views(document, context).items():
        box = image_roi(extent, view)
        if box is None:
            report.append({"pose": name, "offscreen": True})
            continue
        size = (box[1] - box[0]).astype(int)
        origins, directions = image_rays(view, box, *size, 2)
        depth, face = intersect_source(source, origins, directions, prepared=reference)
        original = native_fields(inputs, origins, directions, depth, face)
        row = {"pose": name, "roi": box.tolist(), "samples": len(origins)}
        for label, product in [("baseline", baseline), ("fitted", fitted)]:
            started = time.perf_counter()
            result = (
                render_product(product, origins, directions)
                if label == "baseline"
                else render_source_shading(product, normal_words, origins, directions)
            )
            row[label] = metrics(source, face, depth, original, result)
            row[label]["query_seconds"] = time.perf_counter() - started
            if name in ("moving-023", "static-025"):
                save_rays(
                    output / f"{name}-{label}.npz",
                    source,
                    face,
                    depth,
                    original,
                    result,
                    size,
                )
        report.append(row)
        print(name, row["baseline"]["lost"], row["fitted"]["lost"], flush=True)
    return report


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    source, document, context, receipt = load(args.input)
    path = args.product
    if path is None:
        recipes, _ = load_recipes(args.input, document)
        arrays, _ = compile_product(document, source, recipes)
        path = args.output / "baseline.npz"
        np.savez_compressed(path, **arrays)
    baseline = load_product(path)
    started = time.perf_counter()
    fitted, report = fit_product(baseline, source)
    normal_words = pack(baseline[0].planes[:, :3])
    report["source_shading_normal_words_bytes"] = normal_words.nbytes
    report["fit_and_bounds_seconds"] = time.perf_counter() - started
    with np.load(path, allow_pickle=False) as stored:
        arrays = {name: stored[name] for name in stored.files}
    arrays = geometry_arrays(arrays, fitted, normal_words)
    fitted_path = args.output / "fitted.npz"
    np.savez_compressed(fitted_path, **arrays)
    reloaded = load_product(fitted_path)
    report.update(
        producer=subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        original_receipt=receipt,
        input_sha256=hashlib.sha256(args.input.read_bytes()).hexdigest(),
        baseline_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
        fitted_sha256=hashlib.sha256(fitted_path.read_bytes()).hexdigest(),
        stored_array_bytes=sum(value.nbytes for value in arrays.values()),
        poses=compare_views(
            source, document, context, baseline, reloaded, normal_words, args.output
        ),
        process_peak_rss_bytes=peak_rss_bytes(),
        allocations=None,
        total_physical_copied_bytes=None,
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", required=True, type=Path)
    parser.add_argument(
        "--product", type=Path, help="Existing dense attributed ready product"
    )
    parser.add_argument("--output", required=True, type=Path)
    run(parser.parse_args())
