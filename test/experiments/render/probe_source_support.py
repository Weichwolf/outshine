"""Inspect closed source support and construction cost, not an accepted renderer."""

import argparse
import itertools
import json
import subprocess
import time
from dataclasses import fields
from pathlib import Path

import numpy as np
import shapely
from parent_surface_inputs import open_parent
from PIL import Image, ImageDraw
from process_memory import peak_rss_bytes
from source_support_function import compile_support, sample_support, support_ranges


def world_bounds(support):
    low, high = np.full(3, np.inf), np.full(3, -np.inf)
    floor_centres = []
    for source in range(len(support.source_ids)):
        frame = np.linalg.inv(support.inverses[source])
        begin, end = support.offsets[source : source + 2]
        for i in range(int(begin), int(end)):
            xy = shapely.get_coordinates(support.domains[i])
            top = xy @ support.planes[i, :2] + support.planes[i, 2]
            local = np.concatenate(
                (
                    np.column_stack((xy, top)),
                    np.column_stack((xy, np.full(len(xy), support.bottoms[i]))),
                )
            )
            points = local @ frame + support.origins[source]
            floor_centres.append(float(points[len(xy) :, 2].mean()))
            low, high = np.minimum(low, points.min(0)), np.maximum(high, points.max(0))
    return np.stack((low, high)), float(np.median(floor_centres))


def original_controls(path, support, basis, anchor, output):
    from compare_plane_cloud import image_roi
    from full_view_reference import load, views
    from triangle_image_reference import image_rays, intersect_source, prepare_source

    source, document, context, _ = load(path)
    pose = views(document, context)["moving-023"]
    box = image_roi(source, pose)
    if box is None:
        raise ValueError("the existing original moving-023 control must be visible")
    width, height = (box[1] - box[0]).astype(int)
    origins, directions = image_rays(pose, box, width, height, 2)
    directions /= np.linalg.norm(directions, axis=1)[:, None]
    depth, faces = intersect_source(
        source, origins, directions, prepared=prepare_source(source)
    )
    finite = np.flatnonzero(np.isfinite(depth))
    selected = finite[np.isin(source["binding"][faces[finite], 1], support.source_ids)]
    if not len(selected):
        raise ValueError("original control requires accepted-source surface hits")
    selected = selected[
        np.linspace(0, len(selected) - 1, min(64, len(selected)), dtype=int)
    ]
    triangles = source["positions"][source["indices"][faces[selected]]].astype(float)
    normals = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    normals /= np.linalg.norm(normals, axis=1)[:, None]
    normals *= np.where(
        np.einsum("ij,ij->i", normals, directions[selected]) < 0, 1, -1
    )[:, None]
    hit = origins[selected] + depth[selected, None] * directions[selected]
    plus = (hit + 0.01 * normals) @ basis.T - anchor
    minus = (hit - 0.01 * normals) @ basis.T - anchor
    outside, outside_work = sample_support(support, plus)
    inside, inside_work = sample_support(support, minus)
    np.savez_compressed(
        output / "original-controls.npz",
        outside_points=plus,
        inside_points=minus,
        outside_values=outside,
        inside_values=inside,
        source_ids=source["binding"][faces[selected], 1],
        reference_faces=faces[selected],
    )
    return {
        "pose": "moving-023",
        "samples": len(selected),
        "normal_offset_m": 0.01,
        "outside_positive": int((outside > 0).sum()),
        "inside_nonpositive": int((inside <= 0).sum()),
        "outside_work": outside_work,
        "inside_work": inside_work,
        "scope": "Finite 1cm geometric-normal offsets around actual original ray hits, not pixel/ray/motion or topology admission. Incomplete sources excluded only from this control and retained as named local needs.",
    }


def adaptive_frontier(support, bounds, output):
    """One four-level support control; unresolved leaves stay unresolved."""
    pending = bounds[None]
    leaves, intervals, levels, states = [], [], [], []
    work = {
        "source_point_transforms": 0,
        "domain_point_bound_tests": 0,
        "exact_contour_distance_queries": 0,
    }
    octants = np.array(list(itertools.product([0, 1], repeat=3)))
    for level in range(5):
        ranges, current = support_ranges(support, pending)
        for key in work:
            work[key] += current[key]
        state = np.where(ranges[:, 0] > 0, 1, np.where(ranges[:, 1] < 0, -1, 0))
        terminal = (state != 0) | (level == 4)
        leaves.append(pending[terminal])
        intervals.append(ranges[terminal])
        states.append(state[terminal])
        levels.append(np.full(terminal.sum(), level, np.uint8))
        parent = pending[~terminal]
        if not len(parent):
            break
        half = (parent[:, 1] - parent[:, 0]) / 2
        low = parent[:, 0, None] + octants * half[:, None]
        high = low + half[:, None]
        pending = np.stack((low, high), axis=2).reshape(-1, 2, 3)
    arrays = {
        "bounds": np.concatenate(leaves),
        "ranges": np.concatenate(intervals),
        "states": np.concatenate(states).astype(np.int8),
        "levels": np.concatenate(levels),
    }
    np.savez_compressed(output / "support-frontier.npz", **arrays)
    return {
        "work": work,
        "maximum_level": 4,
        "terminal_boxes": len(arrays["bounds"]),
        "interior_boxes": int((arrays["states"] < 0).sum()),
        "exterior_boxes": int((arrays["states"] > 0).sum()),
        "unresolved_boxes": int((arrays["states"] == 0).sum()),
        "array_bytes": sum(a.nbytes for a in arrays.values()),
        "scope": "Bounded four-level adaptive construction control, no interpolated surface/rendering. Unresolved leaves retain exact-source demand; omitted local Original sources remain separate needs. Frontier bytes exclude the still-required source oracle and are not a replacement-memory result.",
    }


