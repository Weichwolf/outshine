"""Reproduce dark masonry albedo and stress the shared coating/masonry budget."""

import argparse
import hashlib
import json
import os
import tempfile
from pathlib import Path

import numpy as np
from facade_coating_raster import HEIGHT, ROOT, WIDTH, packed_normal, run
from facade_phase_raster import VERTEX
from ground_litter_native import capture
from masonry_facade_raster import compile_pair


def compile_baseline(output, baseline):
    common = [
        "nice",
        "-n",
        "10",
        os.environ.get("GLSLANG", "glslangValidator"),
        "-V",
        "--target-env",
        "vulkan1.0",
        "-S",
        "frag",
        "-I" + str(baseline / "src"),
        "-I" + str(baseline / "src/render/shaders"),
        "-I" + str(baseline / "src/content/shade"),
        "-I" + str(baseline / "build/shaders"),
        "-DSUBJECT_WRITES_VELOCITY=0",
        "-DSUBJECT_NORMAL_LOCATION=0",
        "-DSUBJECT_IDENTITY_LOCATION=0",
        "-DSUBJECT_BASE_LOCATION=1",
        "-DSUBJECT_METALROUGH_LOCATION=2",
        "-DLIT_KIND=0",
        "-DLIT_TEXTURED=0",
        "-DLIT_MAPPED=0",
    ]
    run(
        [
            *common,
            str(baseline / "src/render/shaders/litFragment.glsl"),
            "-o",
            str(output / "old.frag.spv"),
        ],
        output,
        "old-fragment",
    )


def triangle(offsets, quad=False):
    vp = np.eye(4, dtype="<f4")
    vp[0, 0] = vp[1, 1] = 2
    vp[2] = [0, 0, 0, 0.5]
    view = np.concatenate(
        [vp.T.ravel(), vp.T.ravel(), np.eye(4).ravel(), np.zeros(8)]
    ).astype("<f4")
    material = np.zeros(96, dtype="<f4")
    for name, value in zip(
        ("basex", "basey", "basez"), (0.8 * 0.5, 0.6 * 0.75, 0.4 * 0.25), strict=True
    ):
        material[offsets[name]] = value
    for name, value in (
        ("basew", 1),
        ("factor", 1),
        ("roughness", 0.75),
        ("metalness", 0.8),
        ("pattern", 2),
    ):
        material[offsets[name]] = value
    placement = np.zeros(36, dtype="<f4")
    placement[:16] = placement[16:32] = np.eye(4).ravel()
    vertices = np.zeros(6, dtype=VERTEX)
    if quad:
        xy = np.array(
            [
                [-0.5, -0.5],
                [0.5, -0.5],
                [0.5, 0.5],
                [-0.5, -0.5],
                [0.5, 0.5],
                [-0.5, 0.5],
            ]
        )
    else:
        xy = np.tile([[-0.5, -0.5], [0.5, -0.5], [0, 0.5]], (2, 1))
    vertices["p"][:, :2] = xy
    vertices["n"] = packed_normal(np.array([0, 0, 1]))
    vertices["uv"][:, 0] = (6 * 3 * 11 + 1) * 256 + 0.275 + 0.31 * xy[:, 0]
    vertices["uv"][:, 1] = 1.37 + 0.34 * xy[:, 1]
    metres = np.column_stack([xy[:, 0] + 0.53, xy[:, 1] + 0.51]).astype("<f4")
    return (
        view.tobytes()
        + material.tobytes()
        + placement.tobytes()
        + vertices.tobytes()
        + metres.tobytes()
    )


def raster(binary, output, offsets):
    rows = []
    panels = []
    for name, quad in (("authored-triangle", False), ("affine-wall", True)):
        data = output / (name + ".bin")
        data.write_bytes(triangle(offsets, quad))
        images = []
        for label, fragment in (
            ("before", "old.frag.spv"),
            ("after", "masonry.frag.spv"),
        ):
            target = output / (name + "-" + label + ".f32")
            run(
                [
                    str(binary),
                    str(output / "masonry.vert.spv"),
                    str(output / fragment),
                    str(data),
                    str(target),
                    "--uv1",
                ],
                output,
                name + "-" + label,
            )
            images.append(np.fromfile(target, dtype="<f4").reshape(3, HEIGHT, WIDTH, 4))
        old, new = images
        covered = old[1, :, :, 3] == 1
        assert covered.sum() > 1000 and np.array_equal(covered, new[1, :, :, 3] == 1)
        assert old[1, :, :, 2][covered].min() < -1e-5
        assert np.isfinite(new[1]).all() and new[1, :, :, :3][covered].min() >= 0
        assert new[1, :, :, :3][covered].max() <= 1
        np.testing.assert_array_equal(old[2], new[2])
        rows.append(
            {
                "case": name,
                "covered": int(covered.sum()),
                "before_min_rgb": old[1, :, :, :3][covered].min(axis=0).tolist(),
                "after_min_rgb": new[1, :, :, :3][covered].min(axis=0).tolist(),
            }
        )
        panels.append((name, old[1, :, :, 2], new[1, :, :, 2]))
    return rows, panels


