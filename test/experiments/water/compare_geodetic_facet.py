"""Controlled WGS84 height semantics; no reconstruction of legacy Ill XY."""

import argparse
import hashlib
import json
import platform
import resource
import tempfile
import time
import tracemalloc
from pathlib import Path

import numpy as np
import pyproj
from outshine_ref.water.geodetic import WGS84_A, local_points, query_facet
from pyproj import Geod, Transformer
from pyproj.enums import TransformDirection

MODELS = ("constant_enu_up", "constant_geodetic_vertex_height")


def destinations(geod, anchor, bearing, distance):
    bearing, distance = np.broadcast_arrays(bearing, distance)
    lon, lat, _ = geod.fwd(
        np.full(bearing.shape, anchor[0]),
        np.full(bearing.shape, anchor[1]),
        bearing,
        distance,
    )
    return np.stack((lon, lat), axis=-1)


def finite_difference(geod, anchor, triangle, points, step=2):
    columns = []
    for azimuth in (90, 0):
        heights = []
        for distance in (-step, step):
            lon, lat, _ = geod.fwd(
                points[:, 0],
                points[:, 1],
                np.full(len(points), azimuth),
                np.full(len(points), distance),
            )
            heights.append(
                query_facet(anchor, triangle, np.c_[lon, lat], metric_height_m=0)[
                    "plane_height_m"
                ]
            )
        columns.append((heights[1] - heights[0]) / (2 * step))
    return np.array(columns).T


def independent_heights(anchor, triangle, points):
    """PROJ ECEF and a 3x3 ray/barycentric solve, without local plane algebra."""
    transform = Transformer.from_pipeline(
        f"+proj=topocentric +ellps=WGS84 +lon_0={anchor[0]} +lat_0={anchor[1]} +h_0=0"
    )
    world = np.array(
        transform.transform(*triangle.T, direction=TransformDirection.INVERSE)
    ).T
    cartesian = Transformer.from_crs(4979, 4978, always_xy=True)
    sea = np.array(
        cartesian.transform(points[:, 0], points[:, 1], np.zeros(len(points)))
    ).T
    ellipsoid = pyproj.CRS.from_epsg(4979).ellipsoid
    axes_squared = np.square(
        [
            ellipsoid.semi_major_metre,
            ellipsoid.semi_major_metre,
            ellipsoid.semi_minor_metre,
        ]
    )
    up = sea / axes_squared
    up /= np.linalg.norm(up, axis=1)[:, None]
    first, second = world[1:] - world[0]
    matrix = np.stack(
        (np.broadcast_to(first, up.shape), np.broadcast_to(second, up.shape), -up),
        axis=-1,
    )
    return np.linalg.solve(matrix, (sea - world[0])[..., None])[:, 2, 0]


def describe(
    anchor,
    radius,
    shifts,
    projection_error,
    values,
    differences,
    heights,
    bary,
    inside,
    independent,
):
    entry = {
        "anchor_lon_lat": anchor,
        "radius_m": radius,
        "declared_geodetic_vertex_height_m": 500,
        "horizontal_metric_height_m": 0,
        "common_interior_queries": int(inside.sum()),
        "independent_proj_max_error_m": projection_error,
        "height_induced_horizontal_shift_max_m": float(shifts.max()),
        "models": {},
    }
    for i, name in enumerate(MODELS):
        entry["models"][name] = {
            "interior_height_error_range_m": [
                float((heights[i, inside] - 500).min()),
                float((heights[i, inside] - 500).max()),
            ],
            "max_grade_difference_from_enu_up": float(
                abs(
                    values[i]["grade_m_per_m"][inside] - values[i]["enu_up_grade"]
                ).max()
            ),
            "max_gradient_fd_error_m_per_m": float(
                abs(values[i]["grade_m_per_m"] - differences[i]).max()
            ),
            "independent_ecef_height_max_error_m": float(
                abs(heights[i] - independent[i]).max()
            ),
            "corner_plane_extension_error_range_m": [
                float((heights[i, [4, 8, 12]] - 500).min()),
                float((heights[i, [4, 8, 12]] - 500).max()),
            ],
            "corner_barycentric_min": float(bary[i, [4, 8, 12]].min()),
        }
    return entry


