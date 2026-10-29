"""Explain finite native reuse errors with the existing held point/prefix controls."""

import argparse
import json
import os
import resource
import subprocess
import time
from pathlib import Path

import numpy as np
from building_sample_inputs import capture_fields, digest, readbacks, same_sources
from field_view_integrals import summed_field
from field_view_projection import CapturePlane, displacement_bound
from field_view_sampling import ImageView, reconstruct
from replay_building_radiance import compare

ROOT = Path(__file__).resolve().parents[3]
DATA = ROOT / "test/outshine/data/render/building-depth-samples"


def errors(original, candidate):
    return compare(original.reshape(-1, 4).astype(float), candidate.reshape(-1, 4))


def crop_view(manifest, query):
    low, high = np.asarray(manifest["crop"]).reshape(2, 2)
    if not np.array_equal(low + high, manifest["raster"]):
        raise ValueError(
            "This control requires a centred native crop with unchanged rays"
        )
    return ImageView(
        tuple(query["eye"]),
        tuple(map(tuple, query["basis"])),
        query["focal_px"],
        *map(int, high - low),
    )


def occupied_support(field, grid, view):
    y, x = np.nonzero(field[:, :, 3])
    if not len(x):
        raise ValueError("Nonempty original source field required")
    uv = np.array(
        [
            [x.min(), y.min()],
            [x.max() + 1, y.min()],
            [x.max() + 1, y.max() + 1],
            [x.min(), y.max() + 1],
        ],
        float,
    )
    uv = uv / field.shape[0] * 2 - 1
    axes = np.asarray(grid.axes())
    world = np.asarray(grid.centre) + grid.half_extent_m * (
        uv[:, :1] * axes[0] - uv[:, 1:] * axes[1]
    )
    camera = (world - view.eye) @ np.asarray(view.basis).T
    if np.any(camera[:, 2] <= 0):
        raise ValueError("Occupied source support crosses the eye")
    projected = np.array([view.width, view.height]) / 2 + (
        camera[:, :2] * [1, -1] * view.focal_px / camera[:, 2:]
    )
    bounds = np.array([projected.min(0), projected.max(0)])
    if np.any(bounds[0] < 0) or np.any(bounds[1] > [view.width, view.height]):
        raise ValueError("Native crop would omit occupied candidate support")
    return bounds.tolist()


def aligned_blocks(source, grid, view):
    relative = (np.asarray(grid.centre) - view.eye) @ np.asarray(view.basis).T
    side = 2 * grid.half_extent_m * view.focal_px / relative[2]
    size = round(side)
    if (
        not np.allclose(np.asarray(view.basis)[:2], grid.axes(), rtol=0, atol=1e-12)
        or not np.allclose(relative[:2], 0, rtol=0, atol=1e-9)
        or not 1 <= size <= source.shape[0]
        or abs(side - size) > 1e-9
        or source.shape[0] % size
        or (view.width - size) % 2
        or (view.height - size) % 2
    ):
        return None
    stride = source.shape[0] // size
    direct = source.astype(float).reshape(size, stride, size, stride, 4).mean((1, 3))
    result = np.zeros((view.height, view.width, 4), float)
    x, y = (view.width - size) // 2, (view.height - size) // 2
    low, high = (
        np.maximum([x, y], 0),
        np.minimum([x + size, y + size], [view.width, view.height]),
    )
    result[low[1] : high[1], low[0] : high[0]] = direct[
        low[1] - y : high[1] - y, low[0] - x : high[0] - x
    ]
    return result, stride


def case_result(manifest, arrays, index, grid, source, prefix):
    case = manifest["cases"][index]
    query = case["query"]
    axis = query["filter_axis_samples"]
    view = crop_view(manifest, query)
    plane = CapturePlane(
        tuple(manifest["bounds"]["least"]),
        tuple(manifest["bounds"]["most"]),
        grid.centre,
        grid.toward_eye,
    )
    bounds = occupied_support(source, grid, view)
    sampled = reconstruct(prefix, grid, view, approximate=True)
    candidate = np.zeros((view.height, view.width, 4), float)
    x, y = sampled["origin"]
    height, width = sampled["rgba"].shape[:2]
    candidate[y : y + height, x : x + width] = sampled.pop("rgba")
    original = arrays[f"case{index}_original_ss{axis**2}"]
    native = arrays[f"case{index}_samples_ss{axis**2}"]
    direct = aligned_blocks(source, grid, view)
    control = {"available": direct is not None}
    if direct is not None:
        control.update(
            source_points_per_pixel_axis=direct[1],
            maximum_rgba_absolute_error=float(abs(candidate - direct[0]).max()),
        )
        if control["maximum_rgba_absolute_error"] > 1e-10:
            raise ValueError(
                "Existing prefix differs from direct same-source block means"
            )
    return {
        "query": query,
        "declared_whole_source_flattening_bound": displacement_bound(
            plane, view.eye, view.basis, view.focal_px
        ),
        "native_original_vs_stored_samples": errors(original, native),
        "native_original_vs_held_radiance_prefix": errors(original, candidate),
        "held_prefix_vs_native_samples": errors(native, candidate),
        "prefix_vs_native_samples_maximum_coverage_absolute_error": float(
            abs(candidate[:, :, 3] - native[:, :, 3]).max()
        ),
        "direct_same_source_blocks": control,
        "occupied_candidate_bounds_in_crop": bounds,
        "sampling": sampled,
    }, candidate


