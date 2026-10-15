"""Independent state/update controls for the resident GPU air field (#353)."""

import argparse
import json
import struct
import subprocess
from pathlib import Path

import numpy as np
from cloud_air_reference import RAY, SAMPLE
from cloud_air_source_reference import medium_parameters, reference_ray
from cloud_air_transfer_cases import prepare
from cloud_air_transfer_reuse import bilinear, clear_response, cubic, errors, numeric
from cloud_field import Layer, optical_depth
from cloud_radiance import SUN


def inputs(output, step):
    prepare(output, 1920, 1080, [step])
    folder = output / f"step-{step}"
    shape = json.loads((folder / "grid.json").read_text())["shape"]
    (folder / "gpu-grid.bin").write_bytes(
        struct.pack("<4I", 1920, 1080, shape[1], shape[0])
    )


def witnesses(rays, actual, baseline, indices, seconds):
    rows = []
    for index in np.unique(indices):

        def transfer(points):
            return np.exp(
                -optical_depth(
                    points, SUN, Layer(cover=0.65), 6, seconds, maximum_m=1e8
                )
            )

        values = [
            reference_ray(
                rays[index],
                np.array([10.0]),
                medium_parameters(),
                0.65,
                nodes,
                air_sun_transfer=transfer,
                eye_cover=0,
            )[0]
            for nodes in (16, 32)
        ]
        refinement = float(np.max(abs(values[0] - values[1])))
        if refinement > 1e-8:
            raise RuntimeError("the independent state witness is not converged")
        expected = values[1]
        denominator = np.maximum(abs(expected[:3]), 1e-5)
        rows.append(
            {
                "index": int(index),
                "refinement_abs": refinement,
                "reference": expected.tolist(),
                "direct_RGB_relative": float(
                    np.max(abs(baseline[index, :3] - expected[:3]) / denominator)
                ),
                "reused_RGB_relative": float(
                    np.max(abs(actual[index, :3] - expected[:3]) / denominator)
                ),
                "T_abs": float(np.max(abs(actual[index, 3:] - expected[3:]))),
            }
        )
    return rows


