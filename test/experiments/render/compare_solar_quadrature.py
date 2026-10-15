"""Compare solar optical-depth rules at fixed native Medium/LUT coordinates (#286)."""

import argparse
import hashlib
import json
from pathlib import Path

import numpy as np
from air_vertical_reference import VerticalAir

ROOT = Path(__file__).resolve().parents[3]
MODES = (
    "uniform",
    "quadratic",
    "outward",
    "smooth",
    "focused",
    "focused_gauss2",
    "hybrid_gauss2",
)


def extinction(air, height):
    value = air.values
    rayleigh = np.exp(-height / value[2])
    mie = np.exp(-height / value[3])
    ozone = np.clip(1 - np.abs(height - value[12]) / value[13], 0, 1)
    return (
        rayleigh[..., None] * value[4:7]
        + mie[..., None] * value[11]
        + ozone[..., None] * value[8:11]
    )


def height_at(air, radius, projection, distance):
    bottom = air.values[0]
    absolute = np.sqrt(radius**2 + distance**2 + 2 * projection * distance)
    return (
        (radius - bottom) * (radius + bottom) + distance**2 + 2 * projection * distance
    ) / (absolute + bottom)


def span(air, radius, projection):
    bottom, top = air.values[:2]
    end = -projection + np.sqrt(
        np.maximum(0, projection**2 + (top - radius) * (top + radius))
    )
    disc = projection**2 + (bottom - radius) * (bottom + radius)
    ground = -projection - np.sqrt(np.maximum(0, disc))
    return np.where((disc >= 0) & (ground >= 0), np.minimum(ground, end), end)


def reference(air, looks, nodes):
    x, weights = np.polynomial.legendre.leggauss(nodes)
    output = []
    for start in range(0, len(looks), 256):
        radius, cosine = looks[start : start + 256].T
        projection = radius * cosine
        end = span(air, radius, projection)
        cuts = [np.zeros_like(end), end, np.clip(-projection, 0, end)]
        levels = [
            0,
            air.values[3],
            4 * air.values[3],
            air.values[2],
            *list(air.values[12] + air.values[13] * np.array([-1, 0, 1])),
        ]
        for height in levels:
            shell = air.values[0] + height
            disc = projection**2 + (shell - radius) * (shell + radius)
            root = np.sqrt(np.maximum(0, disc))
            for along in (-projection - root, -projection + root):
                cuts.append(np.clip(np.where(disc >= 0, along, 0), 0, end))
        cuts = np.sort(np.stack(cuts, axis=1), axis=1)
        begin, stop = cuts[:, :-1], cuts[:, 1:]
        half = (stop - begin) / 2
        distance = begin[:, :, None] + half[:, :, None] * (x + 1)
        height = height_at(
            air, radius[:, None, None], projection[:, None, None], distance
        )
        sigma = extinction(air, height)
        depth = np.einsum("ijkc,k,ij->ic", sigma, weights, half)
        output.append(np.exp(-depth))
    return np.concatenate(output)


def concentration(air, radius, cosine, distance, cells, target):
    height = radius - air.values[0]
    mie = air.values[11] * np.exp(-height / air.values[3])
    least = np.min(extinction(air, height), axis=1)
    fraction = np.divide(mie, least, out=np.zeros_like(mie), where=least > 0)
    variation = (
        np.maximum(0, cosine) * distance / (cells * air.values[3]) * np.sqrt(fraction)
    )
    return np.maximum(0, 1 - target / np.maximum(target, variation))


def candidate(air, looks, mode, target, nodes=40):
    radius, cosine = looks.T
    projection = radius * cosine
    distance = span(air, radius, projection)
    cells = nodes // 2 if mode in ("focused_gauss2", "hybrid_gauss2") else nodes
    if mode in ("focused", "focused_gauss2", "hybrid_gauss2"):
        focus = concentration(air, radius, cosine, distance, cells, target)
    elif mode == "smooth":
        focus = np.clip(cosine / 0.1, 0, 1)
    elif mode == "outward":
        focus = (cosine >= 0).astype(float)
    else:
        focus = np.full(len(looks), float(mode == "quadratic"))
    unit = np.linspace(0, 1, cells + 1)
    edges = distance[:, None] * unit * (1 + focus[:, None] * (unit - 1))
    half = np.diff(edges, axis=1) / 2
    middle = (edges[:, 1:] + edges[:, :-1]) / 2
    if mode in ("focused_gauss2", "hybrid_gauss2"):
        positions = middle[:, :, None] + half[:, :, None] * np.array([-1, 1]) / np.sqrt(
            3
        )
        weights = half[:, :, None] * np.ones(2)
    else:
        positions = middle[:, :, None]
        weights = 2 * half[:, :, None]
    height = height_at(air, radius[:, None, None], projection[:, None, None], positions)
    depth = np.sum(extinction(air, height) * weights[..., None], axis=(1, 2))
    result = np.exp(-depth)
    if mode == "hybrid_gauss2":
        resolved = focus == 0
        result[resolved] = candidate(air, looks[resolved], "uniform", target, nodes)
    return result


