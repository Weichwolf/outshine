"""Bounded primary-camera Shell-wall selection; native world gates stay separate."""

import argparse
import hashlib
import json
import tempfile
import time
from collections import Counter
from pathlib import Path

import numpy as np
from building_roof_selection import emit_roof, render, render_models, selections
from building_shell_walls import shell_wall_groups
from cached_building_streams import product
from PIL import Image
from roof_fusion import fused_roof_umbra
from verify_native_roof_positions import oriented_triangles


def successful_shell_calls(trace):
    return {
        call["source"]: call
        for row in trace["results"][1:]
        if row["accepted"]
        for call in row["mesh_calls"]
        if call["accepted"] and call["detail"] == 1 and not call["openings"]
    }


def seed_indices(calls, bounds, known, eye):
    """Choose the same bounded cohort solely from original metadata."""
    return sorted(
        (index for index in calls if known[index]),
        key=lambda index: (np.linalg.norm(bounds[index].mean(axis=0) - eye), index),
    )[:32]


def seed_walls(sources, calls, order, bounds, observer, *, certified_opaque=False):
    geometry, packet, counts = {}, [], Counter()
    for index in order:
        groups, work = shell_wall_groups(sources[index], calls[index], retained=True)
        counts.update(work)
        geometry[index] = groups
        for group in groups:
            triangles = group["triangles"]
            counts["seed_triangles"] += len(triangles)
            if (triangles < bounds[index, 0]).any() or (
                triangles > bounds[index, 1]
            ).any():
                counts["outside_bound_groups"] += 1
                continue
            if len(packet) >= 256:
                counts["certificate_limit_groups"] += 1
                continue
            try:
                certificate = fused_roof_umbra(
                    triangles, observer, certified_opaque=certified_opaque
                )
            except ValueError as error:
                counts[f"uncertifiable: {error}"] += 1
                continue
            if len(certificate.low):
                packet.append((certificate, index))
    counts["certificates"] = len(packet)
    return geometry, packet, counts


def verify_seeds(parent, geometry, sources, products):
    """Read cached native controls only after all selection decisions."""
    manifest = json.loads((products / "manifest.json").read_text())
    cohort = next(row for row in manifest["cohorts"] if row["parent"] == parent)
    cells, receipts = {}, []
    for index, groups in geometry.items():
        if not groups:
            continue
        cell = sources[index]["cell"]
        if cell not in cells:
            entry = next(
                (row for row in cohort["products"] if row["cell"] == cell), None
            )
            if entry is None:
                raise ValueError(
                    f"missing original native cell control: {parent}/{cell}"
                )
            path = products / f"{entry['key']}.structure"
            raw = path.read_bytes()
            if hashlib.sha256(raw).hexdigest() != entry["sha256"]:
                raise ValueError("native control artifact SHA mismatch")
            parsed = product(raw, entry["key"])
            vertices = np.frombuffer(parsed["streams"][0], dtype="<f4").reshape(-1, 6)[
                :, :3
            ]
            triangles = np.frombuffer(parsed["streams"][2], dtype="<u4").reshape(-1, 3)
            cells[cell] = oriented_triangles(vertices[triangles]), entry["sha256"]
        actual, digest = cells[cell]
        expected = oriented_triangles(np.concatenate([g["triangles"] for g in groups]))
        missing = expected - actual
        if missing:
            raise ValueError(f"native accepted wall subset differs: {parent}/{index}")
        receipts.append(
            {
                "source": index,
                "cell": cell,
                "oriented_triangles": sum(expected.values()),
                "native_artifact_sha256": digest,
            }
        )
    return receipts


def reference_images(native, calls, seed_geometry, masks, output):
    """Post-selection roof/base-wall reference, explicitly incomplete for Fine."""
    full, omissions = [], Counter()
    for index, source in enumerate(native["sources"]):
        roof, reasons = emit_roof(source, vertex_recipe="snapped-f32")
        omissions.update(reasons)
        groups = seed_geometry.get(index)
        if groups is None:
            if index in calls:
                groups, reasons = shell_wall_groups(source, calls[index], retained=True)
                omissions.update(reasons)
            else:
                groups = []
                omissions["source_without_selected_shell_wall_control"] += 1
        model = np.concatenate([roof, *[group["triangles"] for group in groups]])
        if any(mask[index] for mask in masks.values()) and len(model):
            bound = source["bounds"]
            if (model < bound["min"]).any() or (model > bound["max"]).any():
                raise ValueError(
                    "hidden actual reference geometry escapes source bounds"
                )
        full.append(model)
    models = {
        name: np.concatenate(
            [model for index, model in enumerate(full) if not mask[index]]
        )
        for name, mask in masks.items()
        if name != "cells" or not np.array_equal(mask, masks["whole"])
    }
    output.mkdir()
    poses = render_models(native, native["sources"], models, output)
    removed = (
        np.concatenate([model for i, model in enumerate(full) if masks["joint"][i]])
        if masks["joint"].any()
        else np.empty((0, 3, 3))
    )
    best_pixels, best_depth = 0, None
    for pose in poses:
        view = (
            np.asarray(pose["eye"]),
            np.asarray(pose["basis"]),
            native["view"]["focal_px"],
            1280,
            720,
        )
        depth = render(removed, view)
        pixels = int((depth > 0).sum())
        pose["unoccluded_removed_reference_pixels"] = pixels
        if pixels > best_pixels:
            best_pixels, best_depth = pixels, depth
    if best_depth is not None:
        Image.fromarray(np.where(best_depth > 0, 230, 15).astype(np.uint8)).save(
            output / "removed-reference-only.png"
        )
    return {
        "primitive_counts": {name: len(model) for name, model in models.items()},
        "image_aliases": {"cells": "whole"} if "cells" not in models else {},
        "omissions": dict(omissions),
        "poses": poses,
        "removed_reference_triangles": len(removed),
    }


