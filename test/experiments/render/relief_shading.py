"""Compare ideal procedural normal statistics against integrated GGX shading."""

import argparse
import hashlib
import json
import os
import subprocess
import time
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from facade_coating_raster import ROOT
from ground_litter_native import capture
from masonry_relief_reference import exact
from matplotlib.backends.backend_pdf import PdfPages
from relief_shading_reference import (
    candidates,
    direction,
    distribution,
    mixture,
    normals,
    specular,
    specular_from_dots,
)

ROUGHNESS = (0.2, 0.5, 0.9)
ANGLES = np.linspace(-80, 80, 65)
METHODS = ("mean_gradient", "unit_mean", "toksvig", "slope_trace", "normal_mixture")


def cases():
    for width in (0.0002, 0.003, 0.012, 0.08, 0.25):
        for name, at, dx, dy in (
            ("joint", [0.005, 0.04], [width, 0], [0, width]),
            (
                "sheared-corner",
                [0.005, 0.005],
                [width, width * 0.3],
                [-width * 0.6, width],
            ),
            ("thin", [0.005, 0.04], [width, width * 0.3], [0, width / 64]),
        ):
            yield {
                "name": f"{name}-{width}",
                "at": np.array(at),
                "dx": np.array(dx),
                "dy": np.array(dy),
                "depth": 0.002,
            }
    for index, offset in enumerate(np.linspace(-0.003, 0.003, 7)):
        yield {
            "name": f"motion-{index}",
            "at": np.array([0.005 + offset, 0.04]),
            "dx": np.array([0.012, 0]),
            "dy": np.array([0, 0.012]),
            "depth": 0.002,
        }
    yield {
        "name": "flat-interior",
        "at": np.array([0.125, 0.04]),
        "dx": np.array([0.0002, 0]),
        "dy": np.array([0, 0.0002]),
        "depth": 0.002,
    }
    yield {
        "name": "zero-depth",
        "at": np.array([0.005, 0.005]),
        "dx": np.array([0.012, 0]),
        "dy": np.array([0, 0.012]),
        "depth": 0.0,
    }


def curves(gradient, weights):
    data = []
    unit = normals(gradient)
    lobes, coverage = mixture(gradient, weights)
    for roughness in ROUGHNESS:
        models = candidates(gradient, weights, roughness)
        for view_angle in (0, 50):
            view = direction(view_angle)
            for azimuth in (0, 90):
                lights = [direction(angle, azimuth) for angle in ANGLES]
                reference = np.array(
                    [
                        weights @ specular(unit, roughness, light, view)
                        for light in lights
                    ]
                )
                approximations = {
                    name: np.array(
                        [specular(normal[None], r, light, view)[0] for light in lights]
                    )
                    for name, (normal, r) in models.items()
                }
                approximations["normal_mixture"] = np.array(
                    [
                        coverage @ specular(lobes, roughness, light, view)
                        for light in lights
                    ]
                )
                data.append(
                    dict(
                        roughness=roughness,
                        view_deg=view_angle,
                        light_azimuth_deg=azimuth,
                        reference=reference,
                        **approximations,
                    )
                )
    return data