def stress(binary, output, baseline):
    rng = np.random.default_rng(306)
    grey = np.concatenate(
        [np.linspace(0, 1, 128), [0, 1e-7, 0.032, 0.0672, 0.0736, 0.0768, 1 - 1e-7, 1]]
    )
    colours = np.concatenate(
        [
            rng.random((512, 3)),
            np.repeat(grey[:, None], 3, axis=1),
            [[0.248, 0.1935, 0.032]],
        ]
    ).astype("<f4")
    regions = []
    for opening in (0, 0.1, 0.5, 1):
        for pane in (0, opening):
            for band in (0, 1 - opening):
                regions.append([opening, pane, band])
    records = []
    for colour_index, colour in enumerate(colours):
        for region in regions:
            for coat in (-0.5, 0.5):
                for brick in (-0.16, 0.84):
                    records.append(
                        [
                            *colour,
                            (0, 0.01, 0.75, 1)[colour_index % 4],
                            *region,
                            coat,
                            brick,
                            0,
                            0,
                            0,
                        ]
                    )
    data = np.array(records, dtype="<f4")
    palette = np.fromfile(
        ROOT / "test/outshine/data/materials/ground-litter.palette", dtype="<f4"
    )
    extended = np.zeros(320 + data.size, dtype="<f4")
    extended[: len(palette)] = palette
    extended[320:] = data.ravel()
    extended.tofile(output / "native.palette")
    probes = np.zeros(
        len(data), dtype=[("values", "<f4", 6), ("which", "<i4"), ("mode", "<u4")]
    )
    probes["which"] = np.arange(len(data))
    probes.tofile(output / "probes.bin")
    values = []
    for mode, checkout in ((0, baseline), (1, ROOT)):
        run(
            [
                "nice",
                "-n",
                "10",
                os.environ.get("GLSLANG", "glslangValidator"),
                "-V",
                "-I" + str(checkout / "src/render/shaders"),
                "-I" + str(checkout / "src/content/shade"),
                f"-DJOINT_FACADE_RANGE={mode}",
                "test/experiments/render/facade_composition_range.comp",
                "-o",
                str(output / f"filter-{mode}.spv"),
            ],
            output,
            f"compile-{mode}",
        )
        values.append(capture(binary, output, mode, "native", len(data), 13))
    old, new = values
    assert old[:, :3].min() < 0 and old[:, :3].max() > 1
    assert new[:, :4].min() >= 0 and new[:, :4].max() <= 1
    np.testing.assert_array_equal(old[:, 3:], new[:, 3:])
    return {
        "cases": len(data),
        "before_range": [float(old[:, :3].min()), float(old[:, :3].max())],
        "after_range": [float(new[:, :3].min()), float(new[:, :3].max())],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--raster-binary", type=Path, required=True)
    parser.add_argument("--compute-binary", type=Path, required=True)
    parser.add_argument("--baseline-checkout", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    assert output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) and not output.is_relative_to(ROOT)
    output.mkdir(parents=True, exist_ok=True)
    offsets = compile_pair(output)
    compile_baseline(output, args.baseline_checkout.resolve())
    rows, panels = raster(args.raster_binary.resolve(), output, offsets)
    report = {
        "raster": rows,
        "stress": stress(
            args.compute_binary.resolve(), output, args.baseline_checkout.resolve()
        ),
        "scope": "Same effective painted triangle as CheckMasonry, at160x96; original129x129 atlas still requires integration countercheck.",
        "source_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    import matplotlib

    matplotlib.use("Agg")
    from matplotlib import pyplot as plt

    figure, axes = plt.subplots(2, 2, figsize=(10, 8), constrained_layout=True)
    for row, (name, old, new) in enumerate(panels):
        for column, image in enumerate((old, new)):
            shown = axes[row, column].imshow(
                image, vmin=-0.003, vmax=0.04, cmap="coolwarm"
            )
            axes[row, column].set_title(
                name + (" / before" if column == 0 else " / after")
            )
            figure.colorbar(shown, ax=axes[row, column])
    figure.suptitle(
        "Actual production raster / raw linear blue; no output colour clamp"
    )
    figure.savefig(output / "comparison.pdf")
    plt.close(figure)
    print(json.dumps(report))


if __name__ == "__main__":
    main()
