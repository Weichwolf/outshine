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


def optical_controls(palette, active, rows, tail):
    complete = [int(row) for row in active if palette[tail + 24 * row + 19] == 1]
    assert complete, "a fully coated row is required for optical endpoint controls"
    row = complete[0]
    at, prefix = tail + 24 * row, 4 * (row + 1)
    controls = {}
    for name, fraction in (("green-optics", 0), ("dry-optics", 1)):
        value = palette.copy()
        value[at + 18] = fraction
        endpoint = at + (8 if fraction == 0 else 12)
        value[prefix : prefix + 3] = value[endpoint : endpoint + 3]
        controls[name] = value
    value = palette.copy()
    value[at + 8 : at + 11] = 0
    value[at + 12 : at + 15] = 1
    value[prefix : prefix + 3] = value[at + 18]
    controls["mixture-optics"] = value
    value = value.copy()
    value[4 * (rows + 2) + rows + 1 + row * 8 + 4] = 0
    controls["flat-optics"] = value
    return controls, row


def check_optics(outputs, variants, labels, row, rows, tail):
    at, prefix = tail + 24 * row, 4 * (row + 1)
    selected = np.array([which == row for which, _ in labels])
    far = np.array([which == row and footprint == 64 for which, footprint in labels])
    for name in ("green-optics", "dry-optics"):
        expected = np.broadcast_to(
            variants[name][prefix : prefix + 3], (int(selected.sum()), 3)
        )
        np.testing.assert_array_equal(outputs[name][selected, :3], expected)
        np.testing.assert_array_equal(
            outputs[name][selected, 3:], outputs["full"][selected, 3:]
        )
    fraction = float(variants["full"][at + 18])
    contrast = float(variants["full"][4 * (rows + 2) + rows + 1 + row * 8 + 4])
    extent = min(fraction, 1 - fraction) * contrast
    mixture = outputs["mixture-optics"][selected, :3]
    assert mixture.min() >= fraction - extent - 1e-7
    assert mixture.max() <= fraction + extent + 1e-7
    assert np.std(mixture) > 1e-3
    np.testing.assert_array_equal(mixture[:, 0], mixture[:, 1])
    np.testing.assert_array_equal(mixture[:, 0], mixture[:, 2])
    for name, chosen in (("flat-optics", selected), ("mixture-optics", far)):
        expected = np.full((int(chosen.sum()), 3), np.float32(fraction))
        np.testing.assert_array_equal(outputs[name][chosen, :3], expected)
    return {
        "row": row,
        "endpoints_exact": True,
        "zero_contrast_exact": True,
        "far_mean_exact": True,
        "unchanged_endpoint_normals_roughness": True,
        "mixture_range": [float(mixture.min()), float(mixture.max())],
    }


def execute(binary, output, names, rows, defines=()):
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
    compile_command[1:1] = defines
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
    for name in (
        "short",
        "extra",
        "zero",
        "zero-density",
        "zero-height",
        "zero-width",
        "zero-weight",
    ):
        assert np.array_equal(outputs[name], outputs["legacy"]), name
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
    controls, optical_row = optical_controls(palette, active, rows, tail)
    variants.update(controls)
    for name, value in controls.items():
        value.tofile(output / f"{name}.palette.bin")
    outputs, commands = execute(args.binary.resolve(), output, variants, rows)
    changes = check(outputs, labels, active, palette)
    optics = check_optics(outputs, variants, labels, optical_row, rows, tail)
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
        "optical_controls": optics,
        "commands": commands,
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"{len(variants)} native GPU variants passed, {len(labels)} probes each")


if __name__ == "__main__":
    main()
