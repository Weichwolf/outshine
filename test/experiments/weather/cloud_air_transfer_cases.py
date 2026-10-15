"""Low-priority preparation and independent camera-transfer witnesses (#353)."""

import argparse
import json
import struct
from pathlib import Path

import numpy as np
from cloud_air_reference import RAY, SAMPLE
from cloud_air_source_reference import medium_parameters, reference_ray
from cloud_air_transfer_reuse import (
    bilinear,
    clear_response,
    errors,
    grid_indices,
    numeric,
    weighted_visibility,
)
from cloud_sun_reuse_benchmark import image_rays
from cloud_sun_reuse_cases import write_inputs


def prepare(output, width, height, steps):
    write_inputs(output)
    rays, queries = image_rays(width, height)
    rays.tofile(output / "rays.bin")
    queries.tofile(output / "queries.bin")
    (output / "configuration.json").write_text(
        json.dumps({"width": width, "height": height, "steps": steps}, indent=2)
    )
    for step in steps:
        folder = output / f"step-{step}"
        folder.mkdir(exist_ok=True)
        indices, shape = grid_indices(width, height, step)
        subset = rays[indices].copy()
        subset["begin"] = np.arange(len(indices), dtype="u4")
        subset["end"] = subset["begin"] + 1
        subset.tofile(folder / "rays.bin")
        queries[indices].tofile(folder / "queries.bin")
        (folder / "transfer_grid.glsl").write_text(
            f"const ivec2 transferImageExtent = ivec2({width}, {height});\n"
            f"const int transferStep = {step};\n"
        )
        (folder / "grid.json").write_text(
            json.dumps({"step": step, "shape": shape, "rays": len(indices)})
        )


def field(output, step):
    folder = output / f"step-{step}"
    rays = np.fromfile(folder / "rays.bin", RAY)
    queries = np.fromfile(folder / "queries.bin", "<f4")
    samples = np.fromfile(folder / "producer.bin", SAMPLE)
    shape = json.loads((folder / "grid.json").read_text())["shape"]
    visibility = weighted_visibility(samples, rays, queries, medium_parameters())
    if not np.isfinite(visibility).all() or visibility.min() < -1e-3:
        raise RuntimeError("weighted visibility must be finite and nonnegative")
    values = np.ones((*shape, 4), dtype="<f4")
    values[..., :3] = visibility.reshape(*shape, 3)
    (folder / "visibility.bin").write_bytes(
        struct.pack("<2I", shape[1], shape[0]) + values.tobytes()
    )


def witness(rays, expected, actual, indices, eye_cover):
    reports = []
    medium = medium_parameters()
    for index in np.unique(indices):
        original = reference_ray(
            rays[index], np.array([10.0]), medium, 0.65, 16, eye_cover=eye_cover
        )[0]
        refined = reference_ray(
            rays[index], np.array([10.0]), medium, 0.65, 32, eye_cover=eye_cover
        )[0]
        difference = float(np.max(abs(original - refined)))
        if difference > 1e-8:
            raise RuntimeError("the independent image witness is not converged")
        reports.append(
            {
                "index": int(index),
                "refinement_abs": difference,
                "reference": refined.tolist(),
                "direct_relative_RGB": float(
                    np.max(
                        abs(expected[index, :3] - refined[:3])
                        / np.maximum(abs(refined[:3]), 1e-5)
                    )
                ),
                "reused_relative_RGB": float(
                    np.max(
                        abs(actual[index, :3] - refined[:3])
                        / np.maximum(abs(refined[:3]), 1e-5)
                    )
                ),
                "reused_T_abs": float(np.max(abs(actual[index, 3:] - refined[3:]))),
            }
        )
    return reports


