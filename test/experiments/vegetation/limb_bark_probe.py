"""Rebuild the mature source, compare complete wood, export private bark evidence."""

import argparse
import json
import time
import tracemalloc
from itertools import pairwise
from pathlib import Path

import numpy as np
from limb_bark import mature_recipe, station_distances
from limb_experiment import canonical_wood
from limb_surface import path_recipe
from oak_scaffold import tapered_radii
from oak_skin_experiment import seed_continuations
from scipy.integrate import quad
from spatial_group_source import load_source
from subcrown_products import prepare_case


def check_lengths(crown, radii, paths):
    largest = 0.0
    for path in paths:
        spine, _ = path_recipe(crown.positions, radii, path)
        parameters = np.linspace(0, 1, (len(path) - 1) * 2 + 1)
        reference = [0.0]
        for start, end in pairwise(parameters):
            value = quad(
                lambda u, curve=spine: np.linalg.norm(curve.evaluate(u, 1)),
                start,
                end,
                epsabs=1e-12,
                epsrel=1e-12,
            )[0]
            reference.append(reference[-1] + value)
        distance = station_distances(spine, parameters)
        largest = max(largest, float(np.max(abs(distance - reference))))
    if largest > 1e-9:
        raise AssertionError("fixed quadrature fails independent mature-source control")
    return largest


def run(pilot, original, output):
    if not output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("private outputs must be under /tmp")
    output.mkdir(parents=True, exist_ok=False)
    source_report = prepare_case(pilot, original, output, "limb")
    inputs = output / "limb"
    source = load_source(inputs)
    metadata = json.loads((inputs / "comparison.json").read_text())
    recipe = metadata["recipe"]
    radii = tapered_radii(
        source.crown.parents, recipe["root_radius_m"], recipe["terminal_radius_m"]
    )
    seed_count = metadata["products"]["spatial"]["source_seed_nodes"]
    hints = seed_continuations(source.crown.parents, source.crown.parents[:seed_count])

    def build(bark):
        result = canonical_wood(
            source.crown, radii=radii, max_nodes=16384, continuations=hints, bark=bark
        )
        if result is None:
            raise ValueError("complete wood exceeds declared capacity")
        return result

    timings = {"smooth": [], "bark": []}
    for iteration in range(3):
        for enabled in (False, True) if iteration % 2 == 0 else (True, False):
            start = time.perf_counter()
            mesh, paths = build(enabled)
            timings["bark" if enabled else "smooth"].append(time.perf_counter() - start)
            del mesh
    peaks = {}
    for enabled in (False, True):
        tracemalloc.start()
        mesh, paths = build(enabled)
        _, peaks["bark" if enabled else "smooth"] = tracemalloc.get_traced_memory()
        tracemalloc.stop()
        del mesh
    mesh, paths = build(True)
    with np.load(inputs / "wood.npz", allow_pickle=False) as old:
        for name in old.files:
            np.testing.assert_array_equal(mesh[name], old[name])
    np.savez_compressed(output / "wood-bark.npz", **mesh)
    (output / "material.json").write_text(json.dumps(mature_recipe(), indent=2) + "\n")
    report = {
        "source": source_report,
        "array_exact_old_geometry": True,
        "path_count": len(paths),
        "vertices": len(mesh["vertices"]),
        "triangles": len(mesh["faces"]),
        "extra_retained_array_bytes": mesh["bark_coordinates"].nbytes
        + mesh["bark_radius"].nbytes,
        "quad_velocity_evaluations": 16 * sum((len(p) - 1) * 2 for p in paths),
        "chart_vertex_evaluations": len(mesh["vertices"]),
        "cross_axis_point_queries": 0,
        "independent_quad_max_station_error_m": check_lengths(
            source.crown, radii, paths
        ),
        "construction_seconds_alternating": timings,
        "construction_median_seconds": {
            k: float(np.median(v)) for k, v in timings.items()
        },
        "separate_tracemalloc_build_peak_bytes": peaks,
        "scope": "Same stored C1 geometry and owners. Per-path solid charts; separate branch collars remain. Bump changes shading only. No temporal/LOD/native/device admission; Python traced peaks are not RSS or cumulative allocations.",
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pilot", type=Path, required=True)
    parser.add_argument("--original", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    run(args.pilot, args.original, args.output)
