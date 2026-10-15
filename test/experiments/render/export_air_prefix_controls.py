"""Homogeneous source controls exercise native Rayleigh, Mie, Psi and planet masking."""

import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

import numpy as np
from prepare_air_prefix_luts import write_texture

ROOT = Path(__file__).resolve().parents[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--medium", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output == ROOT or ROOT in output.parents:
        raise ValueError("private controls belong outside the repository")
    medium = np.fromfile(args.medium, dtype="<f4").copy()
    if len(medium) != 20:
        raise ValueError("the native Medium has five float4 rows")
    header = ROOT / "src/world/sky/Atmosphere.h"
    source = (
        header.read_text()
        .split("inline constexpr Medium kEarthAir = {", 1)[1]
        .split("};", 1)[0]
    )
    for index, field in ((7, "MieScatteringPerKm"), (11, "MieExtinctionPerKm")):
        medium[index] = float(
            re.search(r"\." + field + r"\s*=\s*([0-9.]+)f", source)[1]
        )
    medium[2:4] = 1e20
    medium[8:11] = 0
    extinction = medium[4:7].astype(float) + float(medium[11])
    sunlight = np.array([0.8, 0.6, 0.3], dtype="<f4").astype(float)
    psi = np.array([0.01, 0.02, 0.03], dtype="<f4").astype(float)
    rays, queries, references, descriptions = [], [], [], []
    for sun_cos in (1.0, 0.0, -1.0):
        for view_cos in (-0.5, 0.0, 0.5, 1.0):
            view_sun = float(
                np.float32(
                    view_cos * sun_cos
                    + np.sqrt(1 - view_cos**2) * np.sqrt(1 - sun_cos**2)
                )
            )
            name = f"homogeneous/sun={sun_cos}/view={view_cos}"
            rayleigh_phase = 3 * (1 + view_sun**2) / (16 * np.pi)
            g = float(medium[14])
            mie_phase = (
                3
                / (8 * np.pi)
                * (1 - g * g)
                / (2 + g * g)
                * (1 + view_sun**2)
                / (1 + g * g - 2 * g * view_sun) ** 1.5
            )
            direct = sunlight if sun_cos >= 0 else np.zeros(3)
            emission = (
                direct
                * (
                    medium[4:7].astype(float) * rayleigh_phase
                    + float(medium[7]) * mie_phase
                )
                + (medium[4:7].astype(float) + float(medium[7])) * psi
            )
            begin = len(queries)
            for distance in np.concatenate(([0.0], np.geomspace(1e-6, 1.0, 64))).astype(
                "<f4"
            ):
                through = np.exp(-extinction * distance)
                radiance = emission * -np.expm1(-extinction * distance) / extinction
                row = {
                    "case": name,
                    "distance_km": float(distance),
                    "reference_rgb_per_sr": radiance.tolist(),
                    "reference_t_rgb": through.tolist(),
                }
                for _ in range(2):
                    queries.append(distance)
                    references.append(row)
            ray = [float(medium[0]) + 5.0, view_cos, sun_cos, view_sun, 1.0]
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
    medium.tofile(output / "medium.bin")
    (output / "rays.bin").write_bytes(b"".join(rays))
    np.asarray(queries, dtype="<f4").tofile(output / "queries.bin")
    write_texture(output / "veil.bin", np.tile([*sunlight, 1.0], (4, 1)), 2, 2)
    write_texture(output / "multi.bin", np.tile([*psi, 1.0], (4, 1)), 2, 2)
    report = {
        "scope": "Homogeneous closed forms with finite positive constant coefficients, RGB Sun/Psi, Mie phase and explicit planetary night. Functional retention, not atmospheric multiple-scattering convergence.",
        "native_medium_header_sha256": hashlib.sha256(header.read_bytes()).hexdigest(),
        "ray_count": len(rays),
        "query_count": len(queries),
        "descriptions": descriptions,
        "references": references,
    }
    (output / "rays.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps({key: report[key] for key in ("scope", "ray_count", "query_count")})
    )


if __name__ == "__main__":
    main()
