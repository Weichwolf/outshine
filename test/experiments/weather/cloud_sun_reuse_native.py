"""Measure local solar-column reuse without adopting a runtime profile (#353)."""

import argparse
import hashlib
import json
import os
import re
import shlex
import subprocess
import sys
from pathlib import Path

import numpy as np
from cloud_air_reference import SAMPLE
from cloud_sun_reuse_cases import ray_controls, write_inputs

ROOT = Path(__file__).resolve().parents[3]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def gpu_times(stdout):
    if (
        "unavailable" in stdout
        or "available=false" in stdout
        or "truncated=true" in stdout
    ):
        raise RuntimeError("actual dispatch timing is unavailable")
    values = [float(x) for x in re.findall(r" elapsed_ms=([^\s]+)", stdout)]
    if not values or not np.isfinite(values).all() or min(values) < 0:
        raise RuntimeError("no finite dispatch timestamp intervals")
    return values


def build_consumer(args, execute):
    api = args.api_root.resolve()
    archive = args.archive.resolve()
    provenance = json.loads(args.archive_receipt.read_text())
    if digest(archive) != provenance["archive_sha256"]:
        raise RuntimeError("regular archive does not match its receipt")
    commit = execute(
        ["git", "-C", api, "rev-parse", "HEAD"], args.output, "api-commit"
    ).strip()
    dirty = execute(
        ["git", "-C", api, "status", "--porcelain", "--", "src", "include"],
        args.output,
        "api-status",
    ).strip()
    if commit != provenance["commit"] or dirty:
        raise RuntimeError("archive API checkout must match its clean producer commit")
    declaration = dict(
        line.split("=", 1)
        for line in (api / "test/run.sh").read_text().splitlines()
        if line.startswith(("CXXSTD=", "WARN=", "OPT="))
    )
    flags = [
        flag
        for key in ("CXXSTD", "WARN", "OPT")
        for flag in shlex.split(declaration[key].strip('"'))
    ]
    link = shlex.split(
        execute(
            ["sh", api / "test/run.sh", "--native-link-flags"],
            args.output,
            "native-link-flags",
            cwd=api,
        )
    )
    binary = args.output / "air-prefix-source-reuse"
    execute(
        [
            "nice",
            "-n",
            "10",
            *shlex.split(os.environ.get("CXX", "c++")),
            *flags,
            "-fno-exceptions",
            *(
                f"-I{api / name}"
                for name in (
                    "include",
                    "src",
                    "src/world/sky",
                    "test/experiments/render",
                )
            ),
            ROOT / "test/experiments/render/air_prefix_gpu.cpp",
            archive,
            *link,
            "-o",
            binary,
        ],
        args.output,
        "consumer-build",
    )
    return binary, {
        "api_commit": commit,
        "archive_sha256": digest(archive),
        "binary_sha256": digest(binary),
    }


def compile_probe(args, case, name, reuse, diagnostics, execute, cover):
    target = case / f"{name}.spv"
    execute(
        [
            "nice",
            "-n",
            "10",
            *shlex.split(os.environ.get("GLSLANG", "glslangValidator")),
            "-V",
            "-DAIR_PREFIX_INTERVALS=256",
            "-DSOURCE_GRID_LOG=1",
            "-DSOURCE_CLOUD_MODE=4",
            f"-DSOURCE_CLOUD_COVER={cover}",
            f"-DSOURCE_SOLAR_REUSE={reuse}",
            f"-DSOURCE_SOLAR_DIAGNOSTICS={diagnostics}",
            "-Isrc",
            "-Isrc/render/shaders",
            "-Isrc/generators/weather/shaders",
            "-Itest/experiments/weather",
            "-Itest/experiments/render",
            f"-I{case}",
            ROOT / "test/experiments/weather/cloud_air_source.comp",
            "-o",
            target,
        ],
        case,
        f"{name}-compile",
    )
    return target


def capture(binary, shader, fixtures, case, name, execute, repeats, sparse=False):
    suffix = "-sparse" if sparse else ""
    target = case / f"{name}{suffix}.bin"
    log = execute(
        [
            binary,
            shader,
            fixtures / f"rays{suffix}.bin",
            fixtures / f"queries{suffix}.bin",
            *(fixtures / f"{n}.bin" for n in ("veil", "multi", "medium")),
            target,
            repeats,
            "--source-map",
            case / "solar.bin",
            "--gpu-timing",
        ],
        case,
        f"{name}{suffix}-capture",
    )
    return np.fromfile(target, SAMPLE), gpu_times(log)


