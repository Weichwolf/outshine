"""Independent optical references for the actual sampled solar LUT (#286)."""

import argparse
import hashlib
import json
import struct
from pathlib import Path

import numpy as np
from air_vertical_reference import VerticalAir
from compare_solar_quadrature import reference

ROOT = Path(__file__).resolve().parents[3]
SEED = 286307


def probe_looks(air):
    bottom, top = air.values[:2]
    heights = np.r_[0, np.geomspace(0.0005, (top - bottom) * 0.99, 41), top - bottom]
    radius = np.asarray(bottom + heights, dtype="<f4").astype(float)
    horizon = -np.sqrt((radius - bottom) * (radius + bottom)) / radius
    fractions = np.linspace(1e-4, 1, 129)
    cosine = horizon[:, None] + (1 - horizon[:, None]) * fractions
    rows = np.column_stack((np.repeat(radius, len(fractions)), cosine.ravel()))
    rng = np.random.default_rng(SEED)
    random_r = np.asarray(bottom + rng.uniform(0, top - bottom, 2048), dtype="<f4")
    random_horizon = (
        -np.sqrt((random_r.astype(float) - bottom) * (random_r + bottom)) / random_r
    )
    random_mu = random_horizon + (1 - random_horizon) * rng.uniform(1e-4, 1, 2048)
    blocked = np.column_stack((radius, horizon - 0.01))
    return np.asarray(
        np.vstack((rows, np.column_stack((random_r, random_mu)), blocked)), dtype="<f4"
    )


def visible(air, looks):
    radius, cosine = looks.astype(float).T
    bottom = air.values[0]
    return (cosine > 0) | (
        (radius * cosine) ** 2 < (radius - bottom) * (radius + bottom)
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--medium", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT):
        parser.error("private probes belong outside the checkout")
    air = VerticalAir.read(args.medium)
    looks = probe_looks(air)
    lit = visible(air, looks)
    expected = np.zeros((len(looks), 3), dtype="<f8")
    expected[lit] = reference(air, looks[lit].astype(float), 64)
    coarse = reference(air, looks[lit].astype(float), 32)
    refinement = float(np.max(np.abs(expected[lit] - coarse)))
    if refinement > 1e-7:
        raise ValueError(
            f"independent optical reference did not converge: {refinement}"
        )
    output.mkdir(parents=True, exist_ok=True)
    (output / "medium.bin").write_bytes(args.medium.read_bytes())
    looks.tofile(output / "looks.bin")
    expected.tofile(output / "reference.bin")
    np.zeros(len(looks), dtype="<f4").tofile(output / "queries.bin")
    (output / "rays.bin").write_bytes(
        b"".join(
            struct.pack("<5f3I", float(radius), 0, float(mu), 0, 0, i, i + 1, 0)
            for i, (radius, mu) in enumerate(looks)
        )
    )
    report = {
        "scope": "Quantized physical inputs; independent split FP64 optical integration. No LUT-coordinate oracle, multiple scattering, raster or frame proof.",
        "seed": SEED,
        "records": len(looks),
        "blocked_records": int(np.count_nonzero(~lit)),
        "reference_refinement_absolute_T": refinement,
        "files_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in output.glob("*.bin")
        },
    }
    (output / "probes.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
