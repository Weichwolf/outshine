"""Check native facade phase independently of packed style/field/colour magnitude.

Actual lit vertex/fragment pairs consume the same integer endpoint bays, wall
and resultant albedo. A companion fragment observes the original texture UVs.
"""

import argparse
import hashlib
import io
import json
import os
import re
import tempfile
from pathlib import Path

import numpy as np
import shapely
from facade_coating_raster import HEIGHT, ROOT, WIDTH, compile_shaders, fixture, run
from facade_footprint import INSET, native_profiles
from masonry_native import source_colour

VERTEX = np.dtype([("p", "<f4", 3), ("uv", "<f4", 2), ("n", "<u4")])
VERTEX_OFFSET = 224 + 384 + 144


def tints():
    text = (ROOT / "src/render/shaders/facadePattern.glsl").read_text()
    text = text[text.index("  vec3 wallTint") : text.index("  albedo *= wallTint")]
    values = [
        [float(v) for v in item.split(",")]
        for item in re.findall(r"vec3\(([^)]+)\)", text)
    ]
    return np.array([v * 3 if len(v) == 1 else v for v in values])


def compile_pairs(output, baseline):
    offsets = compile_shaders(output, baseline / "litFragment.glsl")
    (output / "lit.vert.spv").replace(output / "new.vert.spv")
    glslang = os.environ.get("GLSLANG", "glslangValidator")
    common = [
        glslang,
        "-V",
        "--target-env",
        "vulkan1.0",
        "-Isrc",
        "-Isrc/render/shaders",
        "-Isrc/content/shade",
        "-DSUBJECT_WRITES_VELOCITY=0",
    ]
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
            str(baseline / "litVertex.glsl"),
            "-o",
            str(output / "old.vert.spv"),
        ],
        output,
        "old-vertex",
    )
    for mode, flag in (("old", 0), ("new", 1)):
        run(
            [
                "nice",
                "-n",
                "10",
                *common,
                "-S",
                "frag",
                f"-DPHASE_SEPARATED={flag}",
                str(Path(__file__).with_name("facade_phase_probe.frag")),
                "-o",
                str(output / f"uv-{mode}.frag.spv"),
            ],
            output,
            f"uv-{mode}",
        )
    return offsets


def payload(frame, offsets, style, field, variant, bays, zoom, anchor, shift):
    span = 3.2 * bays / 1.02
    data = bytearray(
        fixture(frame, offsets, style, np.array(anchor), np.array(shift), span)
    )
    profile = native_profiles()[style]
    fraction = (
        INSET + (1 - profile[2]) / 2
        if field == 1 or style == 7
        else (1 - profile[2]) / 2
    )
    target = np.floor(bays / 2) + fraction
    if bays == 255:
        target = 254 + fraction
    distance_to_edge = min(target, bays - target)
    if bays / (2 * 1.02 * zoom) >= distance_to_edge:
        zoom = max(zoom, bays / (1.02 * distance_to_edge))
    view = np.frombuffer(data, dtype="<f4", count=56).copy()
    matrix = view[:16].reshape(4, 4).T.copy()
    matrix[:2] *= zoom
    matrix[0, 3] -= zoom * (target - bays / 2) * 3.2 / (span / 2)
    matrix[1, 3] -= zoom * 0.27 * 2.9 / (span * HEIGHT / WIDTH / 2)
    view[:16] = matrix.T.ravel()
    view[16:32] = matrix.T.ravel()
    data[:224] = view.tobytes()
    material = np.frombuffer(data, dtype="<f4", count=96, offset=224).copy()
    for name, value in zip(
        ("basex", "basey", "basez"), source_colour() / tints()[variant], strict=True
    ):
        material[offsets[name]] = value
    data[224:608] = material.tobytes()
    vertices = np.frombuffer(data, dtype=VERTEX, count=6, offset=VERTEX_OFFSET).copy()
    vertices["uv"][:, 0] = 256 * (style + 11 * (field + 3 * variant)) + bays * np.array(
        [0, 1, 1, 0, 1, 0]
    )
    data[VERTEX_OFFSET:] = vertices.tobytes()
    row = np.frombuffer(data, dtype="<f4", count=16, offset=608).reshape(4, 4).T
    world = np.column_stack([vertices["p"], np.ones(6)]) @ row.T
    world[:, :3] += shift
    clip = world @ matrix.T
    projected = clip[:, :2] / clip[:, 3:4]
    polygon = shapely.Polygon(projected[[0, 1, 2, 5]])
    assert polygon.covers(shapely.box(-1, -1, 1, 1)), (
        style,
        field,
        variant,
        bays,
        zoom,
        "fixture covers viewport",
    )
    return data


