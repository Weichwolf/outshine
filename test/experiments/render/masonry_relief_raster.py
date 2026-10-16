"""Compare the production relief shader with its disabled contribution."""

import argparse
import hashlib
import json
import tempfile
from pathlib import Path

import numpy as np
from facade_coating_raster import HEIGHT, ROOT, WIDTH, run
from ground_gradients import surface_gradient
from masonry_facade_raster import compile_pair
from masonry_relief_reference import exact

COLOUR_ROUNDOFF_LIMIT = 2**-22


def sources(directory, enabled, normal_export=True):
    directory.mkdir(parents=True, exist_ok=True)
    shader = ROOT / "src/render/shaders"
    fragment = (shader / "litFragment.glsl").read_text()
    old = "outputSurface(vec4(0.0), shadingNormal, surface.identity);"
    assert fragment.count(old) == 1
    if normal_export:
        fragment = fragment.replace(
            old,
            "outputSurface(vec4(shadingNormal, 1.0), shadingNormal, surface.identity);",
        )
    masonry = (shader / "facadeMasonry.glsl").read_text()
    contribution = (
        "if (wall > 0.0) { relief += facadeJointGradient(brick, brickWidth) * wall; }"
    )
    assert masonry.count(contribution) == 1
    if not enabled:
        masonry = masonry.replace(contribution, "")
    (directory / "facadeMasonry.glsl").write_text(masonry)
    (directory / "facadeMasonryRelief.glsl").write_bytes(
        (shader / "facadeMasonryRelief.glsl").read_bytes()
    )
    (directory / "facadePattern.glsl").write_bytes(
        (shader / "facadePattern.glsl").read_bytes()
    )
    target = directory / "litFragment.glsl"
    target.write_text(fragment)
    return target


def prepare(output):
    for name, enabled in (("before", False), ("after", True)):
        folder = output / name
        folder.mkdir(exist_ok=True)
        compile_pair(folder, sources(folder / "source", enabled))
    frame = output / "frame"
    frame.mkdir(exist_ok=True)
    source = frame / "frame.glsl"
    source.write_text("""#version 450
#extension GL_GOOGLE_include_directive : require
#define VARYING in
#include "subjectVaryings.glsl"
#include "material.glsl"
#include "surfaceGradient.glsl"
layout(location=0) out vec4 outAt;
layout(location=1) out vec4 outDx;
layout(location=2) out vec4 outDy;
void main() {
  outAt=vec4(facing(normal,gl_FrontFacing),materialAt().base.a);
  outDx=vec4(dFdx(position),0.0);
  outDy=vec4(dFdy(position),0.0);
}
""")
    compile_pair(frame, source)


def render(binary, shaders, payload, target, label):
    run(
        [
            str(binary),
            str(shaders / "masonry.vert.spv"),
            str(shaders / "masonry.frag.spv"),
            str(payload),
            str(target),
            "--uv1",
        ],
        target.parent,
        label,
    )
    image = np.fromfile(target, dtype="<f4").reshape(3, HEIGHT, WIDTH, 4)
    assert np.isfinite(image).all()
    return image


def references(output, coordinate_source, name, after, frame):
    coordinate = np.fromfile(coordinate_source / f"{name}-d0.f32", dtype="<f4").reshape(
        3, HEIGHT, WIDTH, 4
    )
    settings = np.fromfile(coordinate_source / f"{name}-d1.f32", dtype="<f4").reshape(
        3, HEIGHT, WIDTH, 4
    )
    errors = []
    for y in (17, 41, 65, 89):
        for x in (13, 47, 81, 115, 149):
            at, dx, dy = coordinate[:, y, x].astype(float)
            material_gradient, _ = exact(
                at,
                dx,
                dy,
                settings[1, y, x, 1:3].astype(float),
                float(settings[1, y, x, 0]),
            )
            height_dx, height_dy = (
                material_gradient @ dx[2:],
                material_gradient @ dy[2:],
            )
            n = frame[0, y, x, :3].astype(float)
            gradient = surface_gradient(
                n,
                frame[1, y, x, :3].astype(float),
                frame[2, y, x, :3].astype(float),
                height_dx,
                height_dy,
                True,
            )
            expected = n - gradient
            expected /= np.linalg.norm(expected)
            observed = after[0, y, x, :3].astype(float)
            error = np.rad2deg(
                2 * np.arcsin(min(np.linalg.norm(observed - expected) / 2, 1))
            )
            errors.append(float(error))
    return errors


