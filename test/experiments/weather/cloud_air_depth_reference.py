"""Independent dense-depth RTE, reconstruction and physical controls (#353)."""

import argparse
import json
import time
from pathlib import Path

import numpy as np
from cloud_air_depth_cases import analyze, prepare, solar_transfer
from cloud_air_reference import RAY, SAMPLE
from cloud_air_source_reference import medium_parameters, reference_ray
from cloud_air_transfer_reuse import clear_response, cubic, errors, numeric

PROFILES = ("legacy", "linear-512", "linear-1024", "growth-64", "growth-128")


def independent(output, state, seconds, cover=0.65, eye_cover=0):
    rays = np.fromfile(output / "rays.bin", RAY)
    queries = np.fromfile(output / "queries.bin", "<f4")
    references, rows = [], []
    for index, ray in enumerate(rays):
        start = time.monotonic()
        q = queries[int(ray["begin"]) : int(ray["end"])]
        values = [
            reference_ray(
                ray,
                q,
                medium_parameters(),
                cover,
                nodes,
                air_sun_transfer=lambda points: solar_transfer(points, cover, seconds),
                eye_cover=eye_cover,
                seconds=seconds,
            )
            for nodes in (16, 32)
        ]
        refinement = float(np.max(abs(values[0] - values[1])))
        if refinement > 1e-8:
            raise RuntimeError("the dense independent depth reference is not converged")
        references.append(values[1])
        rows.append(
            {
                "ray": index,
                "refinement_abs": refinement,
                "wall_seconds": time.monotonic() - start,
            }
        )
        print(state, "reference ray", index, "refinement", refinement, flush=True)
    result = np.concatenate(references)
    np.save(output / f"{state}-reference.npy", result)
    (output / f"{state}-reference.json").write_text(json.dumps(rows, indent=2))
    return result


def sparse_identity(output, name):
    rays = np.fromfile(output / "rays.bin", RAY)
    queries = np.fromfile(output / "queries.bin", "<f4")
    sparse_rays = np.fromfile(output / "sparse/rays.bin", RAY)
    sparse_queries = np.fromfile(output / "sparse/queries.bin", "<f4")
    dense = np.fromfile(output / f"{name}.bin", SAMPLE)
    sparse = np.fromfile(output / "sparse" / f"{name}.bin", SAMPLE)
    for ray, subset in zip(rays, sparse_rays):
        first, last = int(ray["begin"]), int(ray["end"])
        a, b = int(subset["begin"]), int(subset["end"])
        indices = first + np.searchsorted(queries[first:last], sparse_queries[a:b])
        if dense[indices].tobytes() != sparse[a:b].tobytes():
            raise RuntimeError(
                "sparse/repeated queries changed native transfer or source work"
            )
    return len(sparse)


def summarize(output):
    rays = np.fromfile(output / "rays.bin", RAY)
    queries = np.fromfile(output / "queries.bin", "<f4")
    repeated = np.repeat(rays, rays["end"] - rays["begin"])
    integral, through, direct, diffuse = clear_response(
        repeated, queries, medium_parameters()
    )
    clear = np.c_[integral * (direct + diffuse), through]
    report = {
        "scope": "homogeneous air; full physical depth; no cloud eye extinction or production field",
        "states": {},
    }
    for state in ("advected", "stationary", "clear"):
        expected = (
            clear if state == "clear" else np.load(output / f"{state}-reference.npy")
        )
        results = {}
        for profile in PROFILES:
            name = f"{profile}-{state}"
            records = np.fromfile(output / f"{name}.bin", SAMPLE)
            actual = numeric(records)
            if not np.isfinite(actual).all() or np.any(actual < 0):
                raise RuntimeError("native transfer must remain finite and nonnegative")
            metrics = errors(actual, expected)
            counts = records["sources"][rays["end"] - 1]
            metrics.update(
                source_positions=int(counts.sum()),
                max_source_positions=int(counts.max()),
                sparse_records=sparse_identity(output, name),
            )
            metrics["passed"] = (
                metrics["relative_RGB_max"] <= 0.01 and metrics["T_abs_max"] <= 2e-6
            )
            results[profile] = metrics
        if not results["growth-128"]["passed"]:
            raise RuntimeError(
                "selected full-depth source curve failed unchanged controls"
            )
        empty = np.fromfile(output / "empty" / f"growth-128-{state}.bin", SAMPLE)
        if (
            len(empty) != 8
            or np.any(empty["radiance"] != 0)
            or np.any(empty["through"] != 1)
            or np.any(empty["sources"] != 0)
        ):
            raise RuntimeError(
                "empty or outward space paths must remain neutral without source work"
            )
        for profile in PROFILES:
            records = np.fromfile(output / "raster" / f"{profile}-{state}.bin", SAMPLE)
            results[profile]["raster_source_positions"] = int(
                records["sources"].astype(np.uint64).sum()
            )
            results[profile]["raster_max_source_positions"] = int(
                records["sources"].max()
            )
        report["states"][state] = results
    x = np.array([0, 0, 1, 1, 1], float)
    values = np.broadcast_to(x[None, :, None], (4, 5, 1))
    edge = cubic(values, 33, 25, 8)
    report["cubic_unit_edge"] = {
        "minimum": float(edge.min()),
        "maximum": float(edge.max()),
    }
    if edge.min() >= 0 or edge.max() <= 1:
        raise RuntimeError("the signed cubic counterexample disappeared")
    (output / "analysis.json").write_text(json.dumps(report, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--task",
        choices=("prepare", "reference", "analyze", "reconstruction"),
        required=True,
    )
    args = parser.parse_args()
    if args.output.resolve().is_relative_to(Path(__file__).resolve().parents[3]):
        parser.error("private outputs belong outside the checkout")
    if args.task == "prepare":
        prepare(args.output)
    elif args.task == "reference":
        for state, seconds in (("advected", 37), ("stationary", 0)):
            independent(args.output, state, seconds)
    elif args.task == "analyze":
        summarize(args.output)
    else:
        for state, cover, seconds in (
            ("legacy-advected", 0.65, 37),
            ("legacy-stationary", 0.65, 0),
            ("legacy-clear", 0, 37),
        ):
            analyze(args.output, state, cover, seconds)


if __name__ == "__main__":
    main()
