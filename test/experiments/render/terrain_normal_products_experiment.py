"""Compare prepared Float32 normal products, source reuse, bytes and repeated queries."""

import argparse
import gc
import json
import os
import platform
import resource
import time
import tracemalloc
from pathlib import Path

import numpy as np
from terrain_cell_traversal_experiment import FIXTURE
from terrain_grid_lod import page_frame, page_steps, read_page
from terrain_normal_products import bind, build, sample, sample_fine, source

METHODS = ("height_stencil", "differences", "rgba_normals")


def inputs(width, height):
    record = json.loads((FIXTURE / "input.json").read_text())
    row = record["row"]
    h, _ = read_page(row, FIXTURE)
    src = source(
        h,
        page_steps(page_frame(row)[2], 33),
        binding=(
            record["asset_key"],
            int(row["page"]),
            row["zoom"],
            row["x"],
            row["y"],
        ),
    )
    if src.digest != record["height_sha256"]:
        raise ValueError("changed fixed source height words")
    y, x = np.indices((33, 33))
    nodes = np.column_stack((y.ravel(), x.ravel()))
    u, v = np.meshgrid(
        np.linspace(0, 1, width, dtype=np.float32),
        np.linspace(0, 1, height, dtype=np.float32),
    )
    return src, nodes, np.column_stack((u.ravel(), v.ravel())), record


def setup(src):
    lookups, reports = {}, {}
    for method in METHODS:
        wall, cpu = time.perf_counter(), time.process_time()
        product = None if method == "height_stencil" else build(src, method)
        bound = bind(src, product)
        record = {
            "build_and_bind_wall_cpu_s": [
                time.perf_counter() - wall,
                time.process_time() - cpu,
            ],
            "retained_field_bytes": 0
            if product is None
            else sum(f.nbytes for f in product.fields),
            "retained_basis_bytes": bound.basis.nbytes,
        }
        gc.collect()
        tracemalloc.start()
        probe = bind(src, None if method == "height_stencil" else build(src, method))
        _, record["build_traced_peak_bytes"] = tracemalloc.get_traced_memory()
        record["build_traced_live_blocks"] = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
        del probe
        lookups[method], reports[method] = bound, record
    return lookups, reports


def measure(lookups, queries, kernel, repeats):
    timing = {m: [] for m in METHODS}
    for i in range(repeats + 1):
        for method in METHODS[i % 3 :] + METHODS[: i % 3]:
            wall, cpu = time.perf_counter(), time.process_time()
            result = kernel(lookups[method], queries)
            elapsed_cpu = time.process_time() - cpu
            timing[method].append([time.perf_counter() - wall, elapsed_cpu])
            del result
    outputs, reports = {}, {}
    for method in METHODS:
        gc.collect()
        tracemalloc.start()
        result = kernel(lookups[method], queries)
        _, peak = tracemalloc.get_traced_memory()
        blocks = sum(
            s.count for s in tracemalloc.take_snapshot().statistics("filename")
        )
        tracemalloc.stop()
        if outputs:
            np.testing.assert_array_equal(
                result["normals"].view(np.uint32),
                outputs["height_stencil"]["normals"].view(np.uint32),
            )
        outputs[method] = result
        reports[method] = {
            "first_wall_cpu_s": timing[method][0],
            "following_wall_cpu_s": timing[method][1:],
            "median_wall_cpu_s": np.median(timing[method][1:], axis=0).tolist(),
            "query_traced_peak_bytes": peak,
            "query_traced_live_blocks": blocks,
            "work": result["work"],
        }
    return outputs, reports


def unique_words(nodes):
    unique = np.unique(nodes, axis=0)
    r, c = unique[:, 0] + 1, unique[:, 1] + 1
    neighbours = np.stack(
        ((r * 35 + c + 1), (r * 35 + c - 1), ((r - 1) * 35 + c), ((r + 1) * 35 + c))
    )
    normal_source = np.unique(neighbours)
    combined = np.union1d(normal_source, r * 35 + c)
    n = len(unique)
    return {
        "distinct_normal_nodes": n,
        "height_stencil_normal_only_words": len(normal_source),
        "combined_normal_and_position_or_receiver": {
            "height_stencil": {
                "different_texture_words": len(combined),
                "different_texture_bytes": len(combined) * 4,
            },
            "differences": {
                "different_texture_words": 3 * n,
                "different_texture_bytes": 12 * n,
            },
            "rgba_normals": {
                "different_texture_words": 5 * n,
                "different_texture_bytes": 20 * n,
            },
        },
        "scope": "Unique logical addresses across the complete query batch, not hardware cache misses, transfer bytes or a guarantee of a fitting cache.",
    }


def fine_nodes(uv):
    at = uv * np.float32(32)
    cell = np.minimum(np.floor(at).astype(np.int32), 31)
    f = at - cell.astype(np.float32)
    first = cell[:, ::-1]
    middle = first + np.where((f[:, 0] >= f[:, 1])[:, None], [0, 1], [1, 0])
    return np.stack((first, middle, first + 1), axis=1).reshape(-1, 2)


