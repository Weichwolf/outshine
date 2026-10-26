"""Compare original boundary placements using native vertex arithmetic/readback.

Requires a prepared native archive and its compilation database. This targeted
probe neither builds the application nor certifies world coverage or frame time.
"""

import argparse
import hashlib
import json
import os
import re
import shlex
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def digest(path):
    checksum = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            checksum.update(block)
    return checksum.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--device-name", required=True, help="exact native device name")
    parser.add_argument("--build-root", type=Path, default=ROOT)
    args = parser.parse_args()
    output = args.output.resolve()
    temporary_roots = (Path(tempfile.gettempdir()).resolve(), Path("/tmp").resolve())
    if not any(output.is_relative_to(path) for path in temporary_roots):
        parser.error("private output must be in a system temporary directory")
    build = args.build_root.resolve()
    archive = build / "build/liboutshine.a"
    database = build / "compile_commands.json"
    if not archive.is_file() or not database.is_file():
        parser.error("prepare the native archive and compilation database first")
    output.mkdir(parents=True, exist_ok=True)
    fixture = ROOT / "test/outshine/data/render/shared_frame/fixture.json"
    source = ROOT / "test/harness/render/NativeSharedFrameProbe.cpp"
    vertex = Path(__file__).with_suffix(".vert")
    fragment = Path(__file__).with_suffix(".frag")
    contracts = (
        "src/render/stages/GpuPlacement.h",
        "src/render/shaders/geometryPlacements.glsl",
        "src/render/shaders/geometryView.glsl",
    )
    if any(digest(ROOT / name) != digest(build / name) for name in contracts):
        parser.error(
            "the supplied archive has a different placement contract; rebuild it"
        )
    product = (ROOT / "src/render/shaders/litVertex.glsl").read_text()
    start = product.index("  mat4 m = placement.current;")
    end = product.index("  position = world.xyz;", start)
    expression = product[start:end].replace("s.shift.xyz", "pointShift.xyz")
    expression = expression.replace("gl_Position =", "captured =")
    compact = lambda text: re.sub(r"\s+", "", text)
    if compact(expression) not in compact(vertex.read_text()):
        parser.error("diagnostic and product placement expressions differ")
    rows = []

    def run(command, name, *, cwd=build):
        result = subprocess.run(
            command, cwd=cwd, text=True, capture_output=True, timeout=600, check=False
        )
        log = output / (name + ".log")
        log.write_text(result.stdout + result.stderr)
        rows.append(
            {
                "command": list(map(str, command)),
                "cwd": str(cwd),
                "returncode": result.returncode,
                "log": str(log),
            }
        )
        (output / "commands.json").write_text(json.dumps(rows, indent=2))
        if result.returncode:
            raise SystemExit(f"{name} exited {result.returncode}; see {log}")
        return result

    glslang = shlex.split(os.environ.get("GLSLANG", "glslangValidator"))
    shaders = []
    for stage in (vertex, fragment):
        target = output / (stage.name + ".spv")
        run(
            [
                "nice",
                "-n",
                "10",
                *glslang,
                "-V",
                "--target-env",
                "vulkan1.0",
                "-I" + str(ROOT / "src/render/shaders"),
                str(stage),
                "-o",
                str(target),
            ],
            stage.suffix[1:] + "-compile",
        )
        shaders.append(target)
    entries = json.loads(database.read_text())
    entry = next(
        row for row in entries if Path(row["file"]).name == "StagePipelines.cpp"
    )
    command = list(entry.get("arguments") or shlex.split(entry["command"]))
    command[command.index(entry["file"])] = str(source)
    command[command.index("-o") + 1] = str(output / "native-shared-frame")
    for flag in ("-MF", "-MT", "-MJ"):
        if flag in command:
            command[command.index(flag) + 1] = str(
                output / ("native-shared-frame" + flag)
            )
    command += [
        "-I" + str(ROOT / "test/harness/render"),
        "-I" + str(ROOT / "test/harness/shared"),
    ]
    (output / "compile_commands.json").write_text(
        json.dumps(
            [
                {
                    "file": str(source),
                    "directory": str(build),
                    "command": shlex.join(command),
                }
            ],
            indent=2,
        )
    )
    command.remove("-c")
    flags = run(["sh", "test/run.sh", "--native-link-flags"], "link-flags").stdout
    command += [str(archive), *shlex.split(flags)]
    run(["nice", "-n", "10", *command], "build")
    result = run(
        [
            str(output / "native-shared-frame"),
            str(fixture),
            *map(str, shaders),
            str(output / "result.json"),
            args.device_name,
        ],
        "run",
    )
    verdict = re.findall(
        r"^CHECKS (\d+) FAILURES 0 SKIPPED 0 UNPREPARED 0 PARTIAL 0$",
        result.stdout,
        re.MULTILINE,
    )
    if len(verdict) != 1 or result.stderr.strip():
        raise SystemExit("native verdict/validation was not clean; see run.log")
    receipt = {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=ROOT, text=True
        ).strip(),
        "build_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=build, text=True
        ).strip(),
        "source_dirty": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain"], cwd=ROOT, text=True
            )
        ),
        "files": {
            str(path.relative_to(ROOT)): digest(path)
            for path in (fixture, source, vertex, fragment, Path(__file__))
        },
        "archive_sha256": digest(archive),
        "placement_contract": {name: digest(ROOT / name) for name in contracts},
        "checks": int(verdict[0]),
        "scope": "diagnostic vertex arithmetic/flat RGBA32F readback; no unchanged product "
        "PSO, raster/LOD/normal/temporal/shadow/world/device-budget acceptance",
    }
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2))
    print(
        json.dumps({"checks": receipt["checks"], "result": str(output / "result.json")})
    )


if __name__ == "__main__":
    main()