def capture(binary, output, name, mode, data, diagnostic=False):
    inputs = output / f"{name}.bin"
    target = output / f"{name}-{mode}.f32"
    inputs.write_bytes(data)
    target.unlink(missing_ok=True)
    fragment = f"uv-{mode}" if diagnostic else mode
    log = run(
        [
            str(binary),
            str(output / f"{mode}.vert.spv"),
            str(output / f"{fragment}.frag.spv"),
            str(inputs),
            str(target),
        ],
        output,
        f"{name}-{mode}",
    )
    assert re.search(r"CHECKS \d+ FAILURES 0 SKIPPED 0 UNPREPARED 0 PARTIAL 0", log)
    pixels = np.fromfile(target, dtype="<f4").reshape(3, HEIGHT, WIDTH, 4)
    assert np.isfinite(pixels).all(), (name, mode, "finite pixels")
    assert (pixels[1, :, :, 3] == 1).all(), (name, mode, "complete wall coverage")
    return pixels


def invariance(binary, output, frame, offsets):
    records, panels = [], []
    views = (
        ("near", 1, 10, [0, 0, 0], [0, 0, 0]),
        ("regular", 4, 2, [0, 0, 0], [0, 0, 0]),
        ("far", 64, 2, [0, 0, 0], [0, 0, 0]),
        ("end255", 255, 2550, [0, 0, 0], [0, 0, 0]),
        ("camera", 1, 10, [0, 0, 0], [-17, -11, 8]),
        ("tile", 1, 10, [220, 75, -310], [0, 0, 0]),
    )
    for style in (1, 8, 7):
        for field in (0, 1, 2):
            for view, bays, zoom, anchor, shift in views:
                images = {mode: [] for mode in ("old", "new")}
                for variant in range(8):
                    name = f"s{style}-f{field}-{view}-v{variant}"
                    data = payload(
                        frame, offsets, style, field, variant, bays, zoom, anchor, shift
                    )
                    for mode, captured in images.items():
                        captured.append(capture(binary, output, name, mode, data))
                errors = {
                    mode: float(
                        max(
                            np.max(np.abs(image[1:] - images[mode][0][1:]))
                            for image in images[mode][1:]
                        )
                    )
                    for mode in images
                }
                assert errors["new"] < 2e-4, (style, field, view, errors)
                records.append(
                    {
                        "style": style,
                        "field": field,
                        "view": view,
                        "variants": 8,
                        "max_metadata_error": errors,
                    }
                )
                if style == 1 and field == 0 and view == "near":
                    panels = [
                        images["old"][0],
                        images["old"][6],
                        images["new"][6],
                        images["new"][0],
                    ]
    assert max(row["max_metadata_error"]["old"] for row in records) > 0.4
    return records, panels


def excluded(binary, output, frame, offsets):
    records = []
    for name, u, pattern in (
        ("plain", 10.37, 0),
        ("paint", -1, 1),
        ("plinth", -9, 1),
        ("glass", -12, 1),
        ("pitch", -2, 1),
        ("metal_roof", -3, 1),
    ):
        data = bytearray(fixture(frame, offsets, 1, np.zeros(3), np.zeros(3)))
        material = np.frombuffer(data, dtype="<f4", count=96, offset=224).copy()
        material[offsets["pattern"]] = pattern
        data[224:608] = material.tobytes()
        vertices = np.frombuffer(
            data, dtype=VERTEX, count=6, offset=VERTEX_OFFSET
        ).copy()
        vertices["uv"][:, 0] = u
        data[VERTEX_OFFSET:] = vertices.tobytes()
        old, new = [
            capture(binary, output, name, mode, data) for mode in ("old", "new")
        ]
        error = float(np.max(np.abs(old - new)))
        assert error < 2e-5, (name, error)
        records.append({"case": name, "max_old_new_error": error})
    return records


