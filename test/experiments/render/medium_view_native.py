"""Replay native camera-ray and opaque-air controls against the regular archive."""

import argparse
import hashlib
import json
import os
import re
import shlex
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SOURCES = tuple(
    ROOT / f"test/experiments/render/medium_view_{name}.cpp"
    for name in ("native", "geometry", "reference", "opaque", "images")
)
SHADERS = (
    ROOT / "test/experiments/render/medium_view_probe.vert",
    ROOT / "test/experiments/render/medium_view_probe.frag",
    ROOT / "src/render/shaders/fullscreen.vert",
    ROOT / "test/experiments/render/medium_view_opaque.frag",
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verdict(run, expected):
    rows = re.findall(
        r"^CHECKS (\d+) FAILURES (\d+) SKIPPED (\d+) UNPREPARED (\d+) PARTIAL (\d+)$",
        run.stdout,
        re.MULTILINE,
    )
    if run.stderr or len(rows) != 1 or any(map(int, rows[0][2:])):
        raise RuntimeError("native run has diagnostics or an incomplete verdict")
    if (int(rows[0][1]) == 0) != (expected == 0):
        raise RuntimeError("native verdict disagrees with the expected control")
    return tuple(map(int, rows[0]))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT):
        parser.error("private outputs belong outside the checkout")
    archive = ROOT / "build/liboutshine.a"
    if not archive.is_file():
        parser.error("build the regular production archive with make all first")
    output.mkdir(parents=True, exist_ok=True)
    receipt = output / "receipt.json"
    receipt.unlink(missing_ok=True)
    record = {"commands": [], "cases": {}}
    compiler = shlex.split(os.environ.get("CXX", "c++"))
    declarations = dict(
        line.split("=", 1)
        for line in (ROOT / "test/run.sh").read_text().splitlines()
        if line.startswith(("CXXSTD=", "WARN=", "OPT="))
    )
    toolchain = [
        flag
        for key in ("CXXSTD", "WARN", "OPT")
        for flag in shlex.split(declarations[key].strip('"'))
    ]
    glslang = shlex.split(os.environ.get("GLSLANG", "glslangValidator"))

    def run(argv, name, expected=0):
        result = subprocess.run(
            list(map(str, argv)),
            cwd=ROOT,
            check=False,
            capture_output=True,
            text=True,
            timeout=180,
        )
        (output / f"{name}.stdout").write_text(result.stdout)
        (output / f"{name}.stderr").write_text(result.stderr)
        record["commands"].append(
            {"argv": list(map(str, argv)), "exit": result.returncode}
        )
        if result.returncode != expected:
            raise RuntimeError(f"unexpected terminal exit for {name}; inspect {output}")
        return result

    record["commit"] = run(["git", "rev-parse", "HEAD"], "commit").stdout.strip()
    record["dirty"] = run(["git", "status", "--porcelain"], "status").stdout.strip()
    members = run(["ar", "t", archive], "members").stdout.splitlines()
    if (
        sum(member.startswith("src-render-stages-MediumView.") for member in members)
        != 1
    ):
        raise RuntimeError(
            "the regular archive must contain the production MediumView unit"
        )
    link = shlex.split(
        run(["sh", "test/run.sh", "--native-link-flags"], "link-flags").stdout
    )
    shaders = []
    for source in SHADERS:
        target = output / (source.name + ".spv")
        run(
            [
                "nice",
                "-n",
                "10",
                *glslang,
                "-V",
                "-Isrc",
                "-Isrc/render/shaders",
                source,
                "-o",
                target,
            ],
            source.name,
        )
        shaders.append(target)
    binary = output / "medium-view-native"
    includes = (
        "include",
        "src",
        "src/render",
        "src/render/device",
        "src/render/device/native",
        "src/render/stages",
        "src/world/sky",
        "test/harness/shared",
        "test/harness/render",
    )
    run(
        [
            "nice",
            "-n",
            "10",
            *compiler,
            *toolchain,
            "-fno-exceptions",
            *(f"-I{directory}" for directory in includes),
            *SOURCES,
            archive,
            *link,
            "-o",
            binary,
        ],
        "build",
    )
    for name in ("actual", "repeat"):
        capture = run([binary, *shaders, output / name], name)
        record["cases"][name] = {"verdict": verdict(capture, 0)}
    for suffix in ("geometry", "opaque", "opaque.reference"):
        if (output / f"actual.{suffix}").read_bytes() != (
            output / f"repeat.{suffix}"
        ).read_bytes():
            raise RuntimeError(f"repeated native output changed: {suffix}")
    record["bitwise_repeat"] = True
    negative = output / "negative"
    negative.mkdir(exist_ok=True)
    controls = (
        (
            "depth-cancellation",
            "mediumView.glsl",
            "  float delta = fragment.z - frame.OrthoProjection.w;\n  float alongViewM = (depth.x - delta * depth.y) / (depth.z + delta * depth.w);",
            "  float foldedA = depth.x + frame.OrthoProjection.w * depth.y;\n  float foldedC = depth.z - frame.OrthoProjection.w * depth.w;\n  float alongViewM = (foldedA - fragment.z * depth.y) / (foldedC + fragment.z * depth.w);",
            1,
            "actual native fragment rays match independently projected world geometry",
        ),
        (
            "geometry",
            "mediumView.glsl",
            "  vec3 offsetKm = offsetM * 0.001;",
            "  offsetM = vec3(0.0);\n  vec3 offsetKm = offsetM * 0.001;",
            1,
            "actual native fragment rays match independently projected world geometry",
        ),
        (
            "opaque",
            "mediumOpaque.glsl",
            "lit.rgb * air.Transmittance + frame.Illuminance * air.RadiancePerLux",
            "lit.rgb * max(air.Transmittance, vec3(1.0)) + frame.Illuminance * max(-air.RadiancePerLux, vec3(0.0))",
            3,
            "native opaque transport matches independent homogeneous radiative transfer",
        ),
    )
    for name, module, original, replacement, slot, claim in controls:
        text = (ROOT / "src/render/shaders" / module).read_text()
        if text.count(original) != 1:
            raise RuntimeError(f"negative control no longer matches {module}")
        (negative / module).write_text(text.replace(original, replacement))
        target = negative / f"{name}.spv"
        run(
            [
                "nice",
                "-n",
                "10",
                *glslang,
                "-V",
                f"-I{negative}",
                "-Isrc",
                "-Isrc/render/shaders",
                SHADERS[slot],
                "-o",
                target,
            ],
            f"negative-{name}-build",
        )
        selected = shaders.copy()
        selected[slot] = target
        captured = run(
            [binary, *selected, output / f"negative-{name}"], f"negative-{name}", 1
        )
        record["cases"][f"negative-{name}"] = {"verdict": verdict(captured, 1)}
        if claim not in captured.stdout:
            raise RuntimeError(
                f"negative control {name} did not fail its intended assertion"
            )
        (negative / module).unlink()
    dependencies = (
        *SOURCES,
        *SHADERS,
        *shaders,
        *negative.glob("*.spv"),
        Path(__file__),
        archive,
        binary,
        ROOT / "test/experiments/render/MediumViewProbe.h",
        ROOT / "src/render/stages/MediumView.cpp",
        ROOT / "src/render/stages/MediumView.h",
        ROOT / "src/render/stages/MediumViewParameters.h",
        ROOT / "src/render/shaders/mediumView.glsl",
        ROOT / "src/render/shaders/mediumViewAir.glsl",
        ROOT / "src/render/shaders/mediumOpaque.glsl",
    )
    record["sha256"] = {str(path): digest(path) for path in dependencies}
    record["captures_sha256"] = {
        str(path): digest(path)
        for name in (
            "actual",
            "repeat",
            *(f"negative-{control[0]}" for control in controls),
        )
        for path in output.glob(name + ".*")
        if path.suffix in (".geometry", ".opaque", ".reference")
    }
    record["scope"] = (
        "Actual native raster camera geometry and homogeneous clear-air controls; no joint-cloud/Thin/Place/temporal/device-budget acceptance."
    )
    receipt.write_text(json.dumps(record, indent=2) + "\n")
    print(
        json.dumps(
            {
                "commit": record["commit"],
                "cases": record["cases"],
                "bitwise_repeat": True,
            }
        )
    )


if __name__ == "__main__":
    main()