def corpus():
    geod = Geod(ellps="WGS84")
    arrays = {
        name: []
        for name in (
            "anchor",
            "radius_m",
            "corner_lon_lat",
            "query_lon_lat",
            "triangles_enu_m",
            "plane_height_m",
            "grade_m_per_m",
            "barycentric",
            "finite_difference_grade",
            "common_interior",
            "independent_height_m",
        )
    }
    records = []
    for anchor in ([0, 0], [10, 47], [179.99, 75], [18, -35]):
        proj_frame = Transformer.from_pipeline(
            f"+proj=pipeline +step +proj=cart +ellps=WGS84 +step +proj=topocentric +ellps=WGS84 +lon_0={anchor[0]} +lat_0={anchor[1]} +h_0=0"
        )
        for radius in (10, 1000, 5000):
            corners = destinations(geod, anchor, np.array([0, 120, 240]), radius)
            queries = np.r_[
                np.array(anchor)[None],
                destinations(
                    geod,
                    anchor,
                    np.repeat([0, 120, 240], 4),
                    np.tile([0.25, 0.5, 0.75, 1], 3) * radius,
                ),
            ]
            sea, lifted = (
                local_points(anchor, corners, 0),
                local_points(anchor, corners, 500),
            )
            proj = np.array(
                proj_frame.transform(corners[:, 0], corners[:, 1], np.full(3, 500))
            ).T
            projection_error = float(abs(lifted - proj).max())
            np.testing.assert_allclose(
                lifted, proj, rtol=0, atol=8 * np.spacing(WGS84_A)
            )
            triangles = np.stack((np.c_[sea[:, :2], np.full(3, 500)], lifted))
            values = [
                query_facet(anchor, triangle, queries, metric_height_m=0)
                for triangle in triangles
            ]
            differences = np.array(
                [
                    finite_difference(geod, anchor, triangle, queries)
                    for triangle in triangles
                ]
            )
            grades = np.array([v["grade_m_per_m"] for v in values])
            np.testing.assert_allclose(
                grades, differences, rtol=0, atol=16 * np.spacing(WGS84_A) / 2
            )
            bary = np.array([v["barycentric"] for v in values])
            inside = (bary > 1e-5).all(axis=(0, 2))
            if not inside.any() or not all(
                v["valid_plane_query"].all() for v in values
            ):
                raise ValueError(
                    "controlled facets lost their common interior or query conditioning"
                )
            heights = np.array([v["plane_height_m"] for v in values])
            independent = np.array(
                [
                    independent_heights(anchor, triangle, queries)
                    for triangle in triangles
                ]
            )
            np.testing.assert_allclose(
                heights, independent, rtol=0, atol=64 * np.spacing(WGS84_A)
            )
            shifts = np.linalg.norm(lifted[:, :2] - sea[:, :2], axis=1)
            entry = describe(
                anchor,
                radius,
                shifts,
                projection_error,
                values,
                differences,
                heights,
                bary,
                inside,
                independent,
            )
            for key, value in zip(
                arrays,
                (
                    anchor,
                    radius,
                    corners,
                    queries,
                    triangles,
                    heights,
                    grades,
                    bary,
                    differences,
                    inside,
                    independent,
                ),
                strict=True,
            ):
                arrays[key].append(value)
            records.append(entry)
    return {key: np.array(value) for key, value in arrays.items()}, records


