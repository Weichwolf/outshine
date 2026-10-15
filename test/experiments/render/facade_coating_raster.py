"""Raster the production lit vertex/fragment pair in the native local scene frame.

Two relative-ECEF asset anchors and a camera shift describe the same wall.
This is an actual offscreen material/derivative test, not a lit Place or budget.
"""

import argparse
import hashlib
import io
import json
import os
import re
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[3]
WIDTH, HEIGHT = 160, 96


def run(command, output, name):
    p = subprocess.run(
        command, cwd=ROOT, capture_output=True, text=True, timeout=60, check=False
    )
    (output / f"{name}.log").write_text(p.stdout + p.stderr)
    assert p.returncode == 0, (name, p.stdout, p.stderr)
    assert not any(s in p.stdout + p.stderr for s in ("Validation Error", "VUID-"))
    return p.stdout


def compile_shaders(output, baseline):
    glslang = os.environ.get("GLSLANG", "glslangValidator")
    cross = os.environ.get("SPIRV_CROSS", "spirv-cross")
    common = [
        glslang,
        "-V",
        "--target-env",
        "vulkan1.0",
        "-Isrc",
        "-Isrc/render/shaders",
        "-Isrc/content/shade",
        "-Ibuild/shaders",
        "-DSUBJECT_WRITES_VELOCITY=0",
        "-DSUBJECT_NORMAL_LOCATION=0",
        "-DSUBJECT_IDENTITY_LOCATION=0",
    ]
    vertex = output / "lit.vert.spv"
    run(
        [
            "nice",
            "-n",
            "10",
            *common,
            "-S",
            "vert",
            "-DLIT_UVS=1",
            "-DLIT_TINTED=0",
            "-DLIT_MAPPED=0",
            "src/render/shaders/litVertex.glsl",
            "-o",
            str(vertex),
        ],
        output,
        "vertex",
    )
    for label, source in (
        ("old", baseline),
        ("new", ROOT / "src/render/shaders/litFragment.glsl"),
    ):
        target = output / f"{label}.frag.spv"
        run(
            [
                "nice",
                "-n",
                "10",
                *common,
                "-DLIT_KIND=0",
                "-DLIT_TEXTURED=0",
                "-DLIT_MAPPED=0",
                "-DSUBJECT_BASE_LOCATION=1",
                "-DSUBJECT_METALROUGH_LOCATION=2",
                "-S",
                "frag",
                str(source),
                "-o",
                str(target),
            ],
            output,
            label,
        )
    metadata = output / "material.json"
    run(
        [cross, str(output / "new.frag.spv"), "--reflect", "--output", str(metadata)],
        output,
        "reflect",
    )
    layout = json.loads(metadata.read_text())
    block = next(v for v in layout["ubos"] if v["name"] == "Material")
    assert block["block_size"] == 384
    return {
        m["name"]: m["offset"] // 4 for m in layout["types"][block["type"]]["members"]
    }


def packed_normal(unit):
    folded = unit[:2] / np.abs(unit).sum()
    if unit[2] < 0:
        folded = (1 - np.abs(folded[::-1])) * np.where(folded >= 0, 1, -1)
    u, v = np.rint((folded + 1) * (65535 / 2)).astype(np.uint32)
    return (u << np.uint32(16)) | v


def fixture(frame, offsets, style, anchor, shift, span=16.0):
    origin, east, up, north = frame
    basis = np.array([east, up, -north])
    anchor_ecef = origin + basis.T @ anchor
    row = np.eye(4)
    row[:3, :3] = basis
    row[:3, 3] = basis @ (anchor_ecef - origin)
    centre = np.array([7.31, 8.73, -4.27])
    u = np.array([np.cos(0.43), 0, -np.sin(0.43)])
    v = np.array([0, 1, 0])
    half = np.array([span, span * HEIGHT / WIDTH]) / 2
    view = np.eye(4)
    view[0, :3] = u / half[0]
    view[1, :3] = v / half[1]
    view[0, 3] = -np.dot(u, centre + shift) / half[0]
    view[1, 3] = -np.dot(v, centre + shift) / half[1]
    view[2] = [0, 0, 0, 0.5]
    view_bytes = (
        np.concatenate(
            [
                view.T.ravel(),
                view.T.ravel(),
                np.eye(4).ravel(),
                [*shift, 0],
                [*shift, 0],
            ]
        )
        .astype("<f4")
        .tobytes()
    )
    material = np.zeros(96, dtype="<f4")
    text = (ROOT / "src/generators/building/BuildingMaterials.h").read_text()
    colour = [
        float(x.strip().removesuffix("f"))
        for x in re.search(r"Colour\{\{([^}]+)", text)[1].split(",")
    ]
    for name, value in zip(("basex", "basey", "basez"), colour):
        material[offsets[name]] = value
    for name, value in (
        ("basew", 1),
        ("factor", 1),
        ("roughness", 0.9),
        ("pattern", 1),
    ):
        material[offsets[name]] = value
    placement = np.zeros(36, dtype="<f4")
    placement[:16] = row.T.ravel()
    placement[16:32] = row.T.ravel()
    geometry = np.zeros(6, dtype=[("p", "<f4", 3), ("uv", "<f4", 2), ("n", "<u4")])
    normal = packed_normal(basis.T @ np.cross(u, v))
    for i, (x, y) in enumerate(((-1, -1), (1, -1), (1, 1), (-1, -1), (1, 1), (-1, 1))):
        xy = np.array([x, y]) * half * 1.02
        world = centre + u * xy[0] + v * xy[1]
        geometry["p"][i] = origin + basis.T @ world - anchor_ecef
        bay = (128.37 if span > 100 else 10.37) + xy[0] / 3.2
        storey = (256.23 if span > 100 else 10.23) + xy[1] / 2.9
        if style == "gable":
            uv = [256 + bay, 0.5]
        elif style == "trim":
            uv = [-1, 0.5]
        elif style == "glass_trim":
            uv = [-12, 0.5]
        elif style == "metal_roof":
            uv = [-3, storey]
        else:
            uv = [int(style) * 256 + bay, 1 + storey]
        geometry["uv"][i] = uv
        geometry["n"][i] = normal
    return view_bytes + material.tobytes() + placement.tobytes() + geometry.tobytes()


