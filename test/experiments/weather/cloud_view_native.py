"""Build the actual cloud-front raster consumer against a matching regular archive."""

import argparse
import hashlib
import json
import os
import shlex
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SHADERS = (
    "test/experiments/render/medium_view_probe.vert",
    "test/experiments/weather/cloud_view_scene.frag",
    "src/render/shaders/fullscreen.vert",
    "test/experiments/weather/cloud_view_opaque.frag",
)
SOURCES = tuple(
    f"test/experiments/weather/cloud_view_{name}.cpp"
    for name in ("native", "pipeline", "cases", "targets", "capture")
) + (
    "test/experiments/weather/cloud_sky_cases.cpp",
    "test/experiments/render/medium_view_reference.cpp",
    "test/experiments/render/medium_view_images.cpp",
)
INCLUDES = (
    "include",
    "src",
    "src/scenario",
    "src/render",
    "src/render/stages",
    "src/render/device",
    "src/render/device/native",
    "src/world/sky",
    "src/world/weather",
    "src/generators/weather",
    "src/base",
    "src/base/io",
    "src/base/format",
    "test/harness/shared",
    "test/harness/render",
    "test/experiments/render",
    "test/experiments/weather",
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive-root", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--view-samples", type=int, choices=range(1, 65), default=16)
    parser.add_argument("--light-samples", type=int, choices=range(1, 33), default=6)
    parser.add_argument("--warp", type=int, choices=(1, 2), default=1)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--boundary-sky", action="store_true")
    mode.add_argument("--cumulus-sky", action="store_true")
    mode.add_argument("--stratus-sky", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private outputs belong in the system temporary directory")
    output.mkdir(parents=True, exist_ok=True)
    (output / "receipt.json").unlink(missing_ok=True)
    archive_root = args.archive_root.resolve()
    archive = archive_root / "build/liboutshine.a"
    if not archive.is_file():
        parser.error(
            "build the regular production archive with the project's toolchain first"
        )
    record = {"commands": []}

    def run(command, name, cwd=ROOT):
        command = list(map(str, command))
        result = subprocess.run(
            command, cwd=cwd, capture_output=True, text=True, timeout=180, check=False
        )
        (output / f"{name}.stdout").write_text(result.stdout)
        (output / f"{name}.stderr").write_text(result.stderr)
        record["commands"].append(
            {"argv": command, "cwd": str(cwd), "exit_code": result.returncode}
        )
        if result.returncode:
            raise RuntimeError(f"{name} failed; inspect {output}")
        return result.stdout

    producer = run(["git", "rev-parse", "HEAD"], "producer", archive_root).strip()
    if run(
        ["git", "status", "--porcelain", "--", "src", "include"],
        "producer-clean",
        archive_root,
    ):
        raise RuntimeError("archive producer has changed production inputs")
    members = run(["ar", "t", archive], "archive-members").splitlines()
    for prefix in ("src-render-stages-MediumView.", "src-generators-weather-LowCloud."):
        if sum(member.startswith(prefix) for member in members) != 1:
            raise RuntimeError(
                f"regular archive must contain exactly one {prefix} object"
            )
    flags = dict(
        line.split("=", 1)
        for line in (ROOT / "test/run.sh").read_text().splitlines()
        if line.startswith(("CXXSTD=", "WARN=", "OPT="))
    )
    toolchain = [
        flag
        for key in ("CXXSTD", "WARN", "OPT")
        for flag in shlex.split(flags[key].strip('"'))
    ]
    link = shlex.split(run(["sh", "test/run.sh", "--native-link-flags"], "native-link"))
    binary = output / "cloud-view-native"
    run(
        [
            "nice",
            "-n",
            "10",
            *shlex.split(os.environ.get("CXX", "c++")),
            *toolchain,
            "-fno-exceptions",
            *(
                f"-I{archive_root / path if path.startswith(('src', 'include')) else ROOT / path}"
                for path in INCLUDES
            ),
            *SOURCES,
            archive,
            *link,
            "-o",
            binary,
        ],
        "build",
    )
    glslang = shlex.split(os.environ.get("GLSLANG", "glslangValidator"))
    shader_paths = []
    definitions = {
        "CLOUD_VIEW_SAMPLES": args.view_samples,
        "CLOUD_LIGHT_SAMPLES": args.light_samples,
        "CLOUD_VIEW_WARP": args.warp,
        "CLOUD_VIEW_BOUNDARY_SKY": int(
            args.boundary_sky or args.cumulus_sky or args.stratus_sky
        ),
    }
    for source in SHADERS:
        target = output / (Path(source).name + ".spv")
        run(
            [
                "nice",
                "-n",
                "10",
                *glslang,
                "-V",
                "-Isrc",
                "-Isrc/render/shaders",
                "-Isrc/generators/weather/shaders",
                "-Itest/experiments/weather",
                *(f"-D{key}={value}" for key, value in definitions.items()),
                source,
                "-o",
                target,
            ],
            Path(source).name,
        )
        shader_paths.append(str(target))
    run(
        [
            binary,
            *shader_paths,
            output / "actual",
            *(
                [43, 31, "all", "cumulus-sky"]
                if args.cumulus_sky
                else [43, 31, "all", "stratus-sky"]
                if args.stratus_sky
                else [43, 31, "all", "boundary-sky"]
                if args.boundary_sky
                else []
            ),
        ],
        "actual",
    )
    record.update(
        commit=run(["git", "rev-parse", "HEAD"], "commit").strip(),
        dirty=run(["git", "status", "--porcelain"], "status").strip(),
        producer_commit=producer,
        archive_sha256=digest(archive),
        binary_sha256=digest(binary),
        sources={
            path: digest(ROOT / path)
            for path in SOURCES
            + SHADERS
            + (
                "test/experiments/weather/CloudViewProbe.h",
                "test/experiments/render/MediumViewProbe.h",
                "test/harness/shared/WeatherCapture.h",
                "src/render/shaders/mediumView.glsl",
                "src/render/shaders/mediumViewAir.glsl",
                "src/render/shaders/mediumOpaque.glsl",
                "test/experiments/weather/cloud_air_prefix.glsl",
                "test/experiments/weather/cloud_view_boundary.glsl",
                "src/generators/weather/shaders/cloudLayer.glsl",
                "src/generators/weather/shaders/cloudCumulus.glsl",
            )
        },
        parameters=definitions,
        binary=str(binary),
        shaders=shader_paths,
    )
    (output / "receipt.json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps({"output": str(output), "archive_commit": producer}))


if __name__ == "__main__":
    main()
