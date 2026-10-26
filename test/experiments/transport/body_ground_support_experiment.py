"""Compare identical body-foot support and a separate controlled exterior recipe."""

import argparse
import gc
import json
import math
import os
import platform
import resource
import time
import tracemalloc
from pathlib import Path

import numpy as np
import shapely
from body_ground_support import prepare, sample
from compare_mvt_terrain_effects import controlled_roles
from earthwork_transition import smoothstep
from terrain_source_effects import qualify_effects
from world_stair_reference import FIXTURE, construct

STRATEGIES = ("scan", "index", "boundary")


def control_body(diagonal=0):
    xy = np.array([[0.0, 0.0], [4.0, 0.0], [4.0, 4.0], [0.0, 4.0]])
    return {
        "body_vertices": np.column_stack([xy, 2 + 0.25 * xy.sum(axis=1)]),
        "bottom_faces": np.array(
            [[0, 1, 2], [0, 2, 3]] if diagonal == 0 else [[0, 1, 3], [1, 2, 3]]
        ),
        "face_sources": np.array([1, 1]),
    }


def native_soft_exterior(model, xy, prior, *, apron=3.0, relief=3.0, most=10.0):
    """Single-key soft exterior algebra at zero planar support, not whole native terrain."""
    sampler = model.sampler
    triangles = sampler.mesh.vertices[sampler.mesh.triangles]
    origin, gradient = triangles[:, 0], sampler._gradient
    delta = xy[None, :, :] - origin[:, None, :2]
    wanted = (
        origin[:, None, 2]
        + delta[:, :, 0] * gradient[:, None, 0]
        + delta[:, :, 1] * gradient[:, None, 1]
    )
    corners = triangles[:, :, :2] - origin[:, None, :2]
    corner_z = (
        origin[:, None, 2]
        + corners[:, :, 0] * gradient[:, None, 0]
        + corners[:, :, 1] * gradient[:, None, 1]
    )
    wanted = np.clip(
        wanted, corner_z.min(axis=1)[:, None], corner_z.max(axis=1)[:, None]
    )
    distances = shapely.distance(
        sampler._polygons[:, None], shapely.points(xy)[None, :]
    )
    if np.any(distances <= 0):
        raise ValueError("native primitive control accepts exterior queries only")
    width = math.hypot(apron, 1.875 * min(abs(relief), most) * 1.5)
    fade = smoothstep(np.minimum(distances / width, 1))
    weights = (1 - fade) / fade
    total, strongest = weights.sum(axis=0), weights.max(axis=0)
    average = np.divide(
        (weights * np.clip(wanted - prior, -most, most)).sum(axis=0),
        total,
        out=np.zeros(len(xy)),
        where=total > 0,
    )
    return prior + strongest * average / (1 + strongest)


def extend(support, prior, *, apron=3.0, relief=3.0, most=10.0):
    width = math.hypot(apron, 1.875 * min(abs(relief), most) * 1.5)
    correction = support["height_m"] - prior
    hard = support["valid"] & (support["distance_m"] == 0)
    if np.any(hard & (abs(correction) > most)):
        raise ValueError("controlled hard contacts exceed the declared height budget")
    fade = smoothstep(np.minimum(support["distance_m"] / width, 1))
    result = prior + np.clip(correction, -most, most) * (1 - fade)
    return np.where(support["valid"], result, np.nan)


def compare(body, points, repeats):
    models, preparation = {}, {}
    for strategy in STRATEGIES:
        wall, cpu = time.perf_counter(), time.process_time()
        models[strategy] = prepare(body, 1, strategy=strategy)
        preparation[strategy] = {
            "wall_s": time.perf_counter() - wall,
            "cpu_s": time.process_time() - cpu,
            **dict(models[strategy].preparation),
        }
        gc.collect()
        tracemalloc.start()
        prepared_probe = prepare(body, 1, strategy=strategy)
        _current, peak = tracemalloc.get_traced_memory()
        blocks = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
        preparation[strategy].update(
            python_traced_peak_bytes=peak, python_traced_live_blocks=blocks
        )
        del prepared_probe
    times = {s: [] for s in STRATEGIES}
    for i in range(repeats + 1):
        order = STRATEGIES[i % 3 :] + STRATEGIES[: i % 3]
        for strategy in order:
            wall, cpu = time.perf_counter(), time.process_time()
            result = sample(models[strategy], points)
            elapsed_cpu = time.process_time() - cpu
            times[strategy].append((time.perf_counter() - wall, elapsed_cpu))
            del result
    outputs, reports = {}, {}
    for strategy in STRATEGIES:
        gc.collect()
        tracemalloc.start()
        result = sample(models[strategy], points)
        _current, peak = tracemalloc.get_traced_memory()
        blocks = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
        outputs[strategy] = result
        values = np.asarray(times[strategy])
        reports[strategy] = {
            "first_wall_cpu_s": values[0].tolist(),
            "following_wall_cpu_s": values[1:].tolist(),
            "median_wall_cpu_s": np.median(values[1:], axis=0).tolist(),
            "python_traced_peak_bytes": peak,
            "python_traced_live_blocks": blocks,
            "work": result["work"],
            "preparation": preparation[strategy],
        }
    reference, errors = outputs["scan"], {}
    for strategy, result in outputs.items():
        if result["reasons"] != reference["reasons"]:
            raise ValueError("nearest strategies disagree on unresolved contacts")
        for key in ("body_cap_face_ids", "source_masks", "valid"):
            np.testing.assert_array_equal(result[key], reference[key])
        errors[strategy] = {}
        for key, tolerance in (
            ("height_m", 1e-7),
            ("foot_xy_m", 1e-8),
            ("distance_m", 1e-8),
        ):
            np.testing.assert_allclose(
                result[key], reference[key], atol=tolerance, rtol=0
            )
            errors[strategy][key] = float(np.max(abs(result[key] - reference[key])))
    return models, outputs, reports, errors


