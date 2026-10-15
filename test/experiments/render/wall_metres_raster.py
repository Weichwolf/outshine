"""Consume actual cached native building material coordinates through UV1."""

import argparse
import json
import os
import re
import tempfile
from pathlib import Path

import numpy as np
import shapely
from facade_coating_raster import HEIGHT, ROOT, WIDTH, run
from facade_phase_perspective import projective
from facade_phase_raster import VERTEX


def compile_shaders(output):
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
    ]
    for stage, source, defines in (
        (
            "vert",
            ROOT / "src/render/shaders/litVertex.glsl",
            [
                "-DLIT_UVS=2",
                "-DLIT_TINTED=0",
                "-DLIT_MAPPED=0",
                "-DSUBJECT_WRITES_VELOCITY=0",
            ],
        ),
        ("frag", Path(__file__).with_name("wall_metres_probe.frag"), []),
    ):
        run(
            [
                *common,
                "-S",
                stage,
                *defines,
                str(source),
                "-o",
                str(output / f"metric.{stage}.spv"),
            ],
            output,
            f"compile-{stage}",
        )


def fixture(raw, span, shift):
    assert len(raw) == 18 * 8 + 6 * 24 + 6 * 8
    frame = np.frombuffer(raw, dtype="<f8", count=18).reshape(6, 3)
    vertices = np.frombuffer(raw, dtype=VERTEX, count=6, offset=18 * 8)
    origin, east, up, north, start, end = frame
    basis = np.array([east, up, -north])
    row = np.eye(4)
    row[:3, :3] = basis
    row[:3, 3] = -basis @ origin
    direction = end - start
    direction /= np.linalg.norm(direction)
    horizontal = direction * [1, 1, -1]
    world = vertices["p"].astype(float) @ basis.T + row[:3, 3]
    centre = world.mean(axis=0) + shift
    view = np.eye(4)
    view[0, :3] = horizontal / (span / 2)
    view[1, :3] = [0, 1 / (span * HEIGHT / WIDTH / 2), 0]
    view[0, 3] = -np.dot(view[0, :3], centre)
    view[1, 3] = -np.dot(view[1, :3], centre)
    view[2] = [0, 0, 0, 0.5]
    block = np.concatenate(
        [view.T.ravel(), view.T.ravel(), np.eye(4).ravel(), [*shift, 0], [*shift, 0]]
    )
    material = np.zeros(96, dtype="<f4")
    material[:3], material[4:7] = start, direction
    placement = np.zeros(36, dtype="<f4")
    placement[:16] = row.T.ravel()
    placement[16:32] = row.T.ravel()
    data = (
        block.astype("<f4").tobytes()
        + material.tobytes()
        + placement.tobytes()
        + raw[18 * 8 :]
    )
    actual_row = placement[:16].reshape(4, 4).T
    actual_view = block[:16].astype("<f4").reshape(4, 4).T
    points = np.column_stack([vertices["p"], np.ones(6)]) @ actual_row.T
    points[:, :3] += shift
    clip = points @ actual_view.T
    assert shapely.Polygon(clip[[0, 1, 2, 5], :2]).covers(shapely.box(-1, -1, 1, 1))
    return data, float(np.floor(vertices["uv"][0, 0] / 256))


def chart(output, panels):
    import matplotlib

    matplotlib.use("Agg")
    from matplotlib import pyplot as plt

    figure, axes = plt.subplots(2, 2, figsize=(10, 7), constrained_layout=True)
    for row, (name, pixels) in enumerate(panels):
        error = 1000 * (pixels[1, :, :, :2] - pixels[2, :, :, :2])
        for col, dimension in enumerate(("U", "V")):
            plot = axes[row, col].imshow(
                error[:, :, col], vmin=-0.2, vmax=0.2, cmap="coolwarm"
            )
            axes[row, col].set_title(f"{name}: {dimension} error (mm)")
            figure.colorbar(plot, ax=axes[row, col])
    figure.suptitle(
        "Actual native building -> cache -> UV1 -> production lit vertex / 160x96, RTX3060"
    )
    figure.savefig(output / "comparison.pdf")
    plt.close(figure)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-binary", type=Path, required=True)
    parser.add_argument("--raster-binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("use an explicit system-temp output")
    output.mkdir(parents=True, exist_ok=True)
    (output / "comparison.json").unlink(missing_ok=True)
    run(
        ["nice", "-n", "10", str(args.source_binary.resolve()), str(output / "wall")],
        output,
        "source",
    )
    compile_shaders(output)
    records, panels = [], []
    for anchor in (0, 500):
        raw = (output / f"wall-{anchor}.bin").read_bytes()
        for view, span in (("near", 0.6), ("regular", 4.0)):
            for perspective in (False, True):
                for camera, shift in (("fixed", [0, 0, 0]), ("shift", [-17, -11, 8])):
                    data, code = fixture(raw, span, shift)
                    if perspective:
                        data, _ = projective(data)
                    name = f"a{anchor}-{view}-p{int(perspective)}-{camera}"
                    source, target = output / f"{name}.bin", output / f"{name}.f32"
                    source.write_bytes(data)
                    target.unlink(missing_ok=True)
                    log = run(
                        [
                            str(args.raster_binary.resolve()),
                            str(output / "metric.vert.spv"),
                            str(output / "metric.frag.spv"),
                            str(source),
                            str(target),
                            "--uv1",
                        ],
                        output,
                        name,
                    )
                    checks = re.search(
                        r"CHECKS (\d+) FAILURES 0 SKIPPED 0 UNPREPARED 0 PARTIAL 0", log
                    )
                    assert checks and int(checks[1]) > 0
                    pixels = np.fromfile(target, dtype="<f4").reshape(
                        3, HEIGHT, WIDTH, 4
                    )
                    assert np.isfinite(pixels).all() and (pixels[:, :, :, 3] == 1).all()
                    assert (pixels[0, :, :, 2] == code).all()
                    error = float(
                        np.max(np.abs(pixels[1, :, :, :2] - pixels[2, :, :, :2]))
                    )
                    assert error < 2e-4, (name, error)
                    records.append(
                        {
                            "case": name,
                            "max_coordinate_error_m": error,
                            "flat_code": code,
                        }
                    )
                    if view == "near" and perspective and camera == "fixed":
                        panels.append((name, pixels))
    chart(output, panels)
    (output / "comparison.json").write_text(
        json.dumps({"scope": __doc__, "cases": records}, indent=2) + "\n"
    )
    print(json.dumps(records, indent=2))


if __name__ == "__main__":
    main()