def analyze(output):
    config = json.loads((output / "configuration.json").read_text())
    width, height = config["width"], config["height"]
    rays = np.fromfile(output / "rays.bin", RAY)
    queries = np.fromfile(output / "queries.bin", "<f4")
    joint = numeric(np.fromfile(output / "joint.bin", SAMPLE))
    air = numeric(np.fromfile(output / "air.bin", SAMPLE))
    integral, analytic_through, direct, diffuse = clear_response(
        rays, queries, medium_parameters()
    )
    report = {
        "scope": "Single-depth homogeneous-air component; no runtime/world profile",
        "steps": [],
    }
    for step in config["steps"]:
        folder = output / f"step-{step}"
        indices, shape = grid_indices(width, height, step)
        coarse = np.fromfile(folder / "producer.bin", SAMPLE)
        actual = numeric(np.fromfile(folder / "reconstructed.bin", SAMPLE))
        if not np.isfinite(actual).all() or np.any(actual < 0):
            raise RuntimeError("reconstructed transfer must be finite and nonnegative")
        if np.any(coarse["sources"] > 531):
            raise RuntimeError("producer exceeded the declared physical source bound")
        plain_joint = bilinear(
            joint[indices].reshape(*shape, 6), width, height, step
        ).reshape(-1, 6)
        plain_air = bilinear(
            air[indices].reshape(*shape, 6), width, height, step
        ).reshape(-1, 6)
        visibility = np.frombuffer(
            (folder / "visibility.bin").read_bytes(), "<f4", offset=8
        ).reshape(*shape, 4)
        interpolated = bilinear(visibility[..., :3], width, height, step).reshape(-1, 3)
        expected_rgb = integral * (direct * interpolated + diffuse)
        metrics = errors(actual, air)
        analytic_t_error = np.max(abs(actual[:, 3:] - analytic_through), axis=-1)
        chosen = np.r_[
            metrics["worst_RGB_indices"],
            metrics["worst_T_index"],
            np.linspace(0, len(rays) - 1, 12, dtype=int),
        ]
        control = witness(rays, air, actual, chosen, 0)
        joint_metrics = errors(plain_joint, joint)
        joint_witness = witness(
            rays, joint, plain_joint, joint_metrics["worst_RGB_indices"], None
        )
        row = {
            "step": step,
            "rays": len(indices),
            "field_bytes": (folder / "visibility.bin").stat().st_size - 8,
            "source_positions": int(coarse["sources"].astype(np.uint64).sum()),
            "peak_source_positions": int(coarse["sources"].max()),
            "producer_matches_full_air_numeric": bool(
                np.array_equal(numeric(coarse), air[indices])
            ),
            "joint_bilinear": joint_metrics,
            "air_bilinear": errors(plain_air, air),
            "normalized_native": metrics,
            "CPU_GPU_RGB_max_abs": float(np.max(abs(expected_rgb - actual[:, :3]))),
            "T_analytic_abs_max": float(analytic_t_error.max()),
            "T_analytic_over_2e6": int(np.count_nonzero(analytic_t_error > 2e-6)),
            "independent": control,
            "joint_bilinear_independent": joint_witness,
            "component_control_passed": max(x["reused_relative_RGB"] for x in control)
            <= 0.01
            and max(x["reused_T_abs"] for x in control) <= 2e-6
            and metrics["relative_RGB_max"] <= 0.01
            and float(analytic_t_error.max()) <= 2e-6,
        }
        report["steps"].append(row)
    (output / "analysis.json").write_text(json.dumps(report, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--task", choices=("prepare", "field", "analyze"), required=True
    )
    parser.add_argument("--width", type=int, default=1920)
    parser.add_argument("--height", type=int, default=1080)
    parser.add_argument("--steps", type=int, nargs="+", default=[16, 8, 4])
    parser.add_argument("--step", type=int)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[3]
    if args.output.resolve().is_relative_to(root):
        parser.error("private outputs belong outside the checkout")
    if args.task == "prepare":
        prepare(args.output, args.width, args.height, args.steps)
    elif args.task == "field":
        if args.step is None:
            parser.error("field requires --step")
        field(args.output, args.step)
    else:
        analyze(args.output)


if __name__ == "__main__":
    main()
