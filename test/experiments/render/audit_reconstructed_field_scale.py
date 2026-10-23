"""Diagnose one fixed field's metric constraints in its actual projected view."""

import argparse
import hashlib
import json
import subprocess
import time
from pathlib import Path

import numpy as np
from field_reconstruction_audit import ReconstructionAudit
from field_scale_features import feature_summary, source_features
from field_scale_projection import (
    box_envelope,
    displacement_bound,
    position_budget,
    project,
    quantiles,
)
from full_view_reference import load, views
from parent_surface_inputs import open_parent
from PIL import Image, ImageDraw
from process_memory import peak_rss_bytes
from reconstructed_source_field import UNKNOWN, FieldBuilder
from source_support_function import compile_support
from trilinear_support import CORNERS


def verify_field(rebuilt, support, basis, anchor, needs, field):
    rebuilt.update(
        accepted_sources=support.source_ids,
        basis=basis,
        anchor=anchor,
        needs_sources=np.asarray([r["source"] for r in needs], np.uint32),
    )
    if set(rebuilt) != set(field):
        raise ValueError("audit must reproduce the exact recorded field arrays")
    for name, value in rebuilt.items():
        if value.dtype != field[name].dtype or not np.array_equal(value, field[name]):
            raise ValueError("fixed field differs from audited build: " + name)


def scale_summary(bounds, view, decisions):
    unknown = decisions[(decisions[:, 3] == 2) | (decisions[:, 3] == 3)]
    budgets, spans = [], []
    for row in unknown:
        box = np.stack((row[10:13], row[10:13] + row[2]))
        budgets.append(position_budget(box, view))
        image = project(box[0] + CORNERS * row[2], view)
        spans.append(np.ptp(image, axis=0))
    return {
        "minimum_camera_depth_m": box_envelope(bounds, view)[0],
        "maximum_camera_depth_m": float(
            (((bounds[0] + CORNERS * np.ptp(bounds, axis=0)) - view[0]) @ view[1].T)[
                :, 2
            ].max()
        ),
        "all_direction_0_125m_displacement_bound_px": displacement_bound(
            bounds, view, 0.125
        ),
        "plus_minus_0_125m_pair_separation_bound_px": 2
        * displacement_bound(bounds, view, 0.125),
        "all_direction_1m_displacement_bound_px": displacement_bound(bounds, view, 1),
        "half_pixel_position_budget_m": position_budget(bounds, view),
        "unknown_cell_half_pixel_position_budget_m": quantiles(budgets),
        "unknown_cell_max_axis_span_px": quantiles(
            np.asarray(spans).reshape(-1, 2).max(axis=1)
        ),
        "scope": "Analytic conservative Euclidean position bounds for the pinhole view. Cell size, scalar residual and support magnitude are not proved surface displacement bounds. The .5px allowance alone says nothing about filtered opacity, normals, BRDF, illumination, shadows or a different view.",
    }


def diagram(features, decisions, view, path):
    sources = features["source_screen_bounds"]
    low = np.floor(sources[:, 0].min(0)) - 3
    high = np.ceil(sources[:, 1].max(0)) + 3
    zoom = 12
    size = np.ceil((high - low) * zoom).astype(int)
    panel_width, panel_height = int(size[0] + 24), int(size[1] + 64)
    image = Image.new("RGB", (panel_width, panel_height * 3), (30, 30, 30))
    draw = ImageDraw.Draw(image)
    for panel, label in enumerate(
        (
            "Actual source projected bounds",
            "Nearest footprint connectors",
            "Unknown cell projected bounds",
        )
    ):
        shift = np.array([12, panel * panel_height + 28])
        draw.text((12, panel * panel_height + 6), label + " / 12x zoom", fill="white")
        for box in sources:
            xy = ((box - low) * zoom + shift).ravel().tolist()
            draw.rectangle(xy, outline=(125, 125, 125))
        if panel == 1:
            for segment in features["gap_witness_segments"]:
                xy = (project(segment, view) - low) * zoom + shift
                draw.line(xy.ravel().tolist(), fill=(40, 220, 230))
        if panel == 2:
            for row in decisions[(decisions[:, 3] == 2) | (decisions[:, 3] == 3)]:
                xy = project(row[10:13] + CORNERS * row[2], view)
                box = np.stack((xy.min(0), xy.max(0)))
                draw.rectangle(
                    ((box - low) * zoom + shift).ravel().tolist(),
                    outline=(230, 45, 200),
                )
        draw.text(
            (12, (panel + 1) * panel_height - 20),
            "Diagnostic extents; no visibility, opacity or rendered-image qualification",
            fill=(190, 190, 190),
        )
    image.save(path)


