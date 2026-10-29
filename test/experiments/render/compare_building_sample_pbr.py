"""Capture-calibrated GGX view control through unchanged source/pixel integrals."""

import argparse
import json
import os
import platform
import subprocess
import time
from pathlib import Path

import numpy as np
from building_sample_inputs import digest, readbacks
from building_sample_pbr import energy_tables, point_responses
from building_sample_source import read_source
from compare_building_sample_prefix import integrated_image
from compare_building_sample_reuse import DATA, aligned_blocks, errors
from field_view_integrals import summed_field
from process_memory import peak_rss_bytes


def analyse(source, high, tables):
    manifest, arrays, grid, view = source
    if manifest["query"]["sun_elevation_deg"] != 45:
        raise ValueError("Same native45-degree source light required")
    fields, work = point_responses(arrays, grid, view, tables)
    fields["native"] = arrays["source_lit_rgba"]
    pictures, sampling = {}, {}
    for name, field in fields.items():
        picture, cost = integrated_image(
            summed_field(field, dtype=np.float64), grid, view
        )
        pictures[name], sampling[name] = picture, cost
    direct = aligned_blocks(fields["capture"], grid, view)
    if direct is None:
        raise ValueError("Independent disjoint aligned block means required")
    max_delta = float(abs(direct[0] - pictures["capture"]).max())
    if max_delta > 1e-10:
        raise ValueError("Existing integral differs from independent source means")
    source_delta = float(
        abs(fields["capture"][:, :, :3] - fields["perspective"][:, :, :3]).sum()
    )
    image_delta = float(
        abs(pictures["capture"][:, :, :3] - pictures["perspective"][:, :, :3]).sum()
    )
    original = high[1]["case0_original_ss4096"]
    reference_energy = float(original[:, :, :3].astype(float).sum())
    bound = source_delta / direct[1] ** 2
    if image_delta > bound + 1e-8:
        raise ValueError("Disjoint positive pixel integration increased L1 mass")
    pictures["original"] = original
    report = {
        "query": manifest["query"],
        "work": work,
        "sampling": sampling,
        "source_native_vs_capture_model": errors(fields["native"], fields["capture"]),
        "source_capture_vs_perspective_model": errors(
            fields["capture"], fields["perspective"]
        ),
        "pixel_native_vs_capture_model": errors(
            pictures["native"], pictures["capture"]
        ),
        "pixel_capture_vs_perspective_model": errors(
            pictures["capture"], pictures["perspective"]
        ),
        "original_vs_model_capture": errors(original, pictures["capture"]),
        "original_vs_model_perspective": errors(original, pictures["perspective"]),
        "original_vs_native_held_prefix": errors(original, pictures["native"]),
        "native_pixel_integral_vs_native_linear": errors(
            arrays["prefix_linear"], pictures["native"]
        ),
        "direct_source_block_axis": direct[1],
        "direct_block_maximum_rgba_error": max_delta,
        "model_view_response_original_relative_l1_bound": bound / reference_energy,
        "model_view_response_original_relative_l1_actual": image_delta
        / reference_energy,
        "model_view_response_alpha_maximum_delta": float(
            abs(pictures["capture"][:, :, 3] - pictures["perspective"][:, :, 3]).max()
        ),
        "scope": "All identical stored source points/materials/coverage retained. Existing GGX capture positive control and view-only model counterfactual, followed by existing Float64 pixel integration. L1 contraction is for these disjoint aligned source means only. Capture agreement does not certify native perspective PBR, continuous pose, original visibility, model error away from capture, reference convergence or World/device admission.",
    }
    return report, pictures, fields


def run(args):
    if args.output.exists() or not args.output.resolve().is_relative_to(Path("/tmp")):
        raise ValueError("Fresh private system-temp output required")
    tracked = [
        Path(__file__),
        *(
            Path(__file__).with_name(n)
            for n in (
                "building_sample_pbr.py",
                "building_sample_source.py",
                "building_sample_inputs.py",
                "compare_building_sample_prefix.py",
                "compare_building_sample_reuse.py",
                "replay_building_radiance.py",
                "material_filter.py",
                "field_view_sampling.py",
                "field_view_integrals.py",
                "process_memory.py",
            )
        ),
    ]
    if subprocess.check_output(
        ["git", "status", "--porcelain", "--", *map(str, tracked)]
    ):
        raise ValueError("Freeze the actual model control and existing kernels first")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    start, cpu, load = time.perf_counter(), time.process_time(), os.getloadavg()
    source = read_source(args.inputs / "source-512", args.inputs / "high-sampling")
    high = readbacks(args.inputs / "high-sampling")
    tables, reference = energy_tables(args.inputs.parent / "material-filter")
    decoded_wall, decoded_cpu = time.perf_counter() - start, time.process_time() - cpu
    report, images, fields = analyse(source, high, tables)
    report.update(
        commit=commit,
        profile=[1280, 720],
        table_reference=reference,
        input_manifests={
            str(p): digest(p / "manifest.json")
            for p in (args.inputs / "source-512", args.inputs / "high-sampling")
        },
        cost={
            "verify_decode_wall_s": decoded_wall,
            "verify_decode_cpu_s": decoded_cpu,
            "model_and_integrals_wall_s": time.perf_counter() - start - decoded_wall,
            "model_and_integrals_cpu_s": time.process_time() - cpu - decoded_cpu,
            "prefix_constructions": 3,
            "prefix_bytes_each": (513 * 513 * 4 * 8),
            "held_field_array_bytes": sum(v.nbytes for v in fields.values()),
            "held_pixel_array_bytes": sum(v.nbytes for v in images.values()),
            "known_copied_byte_subset": {
                "masked_albedo_normal_roughness_channels": report["work"][
                    "source_points"
                ]
                * 7
                * 4,
                "depth_float64_cast": report["work"]["source_points"] * 8,
                "independent_block_mean_input_cast": fields["capture"].nbytes,
                "three_immutable_prefix_table_copies": 3 * 513 * 513 * 4 * 8,
                "scope": "Explicit payload-copy subset from masked source channels, Float64 casts and existing immutable-prefix tobytes. Excludes other temporaries/assignments/IO/compression; not total memory traffic or allocation bytes.",
            },
            "peak_process_rss_bytes": peak_rss_bytes(),
            "nice": os.getpriority(os.PRIO_PROCESS, 0),
            "loadavg_before": load,
            "platform": platform.platform(),
            "python": platform.python_version(),
            "numpy": np.__version__,
            "scope": "One diagnostic run. Model/PBR work and three reused prefix builds are not native/device times; allocations/total copies unmeasured. Held bytes are not added to OS peak.",
        },
    )
    args.output.mkdir(parents=True)
    np.savez_compressed(args.output / "pixel-controls.npz", **images)
    np.savez_compressed(args.output / "source-controls.npz", **fields)
    report["output_sha256"] = {p.name: digest(p) for p in args.output.glob("*.npz")}
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps({"commit": commit, "output": str(args.output)}))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, default=DATA)
    parser.add_argument("--output", type=Path, required=True)
    run(parser.parse_args())
