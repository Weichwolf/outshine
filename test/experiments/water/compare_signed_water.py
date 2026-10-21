"""Signed water controls with independent ellipsoid normals and retained physical contacts."""

import argparse
import json
import resource
import time
import tracemalloc
from pathlib import Path

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.water import fit_physical_surface, frame
from outshine_ref.water.geodetic import WGS84_A
from signed_water_reference import nearest_normals, prism_points

XY = np.array([[-1000, -1000], [1000, -1000], [0, 1000.0]])
FACES = np.array([[0, 1, 2]])
ROWS = np.array([[0, 1, 0, 0, -0.005, 0.005], [0, 0, 1, 0, -0.005, 0.005]])
ANCHORS = [[10.0, 47.0], [35.5, 31.5], [179.99, 75.0], [18.0, -35.0]]


def plane_points(vertices, xy):
    a, b, c = vertices
    weights = np.linalg.solve(
        np.stack([b[:2] - a[:2], c[:2] - a[:2]], axis=1), (xy - a[:2]).T
    ).T
    return np.c_[xy, a[2] + weights @ np.array([b[2] - a[2], c[2] - a[2]])]


def measure_case(anchor, model_height, arrays, case, work):
    target = frame.model_height_targets(anchor, XY, np.full(3, model_height))
    mesh = Mesh(np.c_[XY, target], FACES)
    bounds = np.array([target.min() - 1, target.max() + 1])
    caps = frame.normal_caps(anchor, XY, FACES, bounds)
    samples = prism_points(XY, bounds)
    normals, heights = nearest_normals(anchor, samples, counters=work)
    variation = np.linalg.norm(normals - caps["up"][0], axis=1)
    if (heights - samples[:, 2]).min() < -64 * np.spacing(WGS84_A) or (
        variation - caps["delta"][0]
    ).max() > 0:
        raise ValueError("independent signed support/normal enclosure failed")
    directions = frame.tangent_directions(
        np.repeat(caps["up"], 2, axis=0), ROWS[:, 1:4]
    )
    rows = []
    xy_samples = np.unique(prism_points(XY, [0, 0])[:, :2], axis=0)
    for method in ("soc", "linear", "osqp"):
        start = time.perf_counter()
        result = fit_physical_surface(
            mesh,
            anchor,
            ROWS,
            up_bounds_m=bounds,
            fixed={0: float(target[0])},
            contact_points=mesh.vertices.mean(0)[None],
            primary_rows=None if method == "soc" else np.array([[0, 1]]),
            solver="osqp" if method == "osqp" else "clarabel",
            iterations=50000,
        )
        elapsed = time.perf_counter() - start
        if result.mesh is None:
            raise ValueError(result.report)
        points = plane_points(result.mesh.vertices, xy_samples)
        up, height = nearest_normals(anchor, points, counters=work)
        a, b, c = result.mesh.vertices
        normal = np.cross(b - a, c - a)
        violation = 0.0
        for condition, direction in zip(ROWS, directions, strict=True):
            tangent = direction - (up @ direction)[:, None] * up
            tangent /= np.linalg.norm(tangent, axis=1)[:, None]
            grade = -(tangent @ normal) / (up @ normal)
            violation = max(
                violation,
                float((condition[4] - grade).max()),
                float((grade - condition[5]).max()),
            )
        if (
            max(
                violation,
                abs(result.mesh.vertices[0, 2] - target[0]),
                result.report["fixed_point_up_error_m"],
            )
            > 1e-9
        ):
            raise ValueError("independent physical grade/contact check failed")
        arrays[f"case_{case}_{method}_vertices"] = result.mesh.vertices
        arrays[f"case_{case}_{method}_sample_height_m"] = height
        rows.append(
            {
                "method": method,
                "wall_s": elapsed,
                "fit_report": dict(result.report),
                "independent_grade_violation": violation,
                "independent_geodetic_model_height_error_range_m": [
                    float((height - model_height).min()),
                    float((height - model_height).max()),
                ],
            }
        )
    arrays[f"case_{case}_sample_xy_m"] = xy_samples
    arrays[f"case_{case}_target_up_m"] = target
    arrays[f"case_{case}_cap_delta"] = caps["delta"]
    arrays[f"case_{case}_normal_variation"] = variation
    arrays[f"case_{case}_prism_points"] = samples
    return {
        "anchor_lon_lat": anchor,
        "declared_model_height_m": model_height,
        "input_enu_up_range_m": [float(target.min()), float(target.max())],
        "old_positive_domain_rejected": bool(bounds[0] <= 0),
        "up_bounds_m": bounds.tolist(),
        "independent_prism_points": len(samples),
        "independent_plane_points_per_method": len(xy_samples),
        "max_normal_cap_excess": float((variation - caps["delta"][0]).max()),
        "methods": rows,
    }


def plot(cases, arrays, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from matplotlib.tri import Triangulation

    fig, axes = plt.subplots(
        4, 3, figsize=(12.8, 9.6), dpi=100, sharex=True, sharey=True
    )
    for case, (row, ax) in enumerate(zip(cases, axes.flat, strict=True)):
        points = arrays[f"case_{case}_sample_xy_m"]
        error = (
            arrays[f"case_{case}_linear_sample_height_m"]
            - row["declared_model_height_m"]
        )
        image = ax.tripcolor(
            Triangulation(*points.T),
            error,
            shading="gouraud",
            cmap="viridis",
            vmin=-0.16,
            vmax=0.01,
        )
        ax.set_title(
            f"{row['anchor_lon_lat']}; model h={row['declared_model_height_m']:g} m",
            fontsize=8,
        )
        ax.set_aspect("equal")
    for ax in axes[-1]:
        ax.set_xlabel("East (m)")
    for ax in axes[:, 0]:
        ax.set_ylabel("North (m)")
    fig.suptitle(
        "Declared signed-height controls: faceted geodetic height minus vertex target (m)"
    )
    fig.colorbar(
        image,
        ax=axes.ravel().tolist(),
        label="Facet model-height error (m)",
        shrink=0.75,
    )
    fig.savefig(output / "signed-water.png")
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    start, cpu = time.perf_counter(), time.process_time()
    tracemalloc.start()
    cases, arrays, work = [], {}, {}
    for anchor in ANCHORS:
        for height in (0.0, -430.0, -50000.0):
            cases.append(measure_case(anchor, height, arrays, len(cases), work))
    report = {
        "cases": cases,
        "work": work,
        "scope": "declared geodetic height controls; no observed water levels or native execution",
        "diagnostic_process": {
            "wall_s_excluding_plot": time.perf_counter() - start,
            "cpu_s_excluding_plot": time.process_time() - cpu,
            "tracemalloc_peak_bytes_excluding_plot": tracemalloc.get_traced_memory()[1],
            "retained_array_payload_bytes": sum(a.nbytes for a in arrays.values()),
            "allocation_count": None,
        },
    }
    tracemalloc.stop()
    np.savez_compressed(args.output / "signed-water.npz", **arrays)
    plot(cases, arrays, args.output)
    report["diagnostic_process"]["peak_rss_bytes_linux_including_plot"] = (
        resource.getrusage(resource.RUSAGE_SELF).ru_maxrss * 1024
    )
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["diagnostic_process"]))


if __name__ == "__main__":
    main()
