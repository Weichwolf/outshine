"""Actual native dry-asphalt patch raster; separate from production/Place acceptance."""

import argparse
import hashlib
import itertools
import json
import os
import tempfile
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from asphalt_box_reference import box_reference
from asphalt_field import ALBEDO, ROUGHNESS, parameters
from asphalt_filter import srgb
from asphalt_lighting_reference import lighting_interval
from asphalt_native import ROOT, run, sha
from facade_coating_raster import packed_normal
from matplotlib.backends.backend_pdf import PdfPages

WIDTH, HEIGHT = 160, 96


def compile_shaders(output):
    glslang = os.environ.get("GLSLANG", "glslangValidator")
    common = [
        glslang,
        "-V",
        "--target-env",
        "vulkan1.0",
        "-Isrc/render/shaders",
        "-Itest/experiments/render",
        "-DLIT_UVS=1",
        "-DLIT_TINTED=0",
        "-DLIT_MAPPED=0",
        "-DLIT_TEXTURED=0",
        "-DLIT_KIND=0",
        "-DSURFACE_WRITES_VELOCITY=0",
        "-DSURFACE_BASE_LOCATION=0",
        "-DSURFACE_NORMAL_LOCATION=0",
        "-DSURFACE_IDENTITY_LOCATION=0",
    ]
    for stage, source in (
        ("vert", ROOT / "src/render/shaders/litVertex.glsl"),
        ("frag", Path(__file__).with_suffix(".frag")),
    ):
        run(
            [
                "nice",
                "-n",
                "10",
                *common,
                "-S",
                stage,
                str(source),
                "-o",
                str(output / f"patch.{stage}.spv"),
            ],
            output,
            f"compile-{stage}",
        )
    cross = os.environ.get("SPIRV_CROSS", "spirv-cross")
    run(
        [
            cross,
            str(output / "patch.frag.spv"),
            "--reflect",
            "--output",
            str(output / "reflection.json"),
        ],
        output,
        "reflection",
    )
    layout = json.loads((output / "reflection.json").read_text())
    block = next(b for b in layout["ubos"] if b["name"] == "Material")
    assert block["block_size"] == 384
    return {
        m["name"]: m["offset"] // 4 for m in layout["types"][block["type"]]["members"]
    }


def fixture(offsets, span, shape, enabled, anchor, camera, base, authored_roughness):
    basis = (
        np.array([[1.0, 0], [0, 1.0]])
        if shape == "square"
        else np.array([[1.0, 0.8], [0.3, 1 / 64]])
    )
    spans = np.array([span, span * HEIGHT / WIDTH])
    plane = basis * spans[None]
    centre = np.array([0.031, -0.019])
    inverse = 2 * np.linalg.inv(plane)
    view = np.zeros((4, 4))
    view[:2, 0] = inverse[:, 0]
    view[:2, 2] = -inverse[:, 1]
    world_centre = centre + camera[[0, 2]] * np.array([1, -1])
    view[:2, 3] = -inverse @ world_centre
    view[2, 3], view[3, 3] = 0.5, 1
    views = np.concatenate(
        [view.T.ravel(), view.T.ravel(), np.eye(4).ravel(), [*camera, 0], [*camera, 0]]
    ).astype("<f4")
    material = np.zeros(96, dtype="<f4")
    for name, value in zip(
        ("basex", "basey", "basez", "basew", "roughness", "pattern"),
        (*base, 1, authored_roughness, int(enabled)),
        strict=True,
    ):
        material[offsets[name]] = value
    current = np.eye(4)
    current[:3, 3] = anchor
    placement = np.concatenate(
        [current.T.ravel(), current.T.ravel(), np.zeros(4)]
    ).astype("<f4")
    vertices = np.zeros(
        6, dtype=[("position", "<f4", 3), ("uv", "<f4", 2), ("normal", "<u4")]
    )
    for i, corner in enumerate(
        ((-0.5, -0.5), (0.5, -0.5), (0.5, 0.5), (-0.5, -0.5), (0.5, 0.5), (-0.5, 0.5))
    ):
        point = centre + plane @ corner
        vertices["position"][i] = np.array([point[0], 0, -point[1]]) - anchor
    vertices["normal"] = packed_normal(np.array([0.0, 1, 0]))
    return (
        views.tobytes() + material.tobytes() + placement.tobytes() + vertices.tobytes(),
        centre,
        plane,
    )


def capture(binary, output, label, payload):
    fixture_path = output / f"{label}.fixture"
    fixture_path.write_bytes(payload)
    target = output / f"{label}.f32"
    command = [
        str(binary),
        str(output / "patch.vert.spv"),
        str(output / "patch.frag.spv"),
        str(fixture_path),
        str(target),
    ]
    run(command, output, label)
    actual = np.fromfile(target, dtype="<f4").reshape(3, HEIGHT, WIDTH, 4)
    if not np.all(np.isfinite(actual)) or np.any(actual[..., 3] < 0):
        raise ValueError("incomplete or nonfinite native raster")
    return actual


