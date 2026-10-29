"""Distinguish native pixel integration from filtering that integration again."""

import argparse
import json
import os
import platform
import subprocess
import time
from pathlib import Path

import numpy as np
from building_sample_inputs import digest, readbacks
from building_sample_source import read_source
from compare_building_sample_reuse import DATA, aligned_blocks, errors
from field_view_integrals import summed_field
from field_view_sampling import ImageView, reconstruct
from process_memory import peak_rss_bytes


def jittered_view(view, dx, dy):
    """The native Filtered helper rotates an orthonormal camera, not its viewport."""
    right, up, forward = np.asarray(view.basis)
    forward = forward + right * (dx / view.focal_px) + up * (dy / view.focal_px)
    forward /= np.linalg.norm(forward)
    right = np.cross(forward, up)
    right /= np.linalg.norm(right)
    up = np.cross(right, forward)
    return ImageView(
        view.eye,
        tuple(map(tuple, (right, up, forward))),
        view.focal_px,
        view.width,
        view.height,
    )


def integrated_image(field, grid, view):
    sampled = reconstruct(field, grid, view, approximate=True)
    candidate = np.zeros((view.height, view.width, 4), float)
    x, y = sampled["origin"]
    h, w = sampled["rgba"].shape[:2]
    candidate[y : y + h, x : x + w] = sampled.pop("rgba")
    return candidate, sampled


def filtered_image(field, grid, view, axis):
    if type(axis) is not int or not 1 <= axis <= 64:
        raise ValueError("Explicit bounded native jitter axis1..64 required")
    candidate = np.zeros((view.height, view.width, 4), float)
    visits, excess = 0, 0.0
    for dy in (np.arange(axis) + 0.5) / axis - 0.5:
        for dx in (np.arange(axis) + 0.5) / axis - 0.5:
            picture, sampled = integrated_image(
                field, grid, jittered_view(view, dx, dy)
            )
            candidate += picture / axis**2
            visits += sampled["output_pixels"]
            excess = max(excess, sampled["maximum_rectangle_excess_ratio"])
    return candidate, {
        "jitter_axis": axis,
        "output_pixel_visits": visits,
        "logical_prefix_evaluations": 4 * visits,
        "logical_texel_fetches": 16 * visits,
        "maximum_rectangle_excess_ratio": excess,
    }


def analyse(source, high, axes):
    manifest, arrays, grid, view = source
    _, original_arrays = high
    field = summed_field(arrays["source_lit_rgba"], dtype=np.float64)
    single, sampling = integrated_image(field, grid, view)
    blocks = aligned_blocks(arrays["source_lit_rgba"], grid, view)
    if blocks is None:
        raise ValueError("This native opponent requires independent aligned means")
    original = original_arrays["case0_original_ss4096"]
    images = {
        "original_ss64": original,
        "native_prefix_single": arrays["prefix_linear"],
        "native_prefix_ss64": arrays["prefix_ss4096"],
        "python_prefix_single": single,
    }
    rows = []
    for axis in axes:
        picture, cost = filtered_image(field, grid, view, axis)
        images[f"python_prefix_jitter{axis}"] = picture
        rows.append(
            dict(
                cost,
                native_ss64_vs_python_double_filtered=errors(
                    picture, arrays["prefix_ss4096"]
                ),
                maximum_rgba_error=abs(picture - arrays["prefix_ss4096"])
                .max((0, 1))
                .tolist(),
                original_vs_python_double_filtered=errors(original, picture),
            )
        )
    return {
        "source_capture": manifest["capture"],
        "query": manifest["query"],
        "source_texels": arrays["source_lit_rgba"].shape[0] ** 2,
        "held_source_bytes": arrays["source_lit_rgba"].nbytes,
        "held_prefix_bytes": field.table.nbytes,
        "single_sampling": sampling,
        "independent_source_points_per_pixel_axis": blocks[1],
        "single_vs_independent_block_mean_maximum_rgba": float(
            abs(single - blocks[0]).max()
        ),
        "native_single_vs_python_single": errors(single, arrays["prefix_linear"]),
        "original_vs_native_single": errors(original, arrays["prefix_linear"]),
        "original_vs_native_ss64": errors(original, arrays["prefix_ss4096"]),
        "original_vs_python_single": errors(original, single),
        "double_filter_controls": rows,
    }, images


def run(args):
    if args.output.exists() or not args.output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("Fresh private system-temp output required")
    files = [
        Path(__file__),
        *(
            Path(__file__).with_name(n)
            for n in (
                "building_sample_source.py",
                "building_sample_inputs.py",
                "compare_building_sample_reuse.py",
                "field_view_sampling.py",
                "field_view_integrals.py",
                "process_memory.py",
            )
        ),
    ]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, files)]
    ):
        raise ValueError("Freeze the actual study and existing kernels first")
    began, cpu, load = time.perf_counter(), time.process_time(), os.getloadavg()
    source = read_source(args.inputs / "source-512", args.inputs / "high-sampling")
    high = readbacks(args.inputs / "high-sampling")
    decoded_wall, decoded_cpu = time.perf_counter() - began, time.process_time() - cpu
    report, images = analyse(source, high, args.axes)
    report["cost"] = {
        "decode_and_verify_wall_s": decoded_wall,
        "decode_and_verify_cpu_s": decoded_cpu,
        "comparison_wall_s": time.perf_counter() - began - decoded_wall,
        "comparison_cpu_s": time.process_time() - cpu - decoded_cpu,
        "decoded_array_bytes": sum(a.nbytes for a in source[1].values())
        + sum(a.nbytes for a in high[1].values()),
        "held_output_array_bytes": sum(a.nbytes for a in images.values()),
        "peak_process_rss_bytes": peak_rss_bytes(),
        "nice": os.getpriority(os.PRIO_PROCESS, 0),
        "loadavg_before": load,
        "platform": platform.platform(),
            "python": platform.python_version(),
        "numpy": np.__version__,
        "scope": "Single diagnostic run. Named decode/verification and comparison costs; no allocation-event or total-copy measurement, native speedup or device admission. Held bytes are not added to RSS.",
    }
    report.update(
        commit=subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        profile=[1280, 720],
        input_manifests={
            str(p): digest(p / "manifest.json")
            for p in (
                args.inputs / "source-512",
                args.inputs / "high-sampling",
                args.inputs / "original-geometry",
            )
        },
        scope="Identical capture-lit512 source and full1280x720 rays. Native Filtered camera rotations applied to the existing whole-pixel prefix integral again. Float64 arithmetic/derivative approximation control, not new source visibility or perspective BRDF. OriginalSS64 remains a finite reference; no World/SSE/p99/UMA admission.",
    )
    args.output.mkdir(parents=True)
    np.savez_compressed(args.output / "prefix-filter-controls.npz", **images)
    report["controls_sha256"] = digest(args.output / "prefix-filter-controls.npz")
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"commit": report["commit"], "output": str(args.output)}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, default=DATA)
    parser.add_argument("--axes", type=int, nargs="+", default=[4, 16, 64])
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
