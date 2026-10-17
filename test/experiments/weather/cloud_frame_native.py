"""Feed the production C++ CloudFrame bytes to the verified native GPU probe."""

import argparse
import json
import os
import shlex
import tempfile
from pathlib import Path

import numpy as np
from cloud_density_native import digest
from cloud_wind_native import (
    ROOT,
    Probe,
    anchor_cases,
    check_density,
    default_cases,
    field_cases,
    frame,
    plane_cases,
    reference,
    temporal_cases,
)
from cloud_wind_precision import MONTH_S, VELOCITY, points


class Producer:
    def __init__(self, probe, command_file):
        self.probe = probe
        self.binary = probe.output / "cloud-frame-pack"
        self.records = []
        command = shlex.split(command_file.read_text())
        test_main = "test/outshine/src/generators/weather/CloudFrame/LatticePackingRetainsSignedWindAndAnchors.cpp"
        if command.count(test_main) != 1:
            raise ValueError("use the actual normal CloudFrame module-test command")
        command[command.index(test_main)] = (
            "test/experiments/weather/cloud_frame_pack.cpp"
        )
        if any("sanitize" in arg for arg in command):
            raise ValueError("the byte producer uses the normal production module")
        self.objects = {arg: digest(Path(arg)) for arg in command if arg.endswith(".o")}
        if len(self.objects) != 1:
            raise ValueError(
                "the pure CloudFrame closure has exactly one product object"
            )
        probe.run(
            ["nice", "-n", "10", *command, "-Itest/harness/shared", "-o", self.binary],
            "build-cpp-producer",
        )

    def __call__(self, cover, profile, velocity, phase, origin=None, split=True):
        if not split:
            return frame(cover, profile, velocity, phase, origin, split=False)
        origin = np.zeros(3) if origin is None else np.asarray(origin, dtype=float)
        values = np.array(
            [6360000, 2572, 600, 0.012, cover, profile, *velocity, phase, *origin],
            dtype="<f8",
        )
        name = f"cpp-{len(self.records)}"
        source = self.probe.output / f"{name}.parameters.bin"
        target = self.probe.output / f"{name}.frame.bin"
        source.write_bytes(values.tobytes())
        self.probe.run([self.binary, source, target], name)
        payload = np.fromfile(target, "<f4")
        if payload.size != 28:
            raise AssertionError("the actual C++ object must contain exactly112B")
        self.records.append(
            {
                "input": str(source),
                "input_sha256": digest(source),
                "frame_sha256": digest(target),
            }
        )
        return payload.reshape(7, 4)


def fractional_anchors(probe, producer, positions):
    rows = []
    origins = (
        [22222.12345, -511.123456, -16845.34567],
        [-1399.999999, 0.1234567, 4200.33333],
    )
    for profile in (0, 1):
        for origin in origins:
            for phase in (MONTH_S, -MONTH_S):
                first = None
                truth = None
                for delta in (0.0, 1 / 30, 1 / 60):
                    payload = producer(0.65, profile, VELOCITY, phase + delta, origin)
                    actual = probe.capture(
                        f"fractional-{len(rows)}", payload, positions
                    )
                    expected = reference(positions, payload, VELOCITY, phase + delta)
                    error = check_density(actual, expected, payload)
                    relative = None
                    if first is None:
                        first, truth = actual, expected
                    else:
                        step = actual.astype(float) - first
                        reference_step = expected - truth
                        relative = float(
                            np.linalg.norm(step - reference_step)
                            / np.linalg.norm(reference_step)
                        )
                        if not np.any(step) or relative > 0.05:
                            raise AssertionError(
                                "fractional field anchors must preserve individual wind steps"
                            )
                    rows.append(
                        {
                            "profile": profile,
                            "origin_m": origin,
                            "phase_s": phase + delta,
                            "max_normalized_density_error": error,
                            "relative_step_l2_error": relative,
                        }
                    )
    return rows