def plot(arrays, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    selected = int(
        np.flatnonzero(
            (arrays["anchor"] == [10, 47]).all(axis=1) & (arrays["radius_m"] == 5000)
        )[0]
    )
    anchor = arrays["anchor"][selected]
    distances = np.linspace(-2000, 2000, 101)
    queries = destinations(Geod(ellps="WGS84"), anchor, np.full(101, 90), distances)
    figure, axes = plt.subplots(1, 2, figsize=(12.8, 5), layout="constrained")
    for triangle, name in zip(arrays["triangles_enu_m"][selected], MODELS, strict=True):
        value = query_facet(anchor, triangle, queries, metric_height_m=0)
        if not value["inside_numeric"].all():
            raise ValueError("profile extends beyond its physical triangle")
        axes[0].plot(distances, value["plane_height_m"] - 500, label=name)
        axes[1].plot(distances, value["grade_m_per_m"][:, 0], label=name)
    axes[0].axhline(
        0, color="black", linestyle="--", linewidth=0.7, label="declared vertex level"
    )
    axes[1].axhline(
        0, color="black", linestyle="--", linewidth=0.7, label="flat ENU-Up derivative"
    )
    axes[0].set(
        ylabel="Geodetic height − declared 500 m [m]",
        title="A plane and a curved constant level differ",
    )
    axes[1].set(
        ylabel="Geodetic east grade [m/m at metric h=0]",
        title="ENU derivative is not a geodetic grade",
    )
    for axis in axes:
        axis.set_xlabel("WGS84 geodesic launch distance [m], initial azimuth 90°")
        axis.grid(alpha=0.25)
        axis.legend(fontsize=8)
    figure.suptitle(
        "Synthetic facet, vertices 5 km from anchor 10°E/47°N; no observed ASL conversion"
    )
    figure.savefig(output, dpi=120)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private output belongs in system temp outside the checkout")
    output.mkdir(parents=True, exist_ok=False)
    tracemalloc.start()
    began = time.perf_counter()
    arrays, records = corpus()
    numeric_s = time.perf_counter() - began
    _, peak = tracemalloc.get_traced_memory()
    tracemalloc.stop()
    np.savez(output / "controls.npz", **arrays)
    began = time.perf_counter()
    plot(arrays, output / "height-grade.png")
    plot_s = time.perf_counter() - began
    proc_memory = {}
    if platform.system() == "Linux":
        for line in Path("/proc/self/status").read_text().splitlines():
            if line.startswith(("VmRSS:", "VmHWM:")):
                key, value, _ = line.split()
                proc_memory[key.removesuffix(":")] = int(value) * 1024
    report = {
        "scope": "Controlled ellipsoidal model, no original Ill reconstruction/geoid/source admission/solver/native or device gate.",
        "models": MODELS,
        "cases": records,
        "array_bytes": sum(v.nbytes for v in arrays.values()),
        "main_point_queries": 312,
        "finite_difference_point_queries": 1248,
        "independent_ecef_point_queries": 312,
        "plot_point_queries": 202,
        "instrumented_numeric_wall_s": numeric_s,
        "plot_wall_s": plot_s,
        "python_traced_numeric_peak_bytes": peak,
        "rusage_maxrss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * (1 if platform.system() == "Darwin" else 1024),
        "linux_process_memory_bytes": proc_memory,
        "memory_scope": "Python tracing excludes PROJ/LAPACK/native allocators; RSS includes imports and plotting, not a per-facet/native budget.",
        "pyproj": pyproj.__version__,
        "proj": pyproj.proj_version_str,
        "artifacts_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sorted(output.iterdir())
        },
    }
    (output / "report.json").write_text(
        json.dumps(report, indent=2, allow_nan=False) + "\n"
    )
    print(
        json.dumps(
            {
                k: report[k]
                for k in (
                    "models",
                    "array_bytes",
                    "instrumented_numeric_wall_s",
                    "python_traced_numeric_peak_bytes",
                )
            },
            indent=2,
        )
    )


if __name__ == "__main__":
    main()
