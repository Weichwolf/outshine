"""Reject wrong native cloud-front assemblies and verify clear/moving controls."""

import argparse
import hashlib
import json
import os
import re
import shlex
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from cloud_view_reference import judge

ROOT = Path(__file__).resolve().parents[3]
CONTROLS = (
    ("ortho-origin", "CLOUD_VIEW_DROP_ORTHO_OFFSET", "declared-ortho"),
    ("camera-anchor", "CLOUD_VIEW_DROP_CAMERA_ANCHOR", "ortho-offset"),
    ("receiver-partition", "CLOUD_VIEW_CLIP_PARTITION", "declared-perspective"),
    ("double-front", "CLOUD_VIEW_DOUBLE_FRONT", "declared-perspective"),
    ("clear-opaque", "CLOUD_VIEW_CLEAR_OPAQUE_CONTROL", "captured-clear"),
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def motion(captures):
    def load(name):
        stem = captures / ("actual." + name)
        metadata = json.loads(Path(str(stem) + ".json").read_text())
        return np.fromfile(str(stem) + ".images.bin", "<f4").reshape(
            5, metadata["height"], metadata["width"], 4
        )

    base = load("declared-perspective")
    rows = []
    for name in ("wind-advance", "translation", "wind-motion", "yaw-rotation"):
        changed = load(name)
        if not np.array_equal(base[0], changed[0]) or not np.array_equal(
            base[2, :, :, 3], changed[2, :, :, 3]
        ):
            raise RuntimeError("motion comparison changed its actual scene/depth input")
        delta = float(np.max(np.abs(base[1] - changed[1])))
        if delta == 0.0:
            raise RuntimeError(f"{name} did not change the native field response")
        rows.append({"case": name, "max_composed_delta": delta})
    if (captures / "actual.declared-perspective.rays.bin").read_bytes() != (
        captures / "actual.wind-advance.rays.bin"
    ).read_bytes():
        raise RuntimeError("wind-only advancement changed its camera rays")
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-receipt", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private outputs belong in system temp")
    output.mkdir(parents=True, exist_ok=True)
    receipt = output / "receipt.json"
    receipt.unlink(missing_ok=True)
    native = json.loads(args.native_receipt.read_text())
    binary = Path(native["binary"])
    if digest(binary) != native["binary_sha256"]:
        parser.error("native binary no longer matches its producer receipt")
    definitions = native["parameters"]
    rows, commands = [], []

    def run(argv, name):
        argv = list(map(str, argv))
        result = subprocess.run(
            argv, cwd=ROOT, capture_output=True, text=True, timeout=180, check=False
        )
        (output / (name + ".stdout")).write_text(result.stdout)
        (output / (name + ".stderr")).write_text(result.stderr)
        commands.append({"argv": argv, "exit_code": result.returncode})
        if result.returncode:
            raise RuntimeError(f"{name} failed; inspect {output}")
        return result

    for name, flag, scenario in CONTROLS:
        directory = output / name
        directory.mkdir(exist_ok=True)
        shader = directory / "front.frag.spv"
        run(
            [
                *shlex.split(os.environ.get("GLSLANG", "glslangValidator")),
                "-V",
                "-Isrc",
                "-Isrc/render/shaders",
                "-Isrc/generators/weather/shaders",
                "-Itest/experiments/weather",
                *(f"-D{key}={value}" for key, value in definitions.items()),
                f"-D{flag}=1",
                ROOT / "test/experiments/weather/cloud_view_opaque.frag",
                "-o",
                shader,
            ],
            name + "-shader",
        )
        selected = native["shaders"].copy()
        selected[3] = str(shader)
        result = run(
            [binary, *selected, directory / "actual", "43", "31", scenario], name
        )
        if result.stderr or not re.search(
            r"^CHECKS \d+ FAILURES 0 SKIPPED 0 UNPREPARED 0 PARTIAL 0$",
            result.stdout,
            re.MULTILINE,
        ):
            raise RuntimeError(f"{name} has diagnostics or incomplete native checks")
        metadata = directory / ("actual." + scenario + ".json")
        row = judge(
            metadata,
            definitions["CLOUD_VIEW_SAMPLES"],
            definitions["CLOUD_LIGHT_SAMPLES"],
            definitions["CLOUD_VIEW_WARP"],
        )
        if name == "clear-opaque":
            original = args.native_receipt.parent / ("actual.captured-clear.images.bin")
            if (
                metadata.with_suffix(".images.bin").read_bytes()
                != original.read_bytes()
            ):
                raise RuntimeError("known-zero low layer differs from clear opaque")
            if not row["passed"]:
                raise RuntimeError("the exact clear control failed its reference")
        elif row["passed"]:
            raise RuntimeError(f"wrong {name} assembly escaped the unchanged limit")
        rows.append({"control": name, "reference": row, "spv_sha256": digest(shader)})
    translations = []
    for index, source in enumerate(
        [Path(path) for path in native["shaders"]]
        + [output / name / "front.frag.spv" for name, _, _ in CONTROLS]
    ):
        result = run(
            [
                *shlex.split(os.environ.get("SPIRV_CROSS", "spirv-cross")),
                source,
                "--msl",
                "--msl-version",
                "20300",
            ],
            "msl-" + str(index),
        )
        target = output / f"shader-{index}.metal"
        target.write_text(result.stdout)
        translations.append(
            {"spv_sha256": digest(source), "msl_sha256": digest(target)}
        )
    record = {
        "native_receipt_sha256": digest(args.native_receipt),
        "controls": rows,
        "motion": motion(args.native_receipt.parent),
        "msl_translations": translations,
        "commands": commands,
    }
    receipt.write_text(json.dumps(record, indent=2) + "\n")
    print(json.dumps({"controls": len(rows), "msl_translations": len(translations)}))


if __name__ == "__main__":
    main()