def reflected_layout(probe, reflector):
    reflected = json.loads(
        probe.run([reflector, probe.split_shader, "--reflect"], "reflection")
    )
    blocks = [
        block
        for block in reflected["ubos"]
        if block["set"] == 2 and block["binding"] == 0
    ]
    if len(blocks) != 1 or blocks[0]["block_size"] != 112:
        raise AssertionError(
            "the actual SPIR-V consumer must bind the agreed112B range"
        )
    members = reflected["types"][blocks[0]["type"]]["members"]
    names = (
        "Shell",
        "Field",
        "CoordinateOrigin",
        "BroadBase",
        "BroadFraction",
        "DetailBase",
        "DetailFraction",
    )
    expected = [
        (name, index * 16, "uvec4" if index in (3, 5) else "vec4")
        for index, name in enumerate(names)
    ]
    if [(m["name"], m["offset"], m["type"]) for m in members] != expected:
        raise AssertionError(
            "all seven actual shader offsets and integer types must match C++"
        )
    return {"block": blocks[0], "members": members}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gpu-root", type=Path, required=True)
    parser.add_argument("--control-root", type=Path, required=True)
    parser.add_argument("--module-command", type=Path, required=True)
    parser.add_argument("--reflector", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--shared-layer", action="store_true")
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private outputs belong in system temp")
    output.mkdir(parents=True, exist_ok=True)
    (output / "receipt.json").unlink(missing_ok=True)
    probe = Probe(output)
    probe.binary = args.gpu_root / "cloud-density-native"
    probe.split_shader = args.gpu_root / "wind-split.spv"
    probe.direct_shader = args.gpu_root / "cloud-density.spv"
    original = json.loads((args.gpu_root / "receipt.json").read_text())
    for path, key in (
        (probe.binary, "binary_sha256"),
        (probe.split_shader, "split_shader_sha256"),
    ):
        if digest(path) != original[key]:
            raise ValueError(
                "the immutable native GPU consumer must match its original receipt"
            )
    if args.shared_layer:
        probe.split_shader = output / "cloud-layer-shared.spv"
        probe.run(
            [
                "nice",
                "-n",
                "10",
                *shlex.split(os.environ.get("GLSLANG", "glslangValidator")),
                "-V",
                "-Isrc",
                "-Isrc/generators/weather/shaders",
                ROOT / "test/experiments/weather/cloud_wind_precision.comp",
                "-o",
                probe.split_shader,
            ],
            "shared-layer-shader",
        )
    producer = Producer(probe, args.module_command)
    positions = points()
    record = {
        "layout": reflected_layout(probe, args.reflector),
        "default_cases": default_cases(probe, args.control_root),
        "field_cases": field_cases(probe, positions, pack=producer),
        "temporal_cases": temporal_cases(probe, positions, pack=producer),
        "boundary_cases": temporal_cases(
            probe,
            positions,
            (175.0 - 1 / 120, 175.0 / 3.1 - 1 / 120),
            "boundary",
            pack=producer,
        ),
        "anchor_cases": anchor_cases(probe, positions, pack=producer),
        "fractional_anchor_cases": fractional_anchors(probe, producer, positions),
        "plane": plane_cases(probe, pack=producer),
        "cost_cases": [],
        "producer_objects": producer.objects,
        "producer_binary_sha256": digest(producer.binary),
        "producer_frames": producer.records,
        "gpu_receipt_sha256": digest(args.gpu_root / "receipt.json"),
        "binary_sha256": digest(probe.binary),
        "split_shader_sha256": digest(probe.split_shader),
        "shared_layer": args.shared_layer,
        "source_commit": probe.run(["git", "rev-parse", "HEAD"], "source").strip(),
        "source_dirty": probe.run(["git", "status", "--porcelain"], "dirty"),
        "source_sha256": {
            path: digest(ROOT / path)
            for path in (
                "src/generators/weather/CloudFrame.h",
                "src/generators/weather/CloudFrame.cpp",
                "test/experiments/weather/cloud_frame_pack.cpp",
                "test/experiments/weather/cloud_frame_native.py",
                "test/experiments/weather/cloud_wind_native.py",
                "test/experiments/weather/cloud_wind_precision.comp",
                "src/generators/weather/shaders/cloudLayer.glsl",
                "src/generators/weather/shaders/cloudCumulus.glsl",
            )
        },
        "commands": probe.commands,
        "captures": probe.captures,
        "scope": "Actual product C++ bytes consumed by the unchanged isolated Vulkan density probe; production shader/sky/history and device budgets remain open. Process launch/file IO are harness work, not producer CPU cost.",
    }
    (output / "receipt.json").write_text(json.dumps(record, indent=2) + "\n")
    print(
        json.dumps({key: len(record[key]) for key in record if key.endswith("_cases")})
    )


if __name__ == "__main__":
    main()
