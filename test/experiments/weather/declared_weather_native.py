"""Render a production-parsed Engine weather declaration with the native cloud consumer."""

import argparse
import json
import os
import shlex
import subprocess
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

from cloud_view_native import INCLUDES, SHADERS, SOURCES, digest

ROOT = Path(__file__).resolve().parents[3]
SOURCE = "test/experiments/weather/declared_weather_native.cpp"
INPUT = "test/outshine/data/weather/declared-overcast.scenario"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--archive-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--width", type=int, default=43)
    parser.add_argument("--height", type=int, default=31)
    parser.add_argument("--measure-cost", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private captures belong in system temp")
    if not all(0 < value <= 8192 for value in (args.width, args.height)):
        parser.error("capture extents must be in1..8192")
    output.mkdir(parents=True, exist_ok=True)
    archive_root = args.archive_root.resolve()
    archive = archive_root / "build/liboutshine.a"
    if not archive.is_file():
        parser.error("build the complete regular matching archive first")
    (output / "receipt.json").unlink(missing_ok=True)
    record = {"commands": []}

    def run(command, name, cwd=ROOT, native=False):
        command = list(map(str, command))
        result = subprocess.run(
            command, cwd=cwd, capture_output=True, text=True, timeout=180, check=False
        )
        (output / f"{name}.stdout").write_text(result.stdout)
        (output / f"{name}.stderr").write_text(result.stderr)
        record["commands"].append(
            {"argv": command, "cwd": str(cwd), "exit_code": result.returncode}
        )
        if result.returncode or (native and result.stderr):
            raise RuntimeError(
                f"{name} failed or emitted validation diagnostics: {output}"
            )
        return result.stdout

    producer = run(["git", "rev-parse", "HEAD"], "producer", archive_root).strip()
    if run(
        ["git", "status", "--porcelain", "--", "src", "include"],
        "producer-clean",
        archive_root,
    ):
        raise RuntimeError("the archive producer must have clean production inputs")
    members = run(["ar", "t", archive], "archive-members").splitlines()
    for prefix in (
        "src-engine-WeatherRuntime.",
        "src-generators-weather-DeclaredWeather.",
    ):
        if sum(name.startswith(prefix) for name in members) != 1:
            raise RuntimeError(
                f"the regular archive must contain exactly one {prefix} object"
            )
    flags = dict(
        line.split("=", 1)
        for line in (archive_root / "test/run.sh").read_text().splitlines()
        if line.startswith(("CXXSTD=", "WARN=", "OPT="))
    )
    toolchain = [
        flag
        for key in ("CXXSTD", "WARN", "OPT")
        for flag in shlex.split(flags[key].strip('"'))
    ]
    link = shlex.split(
        run(["sh", "test/run.sh", "--engine-link-flags"], "engine-link", archive_root)
    )
    sources = (SOURCE,) + tuple(
        name for name in SOURCES if not name.endswith("/cloud_view_native.cpp")
    )
    binary = output / "declared-weather-native"
    compiler = [
        "nice",
        "-n",
        "10",
        *shlex.split(os.environ.get("CXX", "c++")),
        *toolchain,
        "-fno-exceptions",
        *(
            f"-I{archive_root / path if path.startswith(('src', 'include')) else ROOT / path}"
            for path in INCLUDES + ("src/scenario",)
        ),
    ]
    run(
        [
            *compiler,
            *sources,
            archive,
            *link,
            "-o",
            binary,
        ],
        "build",
    )
    if args.measure_cost:
        cost_source = "test/experiments/weather/declared_weather_cost.cpp"
        cost_binary = output / "declared-weather-cost"
        run(
            [*compiler, "-gdwarf-4", cost_source, archive, *link, "-o", cost_binary],
            "build-cost",
        )
        run([cost_binary], "cost", archive_root)
        sources += (cost_source,)
        record["cost_binary_sha256"] = digest(cost_binary)
    definitions = {
        "CLOUD_VIEW_SAMPLES": 32,
        "CLOUD_LIGHT_SAMPLES": 12,
        "CLOUD_VIEW_WARP": 1,
        "CLOUD_VIEW_BOUNDARY_SKY": 1,
    }
    shaders = []
    for source in SHADERS:
        target = output / (Path(source).name + ".spv")
        run(
            [
                "nice",
                "-n",
                "10",
                *shlex.split(os.environ.get("GLSLANG", "glslangValidator")),
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
        shaders.append(target)
    inputs = []
    for name in ("cloudy", "overcast", "clear", "missing-low"):
        folder = output / name
        folder.mkdir(exist_ok=True)
        original = (ROOT / INPUT).read_bytes()
        path = folder / "input.scenario"
        if name == "cloudy":
            path.write_bytes(original)
        else:
            tree = ET.fromstring(original)
            weather = tree.find("weather")
            if name == "overcast":
                weather.set("cloudLow", "1")
            elif name == "clear":
                weather.set("cloudLow", "0")
            else:
                del weather.attrib["cloudLow"]
            path.write_bytes(ET.tostring(tree))
        run(
            [binary, *shaders, folder / "actual", path, args.width, args.height],
            "native-" + name,
            native=True,
        )
        inputs.append({"case": name, "path": str(path), "sha256": digest(path)})
    record.update(
        source_commit=run(["git", "rev-parse", "HEAD"], "commit").strip(),
        source_dirty=run(["git", "status", "--porcelain"], "status"),
        producer_commit=producer,
        archive_sha256=digest(archive),
        binary_sha256=digest(binary),
        source_sha256={
            name: digest(ROOT / name)
            for name in sources
            + SHADERS
            + (INPUT, "test/experiments/weather/CloudViewProbe.h")
        },
        shaders={str(path): digest(path) for path in shaders},
        inputs=inputs,
        definitions=definitions,
        dimensions=[args.width, args.height],
        scope="Actual Engine publication and native raster; controlled air/light/geometry recipe, no productive Place/frame/device acceptance",
    )
    (output / "receipt.json").write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps({"cases": len(inputs), "producer_commit": producer}))


if __name__ == "__main__":
    main()