def grid_contact_error(model, x, y, heights):
    mesh = model.sampler.mesh
    points = np.vstack(
        [mesh.vertices[:, :2], mesh.vertices[mesh.triangles].mean(axis=1)[:, :2]]
    )
    expected = model.sampler.sample(points)
    if np.any(expected.face_indices < 0):
        raise ValueError("actual body contact probes must resolve on their own source")
    cell = (points - [x[0, 0], y[0, 0]]) / 0.5
    column, row = np.floor(cell).astype(int).T
    fx, fy = (cell - np.floor(cell)).T
    h = heights.reshape(x.shape)
    a, b, c, d = (
        h[row, column],
        h[row, column + 1],
        h[row + 1, column],
        h[row + 1, column + 1],
    )
    terrain = np.where(
        fy <= fx, a + fx * (b - a) + fy * (d - b), a + fx * (d - c) + fy * (c - a)
    )
    error = terrain - expected.points_enu_m[:, 2]
    return {
        "source_probes": len(points),
        "maximum_above_underside_m": max(0.0, float(error.max())),
        "maximum_gap_below_underside_m": max(0.0, float(-error.min())),
        "scope": "Actual cap centres and used vertices on an ordinary 0.5m grid split on each lower-left/upper-right diagonal; point support is not conforming mesh contact.",
    }