def run(args):
    args.output.mkdir(parents=True, exist_ok=False)
    started = time.perf_counter()
    with np.load(args.field, allow_pickle=False) as archive:
        field = {key: archive[key] for key in archive.files}
    _, document, context, _ = load(args.input)
    with open_parent(args.input.parent / "original-inputs.tar.gz") as parent:
        support, basis, anchor, needs = compile_support(parent, set(args.cells))
        archive_sha = parent.archive_sha256
    pose = views(document, context)[args.pose]
    view = (pose[0] @ basis.T - anchor, pose[1] @ basis.T, *pose[2:])
    features, pairs = source_features(support, view)
    prepared = time.perf_counter()
    audit = ReconstructionAudit()
    builder = FieldBuilder(support, observer=audit)
    probed = time.perf_counter()
    rebuilt, work = builder.run()
    verify_field(rebuilt, support, basis, anchor, needs, field)
    built = time.perf_counter()
    arrays = audit.arrays()
    unknown = arrays["decisions"][:, 3] >= 2
    unknown_rows = arrays["decisions"][unknown]
    nodes = arrays["decisions"][unknown, 0].astype(int)
    if not np.array_equal(
        np.sort(nodes), np.flatnonzero((field["tags"] & UNKNOWN) != 0)
    ):
        raise ValueError("audit must account for every actual Unknown leaf")
    wrong = arrays["wrong_probes"]
    terminal_wrong = wrong[np.isin(wrong[:, 0], nodes)]
    np.savez_compressed(args.output / "audit.npz", **arrays, **features)
    diagram(features, arrays["decisions"], view, args.output / "projection.png")
    report = {
        "producer": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], text=True
        ).strip(),
        "source_archive_sha256": archive_sha,
        "field_sha256": hashlib.sha256(args.field.read_bytes()).hexdigest(),
        "fixed_field_arrays_equal": True,
        "cells": args.cells,
        "pose": args.pose,
        "profile": {"width": view[3], "height": view[4], "focal_px": view[2]},
        "scale": scale_summary(field["source_bounds"], view, arrays["decisions"]),
        "features": {**feature_summary(features), **pairs},
        "decisions": audit.summary(),
        "unknown_exterior_scalar_residual_quantiles_m": quantiles(unknown_rows[:, 5]),
        "unknown_scalar_fit_tolerance_quantiles_m": quantiles(
            0.05 * unknown_rows[:, 2]
        ),
        "unknown_wrong_probe_support_abs_quantiles_m": quantiles(
            abs(terminal_wrong[:, 2])
        ),
        "unknown_wrong_probes": len(terminal_wrong),
        "unknown_wrong_probes_support_abs_above_0_125m": int(
            (abs(terminal_wrong[:, 2]) > 0.125 + 1e-8).sum()
        ),
        "scope": "Fixed-profile diagnosis only. Support magnitude is not exact boundary distance; small values do not prove geometric proximity. No relaxed candidate, .5px geometric correspondence, filtered C/T/S, PBR, World or device qualification.",
        "work": work,
        "prepare_seconds": prepared - started,
        "source_probes_seconds": probed - prepared,
        "unchanged_field_and_audit_seconds": built - probed,
        "process_peak_rss_bytes": peak_rss_bytes(),
        "audit_array_bytes": {key: value.nbytes for key, value in arrays.items()},
        "feature_array_bytes": {key: value.nbytes for key, value in features.items()},
        "audit_file_bytes": (args.output / "audit.npz").stat().st_size,
        "total_allocation_count": None,
        "total_physical_copied_bytes": None,
        "decision_columns": [
            "node",
            "level",
            "side_m",
            "terminal_0split_1fit_2budget_3depth",
            "failure_mask",
            "scalar_error_m",
            "probes",
            "wrong_probes",
            "wrong_inside_probes",
            "sources",
            "low_x",
            "low_y",
            "low_z",
        ],
        "wrong_probe_columns": [
            "node",
            "probe_index",
            "truth_support",
            "predicted_scalar",
        ],
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2), flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--field", type=Path, required=True)
    parser.add_argument("--cells", type=int, nargs="+", required=True)
    parser.add_argument("--pose", default="moving-023")
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
