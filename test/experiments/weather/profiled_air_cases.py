"""Native Earth-profile controls; fixed daylight LUTs, actual Weather source/cells."""

import argparse
import json
from pathlib import Path

import numpy as np
from cloud_air_depth_cases import prepare as prepare_depth
from cloud_air_depth_reference import sparse_identity
from cloud_air_reference import RAY, SAMPLE
from cloud_air_transfer_reuse import errors, numeric
from linear_medium_reference import STATES
from profiled_air_reference import earth_parameters, reference

GRID = [
    "-UAIR_PREFIX_INTERVALS",
    "-DAIR_PREFIX_INTERVALS=1024",
    "-DSOURCE_GRID_GROWTH=0.015625",
    "-DSOURCE_GRID_SYMMETRIC=1",
]
PROFILES = {
    "profiled": [*GRID, "-DAIR_PREFIX_ACCUMULATE_DEPTH=1"],
    "products": GRID,
    "legacy": [],
    "frozen": [
        "-DSOURCE_LINEAR_DEPTH=1",
        "-DSOURCE_LINEAR_EYE=1",
        "-DSOURCE_DEPTH_INTERVALS=1024",
        "-DSOURCE_DEPTH_GROWTH=0.0078125",
    ],
}


def prepare(output):
    prepare_depth(output)
    earth_parameters().tofile(output / "medium.bin")


def independent(output):
    rays = np.fromfile(output / "rays.bin", RAY)
    queries = np.fromfile(output / "queries.bin", "<f4")
    for state, cover, seconds in STATES:
        values, refinements = [], []
        for index, ray in enumerate(rays):
            q = queries[int(ray["begin"]) : int(ray["end"])]
            coarse, fine = [
                reference(ray, q, earth_parameters(), cover, n, seconds)
                for n in (16, 32)
            ]
            error = float(np.max(abs(coarse - fine)))
            if error > 1e-8:
                raise RuntimeError("height-dependent RTE reference did not converge")
            values.append(fine)
            refinements.append(error)
            print(state, index, "refinement", error, flush=True)
        np.save(output / f"{state}-reference.npy", np.concatenate(values))
        (output / f"{state}-reference.json").write_text(json.dumps(refinements))


def physical(records):
    values = numeric(records)
    if not np.isfinite(values).all() or np.any(values < 0) or np.any(values[:, 3:] > 1):
        raise RuntimeError(
            "native transfer must have finite positive RGB and physical T"
        )
    if np.any(records["counts"] & 0x80000000):
        raise RuntimeError("native prefix exceeded its declared bounded work")
    return values


def work(records, frozen=False):
    shift = 12 if frozen else 10
    mask = (1 << shift) - 1
    return {
        "source_positions": int(records["sources"].astype(np.uint64).sum()),
        "max_source_positions": int(records["sources"].max()),
        "union_cells": int((records["counts"] & mask).astype(np.uint64).sum()),
        "cloud_density_positions": int(((records["counts"] >> shift) & 63).sum()),
        "cloud_source_positions": int(((records["counts"] >> (shift + 6)) & 63).sum()),
    }


def analyze(output):
    rays = np.fromfile(output / "rays.bin", RAY)
    report = {
        "scope": "native Earth profiles with prescribed constant Sun/Psi LUTs and Weather Sun-T6; no production raster, reconstruction, night or device/frame claim",
        "states": {},
    }
    for state, cover, _ in STATES:
        expected = np.load(output / f"{state}-reference.npy")
        limit = 2e-6 if cover == 0 else 1e-4
        rows = {}
        for name in PROFILES:
            tag = f"{name}-{state}"
            records = np.fromfile(output / f"{tag}.bin", SAMPLE)
            actual = physical(records)
            row = errors(actual, expected)
            row.update(
                T_limit=limit,
                passed=row["relative_RGB_max"] <= 0.01 and row["T_abs_max"] <= limit,
            )
            row["sparse_records"] = sparse_identity(output, tag)
            row["dense_work"] = work(records[rays["end"] - 1], name == "frozen")
            raster = np.fromfile(output / "raster" / f"{tag}.bin", SAMPLE)
            physical(raster)
            row["coarse_work"] = work(raster, name == "frozen")
            rows[name] = row
        report["states"][state] = rows
        if not rows["profiled"]["passed"] or rows["frozen"]["passed"]:
            raise RuntimeError(
                "variable profile must pass; frozen entry coefficients must fail"
            )
        empty = np.fromfile(output / "empty" / f"profiled-{state}.bin", SAMPLE)
        if (
            len(empty) != 8
            or np.any(empty["radiance"] != 0)
            or np.any(empty["through"] != 1)
            or np.any(empty["sources"] != 0)
        ):
            raise RuntimeError(
                "empty physical spans must remain neutral without source work"
            )
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
    {"prepare": prepare, "reference": independent, "analyze": analyze}[args.task](
        args.output
    )


if __name__ == "__main__":
    main()