def diagnostic(points, values, bounds, support, needs, output):
    membership = (values <= 0).reshape(64, 64)
    signed = np.clip(0.5 + values.reshape(64, 64) / 16, 0, 1)
    image = Image.new("RGB", (1280, 720), "#202020")
    draw = ImageDraw.Draw(image)
    for column, (label, scalar) in enumerate(
        [
            ("Closed support membership (white <= 0)", membership),
            ("Signed support bound (gray zero, +/- 8m range)", signed),
        ]
    ):
        panel = Image.fromarray(np.flipud(np.round(scalar * 255).astype(np.uint8)))
        panel = panel.resize((576, 576), Image.Resampling.NEAREST)
        image.paste(panel, (16 + column * 632, 60))
        draw.text((16 + column * 632, 28), label, fill="white")
    draw.text(
        (16, 655),
        f"{len(support.source_ids)} supported sources, {len(needs)} named local Original needs excluded from this slice; Z={points[0, 2]:.3f}m",
        fill="white",
    )
    draw.text(
        (16, 677),
        f"XY span {(bounds[1] - bounds[0])[:2]}m. Source function only, not an adaptive field, PBR image, opaque World certificate or memory win.",
        fill="white",
    )
    image.save(output)


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    started = time.perf_counter()
    with open_parent(args.input) as parent:
        opened = time.perf_counter()
        support, basis, anchor, needs = compile_support(parent, set(args.cells))
        compiled = time.perf_counter()
        report = {
            "source_archive_sha256": parent.archive_sha256,
            "cells": args.cells,
            "sources": len(support.source_ids),
            "roof_domains": len(support.domains),
            "needs": needs,
            "form_reads": parent.form_reads,
            "form_bytes": parent.form_bytes,
            "source_open_seconds": opened - started,
            "compile_seconds": compiled - opened,
        }
    bounds, floor_height = world_bounds(support)
    axis = (np.arange(64) + 0.5) / 64
    xy = bounds[0, :2] + np.stack(np.meshgrid(axis, axis), axis=-1).reshape(-1, 2) * (
        bounds[1, :2] - bounds[0, :2]
    )
    points = np.column_stack((xy, np.full(len(xy), floor_height + 2)))
    started = time.perf_counter()
    values, work = sample_support(support, points)
    pruned_seconds = time.perf_counter() - started
    started = time.perf_counter()
    exhaustive, exhaustive_work = sample_support(support, points, prune=False)
    exhaustive_seconds = time.perf_counter() - started
    np.testing.assert_allclose(values, exhaustive, atol=1e-12, rtol=1e-12)
    np.savez_compressed(
        args.output / "slice.npz",
        points=points,
        values=values,
        exhaustive=exhaustive,
        bounds=bounds,
        basis=basis,
        anchor=anchor,
    )
    diagnostic(points, values, bounds, support, needs, args.output / "slice.png")
    report.update(
        query_points=len(points),
        pruned_work=work,
        exhaustive_work=exhaustive_work,
        pruned_seconds=pruned_seconds,
        exhaustive_seconds=exhaustive_seconds,
        maximum_pruning_value_difference_m=float(np.abs(values - exhaustive).max()),
        numeric_array_bytes={
            field.name: getattr(support, field.name).nbytes
            for field in fields(support)
            if isinstance(getattr(support, field.name), np.ndarray)
        },
        domain_wkb_bytes=sum(len(domain.wkb) for domain in support.domains),
        source_only_process_peak_rss_bytes=peak_rss_bytes(),
        total_allocation_count=None,
        total_physical_copied_bytes=None,
        geos_and_python_retained_bytes=None,
    )
    started = time.perf_counter()
    report["adaptive_support_frontier"] = adaptive_frontier(
        support, bounds, args.output
    )
    report["adaptive_support_frontier"]["seconds"] = time.perf_counter() - started
    if args.reference is not None:
        report["original_controls"] = original_controls(
            args.reference, support, basis, anchor, args.output
        )
    report.update(
        producer=subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        process_peak_rss_bytes=peak_rss_bytes(),
        scope="Source-only closed contour/height implicit support for field construction, not an exact signed Euclidean distance, accepted ADF, normal/material function, runtime renderer or device/world benefit. CSG joins can have interior zeros. Domain AABB bounds only prune evaluations; they never create opacity or change membership. Floating arithmetic is checked empirically, not an all-input interval certificate.",
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--cells", nargs="+", type=int, required=True)
    parser.add_argument("--reference", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
