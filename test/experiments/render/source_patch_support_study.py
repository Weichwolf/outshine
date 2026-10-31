"""Whole bound source/candidate support comparison, not a native admission gate."""

import argparse
import hashlib
import json
import math
import subprocess
import time
from collections import Counter
from fractions import Fraction as F
from pathlib import Path

import numpy as np
from experiment_output import create_output
from source_patch_inputs import read_bound_sources
from source_patch_product import FIELDS, read_product
from source_patch_support import compare_source

ROOT = Path(__file__).resolve().parents[3]


def grouped(identities):
    order = np.argsort(identities, kind="stable")
    cuts = np.flatnonzero(np.diff(identities[order])) + 1
    return {int(identities[indices[0]]): indices for indices in np.split(order, cuts)}


def run(args):
    files = [
        Path(__file__).with_name(name)
        for name in (
            "source_patch_support_study.py",
            "source_patch_support.py",
            "surface_patch_overlay.py",
            "surface_patch_envelope.py",
            "source_patch_product.py",
            "source_patch_inputs.py",
            "source_patch_candidates.py",
            "building_sample_originals.py",
            "native_building_cell_reference.py",
            "compare_building_originals.py",
        )
    ]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, files)], text=True
    ):
        raise ValueError("Freeze measured comparison sources first")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    hashes = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in files}
    create_output(args.output)
    original, context, provenance = read_bound_sources(
        ROOT / "test/outshine/data/render/building-depth-samples"
    )
    producer = json.loads((args.input / "report.json").read_text())
    if any(
        producer["input_provenance"][key] != provenance[key]
        for key in ("source_binding_sha256", "source_archive_sha256")
    ):
        raise ValueError("Same actual whole source cohort/frame binding required")
    parts = read_product(
        args.input / "candidate",
        producer["diagnostic_native_candidate"],
        context["source_ordinals"],
    )
    vertices = np.c_[
        original["positions"],
        np.concatenate(
            [
                np.concatenate([part[key] for key in FIELDS[1:]], axis=1)
                for part in original["attributes"]
            ]
        ),
    ]
    old = vertices[original["indices"]]
    old_material = np.concatenate(
        [
            np.full(part["triangles"], part["material"], np.int8)
            for part in original["parts"]
        ]
    )
    new = np.concatenate(
        [
            np.concatenate([values[key] for key in FIELDS], axis=1)[values["indices"]]
            for _part, values, _ids in parts
        ]
    )
    new_material = np.concatenate(
        [
            np.full(part["triangles"], part["material"], np.int8)
            for part, _values, _ids in parts
        ]
    )
    old_groups = grouped(context["triangle_binding"][:, 1])
    new_groups = grouped(np.concatenate([ids for _part, _values, ids in parts]))
    if set(old_groups) != set(new_groups):
        raise ValueError("Every original source must have a complete candidate")
    began = time.process_time()
    results = []
    for identity in sorted(old_groups):
        before, after = old_groups[identity], new_groups[identity]
        result = compare_source(
            old[before],
            new[after],
            old_material[before],
            new_material[after],
            boundary_envelope=args.boundary_envelope,
            max_envelope_work=args.max_envelope_work,
        )
        results.append({"prepared_source_ordinal": identity, **result})
    cpu = time.process_time() - began
    proven = [row for row in results if row["whole_source_proven"]]
    unknown = [row for row in results if not row["whole_source_proven"]]
    maximum_squared = max(
        (row["position_error_squared"] for row in proven), default=F(0)
    )
    field_maximum = [
        max((row["field_error_absolute"][i] for row in proven), default=F(0))
        for i in range(11)
    ]
    summary = {
        "whole_sources": len(results),
        "complete_numeric_comparisons": len(proven),
        "same_primitive_word_multiset": sum(
            row["mode"] == "same_primitive_word_multiset" for row in proven
        ),
        "complete_chart_comparisons": sum(
            row["mode"] == "complete_common_charts" for row in proven
        ),
        "complete_boundary_envelopes": sum(
            row["mode"] == "complete_boundary_envelopes" for row in proven
        ),
        "boundary_envelope_enabled": args.boundary_envelope,
        "unknown_retain_original": len(unknown),
        "unknown_reasons": dict(Counter(row["reason"] for row in unknown)),
        "proved_sources_with_zero_geometry_and_varying_error": sum(
            row["position_error_squared"] == 0 and not any(row["field_error_absolute"])
            for row in proven
        ),
        "maximum_proved_correspondence_distance_m": math.sqrt(float(maximum_squared)),
        "maximum_proved_absolute_field_errors": list(map(float, field_maximum)),
        "proved_triangle_pairs": sum(row["triangle_pairs"] for row in proven),
        "proved_overlay_vertex_evaluations": sum(
            row["overlay_vertex_evaluations"] for row in proven
        ),
        "envelope_polygon_clips": sum(
            row.get("envelope_polygon_clips", 0) for row in proven
        ),
        "point_triangle_tests": sum(
            row.get("point_triangle_tests", 0) for row in proven
        ),
        "single_comparison_cpu_s": cpu,
        "scope": "Continuous numeric geometry/linear-varying correspondence through common cells and, when enabled, explicit convex remainder maps. Unknowns are not zero-error. Proved work counters exclude failed attempts, while CPU covers all sources. No native signed-word/interpolation/derivative/raster/PBR/World/device admission and no tolerance policy or replacement selected. Time is one structural run; no repeated cost/peak claim.",
    }
    if hashes != {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in files}:
        raise ValueError("Comparison source changed during the run")
    report = {
        "commit": commit,
        "source_sha256": hashes,
        "producer_commit": producer["commit"],
        "producer_report_sha256": hashlib.sha256(
            (args.input / "report.json").read_bytes()
        ).hexdigest(),
        "input_provenance": provenance,
        "field_order": [
            "normal_x",
            "normal_y",
            "normal_z",
            "uv_x_raw",
            "uv_y_raw",
            "metres_u",
            "metres_v",
            "colour_r",
            "colour_g",
            "colour_b",
            "colour_a",
        ],
        "summary": summary,
        "sources": results,
    }
    (args.output / "report.json").write_text(
        json.dumps(report, default=str, indent=2) + "\n"
    )
    print(json.dumps(summary))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--boundary-envelope", action="store_true")
    parser.add_argument("--max-envelope-work", type=int, default=32768)
    run(parser.parse_args())