def chart(output, panels):
    import matplotlib

    matplotlib.use("Agg")
    from matplotlib import pyplot as plt

    figure, axes = plt.subplots(
        len(panels),
        2,
        figsize=(12, 4 * len(panels)),
        constrained_layout=True,
        squeeze=False,
    )
    for row, (name, before, after) in enumerate(panels):
        for col, image in enumerate((before, after)):
            axes[row, col].imshow(image * 0.5 + 0.5)
            axes[row, col].set_title(
                name + (" / before" if col == 0 else " / production relief")
            )
            axes[row, col].axis("off")
    figure.suptitle(
        "Actual native litFragment normals / diagnostic attachment; unchanged RGB/MR"
    )
    figure.savefig(output / "comparison.pdf")
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--coordinates", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.is_relative_to(ROOT):
        parser.error("use an explicit system-temp output outside the checkout")
    output.mkdir(parents=True, exist_ok=True)
    default = output / "default"
    default.mkdir(exist_ok=True)
    compile_pair(default)
    for stage in ("vert",):
        name = "masonry." + stage + ".spv"
        assert (default / name).read_bytes() == (args.input / name).read_bytes(), name
    production = output / "production"
    production.mkdir(exist_ok=True)
    compile_pair(production, sources(production / "source", True, normal_export=False))
    for stage in ("vert", "frag"):
        name = "masonry." + stage + ".spv"
        assert (default / name).read_bytes() == (production / name).read_bytes(), name
    prepare(output)
    description = json.loads((args.input / "comparison.json").read_text())
    names = [x["case"] for x in description["cases"]] + description["excluded_equal"]
    rows, errors, panels = [], [], []
    for name in names:
        before, after = [
            render(
                args.binary.resolve(),
                output / kind,
                args.input / f"{name}-m2.bin",
                output / f"{name}-{kind}.f32",
                name + "-" + kind,
            )
            for kind in ("before", "after")
        ]
        np.testing.assert_allclose(
            before[1], after[1], rtol=0, atol=COLOUR_ROUNDOFF_LIMIT
        )
        np.testing.assert_array_equal(before[2], after[2])
        normal_change = float(np.abs(after[0] - before[0]).max())
        panes = before[2, :, :, 1] == np.float32(0.12)
        np.testing.assert_array_equal(before[0][panes], after[0][panes])
        assert np.max(abs(np.linalg.norm(after[0, :, :, :3], axis=-1) - 1)) < 1e-5
        if name in description["excluded_equal"]:
            np.testing.assert_array_equal(before, after)
        else:
            frame = render(
                args.binary.resolve(),
                output / "frame",
                args.input / f"{name}-m2.bin",
                output / f"{name}-frame.f32",
                name + "-frame",
            )
            case_errors = references(output, args.coordinates, name, after, frame)
            errors += case_errors
        rows.append(
            {
                "case": name,
                "normal_max_component_change": normal_change,
                "rgb_max_roundoff": float(np.abs(before[1] - after[1]).max()),
                "pure_pane_normals_equal": int(panes.sum()),
            }
        )
        if name in ("a0-near-p0-fixed", "a0-regular-p1-fixed", "gable-a0-p0-fixed"):
            panels.append((name, before[0, :, :, :3], after[0, :, :, :3]))
    assert max(errors) <= 2 and np.sqrt(np.mean(np.square(errors))) <= 0.5, max(errors)
    report = {
        "cases": rows,
        "reference_pixels": len(errors),
        "reference_max_deg": max(errors),
        "reference_rms_deg": float(np.sqrt(np.mean(np.square(errors)))),
        "source_hashes": {
            str(p.relative_to(output)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in output.glob("*/source/*.glsl")
        },
        "scope": "Actual private fused fragment; no production/lighting/frame acceptance",
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    chart(output, panels)
    print({k: v for k, v in report.items() if k not in ("cases", "source_hashes")})


if __name__ == "__main__":
    main()