def compare(air, looks, target, captured):
    original, refined = reference(air, looks, 32), reference(air, looks, 64)
    refinement = float(np.max(np.abs(original - refined)))
    if refinement > 1e-7:
        raise ValueError(f"independent reference did not converge: {refinement}")
    results = {}
    for mode in MODES:
        actual = candidate(air, looks, mode, target)
        error = np.max(np.abs(actual - refined), axis=1)
        worst = int(np.argmax(error))
        results[mode] = {
            "maximum_absolute_T_error": float(error.max()),
            "p99_absolute_T_error": float(np.quantile(error, 0.99)),
            "worst_look": looks[worst].tolist(),
            "native_fp32_geometry_with_fp64_integration": True,
            "sigma_samples_per_ray": 40,
            "extra_start_sigma_samples_maximum": int(
                mode in ("focused", "focused_gauss2", "hybrid_gauss2")
            ),
        }
    native = {}
    for name, values in captured.items():
        error = np.max(np.abs(values - refined), axis=1)
        native[name] = {
            "maximum_absolute_T_error": float(error.max()),
            "p99_absolute_T_error": float(np.quantile(error, 0.99)),
            "worst_look": looks[int(np.argmax(error))].tolist(),
        }
    radius, cosine = looks.T
    focus = concentration(
        air, radius, cosine, span(air, radius, radius * cosine), 20, target
    )
    return {
        "reference_refinement_absolute_max": refinement,
        "results": results,
        "captured": native,
        "candidate_start_density_rays": int(
            np.count_nonzero((cosine > 0) & (air.values[11] > 0))
        ),
        "adaptive_rays": int(np.count_nonzero(focus > 0)),
        "unchanged_uniform_rays": int(np.count_nonzero(focus == 0)),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inputs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--native-captures", type=Path)
    parser.add_argument("--cpu-captures", type=Path)
    parser.add_argument("--width", type=int, default=768)
    parser.add_argument("--stride", type=int, default=32)
    parser.add_argument("--target", type=float, default=0.25)
    args = parser.parse_args()
    if args.width < 1 or args.stride < 1 or not 0 < args.target <= 1:
        parser.error("positive coordinate stride and density target in (0,1] required")
    if args.output.resolve().is_relative_to(ROOT):
        parser.error("private experiment outputs belong outside the checkout")
    report = {
        "scope": "Quantized native LUT coordinates and optional captured CPU/GPU table values against independent FP64 optical depth. No interpolated-prefix/raster/time/Place acceptance; no changed physical expectations.",
        "target_density_variation_per_cell": args.target,
        "stride": args.stride,
        "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        "cases": {},
    }
    for haze in ("0", "0.25", "1", "4"):
        path = args.inputs / f"haze-{haze}"
        air = VerticalAir.read(path / "medium.bin")
        looks = (
            np.fromfile(path / "looks.bin", dtype="<f4").reshape(-1, 2).astype(float)
        )
        if looks.shape != (args.width * 64, 2):
            raise ValueError(f"unexpected look grid extent: {path}")
        indices = np.unique(
            np.r_[
                np.arange(0, len(looks), args.stride),
                np.arange(0, len(looks), args.width),
            ]
        )
        captured = {}
        capture_hashes = {}
        for name, root, filename in (
            ("GPU_RGBA16F", args.native_captures, "samples.bin.veil"),
            ("CPU_FP32", args.cpu_captures, "veil-native-f32.bin"),
        ):
            if root is None:
                continue
            file = root / f"haze-{haze}" / filename
            extent = np.fromfile(file, dtype="<u4", count=2)
            dtype = "<f2" if name == "GPU_RGBA16F" else "<f4"
            values = np.fromfile(file, dtype=dtype, offset=8).reshape(-1, 4)
            if tuple(extent) != (args.width, 64) or values.shape != (len(looks), 4):
                raise ValueError(f"unexpected LUT extent/layout: {file}")
            if not np.isfinite(values).all():
                raise ValueError(f"nonfinite LUT: {file}")
            captured[name] = values[indices, :3].astype(float)
            capture_hashes[name] = hashlib.sha256(file.read_bytes()).hexdigest()
        case = compare(air, looks[indices], args.target, captured)
        case["capture_sha256"] = capture_hashes
        case["coordinates"] = len(indices)
        case["inputs_sha256"] = {
            name: hashlib.sha256((path / name).read_bytes()).hexdigest()
            for name in ("medium.bin", "looks.bin")
        }
        report["cases"][haze] = case
        print(
            haze,
            {k: v["maximum_absolute_T_error"] for k, v in case["results"].items()},
            flush=True,
        )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