def analyse(primary, canonical, high):
    manifest, arrays = primary
    same_sources(manifest, canonical[0])
    same_sources(manifest, high[0])
    grid, fields = capture_fields(*canonical)
    source = fields["samples"]
    prefix = summed_field(source, dtype=np.float64)
    rows, pictures = (
        [],
        {"canonical_original": fields["original"], "canonical_samples": source},
    )
    for name, (data, values) in (("primary", primary), ("high", high)):
        for index, case in enumerate(data["cases"]):
            row, candidate = case_result(data, values, index, grid, source, prefix)
            rows.append(dict(row, reference_set=name))
            pictures[f"{name}_case{index}_prefix"] = candidate
    high_query = high[0]["cases"][0]["query"]
    matching = [
        i
        for i, case in enumerate(manifest["cases"])
        if {k: v for k, v in case["query"].items() if k != "filter_axis_samples"}
        == {k: v for k, v in high_query.items() if k != "filter_axis_samples"}
    ]
    if len(high[0]["cases"]) != 1 or len(matching) != 1:
        raise ValueError("One unchanged-query higher-sample reference required")
    low_axis = manifest["cases"][matching[0]]["query"]["filter_axis_samples"]
    if high_query["filter_axis_samples"] <= low_axis:
        raise ValueError("The higher reference must actually use more samples")
    original_low = arrays[f"case{matching[0]}_original_ss{low_axis**2}"]
    original_high = high[1][
        f"case0_original_ss{high_query['filter_axis_samples'] ** 2}"
    ]
    return {
        "capture_grid": {
            "centre_m": grid.centre,
            "toward_eye": grid.toward_eye,
            "half_extent_m": grid.half_extent_m,
        },
        "source_pixels": source.shape[0],
        "held_source_bytes": source.nbytes,
        "held_prefix_bytes": prefix.table.nbytes,
        "prefix_dtype": str(prefix.table.dtype),
        "canonical_original_vs_stored_sample_points": errors(
            fields["original"], source
        ),
        "canonical_coverage_mask_mismatches": int(
            np.count_nonzero(fields["original"][:, :, 3] != source[:, :, 3])
        ),
        "original_low_vs_high_reference": errors(original_high, original_low),
        "reference_sample_axes": [low_axis, high_query["filter_axis_samples"]],
        "rows": rows,
    }, pictures


def run(args):
    if args.output.exists() or not args.output.resolve().is_relative_to(
        Path("/tmp").resolve()
    ):
        raise ValueError("Fresh explicit private system-temp output required")
    tracked = [Path(__file__), Path(__file__).with_name("building_sample_inputs.py")]
    tracked.extend(
        Path(__file__).with_name(f"field_view_{name}.py")
        for name in ("integrals", "sampling")
    )
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, tracked)]
    ):
        raise ValueError("Freeze the actual analysis and reused kernels first")
    inputs = [args.inputs, args.inputs / "same-rays", args.inputs / "high-sampling"]
    began, cpu, load = time.perf_counter(), time.process_time(), os.getloadavg()
    loaded = tuple(readbacks(path) for path in inputs)
    read_wall, read_cpu = time.perf_counter() - began, time.process_time() - cpu
    result, images = analyse(*loaded)
    result["analysis_cost"] = {
        "native_decode_and_full_buffer_verification_wall_s": read_wall,
        "native_decode_and_full_buffer_verification_cpu_s": read_cpu,
        "prefix_and_comparisons_wall_s": time.perf_counter() - began - read_wall,
        "prefix_and_comparisons_cpu_s": time.process_time() - cpu - read_cpu,
        "decoded_native_array_bytes": sum(
            a.nbytes for _, arrays in loaded for a in arrays.values()
        ),
        "full_frame_checksum_bytes": sum(
            2 * 13 * 4 * 1280 * 720 * len(m["cases"]) for m, _ in loaded
        ),
        "prefix_constructions": 1,
        "source_texels": result["source_pixels"] ** 2,
        "logical_prefix_evaluations": sum(
            r["sampling"]["logical_prefix_evaluations"] for r in result["rows"]
        ),
        "logical_prefix_texel_fetches": sum(
            r["sampling"]["logical_texel_fetches"] for r in result["rows"]
        ),
        "process_peak_rss_bytes": resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
        * 1024,
        "nice": os.getpriority(os.PRIO_PROCESS, 0),
        "loadavg_before": load,
        "scope": "Single CPU diagnostic run, immutable inputs and full-frame verification included as named. No allocation-event/total-copy measurement, alternative speedup or native/device/frame/UMA claim. Retained source and prefix bytes reported separately and not added to process RSS.",
    }
    result.update(
        commit=subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip(),
        profile=[1280, 720],
        input_manifests={str(p): digest(p / "manifest.json") for p in inputs},
        input_npz={str(p): digest(p / "captures.npz") for p in inputs},
        scope="Existing Float64 integration of canonical native point radiance, held without query relighting. Centred crops preserve exact full1280x720 native rays/focal and contain all occupied candidate support. Same-source arithmetic controls do not certify continuous source content, visibility, query PBR, native prefix equality, a pose domain or World/device/p99 admission. Original-source ray diagnostics require all actual native geometry/Frame hashes and remain separate.",
    )
    args.output.mkdir(parents=True)
    np.savez_compressed(args.output / "prefix-controls.npz", **images)
    result["prefix_controls_sha256"] = digest(args.output / "prefix-controls.npz")
    (args.output / "report.json").write_text(json.dumps(result, indent=2) + "\n")
    print(
        json.dumps(
            {
                "commit": result["commit"],
                "cases": len(result["rows"]),
                "output": str(args.output),
            }
        )
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, default=DATA)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