def display(case, x, y, old, new, error, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    figure = plt.figure(figsize=(15, 6))
    data = case["data"]
    vertices = data["body_vertices"]
    cells = np.arange(x.size).reshape(x.shape)
    a, b, c, d = (
        cells[:-1, :-1].ravel(),
        cells[:-1, 1:].ravel(),
        cells[1:, :-1].ravel(),
        cells[1:, 1:].ravel(),
    )
    triangles = np.vstack([np.column_stack([a, b, d]), np.column_stack([a, d, c])])
    for index, (terrain, title) in enumerate(
        (
            (old, "Current exterior-cap primitive + exact hard support"),
            (new, "Proposed nearest-body support + same fade"),
        ),
        1,
    ):
        axes = figure.add_subplot(1, 3, index, projection="3d")
        axes.plot_trisurf(
            x.ravel(),
            y.ravel(),
            terrain.ravel(),
            triangles=triangles,
            color=(0.55, 0.6, 0.5),
            alpha=0.8,
            linewidth=0,
        )
        axes.add_collection3d(
            Poly3DCollection(
                vertices[data["top_faces"]],
                facecolors=(0.2, 0.3, 0.4),
                edgecolors=(0, 0, 0, 0.1),
                linewidths=0.2,
            )
        )
        axes.set(
            xlabel="east / m", ylabel="north / m", zlabel="height / m", title=title
        )
        axes.set_box_aspect((1, 1, 0.45))
        axes.view_init(elev=32, azim=-65)
    axes = figure.add_subplot(1, 3, 3)
    picture = axes.pcolormesh(x, y, error, shading="auto", cmap="coolwarm")
    axes.set_aspect("equal")
    axes.set(title="Proposed − primitive / m", xlabel="east / m", ylabel="north / m")
    figure.colorbar(picture, ax=axes)
    figure.suptitle(
        "Unconformed 0.5m grid / flat prior25m; zero cell support; point guarantee is not mesh-contact admission"
    )
    figure.tight_layout()
    figure.savefig(output / "body-ground-field.png", dpi=150)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=21)
    parser.add_argument("--render", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.repeats <= 100:
        parser.error("bounded positive repeat count required")
    args.output.mkdir(parents=True, exist_ok=True)
    load = os.getloadavg()
    began = time.perf_counter()
    case = construct(json.loads(FIXTURE.read_text()))
    declarations = controlled_roles(case)
    roles = qualify_effects(case["plan"], case["data"], declarations)
    if not np.all(roles["effects"] == 1) or not np.all(roles["contact_keys"] == 1):
        raise ValueError(
            "this finite field experiment requires the declared World ground group"
        )
    build_s = time.perf_counter() - began
    vertices = case["data"]["body_vertices"]
    low, high = vertices[:, :2].min(axis=0), vertices[:, :2].max(axis=0)
    x, y = np.meshgrid(
        np.arange(low[0] - 10, high[0] + 10.25, 0.5),
        np.arange(low[1] - 10, high[1] + 10.25, 0.5),
    )
    points = np.column_stack([x.ravel(), y.ravel()])
    models, outputs, reports, errors = compare(case["data"], points, args.repeats)
    reference = outputs["scan"]
    if not reference["valid"].all():
        raise ValueError("fixed World field contains unresolved controlled queries")
    if np.max(abs(reference["height_m"] - 25)) > 3:
        raise ValueError(
            "controlled relief estimate must cover actual World support correction"
        )
    new = extend(reference, 25)
    old = new.copy()
    exterior = reference["distance_m"] > 0
    old[exterior] = native_soft_exterior(models["scan"], points[exterior], 25)
    probe = np.array([[4.5, 0.25]])
    controls = []
    for diagonal in (0, 1):
        model = prepare(control_body(diagonal), 1)
        support = sample(model, probe)
        controls.append(
            {
                "diagonal": diagonal,
                "primitive_m": native_soft_exterior(model, probe, 0).item(),
                "nearest_m": extend(support, 0).item(),
                "foot_xy_m": support["foot_xy_m"].tolist(),
            }
        )
    timer_pairs = []
    for _ in range(101):
        wall, cpu = time.perf_counter(), time.process_time()
        elapsed_cpu = time.process_time() - cpu
        timer_pairs.append((time.perf_counter() - wall, elapsed_cpu))
    report = {
        "body_and_role_preparation_s": build_s,
        "queries": len(points),
        "cases": reports,
        "support_errors_vs_scan": errors,
        "diagonal_control": controls,
        "world_exterior_recipe_difference_max_m": float(abs(new - old).max()),
        "controlled_prior_m": 25,
        "controlled_relief_m": 3,
        "measured_grid_gradient_norm": {
            name: float(np.hypot(*np.gradient(values.reshape(x.shape), 0.5)).max())
            for name, values in (("primitive", old), ("nearest", new))
        },
        "unconformed_grid_contact_error": {
            name: grid_contact_error(models["scan"], x, y, values)
            for name, values in (("primitive", old), ("nearest", new))
        },
        "start_load": load,
        "timer_pair_median_wall_cpu_s": np.median(timer_pairs, axis=0).tolist(),
        "process_peak_rss_bytes_before_export_plot": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * (1 if platform.system() == "Darwin" else 1024),
        "scope": "Same-support scan/index/boundary query comparison; models and body prepared separately, three timings rotate order. Query tracing separate after GC. RSS includes models/body/recipe diagnostics. Exterior recipe differs from native cap clamp and has no positive cell support/other stamps/real DEM; no same-quality speedup across different recipes, native or device admission.",
    }
    np.savez(args.output / "body.npz", **case["data"])
    for name, result in outputs.items():
        np.savez(
            args.output / (name + "-support.npz"),
            **{k: v for k, v in result.items() if isinstance(v, np.ndarray)},
        )
    np.savez(
        args.output / "controlled-field.npz",
        query_xy_m=points,
        x=x,
        y=y,
        primitive_m=old,
        nearest_m=new,
    )
    bindings = {
        "captured_source": case["capture"],
        "source_instances_in_mask_bit_order": case["plan"]["rows"],
        "ports": case["ports"],
        "terrain_declarations": declarations,
        "scope": "Fixed body/owned ports preserved. Explicit controlled ground group; no provider-role or geodetic-datum inference.",
    }
    for name, record in (("report", report), ("source-bindings", bindings)):
        (args.output / (name + ".json")).write_text(json.dumps(record, indent=2) + "\n")
    if args.render:
        display(
            case,
            x,
            y,
            old.reshape(x.shape),
            new.reshape(x.shape),
            (new - old).reshape(x.shape),
            args.output,
        )
    print(json.dumps({k: v for k, v in report.items() if k != "cases"}))


if __name__ == "__main__":
    main()