def compare(
    surface_path, trace_path, products, output, *, certified_opaque=False, images=False
):
    native, trace = (
        json.loads(path.read_bytes()) for path in (surface_path, trace_path)
    )
    if any(native[key] != trace[key] for key in ("parent", "view", "anchor_ecef")):
        raise ValueError("surface/selection parent, observer or frame mismatch")
    sources = native["sources"]
    for actual, control in zip(sources, trace["sources"], strict=True):
        for key in ("index", "id", "kind", "cell", "bounds", "shell_error_m"):
            if actual[key] != control[key]:
                raise ValueError("surface/selection original metadata mismatch")
    bounds = np.array(
        [
            [source["bounds"]["min"], source["bounds"]["max"]]
            if source["bounds"]
            else [[np.nan] * 3] * 2
            for source in sources
        ]
    )
    known = np.isfinite(bounds).all(axis=(1, 2))
    eye = np.asarray(native["view"]["eye_ecef"]) - native["anchor_ecef"]
    observer = np.array([eye - 2, eye + 2])
    calls = successful_shell_calls(trace)
    order = seed_indices(calls, bounds, known, eye)
    protected = np.zeros(len(sources), dtype=bool)
    protected[order] = True
    start = time.perf_counter()
    geometry, packet, counts = seed_walls(
        sources, calls, order, bounds, observer, certified_opaque=certified_opaque
    )
    compile_s = time.perf_counter() - start
    masks, work = selections(sources, bounds, known, packet, protected)
    report = {
        "scope": "Experimental primary-camera selection with explicitly declared opaque retained Shell wall parts in parent-offset represented positions. Native cached triangle controls are read only after all masks are fixed. No Fine/full-world completeness, actual consumer/GPU transform, shadows/reflections/interactions, invalidation or device budget acceptance.",
        "parent": native["parent"],
        "sources": len(sources),
        "unknown_bounds": int((~known).sum()),
        "observer_box": observer.tolist(),
        "certified_opaque_assumption": certified_opaque,
        "seed_source_indices": order,
        "seed_wall_group_counts": {
            index: len(groups) for index, groups in geometry.items()
        },
        "counts": dict(counts),
        "compile_s_diagnostic": compile_s,
        "work": work,
        "hidden_source_indices": {
            name: np.flatnonzero(mask).tolist() for name, mask in masks.items()
        },
        "inputs": {
            str(path): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in (surface_path, trace_path)
        },
        "code_sha256": {
            path.name: hashlib.sha256(path.read_bytes()).hexdigest()
            for path in [
                Path(__file__),
                *[
                    Path(__file__).with_name(name)
                    for name in (
                        "building_shell_walls.py",
                        "building_positions.py",
                        "roof_fusion.py",
                        "building_roof_selection.py",
                        "building_roof_positions.py",
                        "building_roof_domains.py",
                    )
                ],
            ]
        },
    }
    output.mkdir()
    try:
        report["native_seed_subset_controls"] = verify_seeds(
            native["parent"], geometry, sources, products
        )
    except ValueError as error:
        report["native_seed_controls_complete"] = False
        report["native_seed_controls_error"] = str(error)
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
        raise
    report["native_seed_controls_complete"] = True
    if images:
        report["reference"] = reference_images(
            native, calls, geometry, masks, output / "images"
        )
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--surfaces", type=Path, required=True)
    parser.add_argument("--trace", type=Path, required=True)
    parser.add_argument("--products", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--certified-opaque-shell", action="store_true")
    parser.add_argument("--reference-images", action="store_true")
    args = parser.parse_args()
    target, temporary = args.output.resolve(), Path(tempfile.gettempdir()).resolve()
    if (
        target.exists()
        or target == temporary
        or not target.is_relative_to(temporary)
        or target.is_relative_to(Path(__file__).resolve().parents[3])
    ):
        parser.error("fresh explicit system-temp output outside checkout required")
    result = compare(
        args.surfaces,
        args.trace,
        args.products,
        target,
        certified_opaque=args.certified_opaque_shell,
        images=args.reference_images,
    )
    print(
        json.dumps(
            {
                "parent": result["parent"],
                "hidden": {
                    name: len(indices)
                    for name, indices in result["hidden_source_indices"].items()
                },
                "native_seed_triangles": sum(
                    row["oriented_triangles"]
                    for row in result["native_seed_subset_controls"]
                ),
            }
        ),
        flush=True,
    )
