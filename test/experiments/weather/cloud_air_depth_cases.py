"""Full physical extent controls for depth-dependent air sharing (#353)."""

import json

import numpy as np
from cloud_air_depth import reconstruct, solar_mass
from cloud_air_reference import RAY, SAMPLE, clear_span, direction_for, origin_for
from cloud_air_source_reference import medium_parameters
from cloud_air_transfer_reuse import clear_response, coefficients, grid_indices
from cloud_field import Layer, optical_depth, segments
from cloud_radiance import SUN
from cloud_sun_reuse_benchmark import image_rays
from cloud_sun_reuse_cases import ray_controls, write_inputs

METHODS = (
    "legacy-visibility",
    "visibility",
    "distance-linear",
    "mass-linear",
    "mass-hermite",
)
COUNTS = (32, 64, 128, 256)


def prepare(output):
    write_inputs(output)
    rays, _ = ray_controls()
    medium = medium_parameters()
    rows, queries = [], []
    generator = np.random.default_rng(353)
    for ray in rays:
        origin, direction = origin_for(ray, medium[0]), direction_for(ray)
        begin, end = clear_span(origin, direction, 1e7, 6360000, 6460000)
        begin, end = begin / 1000, end / 1000
        extent = end - begin
        nodes = {
            str(count): 0.1 * np.expm1(np.linspace(0, np.log1p(extent / 0.1), count))
            for count in COUNTS
        }
        dense = 0.1 * np.expm1(np.linspace(0, np.log1p(extent / 0.1), 4097))
        requested = np.r_[
            0,
            begin + dense,
            begin + generator.random(257) * extent,
            *[begin + value for value in nodes.values()],
            10000,
        ].astype("<f4")
        requested = np.unique(requested)
        indices = {}
        for count, values in nodes.items():
            node_queries = (begin + values).astype("<f4")
            node_queries[0], node_queries[-1] = 0, 10000
            indices[count] = np.searchsorted(requested, node_queries).tolist()
        ray["begin"], ray["end"] = len(queries), len(queries) + len(requested)
        queries.extend(requested)
        rows.append({"begin_km": begin, "end_km": end, "nodes": indices})
    rays.tofile(output / "rays.bin")
    np.asarray(queries, "<f4").tofile(output / "queries.bin")
    (output / "depth.json").write_text(json.dumps(rows, indent=2))
    sparse, sparse_rays = [], rays.copy()
    for source, target in zip(rays, sparse_rays):
        values = np.asarray(queries[int(source["begin"]) : int(source["end"])], "<f4")
        subset = np.repeat(np.unique(np.r_[values[::17], values[-1]]), 2)
        target["begin"], target["end"] = len(sparse), len(sparse) + len(subset)
        sparse.extend(subset)
    (output / "sparse").mkdir(exist_ok=True)
    sparse_rays.tofile(output / "sparse/rays.bin")
    np.asarray(sparse, "<f4").tofile(output / "sparse/queries.bin")
    empty = rays[[0, 24]].copy()
    empty[0]["ray"][4] = 0
    empty[1]["ray"][1] = 1
    empty[1]["ray"][3] = 0.8
    empty["begin"], empty["end"] = [0, 4], [4, 8]
    (output / "empty").mkdir(exist_ok=True)
    empty.tofile(output / "empty/rays.bin")
    np.tile([0, 0, 1, 10000], 2).astype("<f4").tofile(output / "empty/queries.bin")
    image, _ = image_rays(1920, 1080)
    chosen, shape = grid_indices(1920, 1080, 8)
    raster = image[chosen].copy()
    raster["begin"] = np.arange(len(raster), dtype="<u4")
    raster["end"] = raster["begin"] + 1
    (output / "raster").mkdir(exist_ok=True)
    raster.tofile(output / "raster/rays.bin")
    np.full(len(raster), 10000, "<f4").tofile(output / "raster/queries.bin")
    (output / "raster/grid.json").write_text(
        json.dumps(
            {
                "shape": shape,
                "image_width": 1920,
                "image_height": 1080,
                "step": 8,
                "query_km": 10000,
                "scope": "coarse source-work control, complete physical prefix; no volume stores or reconstruction",
            }
        )
    )


def solar_transfer(points, cover, seconds):
    layer = Layer(cover=cover)
    intervals = segments(points, SUN, layer)
    active = np.any(intervals[..., 1] > intervals[..., 0], axis=-1)
    result = np.ones(points.shape[:-1])
    result[active] = np.exp(
        -optical_depth(points[active], SUN, layer, 6, seconds, maximum_m=1e8)
    )
    return result


def analyze(output, name, cover, seconds):
    rays = np.fromfile(output / "rays.bin", RAY)
    queries = np.fromfile(output / "queries.bin", "<f4")
    samples = np.fromfile(output / f"{name}.bin", SAMPLE)
    layout = json.loads((output / "depth.json").read_text())
    medium = medium_parameters()
    sigma, _, _ = coefficients(rays, medium)
    results = {str(count): {method: [] for method in METHODS} for count in COUNTS}
    diagnostics = []
    for index, (ray, row) in enumerate(zip(rays, layout)):
        first, last = int(ray["begin"]), int(ray["end"])
        q, native = queries[first:last], samples[first:last]
        repeated = np.repeat(ray[None], len(q), axis=0)
        integral, through, direct, diffuse = clear_response(repeated, q, medium)
        length = np.clip(
            q.astype(float) - row["begin_km"], 0, row["end_km"] - row["begin_km"]
        )
        values = solar_mass(native, length, repeated, medium)
        diagnostics.append(
            {
                "ray": index,
                "T_abs": float(np.max(abs(native["through"] - through))),
                "J_below_zero": float(values.min()),
                "J_above_clear": float(np.max(values - integral)),
                "J_decrease_max": float(np.max(-np.diff(values, axis=0))),
                "source_positions": int(native["sources"].max()),
            }
        )
        for count in COUNTS:
            at = np.asarray(row["nodes"][str(count)])
            nodes = length[at]
            points = origin_for(ray, medium[0]) + direction_for(ray) * (
                (nodes + row["begin_km"])[:, None] * 1000
            )
            point_solar = (
                np.exp(
                    -optical_depth(
                        points, SUN, Layer(cover=cover), 6, seconds, maximum_m=1e8
                    )
                )
                if cover > 0
                else np.ones(len(nodes))
            )
            for method in METHODS:
                result = reconstruct(
                    nodes, values[at], point_solar, length, sigma, method
                )
                rgb = direct * result + diffuse * integral
                error = np.max(
                    abs(rgb - native["radiance"])
                    / np.maximum(abs(native["radiance"]), 1e-5),
                    axis=-1,
                )
                worst = int(np.argmax(error))
                results[str(count)][method].append(
                    {
                        "ray": index,
                        "RGB_relative_max": float(error[worst]),
                        "query_km": float(q[worst]),
                        "query_index": first + worst,
                        "J_below_zero": float(result.min()),
                        "J_above_clear": float(np.max(result - integral)),
                        "J_decrease_max": float(np.max(-np.diff(result, axis=0))),
                    }
                )
    report = {"diagnostics": diagnostics, "methods": results}
    (output / f"{name}-analysis.json").write_text(json.dumps(report, indent=2))
    return report