def display(src, outputs, width, height, count, output):
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    figure, axes = plt.subplots(2, 3, figsize=(13, 7))
    height_map = axes[0, 0].imshow(src.heights[1:-1, 1:-1], cmap="terrain")
    figure.colorbar(height_map, ax=axes[0, 0])
    axes[0, 0].set_title("Same cached source heights / m")
    for slot, method in (
        (1, "height_stencil"),
        (2, "differences"),
        (4, "rgba_normals"),
    ):
        normal = outputs[method]["normals"].reshape(height, width, 3)
        axes.ravel()[slot].imshow(np.clip(normal * 0.5 + 0.5, 0, 1))
        axes.ravel()[slot].set_title(method + " / same Float32 response")
    delta = outputs["rgba_normals"]["normals"] - outputs["height_stencil"]["normals"]
    error = axes[1, 0].imshow(
        np.max(np.abs(delta), axis=1).reshape(height, width),
        vmin=0,
        vmax=1e-7,
        cmap="magma",
    )
    figure.colorbar(error, ax=axes[1, 0])
    axes[1, 0].set_title("Maximum normal component error / exactly0")
    combined = count["combined_normal_and_position_or_receiver"]
    axes[1, 2].bar(METHODS, [combined[m]["different_texture_bytes"] for m in METHODS])
    axes[1, 2].tick_params(axis="x", labelrotation=15)
    axes[1, 2].set_title("Different logical texture bytes / full query set")
    figure.suptitle(
        "Normal-field diagnostic; exact CPU model, no native PBR/device admission"
    )
    figure.tight_layout()
    figure.savefig(output / "terrain-normal-products.png", dpi=150)
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=256)
    parser.add_argument("--height", type=int, default=256)
    parser.add_argument("--repeats", type=int, default=20)
    parser.add_argument("--render", action="store_true")
    args = parser.parse_args()
    if (
        min(args.width, args.height) < 2
        or args.width * args.height > 65536
        or not 1 <= args.repeats <= 50
    ):
        parser.error("bounded positive normal chart dimensions/repeats required")
    args.output.mkdir(parents=True, exist_ok=True)
    load = os.getloadavg()
    wall, cpu = time.perf_counter(), time.process_time()
    src, nodes, uv, record = inputs(args.width, args.height)
    source_preparation = [time.perf_counter() - wall, time.process_time() - cpu]
    lookups, construction = setup(src)
    node_outputs, node_reports = measure(lookups, nodes, sample, args.repeats)
    fine_outputs, fine_reports = measure(lookups, uv, sample_fine, args.repeats)
    unique = {"nodes": unique_words(nodes), "fine": unique_words(fine_nodes(uv))}
    timer = []
    for _ in range(101):
        wall, cpu = time.perf_counter(), time.process_time()
        elapsed_cpu = time.process_time() - cpu
        timer.append([time.perf_counter() - wall, elapsed_cpu])
    report = {
        "width": args.width,
        "height": args.height,
        "source": record,
        "source_snapshot_bytes": src.heights.nbytes + src.step_m.nbytes,
        "source_digest": src.digest,
        "effective_step_m": src.step_m.tolist(),
        "source_preparation_wall_cpu_s": source_preparation,
        "construction": construction,
        "nodes": node_reports,
        "fine": fine_reports,
        "unique_words": unique,
        "start_load": load,
        "timer_pair_median_wall_cpu_s": np.median(timer, axis=0).tolist(),
        "process_peak_rss_bytes_before_export_plot": resource.getrusage(
            resource.RUSAGE_SELF
        ).ru_maxrss
        * (1 if platform.system() == "Darwin" else 1024),
        "scope": "Prepared reuse, not a native cache-hit/offline-process proof. Exact Float32 CPU operations and fixed current fine interpolation; no GPU fast-math/shader equivalence. R32 plane pairs/RGBA32 strides count all words; unique addresses do not measure hardware cache traffic. Original heights remain needed; extra field storage is additional. RSS includes all compared fields/outputs/diagnostics.",
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    np.savez(
        args.output / "source.npz",
        heights=src.heights,
        step_m=src.step_m,
        nodes=nodes,
        uv=uv,
    )
    for method in METHODS:
        np.savez(
            args.output / (method + ".npz"),
            nodes=node_outputs[method]["normals"],
            fine=fine_outputs[method]["normals"],
        )
    if args.render:
        display(src, fine_outputs, args.width, args.height, unique["fine"], args.output)
    print(
        json.dumps(
            {
                k: report[k]
                for k in (
                    "source_snapshot_bytes",
                    "effective_step_m",
                    "unique_words",
                    "process_peak_rss_bytes_before_export_plot",
                )
            }
        )
    )


if __name__ == "__main__":
    main()