def native(binary, output, rows):
    records = []
    for row in rows:
        for normal, _ in row["models"].values():
            for roughness in ROUGHNESS:
                for angle in (-80, -45, 0, 45, 80):
                    records.append(
                        [*normal, roughness, *direction(angle), 0.95, *direction(50), 0]
                    )
    records = np.array(records, dtype="<f4")
    palette = np.zeros(320 + records.size, dtype="<f4")
    original = np.fromfile(
        ROOT / "test/outshine/data/materials/ground-litter.palette", dtype="<f4"
    )
    palette[: len(original)] = original
    palette[320:] = records.ravel()
    palette.tofile(output / "native.palette")
    probes = np.zeros(
        len(records), dtype=[("values", "<f4", 6), ("which", "<i4"), ("mode", "<u4")]
    )
    probes["which"] = np.arange(len(records))
    probes.tofile(output / "probes.bin")
    command = [
        "nice",
        "-n",
        "10",
        os.environ.get("GLSLANG", "glslangValidator"),
        "-V",
        "--target-env",
        "vulkan1.0",
        "-Isrc/render/shaders",
        "test/experiments/render/relief_shading_probe.comp",
        "-o",
        str(output / "filter-0.spv"),
    ]
    compiled = subprocess.run(
        command, cwd=ROOT, capture_output=True, text=True, check=False
    )
    (output / "compile.log").write_text(compiled.stdout + compiled.stderr)
    if compiled.returncode:
        raise RuntimeError("native BRDF shader compilation failed")
    gpu = capture(
        binary, output, 0, "native", len(probes), int(original[:1].view("<u4")[0]) - 1
    )
    cpu = np.array(
        [
            specular(
                r[None, :3].astype(float),
                float(r[3]),
                r[4:7].astype(float),
                r[8:11].astype(float),
            )[0]
            for r in records
        ]
    )
    dots = []
    for r in records.astype(float):
        half = r[4:7] + r[8:11]
        half /= np.linalg.norm(half)
        dots.append(
            [
                max(r[:3] @ r[4:7], 0),
                max(r[:3] @ r[8:11], 1e-6),
                max(r[:3] @ half, 0),
                max(r[8:11] @ half, 0),
            ]
        )
    dot_error = np.abs(gpu[:, 4:] - dots)
    if dot_error.max() > 3e-7:
        raise AssertionError("native dot products exceed fixed float32 tolerance")
    conditioned = specular_from_dots(
        records[:, 3].astype(float), *gpu[:, 4:].astype(float).T
    )
    error = np.abs(gpu[:, 0] - conditioned)
    if not np.all(error <= 3e-6 + 1e-4 * np.abs(conditioned)):
        raise AssertionError(f"native GGX arithmetic parity: {error.max()}")
    raw_error = np.abs(gpu[:, 0] - cpu)
    unstable = raw_error > 3e-6 + 1e-4 * np.abs(cpu)
    if np.any(unstable & (np.array(dots)[:, 0] > 3e-7)):
        raise AssertionError("unexpected non-tangent full-response difference")
    np.savez_compressed(
        output / "native.npz",
        records=records,
        cpu=cpu,
        conditioned_cpu=conditioned,
        gpu=gpu,
    )
    return {
        "count": len(records),
        "max_abs_error": float(error.max()),
        "max_dot_error": float(dot_error.max()),
        "full_response_max_abs_error": float(raw_error.max()),
        "tangent_conditioning_cases": int(unstable.sum()),
        "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "compile_command": command,
    }


def compare(case, output):
    references = []
    started = time.perf_counter()
    for order in (16, 32, 64, 128):
        gradient, weights, work = distribution(
            case["at"], case["dx"], case["dy"], order, depth=case["depth"]
        )
        np.testing.assert_allclose(weights.sum(), 1, atol=1e-12, rtol=0)
        expected, _ = exact(
            np.r_[0, 0, case["at"]],
            np.r_[0, 0, case["dx"]],
            np.r_[0, 0, case["dy"]],
            np.zeros(2),
            0,
            depth=case["depth"],
        )
        np.testing.assert_allclose(weights @ gradient, expected, atol=1e-11, rtol=0)
        references.append(curves(gradient, weights))
        if len(references) < 2:
            continue
        convergence = max(
            float(np.max(np.abs(a["reference"] - b["reference"])))
            for a, b in zip(references[-2], references[-1], strict=True)
        )
        converged = all(
            np.max(np.abs(a["reference"] - b["reference"]))
            <= 1e-6 + 1e-4 * np.max(np.abs(b["reference"]))
            for a, b in zip(references[-2], references[-1], strict=True)
        )
        if converged:
            break
        print(case["name"], "refine quadrature", order, convergence, flush=True)
    else:
        raise AssertionError(f"{case['name']} reference not converged: {convergence}")
    if case["name"] in ("flat-interior", "zero-depth", "joint-0.0002"):
        for row in references[-1]:
            for key in METHODS:
                np.testing.assert_allclose(
                    row[key], row["reference"], atol=1e-11, rtol=0
                )
    np.savez_compressed(
        output / f"{case['name']}.npz",
        gradient=gradient,
        weights=weights,
        response=np.array(
            [[r[k] for k in ("reference", *METHODS)] for r in references[-1]]
        ),
    )
    result = {
        "name": case["name"],
        "at": case["at"].tolist(),
        "dx": case["dx"].tolist(),
        "dy": case["dy"].tolist(),
        "depth": case["depth"],
        "convergence_abs": convergence,
        "reference_peak": max(float(np.max(r["reference"])) for r in references[-1]),
        "work": work,
        "quadrature_order": order,
        "cpu_seconds": time.perf_counter() - started,
        "live_arrays_bytes": gradient.nbytes + weights.nbytes,
    }
    lobes, coverage = mixture(gradient, weights)
    np.testing.assert_allclose(coverage.sum(), 1, atol=1e-12, rtol=0)
    result["mixture_lobes"] = len(lobes)
    models = candidates(gradient, weights, 0.9)
    models.update({f"mixture-{i}": (normal, 0.9) for i, normal in enumerate(lobes)})
    return result, references[-1], models