def capture(binary, output, label, mode, payload):
    source = output / f"{label}.bin"
    target = output / f"{label}-{mode}.f32"
    target.unlink(missing_ok=True)
    source.write_bytes(payload)
    log = run(
        [
            str(binary),
            str(output / "lit.vert.spv"),
            str(output / f"{mode}.frag.spv"),
            str(source),
            str(target),
        ],
        output,
        f"{label}-{mode}",
    )
    assert re.search(r"CHECKS \d+ FAILURES 0 SKIPPED 0 UNPREPARED 0 PARTIAL 0", log)
    pixels = np.fromfile(target, dtype="<f4").reshape(3, HEIGHT, WIDTH, 4)
    assert np.isfinite(pixels).all() and (pixels[1, :, :, 3] == 1).all()
    return pixels


def chart(output, panels):
    canvas = Image.new(
        "RGB", (WIDTH * 4, len(panels) * (HEIGHT * 2 + 20) + 24), "white"
    )
    draw = ImageDraw.Draw(canvas)
    draw.text(
        (4, 4),
        "Production lit raster / local ENU / 2x; no lighting or Place",
        fill="black",
    )
    for row, (name, old, new) in enumerate(panels):
        for col, (mode, pixels) in enumerate((("before", old), ("coated", new))):
            y = 24 + row * (HEIGHT * 2 + 20)
            draw.text((col * WIDTH * 2 + 4, y), f"{name} / {mode}", fill="black")
            srgb = np.where(
                pixels <= 0.0031308,
                pixels * 12.92,
                1.055 * np.maximum(pixels, 0) ** (1 / 2.4) - 0.055,
            )
            image = Image.fromarray(np.rint(np.clip(srgb, 0, 1) * 255).astype(np.uint8))
            canvas.paste(
                image.resize((WIDTH * 2, HEIGHT * 2)), (col * WIDTH * 2, y + 20)
            )
    canvas.save(output / "comparison.png")
    canvas.save(output / "comparison.pdf", resolution=144)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument(
        "--baseline",
        type=Path,
        required=True,
        help="baseline litFragment with its own include directory",
    )
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output, binary = args.output.resolve(), args.binary.resolve()
    if output.is_relative_to(ROOT) or not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("use an explicit system-temp output")
    output.mkdir(parents=True, exist_ok=True)
    (output / "comparison.json").unlink(missing_ok=True)
    offsets = compile_shaders(output, args.baseline.resolve())
    frame = np.loadtxt(io.StringIO(run([str(binary), "--frame"], output, "frame")))
    assert frame.shape == (4, 3)
    records, panels = [], []
    for style in (1, 8, 0, "gable", 7, 12, "trim", "glass_trim", "metal_roof", "far"):
        name = str(style)
        actual_style, span = (1, 640.0) if style == "far" else (style, 16.0)
        payload = fixture(frame, offsets, actual_style, np.zeros(3), np.zeros(3), span)
        old, new = [
            capture(binary, output, name, mode, payload) for mode in ("old", "new")
        ]
        difference = float(np.max(np.abs(old[1, :, :, :3] - new[1, :, :, :3])))
        roughness = float(np.max(np.abs(old[2, :, :, :2] - new[2, :, :, :2])))
        assert roughness < 2e-5, (style, roughness)
        if style in (7, 12, "trim", "glass_trim", "metal_roof", "far"):
            assert difference < 2e-5, (style, difference)
        else:
            assert difference > 0.002, (style, difference)
        equivalents = []
        if style in (1, 0, "gable"):
            for label, anchor, shift in (
                ("tile", [220, 75, -310], [0, 0, 0]),
                ("camera", [0, 0, 0], [-17, -11, 8]),
            ):
                p = fixture(
                    frame,
                    offsets,
                    actual_style,
                    np.array(anchor),
                    np.array(shift),
                    span,
                )
                equivalent = capture(binary, output, name + "-" + label, "new", p)
                error = float(np.max(np.abs(new[1:] - equivalent[1:])))
                assert error < 2e-4, (style, label, error)
                equivalents.append({"change": label, "max_error": error})
        records.append(
            {
                "style": name,
                "max_rgb_change": difference,
                "max_metal_rough_change": roughness,
                "equivalents": equivalents,
            }
        )
        if style in (1, 8, 0, "gable"):
            panels.append((name, old[1, :, :, :3], new[1, :, :, :3]))
    chart(output, panels)
    sources = [
        ROOT / "src/render/shaders" / name
        for name in (
            "litVertex.glsl",
            "litFragment.glsl",
            "facadePattern.glsl",
            "facadeCoating.glsl",
        )
    ]
    sources += [
        ROOT / "src/engine/TilePieces.cpp",
        ROOT / "test/harness/render/NativeFacadeProbe.cpp",
        Path(__file__),
    ]
    report = {
        "scope": __doc__,
        "native_raster": True,
        "images": 26,
        "size": [WIDTH, HEIGHT],
        "frame_ecef_enu": frame.tolist(),
        "cases": records,
        "source_sha256": {
            str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sources
        },
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
