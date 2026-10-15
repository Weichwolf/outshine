"""Export inhomogeneous Mie/Rayleigh analytic controls and compare the air-prefix fit."""

import argparse
import hashlib
import json
import struct
from pathlib import Path

import numpy as np
from air_path_prefix import AirCells, AirPrefix
from air_vertical_reference import VerticalAir
from compare_air_paths import RADIANCE_FLOOR_PER_SR

ROOT = Path(__file__).resolve().parents[3]


def fitted_cells(air, height, limit, count=22):
    physical = air.values[12] + air.values[13] * np.array([-1, 0, 1]) - height
    edges = np.unique(
        np.clip(
            np.concatenate(
                ([0, limit], physical, np.linspace(0, 1, count + 1) ** 2 * limit)
            ),
            0,
            limit,
        )
    )
    positions = np.stack([edges[:-1], (edges[:-1] + edges[1:]) / 2, edges[1:]], axis=1)
    unique, indices = np.unique(positions, return_inverse=True)
    extinction, emission = air.coefficients(height + unique)
    shape = (len(edges) - 1, 3, 3)
    extinction, emission = (
        values[indices].reshape(shape) for values in (extinction, emission)
    )
    for values in (extinction, emission):
        values[:, 1] = np.maximum(
            0, 2 * values[:, 1] - (values[:, 0] + values[:, 2]) / 2
        )
    return AirCells(edges, extinction, emission, len(unique))


def export(medium, output):
    air = VerticalAir.read(medium)
    source_counts = []
    rays, queries, references, descriptions, comparisons = [], [], [], [], []
    for requested_height in (0.005, 1.0, 10.0, 50.0):
        radius = float(np.float32(air.values[0] + requested_height))
        height = radius - air.values[0]
        if not 0 <= height < air.top_height_km:
            raise ValueError(
                "the fixed vertical camera heights must be inside the medium"
            )
        limit = float(np.float32((air.top_height_km - height) * 0.95))
        distances = np.concatenate(([0.0], np.geomspace(1e-6, limit, 128))).astype(
            "<f4"
        )
        cells = fitted_cells(air, height, limit)
        source_counts.append(cells.source_evaluations)
        prefix = AirPrefix(cells)
        sky = air.transfer(height, air.top_height_km - height)[0]
        begin = len(queries)
        name = f"vertical/height-km={height:g}"
        for distance in distances:
            radiance, through = air.transfer(height, distance)
            fitted, fitted_t = prefix.advance(float(distance))
            scale = np.maximum(radiance, RADIANCE_FLOOR_PER_SR)
            comparisons.append(
                {
                    "case": name,
                    "distance_km": float(distance),
                    "prefix_l_error": float(np.max(np.abs(fitted - radiance) / scale)),
                    "prefix_t_error": float(np.max(np.abs(fitted_t - through))),
                    "legacy_sky_scaling_l_error": float(
                        np.max(np.abs(sky * (1 - through) - radiance) / scale)
                    ),
                }
            )
            row = {
                "case": name,
                "distance_km": float(distance),
                "reference_rgb_per_sr": radiance.tolist(),
                "reference_t_rgb": through.tolist(),
            }
            for _ in range(2):
                queries.append(distance)
                references.append(row)
        ray = [radius, 1.0, 1.0, 1.0, limit]
        rays.append(struct.pack("<5f3I", *ray, begin, len(queries), 0))
        descriptions.append(
            {
                "case": {"name": name},
                "ray_f32": ray,
                "begin": begin,
                "end": len(queries),
            }
        )
    output.mkdir(parents=True, exist_ok=True)
    (output / "rays.bin").write_bytes(b"".join(rays))
    np.asarray(queries, dtype="<f4").tofile(output / "queries.bin")
    report = {
        "scope": "Analytic vertical zenith-Sun inhomogeneous Rayleigh/Mie/ozone; native quantized Medium and ray/query inputs. Psi=0, no clouds/ground bounce. Python fit uses exact source without LUT/FP32 error; not native execution or multiple-scattering convergence.",
        "medium_sha256": hashlib.sha256(medium.read_bytes()).hexdigest(),
        "ray_count": len(rays),
        "query_count": len(queries),
        "native_medium_fp32": air.values.tolist(),
        "maximum_source_positions": max(source_counts),
        "descriptions": descriptions,
        "references": references,
        "comparisons": comparisons,
        "summary": {
            key: max(row[key] for row in comparisons)
            for key in (
                "prefix_l_error",
                "prefix_t_error",
                "legacy_sky_scaling_l_error",
            )
        },
    }
    (output / "rays.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--medium", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == ROOT or ROOT in output.parents:
        raise ValueError("private outputs belong outside the repository")
    report = export(args.medium, output)
    print(json.dumps({key: report[key] for key in ("scope", "query_count", "summary")}))


if __name__ == "__main__":
    main()
