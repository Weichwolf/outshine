"""Compare pre-emission longitudinal cuts against fixed native road meshes."""

import argparse
import json
from pathlib import Path
from time import perf_counter

import numpy as np
from compare_upper_strip_envelope import surface_rings
from longitudinal_strip import simplify
from native_road_parts import checked_payload, load


def run(output):
    manifest, _, parts, payload, _ = load()
    original = sum(part["counts"][3] // 3 for part in parts)
    budgets = [0.01, 0.05, 0.25, 1.0, 4.0]
    results = {
        error: {"triangles": original, "source_rings": 0, "selected_rings": 0, "ring_visits": 0}
        for error in budgets
    }
    surfaces = 0
    floor = original
    began = perf_counter()
    for part in parts:
        if part["kind"] != "edge":
            continue
        streams = checked_payload(part, payload)
        positions = np.frombuffer(streams["positions"], dtype="<f4").reshape(-1, 3)
        indices = np.frombuffer(streams["indices"], dtype="<u4") - part["base_vertex"]
        rings = surface_rings(positions, indices)
        if rings is None or rings.shape[1] != 1:
            continue
        surfaces += 1
        floor -= len(indices) // 3 - 6
        rings = rings[:, 0]
        for error, counts in results.items():
            selected, _, visits = simplify(rings, error)
            counts["triangles"] -= (len(rings) - len(selected)) * 6
            counts["source_rings"] += len(rings)
            counts["selected_rings"] += len(selected)
            counts["ring_visits"] += visits
    report = {
        "archive_sha256": manifest["archive_sha256"],
        "original_triangles": original,
        "surface_parts": surfaces,
        "fixed_boundary_floor": {
            "triangles": floor,
            "maximum_reduction_fraction": 1 - floor / original,
            "scope": "one six-triangle surface span per recognized surface part; closed, unknown and junction triangles unchanged; no global optimality claim",
        },
        "comparisons": results,
        "python_elapsed_ms": 1000 * (perf_counter() - began),
        "boundary": "Float64 finite source probe; no native, projection, contact/terrain, shadow or device qualification. Closed/unknown/junction products unchanged.",
    }
    output.write_text(json.dumps(report, indent=2) + "\n")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    print(json.dumps(run(parser.parse_args().output), indent=2))
