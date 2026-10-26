"""Replay actual Weather shadows through the common air/cloud prefix (#320)."""

import argparse
import hashlib
import json
import os
import shlex
import struct
import subprocess
from pathlib import Path

import numpy as np
from cloud_air_reference import SAMPLE
from cloud_air_source_reference import PSI, SOLAR, inputs, medium_parameters, reference

ROOT = Path(__file__).resolve().parents[3]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--probe-receipt", type=Path, required=True)
    args = parser.parse_args()
    args.output = args.output.resolve()
    args.probe = args.probe.resolve()
    if args.output.is_relative_to(ROOT):
        parser.error("private outputs belong outside the checkout")
    provenance = json.loads(args.probe_receipt.read_text())
    if provenance["consumer_sha256"] != digest(args.probe):
        parser.error("probe does not match its regular-archive receipt")
    args.output.mkdir(parents=True, exist_ok=True)
    receipt = args.output / "receipt.json"
    receipt.unlink(missing_ok=True)
    record = {
        "probe_sha256": digest(args.probe),
        "probe_receipt_sha256": digest(args.probe_receipt),
        "commands": [],
        "cases": {},
    }

    def run(argv, folder, name):
        result = subprocess.run(
            list(map(str, argv)),
            cwd=ROOT,
            capture_output=True,
            text=True,
            timeout=180,
            check=False,
        )
        (folder / f"{name}.stdout").write_text(result.stdout)
        (folder / f"{name}.stderr").write_text(result.stderr)
        record["commands"].append(
            {"argv": list(map(str, argv)), "exit": result.returncode}
        )
        if result.returncode or result.stderr:
            raise RuntimeError(
                f"{name} failed or emitted diagnostics; inspect {folder}"
            )
        return result.stdout

    record["commit"] = run(["git", "rev-parse", "HEAD"], args.output, "commit").strip()
    record["dirty"] = run(
        ["git", "status", "--porcelain"], args.output, "status"
    ).strip()
    glslang = shlex.split(os.environ.get("GLSLANG", "glslangValidator"))
    medium = medium_parameters()
    medium.tofile(args.output / "medium.bin")
    for name, rgb in (("veil", SOLAR), ("multi", PSI)):
        image = np.tile([*rgb, 1], (2, 2, 1)).astype("<f2")
        (args.output / f"{name}.bin").write_bytes(
            struct.pack("<2I", 2, 2) + image.tobytes()
        )
    rays, distances = inputs()
    rays.tofile(args.output / "rays.bin")
    distances.tofile(args.output / "queries.bin")
    sparse_rays, sparse_distances = inputs(True)
    sparse_rays.tofile(args.output / "rays-sparse.bin")
    sparse_distances.tofile(args.output / "queries-sparse.bin")
    for cover in (0, 0.65, 1):
        case = args.output / f"cover-{cover}"
        case.mkdir(exist_ok=True)
        expected = reference(rays, distances, medium, cover)
        refined = reference(rays, distances, medium, cover, 8)
        refinement = float(np.max(np.abs(expected - refined)))
        if refinement > 1e-8:
            raise RuntimeError(f"reference is not converged: {refinement}")
        np.save(case / "reference.npy", expected)
        reports = []
        for name, count, log, mode in (
            ("quadratic22", 22, 0, 0),
            ("log64", 64, 1, 0),
            ("log128", 128, 1, 0),
            ("log256", 256, 1, 0),
            ("unshadowed", 256, 1, 1),
            ("whole-source", 256, 1, 2),
            ("bad-origin", 256, 1, 3),
        ):
            folder = case / name
            folder.mkdir(exist_ok=True)
            shader = folder / "probe.spv"
            run(
                [
                    "nice",
                    "-n",
                    "10",
                    *glslang,
                    "-V",
                    f"-DSOURCE_CLOUD_COVER={cover}",
                    f"-DAIR_PREFIX_INTERVALS={count}",
                    f"-DSOURCE_GRID_LOG={log}",
                    f"-DSOURCE_CLOUD_MODE={mode}",
                    "-Isrc",
                    "-Isrc/render/shaders",
                    "-Isrc/generators/weather/shaders",
                    "-Itest/experiments/weather",
                    "-Itest/experiments/render",
                    "test/experiments/weather/cloud_air_source.comp",
                    "-o",
                    shader,
                ],
                folder,
                "compile",
            )

            def capture(suffix="", query_suffix="", folder=folder, shader=shader):
                target = folder / f"native{suffix}.bin"
                run(
                    [
                        args.probe,
                        shader,
                        args.output / f"rays{query_suffix}.bin",
                        args.output / f"queries{query_suffix}.bin",
                        *[
                            args.output / f"{p}.bin"
                            for p in ("veil", "multi", "medium")
                        ],
                        target,
                    ],
                    folder,
                    f"capture{suffix}",
                )
                return np.fromfile(target, SAMPLE)

            actual = capture()
            measured = np.c_[actual["radiance"], actual["through"]]
            errors = np.abs(measured - expected)
            relative = errors[:, :3] / np.maximum(np.abs(expected[:, :3]), 1e-5)
            if (
                not np.isfinite(measured).all()
                or np.any(measured < 0)
                or np.any(actual["through"] > 1)
                or np.any(actual["counts"] & 0x80000000)
                or np.any(actual["sources"] > 2 * count + 19)
                or errors[:, 3:].max() > (2e-6 if cover == 0 else 1e-4)
            ):
                raise RuntimeError(f"invalid native transfer or work bound: {folder}")
            maximum = float(relative.max())
            if name == "log256":
                if maximum > 0.01:
                    raise RuntimeError(
                        f"source exceeds unchanged1% limit: {folder}: {maximum}"
                    )
                sparse = capture("-sparse", "-sparse")
                for ray, short_ray in zip(rays, sparse_rays, strict=True):
                    full = actual[int(ray["begin"]) : int(ray["end"])].reshape(81, 2)
                    subset = sparse[
                        int(short_ray["begin"]) : int(short_ray["end"])
                    ].reshape(11, 2)
                    if full[::8].tobytes() != subset.tobytes():
                        raise RuntimeError("sparse queries changed full native records")
            if actual[::2].tobytes() != actual[1::2].tobytes():
                raise RuntimeError("repeated query changed full native records")
            reports.append(
                {
                    "profile": name,
                    "count": count,
                    "mode": mode,
                    "max_relative": maximum,
                    "max_absolute": float(errors[:, :3].max()),
                    "through_max_absolute": float(errors[:, 3:].max()),
                    "max_air_sources": int(actual["sources"].max()),
                    "max_union_cells": int((actual["counts"] & 1023).max()),
                    "max_cloud_density": int(((actual["counts"] >> 10) & 63).max()),
                    "max_cloud_sources": int(((actual["counts"] >> 16) & 63).max()),
                    "air_shadow_density_upper_bound": (
                        0 if cover == 0 else int((12 * actual["sources"]).max())
                    ),
                    "repeated_bitwise": True,
                    "samples": len(actual),
                }
            )
        if cover > 0 and any(row["max_relative"] <= 0.01 for row in reports[-3:]):
            raise RuntimeError("wrong source control unexpectedly passes")
        if cover == 0:
            clear = (case / "quadratic22/native.bin").read_bytes()
            for name in (row["profile"] for row in reports):
                if clear != (case / name / "native.bin").read_bytes():
                    raise RuntimeError("null cloud changed the clear output")
        record["cases"][str(cover)] = {
            "reference_refinement": refinement,
            "profiles": reports,
        }
        print(cover, [(r["profile"], r["max_relative"]) for r in reports], flush=True)
    record["hashes"] = {
        str(p.relative_to(args.output)): digest(p)
        for p in args.output.rglob("*")
        if p.is_file() and p.suffix in (".bin", ".spv", ".npy")
    }
    record["sources_sha256"] = {
        str(p.relative_to(ROOT)): digest(p)
        for p in (
            ROOT / "src/render/shaders/airSource.glsl",
            ROOT / "src/render/shaders/airPrefix.glsl",
            *sorted((ROOT / "src/generators/weather/shaders").glob("*.glsl")),
            *sorted((ROOT / "test/experiments/weather").glob("cloud_air_source*")),
            ROOT / "test/experiments/weather/cloud_air_prefix.glsl",
        )
    }
    receipt.write_text(json.dumps(record, indent=2))


if __name__ == "__main__":
    main()
