"""Replay saved PR175 coefficients and compare complete bounded planner products."""

import argparse
import hashlib
import json
import time
from pathlib import Path

import numpy as np
from outshine_ref.transport import PortPlaneField


def load(directory):
    report = json.loads((directory / "report.json").read_text())
    path = directory / report["mesh_file"]
    if not report["valid_geometry"] or report["working_tree_dirty"]:
        raise ValueError(
            "complete clean-commit products required for the release comparison"
        )
    if hashlib.sha256(path.read_bytes()).hexdigest() != report["mesh_sha256"]:
        raise ValueError("saved comparison input changed")
    with np.load(path) as data:
        arrays = {key: data[key] for key in data.files}
    return report, arrays


def replay_coefficients(data):
    began = time.monotonic()
    field = PortPlaneField(
        data["plane_origins"],
        data["plane_ids"],
        data["cut_centres"],
        data["inward_normals"],
        data["scales"],
    )
    coefficients = data["coefficients"][: 3 * len(field.origins)]
    road = data["parts"] >= 0
    owners = np.full(len(data["xy"]), -1, int)
    np.maximum.at(
        owners, data["faces"][road].ravel(), np.repeat(data["parts"][road], 3)
    )
    selected = np.flatnonzero(owners >= 0)
    heights, _ = field.evaluate(data["xy"][selected], owners[selected], coefficients)
    error = float(abs(heights + data["datum_m"] - data["heights"][selected]).max())
    if error > 1e-7:
        raise ValueError(
            "installed kernel does not reproduce the saved actual road vertices"
        )
    faces = np.flatnonzero(road)
    difference, maximum = 0.0, 0.0
    for start in range(0, len(faces), 256):
        ids = faces[start : start + 256]
        grade, bound = field.bounds(
            data["xy"][data["faces"][ids]],
            data["parts"][ids],
            coefficients,
            data["frames"][ids],
        )
        difference = max(
            difference, float(abs(bound - data["height_errors"][ids]).max())
        )
        maximum = max(maximum, float(grade.max()))
    if difference > 1e-10 or maximum > 1.0000001:
        raise ValueError("installed kernel changed the saved whole-face certificates")
    return {
        "road_vertices": len(selected),
        "road_faces": len(faces),
        "maximum_saved_height_difference_m": error,
        "maximum_saved_chord_bound_difference_m": difference,
        "maximum_normalized_gradient": maximum,
        "replay_ms": (time.monotonic() - began) * 1000,
    }


def compare(baseline, current, output):
    before, a = load(baseline)
    after, b = load(current)
    if (
        before["frame"] != after["frame"]
        or before["source_usage"] != after["source_usage"]
    ):
        raise ValueError("frame or original source usage changed")
    np.testing.assert_array_equal(a["plane_ids"], b["plane_ids"])
    layout_error = {}
    for key in ("plane_origins", "cut_centres", "inward_normals", "scales"):
        np.testing.assert_allclose(a[key], b[key], atol=1e-10, rtol=0)
        layout_error[key] = float(abs(a[key] - b[key]).max(initial=0))
    xy_a, xy_b = a["xy"][a["original_vertex_map"]], b["xy"][b["original_vertex_map"]]
    np.testing.assert_array_equal(xy_a, xy_b)
    delta = abs(
        a["heights"][a["original_vertex_map"]] - b["heights"][b["original_vertex_map"]]
    )
    result = {
        "baseline_commit": before["generation_commit"],
        "current_commit": after["generation_commit"],
        "baseline_sha256": before["mesh_sha256"],
        "current_sha256": after["mesh_sha256"],
        "fixed_coefficient_replay": replay_coefficients(a),
        "maximum_layout_differences": layout_error,
        "maximum_original_anchor_height_difference_m": float(delta.max()),
        "p95_original_anchor_height_difference_m": float(np.quantile(delta, 0.95)),
        "baseline_prior_error_m": before["maximum_original_prior_error_m"],
        "current_prior_error_m": after["maximum_original_prior_error_m"],
        "baseline_total_ms": before["total_ms"],
        "current_total_ms": after["total_ms"],
        "baseline_peak_rss_bytes": before["peak_rss_bytes"],
        "current_peak_rss_bytes": after["peak_rss_bytes"],
        "current_quality": after["audit"],
        "scope": "Same captured geometry, original observations, quality targets and hard worker caps. Fixed coefficients verify migration; independently bounded MaxTime fits need not be bit-identical and do not prove an optimum. Host/offline costs, not runtime or device evidence.",
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result), flush=True)
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline", type=Path)
    parser.add_argument("current", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    compare(args.baseline, args.current, args.output)
