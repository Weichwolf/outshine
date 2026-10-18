"""Check the canopy tail through the existing native opaque-float GPU probe.

The input must be an actual GroundMaterialPalette export. This tests composition
and format fallbacks; it is not a directional canopy transport reference.
"""

import argparse
import hashlib
import json
import os
import struct
import subprocess
import tempfile
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]


def prepare(palette, output):
    rows = int(palette[:1].view("<u4")[0])
    layers = int(palette[3:4].view("<u4")[0])
    tail = layers + 8 * (rows + 1)
    assert rows > 0 and len(palette) == tail + 24 * (rows + 1)
    active = np.flatnonzero(palette[tail:].reshape(-1, 24)[:rows, 19] > 0)
    assert len(active) > 0, "at least one declared canopy is required"
    points = np.random.default_rng(451).uniform(-32, 32, (128, 2))
    probes, labels = [], []
    for row in range(-1, rows + 1):
        for footprint in (0.0005, 0.01, 0.1, 1.0, 64.0):
            for x, y in points:
                probes.append(
                    struct.pack("<6fiI", x, y, footprint, 0, 0, footprint, row, 1)
                )
                labels.append((row, footprint))
    (output / "probes.bin").write_bytes(b"".join(probes))
    variants = {
        "full": palette.copy(),
        "legacy": palette[:tail].copy(),
        "short": palette[:-1].copy(),
        "extra": np.append(palette, np.float32(0)),
        "zero": palette.copy(),
    }
    variants["zero"][tail:] = 0
    for name, offset in [
        ("zero-density", 11),
        ("zero-height", 15),
        ("zero-width", 16),
        ("zero-weight", 19),
    ]:
        value = palette.copy()
        value[tail + offset : len(palette) : 24] = 0
        variants[name] = value
    for name, value in variants.items():
        value.astype("<f4").tofile(output / f"{name}.palette.bin")
    return variants, labels, active, rows, tail


def execute(binary, output, names, rows):
    outputs, commands = {}, []
    shader = output / "canopy-probe.spv"
    compile_command = [
        os.environ.get("GLSLANG", "glslangValidator"),
        "-V",
        "--target-env",
        "vulkan1.0",
        "-S",
        "comp",
        "-Isrc/render/shaders",
        "test/experiments/render/ground_litter_probe.comp",
        "-o",
        str(shader),
    ]
    result = subprocess.run(
        compile_command,
        cwd=ROOT,
        check=False,
        capture_output=True,
        text=True,
        timeout=60,
    )
    (output / "compile.log").write_text(result.stdout + result.stderr)
    result.check_returncode()
    commands.append({"command": compile_command, "exit": result.returncode})
    for name in names:
        command = [
            str(binary),
            str(shader),
            str(output / f"{name}.palette.bin"),
            str(output / "probes.bin"),
            str(output / f"{name}.results.bin"),
            str(rows - 1),
        ]
        result = subprocess.run(
            command, check=False, capture_output=True, text=True, timeout=120
        )
        (output / f"{name}.gpu.log").write_text(result.stdout + result.stderr)
        result.check_returncode()
        commands.append({"command": command, "exit": result.returncode})
        outputs[name] = np.fromfile(
            output / f"{name}.results.bin", dtype="<f4"
        ).reshape(-1, 8)
        assert np.isfinite(outputs[name]).all(), name
    return outputs, commands


def check(outputs, labels, active, palette):
    for name, result in outputs.items():
        if name not in ("full", "legacy"):
            assert np.array_equal(result, outputs["legacy"]), name
    noncanopy = np.array([row not in active for row, _ in labels])
    far = np.array([footprint == 64.0 for _, footprint in labels])
    assert np.array_equal(outputs["full"][noncanopy], outputs["legacy"][noncanopy])
    np.testing.assert_array_equal(outputs["full"][far], outputs["legacy"][far])
    for row in active:
        selected = np.array([r == row and footprint == 64.0 for r, footprint in labels])
        expected = np.broadcast_to(
            palette[4 * (row + 1) : 4 * (row + 2)], (int(selected.sum()), 4)
        )
        np.testing.assert_array_equal(outputs["full"][selected, :4], expected)
        np.testing.assert_array_equal(outputs["full"][selected, 4:], 0)
    changes = {}
    for row in active:
        near = np.array([r == row and footprint == 0.0005 for r, footprint in labels])
        difference = np.max(
            np.abs(outputs["full"][near] - outputs["legacy"][near]), axis=0
        )
        assert np.max(difference[:3]) > 1e-5 and difference[3] > 1e-5
        assert np.max(difference[4:6]) > 1e-5
        changes[str(row)] = difference.tolist()
    return changes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--palette", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("use a directory in system temp")
    output.mkdir(parents=True, exist_ok=True)
    (output / "comparison.json").unlink(missing_ok=True)
    palette = np.fromfile(args.palette, dtype="<f4")
    variants, labels, active, rows, tail = prepare(palette, output)
    outputs, commands = execute(args.binary.resolve(), output, variants, rows)
    changes = check(outputs, labels, active, palette)
    report = {
        "status": "passed",
        "probes_per_variant": len(labels),
        "variants": len(variants),
        "rows": rows,
        "palette_bytes": palette.nbytes,
        "tail_bytes": (len(palette) - tail) * 4,
        "palette_sha256": hashlib.sha256(palette.tobytes()).hexdigest(),
        "binary_sha256": hashlib.sha256(args.binary.read_bytes()).hexdigest(),
        "shader_sha256": hashlib.sha256(
            (output / "canopy-probe.spv").read_bytes()
        ).hexdigest(),
        "noncanopy_bit_exact": True,
        "legacy_controls_bit_exact": True,
        "far_mean_bit_exact": True,
        "near_max_changes": changes,
        "commands": commands,
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"{len(variants)} native GPU variants passed, {len(labels)} probes each")


if __name__ == "__main__":
    main()
