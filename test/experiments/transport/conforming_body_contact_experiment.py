"""First fixed-World conforming contact control, with explicit local outputs."""

import argparse
import gc
import json
import os
import resource
import sys
import time
import tracemalloc
from pathlib import Path

import numpy as np
from body_ground_support import prepare, sample
from body_ground_support_experiment import extend
from conforming_body_contact import conform, contact_error, grid, local_conform
from outshine_ref.contracts import Mesh
from outshine_ref.geometry.sampling import SurfaceSampler
from world_stair_reference import FIXTURE, construct


def heights(model, xy):
    support = sample(model, xy, max_pairs=1048576)
    if not support["valid"].all():
        raise ValueError("controlled contact field has unresolved points")
    return extend(support, 25)


def measure(model, xy, faces, repeats):
    methods = {
        "ordinary": lambda: (
            np.column_stack((xy, heights(model, xy))),
            faces.copy(),
            {},
        ),
        "conformed": lambda: conform(model, xy, faces, lambda p: heights(model, p)),
        "local": lambda: local_conform(model, xy, faces, lambda p: heights(model, p)),
    }
    times = {k: [] for k in methods}
    for i in range(repeats + 1):
        order = tuple(methods)
        for name in order[i % 3 :] + order[: i % 3]:
            wall, cpu = time.perf_counter(), time.process_time()
            result = methods[name]()
            times[name].append([time.perf_counter() - wall, time.process_time() - cpu])
            del result
    costs = {}
    for name, method in methods.items():
        gc.collect()
        tracemalloc.start()
        result = method()
        _, peak = tracemalloc.get_traced_memory()
        blocks = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
        costs[name] = {
            "first_wall_cpu_s": times[name][0],
            "following_wall_cpu_s": times[name][1:],
            "median_wall_cpu_s": np.median(times[name][1:], axis=0).tolist(),
            "python_traced_peak_bytes": peak,
            "python_traced_live_blocks": blocks,
            "output_numeric_bytes": result[0].nbytes + result[1].nbytes,
        }
        del result
    return costs


def geometry_image(body, vertices, faces):
    from current_visibility import project, raster

    source = body["body_vertices"]
    target = (source.min(axis=0) + source.max(axis=0)) * 0.5
    eye = target + [-10.0, -14.0, 12.0]
    forward = (target - eye) / np.linalg.norm(target - eye)
    right = np.cross(forward, [0, 0, 1])
    right /= np.linalg.norm(right)
    up = np.cross(right, forward)
    view = (
        eye,
        np.array([right, up, forward]),
        720 / (2 * np.tan(np.deg2rad(60) / 2)),
        1280,
        720,
    )
    points = np.vstack((vertices, source))
    triangles = np.vstack((faces, body["body_faces"] + len(vertices)))
    projected, z = project(points, view)
    if np.any(z <= 0.1):
        raise ValueError("geometry diagnostic requires no near-crossing triangles")
    _, owners = raster(projected, z, triangles, 1280, 720, with_faces=True)
    p = points[triangles]
    normal = np.cross(p[:, 1] - p[:, 0], p[:, 2] - p[:, 0])
    normal /= np.linalg.norm(normal, axis=1)[:, None]
    light = np.array([-0.2, -0.3, 1.0])
    light /= np.linalg.norm(light)
    shade = 0.3 + 0.7 * abs(normal @ light)
    colour = np.tile([0.65, 0.60, 0.47], (len(triangles), 1))
    colour[len(faces) :] = [0.37, 0.54, 0.72]
    image = np.full((720, 1280, 3), [0.82, 0.85, 0.88])
    visible = owners >= 0
    image[visible] = colour[owners[visible]] * shade[owners[visible], None]
    return image