def execute(binary, output, offsets):
    records, panels = [], []
    catalogue = json.loads(
        (ROOT / "src/assets/world/ground-materials.json").read_text()
    )
    asphalt = next(row for row in catalogue["classes"] if row["name"] == "asphalt")
    recipes = (
        ("route", ALBEDO, ROUGHNESS),
        ("catalogue", np.array(asphalt["albedo"]), asphalt["roughness"]),
    )
    footprints = (
        ("near", 0.384, "square"),
        ("transition", 1.536, "square"),
        ("grazing", 1.536, "sheared"),
        ("far", 15.36, "square"),
    )
    for (recipe, base, authored_roughness), (kind, span, shape) in itertools.product(
        recipes, footprints
    ):
        name = f"{recipe}-{kind}"
        captures = []
        for enabled in (False, True):
            payload, centre, plane = fixture(
                offsets,
                span,
                shape,
                enabled,
                np.zeros(3),
                np.zeros(3),
                base,
                authored_roughness,
            )
            captures.append(capture(binary, output, f"{name}-{int(enabled)}", payload))
        old, actual = captures
        yy, xx = np.indices((HEIGHT, WIDTH))
        screen = np.column_stack(
            [(xx.ravel() + 0.5) / WIDTH - 0.5, 0.5 - (yy.ravel() + 0.5) / HEIGHT]
        )
        points = (centre + screen @ plane.T) / 0.024
        dx, dy = plane[:, 0] / WIDTH / 0.024, -plane[:, 1] / HEIGHT / 0.024
        reference = box_reference(points, dx, dy, 16)
        refined = box_reference(points, dx, dy, 32)
        convergence = float(np.max(np.abs(refined - reference)))
        if convergence > 1e-6:
            raise ValueError(
                f"native patch oracle not converged: {name}: {convergence}"
            )
        ref_colour, ref_roughness = parameters(refined, base, authored_roughness)
        lower, upper = lighting_interval(ref_colour, authored_roughness)
        shaded = actual[0, :, :, :3].reshape(-1, 3)
        light_bound = float(
            max(
                np.max(np.abs(srgb(shaded * 4) - srgb((lower - 3e-6) * 4))),
                np.max(np.abs(srgb(shaded * 4) - srgb((upper + 3e-6) * 4))),
            )
            * 255
        )
        colour_error = float(
            np.max(np.abs(srgb(actual[1, :, :, :3].reshape(-1, 3)) - srgb(ref_colour)))
            * 255
        )
        roughness_error = float(
            np.max(np.abs(actual[1, :, :, 3].ravel() - ref_roughness)) * 255
        )
        np.savez_compressed(
            output / f"{name}-reference.npz",
            field=refined,
            colour=ref_colour,
            roughness=ref_roughness,
        )
        record = {
            "name": name,
            "span_m": span,
            "shape": shape,
            "oracle_convergence": convergence,
            "colour_max_srgb_lsb": colour_error,
            "roughness_max_255_steps": roughness_error,
            "passes_parameter_limits": colour_error <= 1 and roughness_error <= 1,
            "direct_mr_display_error_bound_lsb": light_bound,
            "passes_direct_mr_bound": light_bound <= 1,
            "shaded_max_change": float(np.max(np.abs(old[0] - actual[0]))),
        }
        records.append(record)
        panels.append((name, old, actual))
        print(record, flush=True)
    return records, panels


def pdf(output, panels):
    with PdfPages(output / "native-asphalt.pdf") as pages:
        for name, old, actual in panels:
            figure, axes = plt.subplots(
                2, 3, figsize=(11.7, 8.3), constrained_layout=True
            )
            for index, (label, values) in enumerate(
                (("Flat authored asphalt", old), ("Filtered grain/binder", actual))
            ):
                axes[index, 0].imshow(np.clip(srgb(values[0, :, :, :3] * 4), 0, 1))
                axes[index, 1].imshow(np.clip(srgb(values[1, :, :, :3]), 0, 1))
                roughness = float(np.median(old[1, :, :, 3]))
                axes[index, 2].imshow(
                    values[1, :, :, 3],
                    vmin=roughness - 0.03,
                    vmax=roughness + 0.03,
                    cmap="viridis",
                )
                axes[index, 0].set_ylabel(label)
                for ax in axes[index]:
                    ax.set_xticks([])
                    ax.set_yticks([])
            for ax, title in zip(
                axes[0],
                (
                    "Native direct MR light, display exposure4",
                    "Native albedo, sRGB",
                    "Native roughness",
                ),
                strict=True,
            ):
                ax.set_title(title)
            figure.suptitle(
                f"#781: actual native raster — {name}\n160×96 patch, production MR core; no production/Place acceptance"
            )
            pages.savefig(figure)
            plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output, args.binary = args.output.resolve(), args.binary.resolve()
    if args.output.is_relative_to(ROOT) or not args.output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("output must be outside the checkout in system temp")
    args.output.mkdir(parents=True, exist_ok=True)
    records, panels = execute(args.binary, args.output, compile_shaders(args.output))
    pdf(args.output, panels)
    report = {
        "scope": __doc__,
        "native_raster": True,
        "width": WIDTH,
        "height": HEIGHT,
        "binary_sha256": sha(args.binary),
        "cases": records,
        "source_sha256": {
            p.name: hashlib.sha256(p.read_bytes()).hexdigest()
            for p in (
                Path(__file__),
                Path(__file__).with_suffix(".frag"),
                Path(__file__).with_name("asphalt_field.glsl"),
                ROOT / "src/render/shaders/metalRoughBrdf.glsl",
                ROOT / "src/assets/world/ground-materials.json",
                Path(__file__).with_name("asphalt_box_reference.py"),
                Path(__file__).with_name("asphalt_lighting_reference.py"),
                Path(__file__).with_name("asphalt_field.py"),
            )
        },
    }
    (args.output / "raster.json").write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
