"""Build native block bounds and verify them against exact rational references."""

import argparse
import hashlib
import itertools
import json
import os
import random
import shlex
import subprocess
import tempfile
import time
from fractions import Fraction
from pathlib import Path

import adaptive
import verify

SOURCE = Path(__file__).resolve().parent


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def execute(command, directory, output, name, steps):
    row = {"name": name, "command": list(map(str, command)), "returncode": None}
    steps.append(row)
    start = time.monotonic()
    try:
        with (output / f"{name}.log").open("w") as log:
            result = subprocess.run(
                row["command"],
                cwd=directory,
                stdout=log,
                stderr=subprocess.STDOUT,
                check=False,
            )
        row["returncode"] = result.returncode
        if result.returncode:
            raise RuntimeError(f"{name}: {result.returncode}")
    finally:
        row["seconds"] = time.monotonic() - start


def native_checks(inputs, outputs):
    rng = random.Random(inputs["seed"])
    comparisons = 0
    for source, result in zip(inputs["cases"], outputs["cases"], strict=True):
        supported = all(
            [m[3], m[7], m[11], m[15]] == [0, 0, 0, 1] for m in source["lights"]
        )
        if result["prepared"] != supported:
            raise ValueError(f"unexpected prepared state: {source['name']}")
        if not supported:
            continue
        if result["queries"][2]["known"]:
            raise ValueError("query outside the prepared error domain accepted")
        for query_index in (0, 1):
            query = result["queries"][query_index]
            if not query["known"]:
                raise ValueError("supported query unexpectedly unknown")
            domain = source["queries"][query_index]
            points = [
                tuple(lo + (hi - lo) * rng.random() for lo, hi in domain)
                for _ in range(64)
            ]
            points.extend(itertools.product(*domain))
            for point in points:
                exact, actual = verify.reference(
                    point, source["inverse"], source["eye"], source["lights"]
                )
                for i, bounds in enumerate(query["bounds"]):
                    for axis, (lo, hi) in enumerate(bounds):
                        if (
                            not Fraction(lo) <= exact[i][axis] <= Fraction(hi)
                            or not lo <= actual[i][axis] <= hi
                        ):
                            raise ValueError(
                                f"native bound escaped: {source['name']}, {point}, {i}, {axis}"
                            )
                        comparisons += 1
    if (
        outputs["steps"]["failures"]
        or outputs["coverage"]["failures"]
        or not outputs["rounding_mode_rejected"]
    ):
        raise ValueError("native primitive checks failed")
    return comparisons


def frame_cases():
    matrix = verify.identity()
    regular = {
        "name": "plane",
        "inverse": matrix,
        "eye": [0.0, 0.0, 0.0],
        "width": 16,
        "height": 16,
        "depth": [0.5] * 256,
        "atlas": 128,
        "regions": [{"matrix": matrix, "x": 0, "y": 0}],
    }
    empty = dict(regular, name="empty", depth=[0.0] * 256)
    mismatch = dict(regular, name="mismatched-depth", depth=[0.5] * 255)
    light = verify.identity()
    light[3] = 0.1
    unknown = dict(
        regular, name="unsupported-light", regions=[{"matrix": light, "x": 0, "y": 0}]
    )
    split = dict(
        regular,
        name="discontinuous",
        width=17,
        height=9,
        depth=[0.0 if i % 5 == 0 else 0.25 if i % 2 else 0.75 for i in range(153)],
    )
    return [regular, empty, mismatch, unknown, split]