def plot(output, results):
    with PdfPages(output / "comparison.pdf") as pdf:
        for name, azimuth in (
            ("joint-0.012", 0),
            ("joint-0.25", 0),
            ("sheared-corner-0.003", 90),
            ("joint-0.08", 90),
        ):
            data = next(rows for case, rows, _ in results if case["name"] == name)
            fig, axes = plt.subplots(2, 3, figsize=(12, 6), constrained_layout=True)
            for axis, row in zip(
                axes.flat,
                [r for r in data if r["light_azimuth_deg"] == azimuth],
                strict=True,
            ):
                for key in ("reference", *METHODS):
                    axis.plot(ANGLES, row[key], label=key)
                axis.set_title(f"r={row['roughness']}, view={row['view_deg']}deg")
                axis.set_xlabel("Light angle (degrees)")
                axis.set_ylabel("Specular radiance / unit irradiance")
            axes.flat[0].legend(fontsize=7)
            fig.suptitle(
                f"{name}, light azimuth={azimuth}deg\nIdeal unmasked statistics; no Place/Metal acceptance"
            )
            pdf.savefig(fig)
            plt.close(fig)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--binary", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    results = []
    for case in cases():
        result = compare(case, args.output)
        results.append(result)
        print(case["name"], "reference converged", flush=True)
    errors = []
    for case, rows, _ in results:
        for row in rows:
            for method in METHODS:
                delta = row[method] - row["reference"]
                errors.append(
                    {
                        "case": case["name"],
                        "roughness": row["roughness"],
                        "view_deg": row["view_deg"],
                        "azimuth_deg": row["light_azimuth_deg"],
                        "method": method,
                        "max_abs": float(np.max(np.abs(delta))),
                        "rms": float(np.sqrt(np.mean(delta * delta))),
                        "signed_mean": float(delta.mean()),
                        "reference_peak": float(np.max(row["reference"])),
                    }
                )
    report = {
        "scope": "Unmasked declared brick plane; ideal reference statistics, not a bounded production filter or full GGX convolution",
        "cases": [r[0] for r in results],
        "errors": errors,
    }
    if args.binary:
        report["native"] = native(
            args.binary.resolve(), args.output, [{"models": r[2]} for r in results]
        )
    plot(args.output, results)
    report["source_sha256"] = {
        str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in (
            Path(__file__).resolve(),
            Path(__file__).with_name("relief_shading_reference.py").resolve(),
            ROOT / "src/render/shaders/metalRoughBrdf.glsl",
            Path(__file__).with_name("relief_shading_probe.comp").resolve(),
            Path(__file__).with_name("masonry_relief_reference.py").resolve(),
        )
    }
    (args.output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        "PASS", len(results), "cases", len(errors), "response comparisons", flush=True
    )


if __name__ == "__main__":
    main()
