"""Compare exact source triangle rays, cell boxes and an implicit height hierarchy."""

import argparse
import gc
import hashlib
import json
import os
import platform
import resource
import time
import tracemalloc
from pathlib import Path

import numpy as np
from current_visibility import project, raster
from terrain_cell_traversal import prepare, sample
from terrain_grid_lod import page_frame, page_geometry, page_steps, triangles
from terrain_occluders import skirts

FIXTURE = Path(__file__).parents[2] / "outshine/data/terrain/native-cell-page"
METHODS = ("scan", "cells", "tree")


def source_page(directory=FIXTURE):
    record = json.loads((directory / "input.json").read_text())
    row = record["row"]
    raw = (directory / (row["page"] + ".f32")).read_bytes()
    if hashlib.sha256(raw).hexdigest() != record["height_sha256"]:
        raise ValueError("changed fixed native source words")
    _, local, _, _, fingerprint = page_geometry(row, directory)
    _, _, corners = page_frame(row)
    drop = 16 * max(*page_steps(corners, 33), 0.001)
    vertices, extra, _ = skirts(local, drop)
    record = {
        **record,
        "geometry_input_sha256": fingerprint.hex(),
        "skirt_drop_m": drop,
        "geometry_scope": "Existing ideal Double page_geometry evaluation of decoded native words, fractions and sag; not GPU Float32 raster rounding.",
    }
    return vertices, np.vstack((triangles(33, 1), extra)), record


def camera(vertices, width, height):
    low, high = vertices[:1089].min(axis=0), vertices[:1089].max(axis=0)
    centre = (low + high) / 2
    span = np.linalg.norm(high[:2] - low[:2])
    eye = centre + span * np.array([0.731, -0.843, 0.617])
    forward = centre - eye
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, [0, 0, 1])
    right /= np.linalg.norm(right)
    basis = np.stack([right, np.cross(right, forward), forward])
    focal = height / (2 * np.tan(np.deg2rad(50) / 2))
    x, y = np.meshgrid(np.arange(width) + 0.5, np.arange(height) + 0.5)
    directions = (
        np.column_stack(
            (
                (x.ravel() - width / 2) / focal,
                (height / 2 - y.ravel()) / focal,
                np.ones(x.size),
            )
        )
        @ basis
    )
    directions /= np.linalg.norm(directions, axis=1)[:, None]
    return (eye, basis, focal, width, height), np.tile(eye, (x.size, 1)), directions


def measure(vertices, faces, origins, directions, repeats):
    models, records = {}, {}
    for method in METHODS:
        wall, cpu = time.perf_counter(), time.process_time()
        model = prepare(vertices, faces, 33, strategy=method)
        record = {
            "preparation_wall_cpu_s": [
                time.perf_counter() - wall,
                time.process_time() - cpu,
            ],
            "owned_mesh_bytes": model.mesh.vertices.nbytes
            + model.mesh.triangles.nbytes,
            "retained_height_bound_bytes": sum(
                b.nbytes for b in model.height_bounds[1:]
            ),
            "retained_xy_margin_bytes": model.xy_margin.nbytes,
            "xy_margin_m": model.xy_margin.tolist(),
        }
        gc.collect()
        tracemalloc.start()
        probe = prepare(vertices, faces, 33, strategy=method)
        _, record["preparation_traced_peak_bytes"] = tracemalloc.get_traced_memory()
        record["preparation_traced_live_blocks"] = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
        del probe
        models[method], records[method] = model, record
    timings = {m: [] for m in METHODS}
    for i in range(repeats + 1):
        for method in METHODS[i % 3 :] + METHODS[: i % 3]:
            wall, cpu = time.perf_counter(), time.process_time()
            output = sample(models[method], origins, directions)
            elapsed_cpu = time.process_time() - cpu
            timings[method].append([time.perf_counter() - wall, elapsed_cpu])
            del output
    outputs = {}
    for method in METHODS:
        gc.collect()
        tracemalloc.start()
        output = sample(models[method], origins, directions)
        _, peak = tracemalloc.get_traced_memory()
        blocks = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
        record = records[method]
        record.update(
            first_wall_cpu_s=timings[method][0],
            following_wall_cpu_s=timings[method][1:],
            median_wall_cpu_s=np.median(timings[method][1:], axis=0).tolist(),
            query_traced_peak_bytes=peak,
            query_traced_live_blocks=blocks,
            work=output["work"],
            result_array_bytes=sum(
                v.nbytes for v in output.values() if isinstance(v, np.ndarray)
            ),
        )
        if outputs:
            for key in ("ray_t", "face_ids", "barycentric", "valid"):
                np.testing.assert_array_equal(output[key], outputs["scan"][key])
        outputs[method] = output
    return outputs, records