def check_frames(inputs, outputs):
    checked = 0
    for source, result in zip(inputs, outputs, strict=True):
        if source["name"] == "mismatched-depth":
            if result["accepted"] or result["repeated"]:
                raise ValueError("invalid depth extent accepted")
            continue
        if not result["accepted"] or not result["repeated"]:
            raise ValueError("valid frame extent rejected")
        if source["name"] == "unsupported-light":
            if not all(result["mask"]) or result["unknown"] != 1:
                raise ValueError("unknown frame failed to retain all receivers")
            continue
        frame = {
            "width": source["width"],
            "height": source["height"],
            "inverse_camera_clip": source["inverse"],
            "eye": source["eye"],
            "shadow": {
                "side": source["atlas"],
                "regions": [
                    {
                        "projection": r["matrix"],
                        "atlas": [r["x"] / source["atlas"], r["y"] / source["atlas"]],
                    }
                    for r in source["regions"]
                ],
            },
        }
        expected, _ = adaptive.scalar(source["depth"], frame)
        side = source["atlas"] // 16
        for y, row in enumerate(expected):
            for x in range(side):
                if row >> x & 1 and not result["mask"][y * side + x]:
                    raise ValueError(
                        f"native frame lost scalar receiver: {source['name']}, {x}, {y}"
                    )
                checked += 1
        if result["repeat_word_updates"] != 0:
            raise ValueError("unchanged frame repeated union work")
    return checked


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compile-database", required=True, type=Path)
    parser.add_argument("--frame", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    output.relative_to(Path(tempfile.gettempdir()).resolve())
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        parser.error("output must be empty")
    database = args.compile_database.resolve()
    entry = next(
        r
        for r in json.loads(database.read_text())
        if Path(r["file"]).name == "PreparedRoadManifests.cpp"
    )
    directory = Path(entry["directory"])
    steps = []
    receipt = {
        "source_commit": subprocess.check_output(
            ["git", "rev-parse", "HEAD"], cwd=SOURCE, text=True
        ).strip(),
        "source_worktree_dirty": bool(
            subprocess.check_output(
                ["git", "status", "--porcelain"], cwd=SOURCE, text=True
            )
        ),
        "sources_sha256": {
            str(p): digest(p) for p in sorted(SOURCE.iterdir()) if p.is_file()
        },
        "compile_database_sha256": digest(database),
        "frame_sha256": digest(args.frame),
        "coverage_header_sha256": digest(
            SOURCE.parent / "receiver_coverage/Coverage.h"
        ),
        "native_transform_header_sha256": digest(
            directory / "src/base/math/TransformMatrix.h"
        ),
        "load_before": list(os.getloadavg()),
        "steps": steps,
        "success": False,
        "scope": "Native CPU block bounds/bit stepping/mask checks; no engine archive, GPU contract or page admission",
    }
    try:
        commands = []
        for name in ("Projection", "Receiver", "NativeProbe", "ExportCamera"):
            command = list(entry.get("arguments") or shlex.split(entry["command"]))
            command[command.index(entry["file"])] = str(SOURCE / f"{name}.cpp")
            command[command.index("-o") + 1] = str(output / f"{name}.o")
            for flag, suffix in (("-MF", ".d"), ("-MJ", "-command.json")):
                if flag in command:
                    command[command.index(flag) + 1] = str(output / f"{name}{suffix}")
            if "-ffp-contract=off" not in command or any(
                f in command
                for f in ("-ffast-math", "-Ofast", "-funsafe-math-optimizations")
            ):
                raise ValueError(
                    "block reference requires strict sequential floating-point flags"
                )
            commands.append(
                {
                    "directory": str(directory),
                    "file": str(SOURCE / f"{name}.cpp"),
                    "arguments": command,
                }
            )
        (output / "compile_commands.json").write_text(
            json.dumps(commands, indent=2) + "\n"
        )
        for row in commands:
            execute(
                ["nice", "-n", "10", *row["arguments"]],
                directory,
                output,
                "compile-" + Path(row["file"]).stem,
                steps,
            )
        common = [
            commands[0]["arguments"][0],
            *[
                f
                for f in commands[0]["arguments"]
                if f.startswith(("-std=", "-stdlib=", "-fsanitize=", "-fuse-ld="))
            ],
        ]
        execute(
            [
                "nice",
                "-n",
                "10",
                *common,
                output / "Projection.o",
                output / "Receiver.o",
                output / "NativeProbe.o",
                "-o",
                output / "probe",
            ],
            directory,
            output,
            "link-probe",
            steps,
        )
        execute(
            [
                "nice",
                "-n",
                "10",
                *common,
                output / "ExportCamera.o",
                "-o",
                output / "export-camera",
            ],
            directory,
            output,
            "link-export",
            steps,
        )
        receipt["binary_sha256"] = digest(output / "probe")
        receipt["export_binary_sha256"] = digest(output / "export-camera")
        execute(
            [output / "export-camera", args.frame.resolve(), output / "frame.json"],
            directory,
            output,
            "export",
            steps,
        )
        frame = json.loads((output / "frame.json").read_text())
        cases = []
        for name, box, inverse, eye, lights in verify.cases(frame):
            cases.append(
                {
                    "name": name,
                    "inverse": inverse,
                    "eye": eye,
                    "lights": lights,
                    "frame": box,
                    "queries": [
                        box,
                        [[sum(a) / 2] * 2 for a in box],
                        [[box[0][0] - 1, box[0][1]], box[1], box[2]],
                    ],
                }
            )
        inputs = {"seed": 1847703, "cases": cases, "frames": frame_cases()}
        (output / "input.json").write_text(json.dumps(inputs, indent=2) + "\n")
        execute(
            [
                "nice",
                "-n",
                "10",
                output / "probe",
                output / "input.json",
                output / "native.json",
            ],
            directory,
            output,
            "native",
            steps,
        )
        comparisons = native_checks(
            inputs, json.loads((output / "native.json").read_text())
        )
        frame_checks = check_frames(
            inputs["frames"], json.loads((output / "native.json").read_text())["frames"]
        )
        (output / "verification.json").write_text(
            json.dumps(
                {
                    "axis_comparisons": comparisons,
                    "frame_cell_checks": frame_checks,
                    "success": True,
                },
                indent=2,
            )
            + "\n"
        )
        receipt["load_after"] = list(os.getloadavg())
        receipt["success"] = True
    finally:
        (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")


if __name__ == "__main__":
    main()