def analyze(output, step):
    shape = json.loads((output / f"step-{step}/grid.json").read_text())["shape"]
    rays = np.fromfile(output / "rays.bin", RAY)
    queries = np.fromfile(output / "queries.bin", "<f4")
    integral, through, direct, diffuse = clear_response(
        rays, queries, medium_parameters()
    )
    clear = np.c_[integral * (direct + diffuse), through]
    baseline = [
        numeric(np.fromfile(output / f"direct-{name}.bin", SAMPLE))
        for name in ("advected", "stationary")
    ]
    report = {
        "scope": "resident homogeneous-air GPU component, fixed10km query",
        "profiles": {},
    }
    results = {}
    for name in ("bilinear", "fused", "staged"):
        folder = output / name
        records = [
            numeric(np.fromfile(folder / f"gpu-state-{n}.bin", SAMPLE))
            for n in range(4)
        ]
        fields = [
            np.fromfile(folder / f"gpu-state-{n}.field", "<f4").reshape(-1, 4)
            for n in range(4)
        ]
        if not all(np.isfinite(x).all() and np.all(x >= 0) for x in records):
            raise RuntimeError("state transfer must remain finite and nonnegative")
        for field in fields:
            if (
                not np.isfinite(field).all()
                or field[:, :3].min() < -1e-3
                or field[:, 3].max() > 531
                or np.any(field[:, 3] < 0)
                or np.any(field[:, 3] != np.floor(field[:, 3]))
            ):
                raise RuntimeError("field violates its numeric/source-work bounds")
        if not np.array_equal(records[0], records[3]) or not np.array_equal(
            fields[0], fields[3]
        ):
            raise RuntimeError(
                "returning to the same state must restore identical GPU values"
            )
        states = []
        for state in range(3):
            expected = clear if state == 1 else baseline[state // 2]
            metrics = errors(records[state], expected)
            t_error = float(np.max(abs(records[state][:, 3:] - through)))
            interpolation = bilinear if name == "bilinear" else cubic
            reconstructed_visibility = interpolation(
                fields[state][:, :3].reshape(*shape, 3), 1920, 1080, step
            ).reshape(-1, 3)
            expected_rgb = integral * (direct * reconstructed_visibility + diffuse)
            chosen = np.r_[
                metrics["worst_RGB_indices"],
                metrics["worst_T_index"],
                np.linspace(0, len(rays) - 1, 12, dtype=int),
            ]
            controls = (
                []
                if state == 1
                else witnesses(
                    rays, records[state], expected, chosen, 37 if state == 0 else 0
                )
            )
            states.append(
                {
                    "state": state,
                    "against_direct": metrics,
                    "T_analytic_abs_max": t_error,
                    "CPU_GPU_RGB_abs_max": float(
                        np.max(abs(records[state][:, :3] - expected_rgb))
                    ),
                    "source_positions": int(
                        fields[state][:, 3].astype(np.uint64).sum()
                    ),
                    "field_visibility_min": float(fields[state][:, :3].min()),
                    "field_visibility_max": float(fields[state][:, :3].max()),
                    "witnesses": controls,
                    "component_controls_passed": metrics["relative_RGB_max"] <= 0.01
                    and t_error <= 2e-6
                    and all(x["reused_RGB_relative"] <= 0.01 for x in controls),
                }
            )
        stale = errors(records[0], clear)
        if stale["relative_RGB_max"] <= 0.01:
            raise RuntimeError(
                "stale covered-state output must fail the clear-state control"
            )
        report["profiles"][name] = {
            "states": states,
            "return_state_byte_identical": True,
            "stale_state_negative_control": stale,
            "field_bytes": fields[0].nbytes,
        }
        results[name] = records
    report["fused_staged_numeric_max_abs"] = max(
        float(np.max(abs(a - b)))
        for a, b in zip(results["fused"], results["staged"], strict=True)
    )
    report["component_controls_passed"] = all(
        s["component_controls_passed"]
        for name in ("fused", "staged")
        for s in report["profiles"][name]["states"]
    )
    report["runtime_profile_accepted"] = False
    (output / "analysis.json").write_text(json.dumps(report, indent=2))


def guards(output, step, binary, producer, reconstruction):
    root = output / "guards"
    root.mkdir(exist_ok=True)
    records = []
    reasons = {
        "query": "invalid ray or query outside the declared fixed-depth control",
        "medium": "bounded grid and homogeneous-air control",
        "layout": "actual SPIR-V bindings match the private field producer/consumer contract",
        "header": "fixture has a bounded word-aligned size",
    }
    for name in ("query", "medium", "layout", "header"):
        folder = root / name
        folder.mkdir(exist_ok=True)
        full, field = folder / "image", folder / "field"
        full.mkdir(exist_ok=True)
        field.mkdir(exist_ok=True)
        for key in ("rays.bin", "queries.bin", "medium.bin", "veil.bin", "multi.bin"):
            target = full / key
            if not target.exists():
                target.symlink_to(output / key)
        for key in ("rays.bin", "queries.bin", "gpu-grid.bin"):
            target = field / key
            if not target.exists():
                target.symlink_to(output / f"step-{step}" / key)
        if name == "query":
            values = np.fromfile(field / "queries.bin", "<f4")
            values[0] = 3
            (field / "queries.bin").unlink()
            values.tofile(field / "queries.bin")
        elif name == "medium":
            values = medium_parameters()
            values[2] = 8
            (full / "medium.bin").unlink()
            values.tofile(full / "medium.bin")
        elif name == "header":
            (field / "gpu-grid.bin").unlink()
            (field / "gpu-grid.bin").write_bytes(b"bad")
        kernel = output / "direct-advected.spv" if name == "layout" else producer
        command = list(
            map(str, [binary, kernel, reconstruction, full, field, folder / "out", 1])
        )
        result = subprocess.run(
            command, capture_output=True, text=True, check=False, timeout=30
        )
        (folder / "stdout").write_text(result.stdout)
        (folder / "stderr").write_text(result.stderr)
        if (
            result.returncode != 1
            or reasons[name] not in result.stdout
            or list(folder.glob("out-state-*"))
        ):
            raise RuntimeError(
                f"native guard {name} failed to reject before producing an artifact"
            )
        records.append(
            {
                "name": name,
                "argv": command,
                "exit": result.returncode,
                "output_absent": True,
            }
        )
    (root / "receipt.json").write_text(json.dumps(records, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--step", type=int, default=8)
    parser.add_argument(
        "--task", choices=("prepare", "analyze", "guards"), required=True
    )
    parser.add_argument("--binary", type=Path)
    parser.add_argument("--producer", type=Path)
    parser.add_argument("--reconstruction", type=Path)
    args = parser.parse_args()
    if (
        args.output.resolve().is_relative_to(Path(__file__).resolve().parents[3])
        or args.step < 1
    ):
        parser.error("positive step and explicit system-temp output required")
    if args.task == "prepare":
        inputs(args.output, args.step)
    elif args.task == "analyze":
        analyze(args.output, args.step)
    else:
        if not all((args.binary, args.producer, args.reconstruction)):
            parser.error("guards requires binary/producer/reconstruction")
        guards(args.output, args.step, args.binary, args.producer, args.reconstruction)


if __name__ == "__main__":
    main()
