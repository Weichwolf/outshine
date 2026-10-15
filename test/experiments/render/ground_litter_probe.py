"""Execute production ground GLSL through SDL/Vulkan; this is not a Place gate.

Requires the test runner's C++ toolchain, SDL3, glslangValidator, NumPy and Pillow.
Choose concurrency for available host resources. Outputs need an explicit temp path.
"""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

import numpy as np
from PIL import Image, ImageDraw

from ground_detail import grains, noise2
from ground_gradients import grain_gradient, noise_gradient

ROOT = Path(__file__).resolve().parents[3]
DTYPE = np.dtype([("point_dx", "<f4", (4,)), ("dy", "<f4", (2,)),
                  ("which", "<i4"), ("mode", "<u4")])


def run(command, **kwargs):
    result = subprocess.run(command, cwd=ROOT, check=True, text=True,
                            capture_output=True, timeout=180, **kwargs)
    print(result.stdout, end="", flush=True)
    if result.stderr:
        print(result.stderr, end="", flush=True)
    return result


def build(output):
    shader = output / "ground_litter_probe.spv"
    run([os.environ.get("GLSLANG", "glslangValidator"), "-V", "--target-env", "vulkan1.0",
         "-I" + str(ROOT / "src/render/shaders"),
         str(Path(__file__).with_suffix(".comp")), "-o", str(shader)])
    source = ["render/device/GroundPalette", "world/ground/GroundMaterials",
              "world/ground/VegetationTemplates", "world/ground/AlpineLimit",
              "base/format/Json", "base/io/ReadTextFile", "base/io/Log"]
    includes = ["include", "src/base", "src/base/io", "src/base/format",
                "src/world/ground", "src/render/device"]
    flags = shlex.split(subprocess.check_output(["pkg-config", "--cflags", "--libs", "sdl3"], text=True))
    declarations = dict(line.split("=", 1) for line in (ROOT / "test/run.sh").read_text().splitlines()
                        if line.startswith(("CXXSTD=", "WARN=", "OPT=")))
    toolchain = [flag for key in ("CXXSTD", "WARN", "OPT")
                 for flag in shlex.split(declarations[key].strip('"'))]
    binary = output / "ground_litter_probe"
    run([*shlex.split(os.environ.get("CXX", "c++")), *toolchain, "-fno-exceptions",
         *["-I" + str(ROOT / path) for path in includes],
         str(ROOT / "test/harness/render/GroundLitterProbe.cpp"),
         *[str(ROOT / "src" / (path + ".cpp")) for path in source], *flags, "-o", str(binary)])
    return binary, shader


def capture(binary, shader, probes, output, variant):
    path = output / "probes.bin"
    probes.tofile(path)
    target = output / (variant + ".f32")
    result = run([str(binary), str(shader), str(path), str(target), variant])
    (output / (variant + ".log")).write_text(result.stdout + result.stderr)
    assert "Validation Error" not in result.stdout + result.stderr
    return np.fromfile(target, dtype="<f4").reshape(-1, 8), np.fromfile(str(target) + ".palette", dtype="<f4")


def make_probes(palette):
    rows = int(palette[:1].view("<u4")[0])
    surfaces = 4 * (rows + 2) + rows + 1
    rng = np.random.default_rng(196)
    controls = []
    for row in range(rows + 2):
        scale = float(palette[surfaces + 8 * min(row, rows - 1) + 2]) or 1.0
        for width in (0., .0001, .05, .25, .8, 1., 1.5, 2., 8.):
            for skew in (False, True):
                p = np.zeros(8, dtype=DTYPE)
                p["point_dx"][:, :2] = rng.uniform(-4, 4, (8, 2)) * scale
                p["point_dx"][:, 2:] = [width * scale, width * scale * .25 if skew else 0.]
                p["dy"] = [-width * scale * .125 if skew else 0., width * scale * .5 if skew else width * scale]
                p["which"] = row if row < rows else -1 if row == rows else rows + 17
                controls.append(p)
    controls = np.concatenate(controls)
    surfaces_probes = controls.copy()
    surfaces_probes["mode"] = 1
    probes = [controls, surfaces_probes]
    panels = []
    for row in (0, 1, 3):
        scale = float(palette[surfaces + 8 * row + 2])
        for width in (.002, .8, 2.):
            p = np.zeros(128 * 96, dtype=DTYPE)
            x, y = np.meshgrid(np.linspace(-3, 3, 128), np.linspace(-2.25, 2.25, 96))
            p["point_dx"][:, :2] = np.stack([x.ravel(), y.ravel()], axis=-1) * scale
            p["point_dx"][:, 2] = width * scale
            p["dy"][:, 1] = width * scale
            p["which"], p["mode"] = row, 1
            start = sum(len(part) for part in probes)
            panels.append((row, width, start))
            probes.append(p)
    return np.concatenate(probes), len(controls), panels