def raw_uvs(binary, output, frame, offsets):
    data = bytearray(fixture(frame, offsets, 1, np.zeros(3), np.zeros(3)))
    vertices = np.frombuffer(data, dtype=VERTEX, count=6, offset=VERTEX_OFFSET).copy()
    vertices["uv"] = [
        [-257.5, -10],
        [513.25, -10],
        [513.25, 37.25],
        [-257.5, -10],
        [513.25, 37.25],
        [-257.5, 37.25],
    ]
    data[VERTEX_OFFSET:] = vertices.tobytes()
    old, new = [
        capture(binary, output, "raw-uv", mode, data, diagnostic=True)
        for mode in ("old", "new")
    ]
    np.testing.assert_array_equal(old[1], new[1])
    np.testing.assert_array_equal(old[2, :, :, 1:], new[2, :, :, 1:])
    return {
        "raw_uv0_byte_equal": True,
        "unchanged_uv1_zero_default": True,
        "range": [float(new[1, :, :, :2].min()), float(new[1, :, :, :2].max())],
    }


def pdf(output, panels):
    import matplotlib

    matplotlib.use("Agg")
    from matplotlib import pyplot as plt

    figure, axes = plt.subplots(2, 4, figsize=(13, 6), constrained_layout=True)
    for i, (name, pixels) in enumerate(
        zip(
            ("old low code", "old brown code", "new brown code", "new low code"),
            panels,
            strict=True,
        )
    ):
        rgb = pixels[1, :, :, :3]
        srgb = np.where(
            rgb <= 0.0031308,
            rgb * 12.92,
            1.055 * np.maximum(rgb, 0) ** (1 / 2.4) - 0.055,
        )
        axes[0, i].imshow(np.clip(srgb, 0, 1), interpolation="nearest")
        axes[0, i].set_title(name)
        axes[1, i].plot(pixels[2, HEIGHT // 2, :, 1])
        axes[1, i].set_ylim(0.08, 0.94)
        axes[1, i].set_ylabel("actual roughness")
        axes[1, i].set_xlabel("pixel column")
        axes[1, i].grid(alpha=0.2)
    figure.suptitle(
        "Same native wall/source colour; metadata must not alter the window. 160x96, no lighting"
    )
    figure.savefig(output / "comparison.pdf")
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--baseline", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("use an explicit system-temp output")
    output.mkdir(parents=True, exist_ok=True)
    (output / "comparison.json").unlink(missing_ok=True)
    offsets = compile_pairs(output, args.baseline.resolve())
    frame = np.loadtxt(
        io.StringIO(run([str(args.binary.resolve()), "--frame"], output, "frame"))
    )
    records, panels = invariance(args.binary.resolve(), output, frame, offsets)
    controls = excluded(args.binary.resolve(), output, frame, offsets)
    raw = raw_uvs(args.binary.resolve(), output, frame, offsets)
    pdf(output, panels)
    sources = [Path(__file__), Path(__file__).with_name("facade_phase_probe.frag")]
    sources += [
        ROOT / "src/render/shaders" / name
        for name in ("litVertex.glsl", "litFragment.glsl", "facadePattern.glsl")
    ]
    report = {
        "scope": __doc__,
        "native_raster": True,
        "cases": records,
        "excluded": controls,
        "uv_control": raw,
        "size": [WIDTH, HEIGHT],
        "frame": frame.tolist(),
        "source_sha256": {
            str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in sources
        },
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    print(
        json.dumps(
            {
                "native_raster": True,
                "metadata_groups": len(records),
                "max_fixed_error": max(r["max_metadata_error"]["new"] for r in records),
            }
        )
    )


if __name__ == "__main__":
    main()
