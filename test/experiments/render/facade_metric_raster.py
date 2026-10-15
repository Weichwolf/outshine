"""Measure view-dependent error when inferring material metres from facade UVs."""

import argparse
import io
import json
import os
import tempfile
from pathlib import Path

import numpy as np
from facade_coating_raster import HEIGHT, ROOT, WIDTH, compile_shaders, run
from facade_phase_perspective import projective
from facade_phase_raster import VERTEX, VERTEX_OFFSET, payload


def compile_metrics(output):
    for mode in ("inferred", "explicit"):
        run(
            [
                "nice",
                "-n",
                "10",
                os.environ.get("GLSLANG", "glslangValidator"),
                f"-DEXPLICIT_METRES={int(mode == 'explicit')}",
                "-V",
                "--target-env",
                "vulkan1.0",
                "-S",
                "frag",
                "-Isrc",
                "-Isrc/render/shaders",
                "-Isrc/content/shade",
                str(Path(__file__).with_name("facade_metric_probe.frag")),
                "-o",
                str(output / f"{mode}.frag.spv"),
            ],
            output,
            f"{mode}-compile",
        )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.is_relative_to(ROOT) or not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("use an explicit system-temp output")
    output.mkdir(parents=True, exist_ok=True)
    (output / "report.json").unlink(missing_ok=True)
    offsets = compile_shaders(output, ROOT / "src/render/shaders/litFragment.glsl")
    compile_metrics(output)
    frame = np.loadtxt(
        io.StringIO(run([str(args.binary.resolve()), "--frame"], output, "frame"))
    )
    records = []
    for mode in ("inferred", "explicit"):
        for name, bays, zoom, anchor, shift in (
            ("near", 1, 10, [0, 0, 0], [0, 0, 0]),
            ("regular", 4, 2, [0, 0, 0], [0, 0, 0]),
            ("far", 64, 2, [0, 0, 0], [0, 0, 0]),
            ("end255", 255, 2550, [0, 0, 0], [0, 0, 0]),
            ("camera", 1, 10, [0, 0, 0], [-17, -11, 8]),
            ("tile", 1, 10, [220, 75, -310], [0, 0, 0]),
        ):
            for perspective in (False, True):
                data = payload(frame, offsets, 1, 0, 6, bays, zoom, anchor, shift)
                if perspective:
                    data, _ = projective(data)
                data = bytearray(data)
                material = np.frombuffer(data, dtype="<f4", count=96, offset=224)
                material[offsets["basex"]] = bays
                vertices = np.frombuffer(
                    data, dtype=VERTEX, count=6, offset=VERTEX_OFFSET
                )
                material[offsets["basey"]] = (
                    vertices["uv"][:, 1].astype(float).mean() - 1
                ) * 2.9
                label = (
                    mode + "-" + name + ("-perspective" if perspective else "-ortho")
                )
                if mode == "explicit":
                    vertices["uv"][:, 0] = (
                        np.mod(vertices["uv"][:, 0], 256).astype(float) * 3.2
                    )
                    vertices["uv"][:, 1] = (
                        vertices["uv"][:, 1].astype(float) - 1
                    ) * 2.9 + 1
                source, target = output / f"{label}.bin", output / f"{label}.f32"
                source.write_bytes(data)
                target.unlink(missing_ok=True)
                log = run(
                    [
                        str(args.binary.resolve()),
                        str(output / "lit.vert.spv"),
                        str(output / f"{mode}.frag.spv"),
                        str(source),
                        str(target),
                    ],
                    output,
                    label,
                )
                assert "CHECKS 35 FAILURES 0 SKIPPED 0 UNPREPARED 0 PARTIAL 0" in log
                pixels = np.fromfile(target, dtype="<f4").reshape(3, HEIGHT, WIDTH, 4)
                assert np.isfinite(pixels).all() and (pixels[1, :, :, 3] == 1).all()
                error = pixels[1, :, :, :2] - pixels[2, :, :, :2]
                if mode == "explicit":
                    assert np.max(np.abs(error)) < 2e-4, label
                records.append(
                    {
                        "case": label,
                        "max_error_m": np.max(np.abs(error), axis=(0, 1)).tolist(),
                        "rms_error_m": np.sqrt(
                            np.mean(error.astype(float) ** 2, axis=(0, 1))
                        ).tolist(),
                        "metres_per_bay_range": [
                            float(pixels[2, :, :, 2].min()),
                            float(pixels[2, :, :, 2].max()),
                        ],
                        "metres_per_storey_range": [
                            float(pixels[2, :, :, 3].min()),
                            float(pixels[2, :, :, 3].max()),
                        ],
                    }
                )
    (output / "report.json").write_text(
        json.dumps({"scope": __doc__, "cases": records}, indent=2) + "\n"
    )
    print(json.dumps(records, indent=2))


if __name__ == "__main__":
    main()