def cohort(args, binary, execute, cover):
    case = args.output / f"cover-{cover}"
    case.mkdir(exist_ok=True)
    rays, queries = ray_controls()
    execute(
        [
            "nice",
            "-n",
            "10",
            sys.executable,
            ROOT / "test/experiments/weather/cloud_sun_reuse_cases.py",
            "--output",
            case,
            "--cover",
            cover,
        ],
        case,
        "prepare",
    )
    prepared = json.loads((case / "preparation.json").read_text())
    atlas_bytes = prepared["atlas_bytes"]
    expected = np.load(case / "reference.npy")
    profiles, numerical = [], {}
    last_records = rays["end"].astype(np.int64) - 1
    for name, reuse, diagnostic in (
        ("direct", 0, 0),
        ("reuse", 1, 0),
        ("direct-work", 0, 1),
        ("reuse-work", 1, 1),
    ):
        shader = compile_probe(args, case, name, reuse, diagnostic, execute, cover)
        samples, times = capture(
            binary, shader, args.output, case, name, execute, args.repeats
        )
        values = np.c_[samples["radiance"], samples["through"]]
        error = np.abs(values - expected)
        relative = error[:, :3] / np.maximum(abs(expected[:, :3]), 1e-5)
        if (
            not np.isfinite(values).all()
            or np.any(values < 0)
            or np.any(samples["through"] > 1)
            or np.any(samples["counts"] == 0xFFFFFFFF)
            or relative.max() > 0.01
            or error[:, 3:].max() > (2e-6 if cover == 0 else 1e-4)
        ):
            raise RuntimeError(f"invalid or inaccurate positive cohort: {name}/{cover}")
        if samples[::2].tobytes() != samples[1::2].tobytes():
            raise RuntimeError("repeated query changed full records")
        if diagnostic:
            air = samples["sources"] & 1023
            density = samples["sources"] >> 10
            textures = samples["counts"] >> 21
            if air.max() > 531 or density.max() > 12 * 531 or textures.max() > 3 * 531:
                raise RuntimeError("diagnostic work bound exceeded")
        else:
            air = samples["sources"]
            density = textures = np.zeros(len(samples), dtype="u4")
        numerical[name] = values
        profiles.append(
            {
                "profile": name,
                "max_relative_RGB": float(relative.max()),
                "transmittance_max_abs": float(error[:, 3:].max()),
                "dispatch_ms": times,
                "max_air_sources": int(air.max()),
                "sum_air_sources": int(air[last_records].sum(dtype=np.uint64)),
                "max_air_solar_density": int(density.max()),
                "sum_air_solar_density": int(
                    density[last_records].sum(dtype=np.uint64)
                ),
                "max_solar_texture_samples": int(textures.max()),
                "sum_solar_texture_samples": int(
                    textures[last_records].sum(dtype=np.uint64)
                ),
            }
        )
        if name == "reuse":
            sparse, _ = capture(
                binary, shader, args.output, case, name, execute, 0, True
            )
            sparse_rays, _ = ray_controls(True)
            for ray, short in zip(rays, sparse_rays, strict=True):
                full = samples[int(ray["begin"]) : int(ray["end"])].reshape(86, 2)
                part = sparse[int(short["begin"]) : int(short["end"])].reshape(18, 2)
                if full[::5].tobytes() != part.tobytes():
                    raise RuntimeError("sparse query schedule changed full records")
    for name in ("direct", "reuse"):
        if numerical[name].tobytes() != numerical[name + "-work"].tobytes():
            raise RuntimeError("diagnostic shader changed numeric output")
    if cover == 0 and numerical["direct"].tobytes() != numerical["reuse"].tobytes():
        raise RuntimeError("null cloud changed numeric output")
    result = {
        "rays": len(rays),
        "queries": len(queries),
        "scenes": prepared["scenes"],
        "atlas_bytes": atlas_bytes,
        "atlas_cpu_build_wall_s": prepared["atlas_cpu_build_wall_s"],
        "map_density_build_upper": 0 if cover == 0 else 6 * atlas_bytes // 4,
        "reference_refinement_max_abs": prepared["reference_refinement_max_abs"],
        "profiles": profiles,
        "map_bytes_per_virtual_camera": atlas_bytes // prepared["field_count"],
    }
    (case / "report.json").write_text(json.dumps(result, indent=2))
    print(cover, [(p["profile"], p["max_relative_RGB"]) for p in profiles], flush=True)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--api-root", type=Path, required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--archive-receipt", type=Path, required=True)
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument("--benchmark", action="store_true")
    args = parser.parse_args()
    args.output = args.output.resolve()
    if args.output.is_relative_to(ROOT) or not 0 <= args.repeats <= 120:
        parser.error("use explicit system-temp output and0..120 repeats")
    args.output.mkdir(parents=True, exist_ok=True)
    receipt = args.output / "receipt.json"
    receipt.unlink(missing_ok=True)
    record = {"commands": [], "cases": {}}

    def execute(argv, folder, name, cwd=ROOT):
        result = subprocess.run(
            list(map(str, argv)),
            cwd=cwd,
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

    record["commit"] = execute(
        ["git", "rev-parse", "HEAD"], args.output, "commit"
    ).strip()
    record["dirty"] = execute(
        ["git", "status", "--porcelain"], args.output, "status"
    ).strip()
    binary, record["consumer"] = build_consumer(args, execute)
    write_inputs(args.output)
    for cover in (0, 0.65, 1):
        record["cases"][str(cover)] = cohort(args, binary, execute, cover)
    if args.benchmark:
        from cloud_sun_reuse_benchmark import benchmark

        record["benchmark"] = benchmark(args, binary, execute, compile_probe, gpu_times)
    record["hashes"] = {
        str(p.relative_to(args.output)): digest(p)
        for p in args.output.rglob("*")
        if p.is_file() and p.suffix in (".bin", ".spv", ".npy", ".glsl")
    }
    record["source_hashes"] = {
        str(p.relative_to(ROOT)): digest(p)
        for p in (
            ROOT / "test/experiments/render/air_prefix_gpu.cpp",
            *sorted(
                p
                for p in (ROOT / "test/experiments/weather").iterdir()
                if p.suffix in (".py", ".glsl", ".comp")
            ),
            *sorted((ROOT / "src/generators/weather/shaders").glob("*.glsl")),
            *sorted((ROOT / "src/render/shaders").glob("*.glsl")),
            *sorted((ROOT / "src/world/sky").glob("*.h")),
        )
    }
    receipt.write_text(json.dumps(record, indent=2))


if __name__ == "__main__":
    main()