def display(vertices, faces, view, depth, inverse, cost, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    figure = plt.figure(figsize=(14, 7))
    axes = figure.add_subplot(2, 3, 1, projection="3d")
    axes.add_collection3d(
        Poly3DCollection(
            vertices[faces[:2048]], facecolors=(0.5, 0.6, 0.5), linewidths=0
        )
    )
    axes.add_collection3d(
        Poly3DCollection(
            vertices[faces[2048:]],
            facecolors=(0.25, 0.35, 0.4),
            linewidths=0,
            alpha=0.6,
        )
    )
    low, high = vertices.min(axis=0), vertices.max(axis=0)
    axes.set(
        xlim=(low[0], high[0]),
        ylim=(low[1], high[1]),
        zlim=(low[2], high[2]),
        title="Actual fine source + complete outer skirts",
    )
    for slot, image, title in (
        (2, depth, "Independent raster / reversed depth"),
        (3, inverse, "Exact rays / reversed depth"),
        (5, inverse - depth, "Ray minus raster / reversed depth"),
    ):
        axes = figure.add_subplot(2, 3, slot)
        picture = axes.imshow(
            image, origin="upper", cmap="coolwarm" if slot == 5 else "viridis"
        )
        axes.set(title=title, xlabel="diagnostic pixel x", ylabel="diagnostic pixel y")
        figure.colorbar(picture, ax=axes)
    axes = figure.add_subplot(2, 3, 4)
    axes.bar(METHODS, [cost[m]["work"]["triangle_tests"] for m in METHODS])
    axes.set(
        ylabel="actual reference ray/triangle tests",
        title="Same ray/mesh query work, not GPU invocations",
    )
    axes = figure.add_subplot(2, 3, 6)
    axes.bar(METHODS, [cost[m]["work"]["ray_node_tests"] for m in METHODS])
    axes.set(ylabel="ray/node interval tests", title="Additional traversal work")
    figure.suptitle(
        f"Single existing page / {view[3]}×{view[4]} diagnostic; no native/world/device admission"
    )
    figure.tight_layout()
    figure.savefig(output / "terrain-cell-traversal.png", dpi=150)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--input", type=Path, default=FIXTURE)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=128)
    parser.add_argument("--height", type=int, default=72)
    parser.add_argument("--repeats", type=int, default=8)
    parser.add_argument("--render", action="store_true")
    args = parser.parse_args()
    if (
        min(args.width, args.height) < 1
        or args.width * args.height > 16384
        or not 1 <= args.repeats <= 21
    ):
        parser.error("bounded positive diagnostic dimensions/repeats required")
    args.output.mkdir(parents=True, exist_ok=True)
    load = os.getloadavg()
    wall, cpu = time.perf_counter(), time.process_time()
    vertices, faces, source = source_page(args.input)
    view, origins, directions = camera(vertices, args.width, args.height)
    preparation = [time.perf_counter() - wall, time.process_time() - cpu]
    outputs, records = measure(vertices, faces, origins, directions, args.repeats)
    projected, z = project(vertices, view)
    if not (z > 0.1).all():
        raise ValueError(
            "independent raster requires no near-crossing source triangles"
        )
    counters = {}
    wall, cpu = time.perf_counter(), time.process_time()
    depth = raster(projected, z, faces, args.width, args.height, counters=counters)
    raster_validation = [time.perf_counter() - wall, time.process_time() - cpu]
    result = outputs["scan"]
    np.testing.assert_array_equal(result["valid"].reshape(depth.shape), depth > 0)
    forward_z = result["ray_t"] * (directions @ view[1][2])
    inverse = np.divide(
        0.1, forward_z, out=np.zeros(len(origins)), where=result["valid"]
    ).reshape(depth.shape)
    np.testing.assert_allclose(inverse, depth, atol=1e-12, rtol=0)
    timer = []
    for _ in range(101):
        wall, cpu = time.perf_counter(), time.process_time()
        elapsed_cpu = time.process_time() - cpu
        timer.append([time.perf_counter() - wall, elapsed_cpu])
    report = {
        "width": args.width,
        "height": args.height,
        "rays": len(origins),
        "source_vertices": len(vertices),
        "source_triangles": len(faces),
        "source": source,
        "source_and_camera_preparation_wall_cpu_s": preparation,
        "methods": records,
        "raster_counters": counters,
        "raster_validation_wall_cpu_s": raster_validation,
        "raster_changed_coverage_pixels": 0,
        "raster_maximum_reversed_depth_difference": float(
            np.abs(inverse - depth).max()
        ),
        "hits": int(result["valid"].sum()),
        "skirt_hits": int((result["face_ids"] >= 2048).sum()),
        "start_load": load,
        "timer_pair_median_wall_cpu_s": np.median(timer, axis=0).tolist(),
        "process_peak_rss_bytes_before_export_plot": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * (1 if platform.system() == "Darwin" else 1024),
        "scope": "Same ideal Double fine triangles/rays and all outer skirts. Independent CPU raster, no near crossing. Source is one pinned cached virtual page; no geometry LOD selection, native material/normal/temporal/streaming/global hierarchy or GPU cost proof. Counts/selected copied bytes are not total allocations or hardware memory traffic.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    np.savez(
        args.output / "source.npz",
        vertices=vertices,
        faces=faces,
        origins=origins,
        directions=directions,
    )
    np.savez(args.output / "depth.npz", raster=depth, ray=inverse)
    for method, result in outputs.items():
        np.savez(
            args.output / (method + ".npz"),
            **{k: v for k, v in result.items() if isinstance(v, np.ndarray)},
        )
    if args.render:
        display(vertices, faces, view, depth, inverse, records, args.output)
    print(
        json.dumps({k: v for k, v in report.items() if k not in ("source", "methods")})
    )


if __name__ == "__main__":
    main()
