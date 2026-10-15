"""Render native cached facade UV0/UV1 with explicit masonry material selection."""

import argparse
import json
import os
import re
import tempfile
from pathlib import Path

import numpy as np
from facade_coating_raster import HEIGHT, ROOT, WIDTH, run
from facade_footprint import native_profiles
from facade_phase_perspective import projective
from facade_phase_raster import VERTEX, VERTEX_OFFSET
from wall_metres_raster import fixture


def compile_pair(output):
    common = [
        "nice",
        "-n",
        "10",
        os.environ.get("GLSLANG", "glslangValidator"),
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
    for stage, source, defines in (
        ("vert", "litVertex.glsl", ["-DLIT_UVS=2", "-DLIT_TINTED=0", "-DLIT_MAPPED=0"]),
        (
            "frag",
            "litFragment.glsl",
            [
                "-DLIT_KIND=0",
                "-DLIT_TEXTURED=0",
                "-DLIT_MAPPED=0",
                "-DSUBJECT_BASE_LOCATION=1",
                "-DSUBJECT_METALROUGH_LOCATION=2",
            ],
        ),
    ):
        run(
            [
                *common,
                "-S",
                stage,
                *defines,
                "src/render/shaders/" + source,
                "-o",
                str(output / ("masonry." + stage + ".spv")),
            ],
            output,
            "compile-" + stage,
        )
    cross = os.environ.get("SPIRV_CROSS", "spirv-cross")
    run(
        [
            cross,
            str(output / "masonry.frag.spv"),
            "--reflect",
            "--output",
            str(output / "material.json"),
        ],
        output,
        "reflect",
    )
    layout = json.loads((output / "material.json").read_text())
    block = next(x for x in layout["ubos"] if x["name"] == "Material")
    assert block["block_size"] == 384
    return {
        x["name"]: x["offset"] // 4 for x in layout["types"][block["type"]]["members"]
    }


def native_view(raw, span, shift):
    data, code = fixture(raw, span, shift)
    data = bytearray(data)
    metres = np.frombuffer(raw, dtype="<f4", offset=288).reshape(6, 2)
    target_height_m = 3.85
    rise_m = target_height_m - metres[:, 1].mean(dtype=np.float64)
    view = np.frombuffer(data, dtype="<f4", count=56).copy()
    matrix = view[:16].reshape(4, 4).T.copy()
    matrix[1, 3] -= rise_m * matrix[1, 1]
    view[:16] = matrix.T.ravel()
    view[16:32] = matrix.T.ravel()
    data[:224] = view.tobytes()
    return data, code


def material(data, offsets, mode):
    data = bytearray(data)
    values = np.zeros(96, dtype="<f4")
    text = (ROOT / "src/generators/building/BuildingMaterials.h").read_text()
    colour = [
        float(x.strip().removesuffix("f"))
        for x in re.search(r"Colour\{\{([^}]+)", text)[1].split(",")
    ]
    for name, value in zip(("basex", "basey", "basez"), colour, strict=True):
        values[offsets[name]] = value
    for name, value in (
        ("basew", 1),
        ("factor", 1),
        ("roughness", 0.9),
        ("pattern", mode),
    ):
        values[offsets[name]] = value
    data[224:608] = values.tobytes()
    return data


def capture(binary, output, name, data):
    source, target = output / (name + ".bin"), output / (name + ".f32")
    source.write_bytes(data)
    target.unlink(missing_ok=True)
    log = run(
        [
            str(binary),
            str(output / "masonry.vert.spv"),
            str(output / "masonry.frag.spv"),
            str(source),
            str(target),
            "--uv1",
        ],
        output,
        name,
    )
    assert re.search(r"CHECKS \d+ FAILURES 0 SKIPPED 0 UNPREPARED 0 PARTIAL 0", log)
    device = re.search(r"^DEVICE ([^\r\n]+)$", log, re.MULTILINE)
    assert device, "the native consumer must report the actual device"
    pixels = np.fromfile(target, dtype="<f4").reshape(3, HEIGHT, WIDTH, 4)
    assert np.isfinite(pixels[:, :, :, :2]).all()
    assert (pixels[1, :, :, 3] == 1).all(), name
    assert pixels[1, :, :, :3].min() >= 0 and pixels[1, :, :, :3].max() <= 1
    assert pixels[2, :, :, 1].min() >= 0 and pixels[2, :, :, 1].max() <= 1
    return pixels, device[1]


def chart(output, panels, device):
    import matplotlib

    matplotlib.use("Agg")
    from matplotlib import pyplot as plt

    figure, axes = plt.subplots(
        len(panels),
        2,
        figsize=(10, 3 * len(panels)),
        constrained_layout=True,
        squeeze=False,
    )
    for row, (name, before, after) in enumerate(panels):
        for col, (label, pixels) in enumerate(
            (("Facade1", before), ("FacadeMetres2", after))
        ):
            rgb = pixels[1, :, :, :3]
            rgb = np.where(
                rgb <= 0.0031308,
                rgb * 12.92,
                1.055 * np.maximum(rgb, 0) ** (1 / 2.4) - 0.055,
            )
            axes[row, col].imshow(np.clip(rgb, 0, 1), interpolation="nearest")
            axes[row, col].set_title(name + " / " + label)
            axes[row, col].axis("off")
    figure.suptitle(
        f"Native generated/cached facade, production lit pair / {device}\n"
        "160x96 unlit material attachments; display uses sRGB encoding"
    )
    figure.savefig(output / "comparison.pdf")
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-binary", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("use an explicit system-temp output")
    output.mkdir(parents=True, exist_ok=True)
    (output / "comparison.json").unlink(missing_ok=True)
    run([str(args.source_binary.resolve()), str(output / "wall")], output, "source")
    offsets = compile_pair(output)
    records, panels, devices = [], [], set()
    for anchor in (0, 500):
        raw = (output / f"wall-{anchor}.bin").read_bytes()
        for name, span in (("near", 0.6), ("regular", 4.0)):
            for perspective in (False, True):
                for camera, shift in (("fixed", [0, 0, 0]), ("shift", [-17, -11, 8])):
                    data, _ = native_view(raw, span, shift)
                    if perspective:
                        data, _ = projective(data)
                    label = f"a{anchor}-{name}-p{int(perspective)}-{camera}"
                    images = []
                    for mode in (1, 2):
                        pixels, device = capture(
                            args.binary.resolve(),
                            output,
                            label + f"-m{mode}",
                            material(data, offsets, mode),
                        )
                        devices.add(device)
                        images.append(pixels)
                    delta = images[1][1, :, :, :3] - images[0][1, :, :, :3]
                    panes = images[0][2, :, :, 1] == np.float32(0.12)
                    if name == "regular":
                        assert panes.sum() > 0, label
                        assert np.array_equal(
                            images[0][1][panes], images[1][1][panes]
                        ), label
                        assert np.array_equal(
                            images[0][2][panes], images[1][2][panes]
                        ), label
                    records.append(
                        {
                            "case": label,
                            "max_rgb_change": float(np.abs(delta).max()),
                            "mean_rgb_change": delta.mean(axis=(0, 1)).tolist(),
                            "max_roughness_change": float(
                                np.abs(
                                    images[1][2, :, :, 1] - images[0][2, :, :, 1]
                                ).max()
                            ),
                        }
                    )
                    if anchor == 0 and camera == "fixed":
                        panels.append((label, *images))
    data, code = native_view((output / "wall-0.bin").read_bytes(), 4.0, [0, 0, 0])
    count = len(native_profiles())
    style, field = int(code) % count, (int(code) // count) % 3
    excluded = {
        "source_colour": field * count + style,
        "other_variant": (3 + field) * count + style,
        "glass": (6 * 3 + field) * count + 7,
        "glazed_field": (6 * 3 + 1) * count + style,
        "plinth": -1,
        "pitched_roof": -2,
        "metal_roof": -3,
        "glass_trim": -12,
    }
    for name, category in excluded.items():
        changed = bytearray(data)
        vertices = np.frombuffer(changed, dtype=VERTEX, count=6, offset=VERTEX_OFFSET)
        vertices["uv"][:, 0] = (
            category if category < 0 else category * 256 + vertices["uv"][:, 0] % 256
        )
        images = [
            capture(
                args.binary.resolve(),
                output,
                name + f"-m{mode}",
                material(changed, offsets, mode),
            )[0]
            for mode in (1, 2)
        ]
        assert np.array_equal(images[0], images[1]), name
    assert len(devices) == 1 and max(x["max_rgb_change"] for x in records) > 0.01
    device = devices.pop()
    chart(output, panels, device)
    (output / "comparison.json").write_text(
        json.dumps(
            {"device": device, "cases": records, "excluded_equal": list(excluded)},
            indent=2,
        )
        + "\n"
    )
    print(json.dumps(records, indent=2))


if __name__ == "__main__":
    main()