def box_reference(at, dx, dy, scale):
    width = (np.abs(dx) + np.abs(dy)) / scale[:, None]
    centre = at / scale[:, None]
    width = np.where(width <= .01, 0., width)
    total = np.zeros(len(at))
    # Independent midpoint quadrature, not the production primitive/corner weights.
    active = width.max(axis=-1) < 2
    for y in range(64):
        for x in range(64):
            shift = (np.array([x, y]) + .5) / 64 - .5
            total[active] += noise2(centre[active] + width[active] * shift)
    fade = np.clip(width.max(axis=-1) - 1, 0, 1)
    return np.where(active, (total / 4096 - .5) * (1 - fade * fade * (3 - 2 * fade)), 0.)


def detail(at, dx, dy, recipe, contrast):
    grain, height, coarse, fine = map(float, recipe)
    footprint = np.maximum(np.linalg.norm(dx, axis=-1), np.linalg.norm(dy, axis=-1))
    result = np.zeros((len(at), 4))
    result[:, 0] = 1
    if coarse <= 0 or fine <= 0:
        return result
    def filtered(scale):
        if scale <= 0:
            return np.zeros(len(at)), np.zeros_like(at)
        t = np.clip((footprint / scale - .25) / .55, 0, 1)
        visibility = 1 - t * t * (3 - 2 * t)
        n, gradient = noise_gradient(np.column_stack([at / scale, np.zeros(len(at))]))
        return (n - .5) * visibility, gradient[:, :2] * (visibility / scale)[:, None]
    c, _ = filtered(coarse)
    f, gradient = filtered(fine)
    tint = np.ones(len(at))
    if grain > 0 and height > 0:
        parameters = dict(grainSizeM=grain, heightAmplitudeM=height)
        _, grain_derivative = grain_gradient(at, parameters)
        t = np.clip((footprint / grain - .25) / .55, 0, 1)
        gradient = height * gradient + grain_derivative * (1 - t * t * (3 - 2 * t))[:, None]
        for i, size in enumerate(footprint):
            tint[i] = grains(at[i:i + 1], size, parameters)[0][0]
    else:
        gradient *= height
    result[:, 0] = (1 + contrast * (.7 * c + .3 * f)) * tint
    result[:, 1] = .06 * c + .04 * f
    result[:, 2:] = np.column_stack([np.sum(gradient * dx, axis=-1), np.sum(gradient * dy, axis=-1)])
    result[footprint >= max(coarse, fine, grain) * .8] = [1, 0, 0, 0]
    return result