def display(model, body, baseline, faces, vertices, refined, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figure = plt.figure(figsize=(16, 6))
    source = body["body_vertices"]
    low, high = source[:, :2].min(axis=0) - 1, source[:, :2].max(axis=0) + 1
    for i, (points, triangles, title) in enumerate(
        (
            (baseline, faces, "Ordinary 0.5 m grid"),
            (vertices, refined, "Conforming contact mesh"),
        ),
        1,
    ):
        axis = figure.add_subplot(1, 3, i)
        axis.imshow(geometry_image(body, points, triangles))
        axis.set(
            title=title + " / 1280×720 CPU depth", xlabel="pixel x", ylabel="pixel y"
        )
    cap_points = model.sampler.mesh.vertices[:, :2]
    centre = cap_points.mean(axis=0)
    _, directions = np.linalg.eigh(np.cov(cap_points.T))
    direction = directions[:, -1]
    distance = np.linspace(
        -0.5 * np.linalg.norm(high - low), 0.5 * np.linalg.norm(high - low), 2048
    )
    line = centre + distance[:, None] * direction
    source_samples = model.sampler.sample(line)
    inside = source_samples.face_indices >= 0
    axis = figure.add_subplot(1, 3, 3)
    for points, triangles, label in (
        (baseline, faces, "ordinary"),
        (vertices, refined, "conforming"),
    ):
        samples = SurfaceSampler(Mesh(points, triangles)).sample(line[inside])
        if np.any(samples.face_indices < 0):
            raise ValueError("diagnostic section has unresolved terrain contacts")
        error = samples.points_enu_m[:, 2] - source_samples.points_enu_m[inside, 2]
        axis.plot(distance[inside], error * 100, label=label)
    axis.axhline(0, color="black", linewidth=0.5)
    axis.set(
        title="Centre cut: terrain − underside",
        xlabel="along principal axis / m",
        ylabel="contact difference / cm",
    )
    axis.legend()
    figure.suptitle(
        "Fixed World body and unchanged controlled exterior field; local geometry diagnostic, no PBR/device claim"
    )
    figure.tight_layout()
    figure.savefig(output / "conforming-body-contact.png", dpi=150)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=7)
    parser.add_argument("--render", action="store_true")
    args = parser.parse_args()
    if not 1 <= args.repeats <= 21:
        parser.error("repeat count must be between 1 and 21")
    case = construct(json.loads(FIXTURE.read_text()))
    body = case["data"]
    saved_body = {k: v.copy() for k, v in body.items() if isinstance(v, np.ndarray)}
    model = prepare(body, 1)
    low, high = (
        body["body_vertices"][:, :2].min(axis=0),
        body["body_vertices"][:, :2].max(axis=0),
    )
    xy, faces = grid(
        np.arange(low[0] - 10, high[0] + 10.25, 0.5),
        np.arange(low[1] - 10, high[1] + 10.25, 0.5),
    )
    baseline = np.column_stack((xy, heights(model, xy)))
    wall, cpu = time.perf_counter(), time.process_time()
    vertices, refined, work = local_conform(
        model, xy, faces, lambda points: heights(model, points)
    )
    cost = {"wall_s": time.perf_counter() - wall, "cpu_s": time.process_time() - cpu}
    report = {
        "loadavg": os.getloadavg(),
        "cost": cost,
        "work": work,
        "ordinary_grid": contact_error(model, baseline, faces),
        "conformed": contact_error(model, vertices, refined),
        "scope": "Same fixed World body, declared single ground group, and controlled nearest-support exterior field; not actual native DEM or a new exterior recipe.",
    }
    global_vertices, global_faces, global_work = conform(
        model, xy, faces, lambda p: heights(model, p)
    )
    report["global_reference"] = {
        "work": global_work,
        "contact": contact_error(model, global_vertices, global_faces),
    }
    np.testing.assert_array_equal(vertices, global_vertices)
    if len(refined) != len(global_faces):
        raise ValueError("fixed World local/global triangle count changed")

    def canonical(indices):
        indices = np.sort(indices, axis=1)
        return indices[np.lexsort(indices.T)]

    np.testing.assert_array_equal(canonical(refined), canonical(global_faces))
    report["local_global_vertex_words_and_unordered_triangles_equal"] = True
    if (
        max(
            report["conformed"]["maximum_above_underside_m"],
            report["conformed"]["maximum_gap_below_underside_m"],
        )
        > 1e-7
    ):
        raise ValueError("conformed terrain failed the actual body contact tolerance")
    args.output.mkdir(parents=True, exist_ok=True)
    np.savez(
        args.output / "geometry.npz",
        source_vertices=body["body_vertices"],
        source_faces=body["body_faces"],
        baseline_vertices=baseline,
        baseline_faces=faces,
        vertices=vertices,
        faces=refined,
    )
    np.savez(
        args.output / "body.npz",
        **{k: v for k, v in body.items() if isinstance(v, np.ndarray)},
    )
    report["measurements"] = measure(model, xy, faces, args.repeats)
    for key, original in saved_body.items():
        np.testing.assert_array_equal(body[key], original)
    report["unchanged_body_arrays"] = len(saved_body)
    (args.output / "bindings.json").write_text(
        json.dumps(
            {
                "source_rows": case["plan"]["rows"],
                "ports": case["ports"],
                "capture_digest": case["capture"]["input_digest"],
                "ground_group": 1,
                "role_origin": "controlled generator declaration, not provider inference",
            },
            indent=2,
        )
        + "\n"
    )
    report["source_numeric_bytes"] = sum(
        v.nbytes for v in body.values() if isinstance(v, np.ndarray)
    )
    report["peak_process_rss_bytes_before_plot"] = resource.getrusage(
        resource.RUSAGE_SELF
    ).ru_maxrss * (1 if sys.platform == "darwin" else 1024)
    report["measurement_scope"] = (
        "Ordinary/conformed differ in contact quality; no speed contest between those. Local/global conformed geometry is word-equal up to triangle order. Separate tracemalloc invocations exclude GEOS allocations; whole-process RSS includes all alternatives and source. GEOS calls are not individually cancellable."
    )
    if args.render:
        display(model, body, baseline, faces, vertices, refined, args.output)
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
