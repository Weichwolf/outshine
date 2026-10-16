"""Joint cloud/air controls using actual native outputs and independent RTE."""

import argparse
import json
from pathlib import Path

import numpy as np
from cloud_air_depth_cases import prepare
from cloud_air_depth_reference import independent, sparse_identity
from cloud_air_reference import RAY, SAMPLE
from cloud_air_source_reference import medium_parameters
from cloud_air_transfer_reuse import clear_response, errors, numeric
from cloud_field import Layer, density, segments

BASE = [
    "-DSOURCE_LINEAR_DEPTH=1",
    "-DSOURCE_LINEAR_EYE=1",
    "-DSOURCE_DEPTH_INTERVALS=1024",
]
PROFILES = {
    "joint": [*BASE, "-DSOURCE_DEPTH_GROWTH=0.0078125"],
    "coarse": [*BASE, "-DSOURCE_DEPTH_GROWTH=0.015625"],
    "growth-96": [*BASE, "-DSOURCE_DEPTH_GROWTH=0.0104166666666667"],
    "unshaded": [*BASE, "-DSOURCE_LINEAR_UNSHADED=1"],
    "legacy": [],
}
STATES = (
    ("advected", 0.65, 37),
    ("stationary", 0.65, 0),
    ("overcast", 1.0, 37),
    ("clear", 0.0, 37),
)


def raster_transmittance(output, cover, seconds):
    rays = np.fromfile(output / "raster/rays.bin", RAY)
    queries = np.fromfile(output / "raster/queries.bin", "<f4")
    origin = np.zeros((len(rays), 3))
    origin[:, 1] = ((rays["ray"][:, 0] - np.float32(6360)) * np.float32(1000)).astype(
        float
    )
    cosine = rays["ray"][:, 1].astype(float)
    horizontal = np.sqrt(1 - cosine * cosine)
    azimuth = rays["azimuth"].astype(float)
    direction = np.c_[
        horizontal * np.cos(azimuth), cosine, horizontal * np.sin(azimuth)
    ]
    _, through, _, _ = clear_response(rays, queries, medium_parameters())
    layer = Layer(cover=cover)
    intervals = segments(origin, direction, layer)
    tau = np.zeros(len(rays))
    if cover > 0:
        for interval in range(2):
            a, b = intervals[:, interval, 0], intervals[:, interval, 1]
            width = (b - a) / 16
            for index in range(16):
                points = origin + direction * (a + (index + 0.5) * width)[:, None]
                tau += density(points, layer, seconds) * width
    return through * np.exp(-tau[:, None])


def analyze(output):
    rays = np.fromfile(output / "rays.bin", RAY)
    queries = np.fromfile(output / "queries.bin", "<f4")
    repeated = np.repeat(rays, rays["end"] - rays["begin"])
    integral, through, direct, diffuse = clear_response(
        repeated, queries, medium_parameters()
    )
    clear = np.c_[integral * (direct + diffuse), through]
    report = {
        "scope": "joint homogeneous-air/declared-eye-cloud native component; no resident depth volume or runtime pass",
        "states": {},
    }
    for state, cover, seconds in STATES:
        ref = clear if cover == 0 else np.load(output / f"{state}-reference.npy")
        raster_t = raster_transmittance(output, cover, seconds)
        limit = 2e-6 if cover == 0 else 1e-4
        rows = {}
        for name in PROFILES:
            tag = f"{name}-{state}"
            records = np.fromfile(output / f"{tag}.bin", SAMPLE)
            actual = numeric(records)
            if (
                not np.isfinite(actual).all()
                or np.any(actual < 0)
                or np.any(actual[:, 3:] > 1)
            ):
                raise RuntimeError(
                    "joint transfer violates finite positive RGB/physical T"
                )
            row = errors(actual, ref)
            row["T_limit"] = limit
            row["T_over_limit"] = int(
                np.count_nonzero(
                    np.max(abs(actual[:, 3:] - ref[:, 3:]), axis=-1) > limit
                )
            )
            row["passed"] = (
                row["relative_RGB_max"] <= 0.01 and row["T_abs_max"] <= limit
            )
            row["sparse_records"] = sparse_identity(output, tag)
            final = records[rays["end"] - 1]
            row["source_positions"] = int(final["sources"].astype(np.uint64).sum())
            raster = np.fromfile(output / "raster" / f"{tag}.bin", SAMPLE)
            values = numeric(raster)
            if (
                not np.isfinite(values).all()
                or np.any(values < 0)
                or np.any(values[:, 3:] > 1)
            ):
                raise RuntimeError(
                    "coarse native raster violates physical numeric bounds"
                )
            row["raster_T_abs"] = float(np.max(abs(values[:, 3:] - raster_t)))
            row["raster_source_positions"] = int(
                raster["sources"].astype(np.uint64).sum()
            )
            if name != "legacy" and cover > 0:
                if np.any(records["counts"] & 0x80000000) or np.any(
                    raster["counts"] & 0x80000000
                ):
                    raise RuntimeError(
                        "joint native prefix exceeded its fixed cell/work bounds"
                    )
                row["union_cells"] = int(
                    (final["counts"] & 4095).astype(np.uint64).sum()
                )
                row["cloud_density_positions"] = int(
                    ((final["counts"] >> 12) & 63).sum()
                )
                row["cloud_source_positions"] = int(
                    ((final["counts"] >> 18) & 63).sum()
                )
            rows[name] = row
        if not all(
            rows[name]["passed"] and rows[name]["raster_T_abs"] <= limit
            for name in ("joint", "growth-96")
        ):
            raise RuntimeError(
                "joint source/extinction integration failed unchanged limits"
            )
        if cover > 0 and rows["unshaded"]["passed"]:
            raise RuntimeError(
                "unshadowed direct-air control must fail under declared clouds"
            )
        empty = np.fromfile(output / "empty" / f"joint-{state}.bin", SAMPLE)
        if (
            len(empty) != 8
            or np.any(empty["radiance"] != 0)
            or np.any(empty["through"] != 1)
            or np.any(empty["sources"] != 0)
        ):
            raise RuntimeError(
                "empty physical rays must be neutral without source work"
            )
        report["states"][state] = rows
    report["selected_finite_cohort"] = "growth-96"
    (output / "analysis.json").write_text(json.dumps(report, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--task", choices=("prepare", "reference", "analyze"), required=True
    )
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private outputs belong outside the checkout")
    if args.task == "prepare":
        prepare(args.output)
    elif args.task == "reference":
        for state, cover, seconds in STATES:
            if cover > 0:
                independent(args.output, state, seconds, cover=cover, eye_cover=cover)
    else:
        analyze(args.output)


if __name__ == "__main__":
    main()