def validate(palette, probes, native, legacy, neutral, count, field_reference=box_reference,
             reference_description="64x64 independent midpoint integration"):
    rows = int(palette[:1].view("<u4")[0])
    layers = int(palette[3:4].view("<u4")[0])
    surfaces = 4 * (rows + 2) + rows + 1
    p = probes[:count]
    # Unmapped substrate uses its native row, while wearing remains the explicit fallback.
    unmapped = rows - 1
    indices = np.where((p["which"] >= 0) & (p["which"] < rows), p["which"], unmapped)
    scales = palette[surfaces + 8 * indices + 2].astype(float)
    scales[scales <= 0] = 1
    field = field_reference(p["point_dx"][:, :2].astype(float), p["point_dx"][:, 2:].astype(float),
                            p["dy"].astype(float), scales)
    field_error = float(np.max(np.abs(native[:count, 0] - field)))
    assert field_error < .00015, field_error
    full_error, neutral_error, old_error = 0., 0., 0.
    per_channel = np.zeros(6)
    for row in range(rows + 2):
        selected = np.flatnonzero(p["which"] == (row if row < rows else -1 if row == rows else rows + 17))
        q = p[selected]
        at, dx, dy = q["point_dx"][:, :2].astype(float), q["point_dx"][:, 2:].astype(float), q["dy"].astype(float)
        substrate = row if row < rows else unmapped
        recipe = palette[surfaces + 8 * substrate:surfaces + 8 * substrate + 4]
        contrast = palette[surfaces + 8 * substrate + 4]
        mean = palette[4 + 4 * min(row, rows):8 + 4 * min(row, rows)].astype(float)
        coverage = palette[surfaces + 8 * substrate + 7] if row < rows else 0.
        delta = palette[layers + 8 * min(row, rows):layers + 8 * min(row, rows) + 4].astype(float)
        weight = coverage + 2 * min(coverage, 1 - coverage) * field[selected]
        first = mean - coverage * delta
        expected_neutral = first + weight[:, None] * delta
        actual = neutral[count + selected]
        neutral_error = max(neutral_error, float(np.max(np.abs(actual[:, :4] - expected_neutral))),
                            float(np.max(np.abs(actual[:, 4:]))))
        base = detail(at, dx, dy, recipe, contrast)
        litter_recipe = palette[layers + 8 * min(row, rows) + 4:layers + 8 * min(row, rows) + 8]
        litter = detail(at, dx, dy, litter_recipe, contrast) if coverage > 0 else base
        def wearing(optics, micro):
            return np.column_stack([optics[:3] * micro[:, :1], optics[3] + micro[:, 1], micro[:, 2:]])
        expected = wearing(first, base) * (1 - weight[:, None]) + wearing(first + delta, litter) * weight[:, None]
        residual = np.abs(native[count + selected, :6] - expected)
        per_channel = np.maximum(per_channel, residual.max(axis=0))
        full_error = max(full_error, float(residual.max()))
        old_error = max(old_error, float(np.max(np.abs(legacy[count + selected, :6] - expected))))
        if coverage == 0:
            np.testing.assert_array_equal(native[count + selected], legacy[count + selected])
    assert neutral_error < .0001, neutral_error
    assert full_error < .0002, full_error
    assert old_error > .01, "old constant composition must fail the same reference"
    return dict(control_points=count, field_max_abs_error=field_error,
                neutral_composition_max_abs_error=neutral_error, full_surface_max_abs_error=full_error,
                full_surface_channel_max_abs_error=per_channel.tolist(),
                legacy_max_abs_error_against_same_reference=old_error,
                reference=reference_description + "; CPU analytic micro-gradients",
                scope="Shader component parameters only; no Place, lighting, frame-time or A18-Pro proof")


def pictures(output, panels, native, legacy):
    names = {0: "mixed_broadleaf", 1: "conifer_forest", 3: "alpine_grassland"}
    for row, name in names.items():
        canvas = Image.new("RGB", (768, 360), "white")
        draw = ImageDraw.Draw(canvas)
        for col, (_, width, start) in enumerate(p for p in panels if p[0] == row):
            draw.text((col * 256 + 4, 4), f"{name} footprint/scale={width}", fill="black")
            for line, (label, data) in enumerate((("old prefix", legacy), ("layer composition", native))):
                rgb = np.clip(data[start:start + 128 * 96, :3], 0, 1).reshape(96, 128, 3)
                srgb = np.where(rgb <= .0031308, 12.92 * rgb, 1.055 * rgb ** (1 / 2.4) - .055)
                image = Image.fromarray(np.uint8(srgb * 255)).resize((256, 144))
                draw.text((col * 256 + 4, 25 + line * 164), label + " linear albedo -> sRGB", fill="black")
                canvas.paste(image, (col * 256, 42 + line * 164))
        canvas.save(output / (name + ".png"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private outputs must use the system temp directory")
    output.mkdir(parents=True, exist_ok=True)
    binary, shader = build(output)
    bootstrap = np.zeros(1, dtype=DTYPE)
    _, palette = capture(binary, shader, bootstrap, output, "native")
    probes, count, panels = make_probes(palette)
    native, palette = capture(binary, shader, probes, output, "native")
    legacy, _ = capture(binary, shader, probes, output, "legacy")
    neutral, _ = capture(binary, shader, probes, output, "neutral")
    assert np.isfinite(native).all() and np.isfinite(legacy).all() and np.isfinite(neutral).all()
    report = validate(palette, probes, native, legacy, neutral, count)
    far_indices = np.concatenate([np.arange(start, start + 128 * 96) for _, width, start in panels if width == 2.])
    np.testing.assert_allclose(native[far_indices], legacy[far_indices], rtol=0, atol=1e-7)
    report["far_field_matches_legacy_max_abs_error"] = float(np.max(np.abs(native[far_indices] - legacy[far_indices])))
    report["device_selection_environment"] = {key: os.environ.get(key, "") for key in
                                               ("VK_DRIVER_FILES", "VK_INSTANCE_LAYERS", "VK_LAYER_PATH")}
    report["commit"] = subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip()
    report["working_tree_dirty"] = bool(subprocess.check_output(["git", "status", "--porcelain"], cwd=ROOT))
    report["shader_sha256"] = hashlib.sha256(shader.read_bytes()).hexdigest()
    report["total_probes_per_variant"] = len(probes)
    pictures(output, panels, native, legacy)
    (output / "result.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
