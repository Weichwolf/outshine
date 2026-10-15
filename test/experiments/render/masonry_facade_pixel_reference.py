"""Check actual native masonry pixels with separate-coordinate exact-area clipping."""

import argparse
import json
import os
import tempfile
from pathlib import Path

import numpy as np
from facade_coating_raster import HEIGHT, ROOT, WIDTH, run
from masonry_facade_reference import controls, exact
from masonry_native import amplitude, source_colour


def legacy_controls(binary, source, output, legacy):
    fragment = output / "legacy.frag.spv"
    run(
        [
            "nice",
            "-n",
            "10",
            os.environ.get("GLSLANG", "glslangValidator"),
            "-V",
            "--target-env",
            "vulkan1.0",
            "-S",
            "frag",
            "-I" + str(legacy / "src"),
            "-I" + str(legacy / "src/render/shaders"),
            "-I" + str(legacy / "src/content/shade"),
            "-I" + str(legacy / "build/shaders"),
            "-DSUBJECT_WRITES_VELOCITY=0",
            "-DSUBJECT_NORMAL_LOCATION=0",
            "-DSUBJECT_IDENTITY_LOCATION=0",
            "-DSUBJECT_BASE_LOCATION=1",
            "-DSUBJECT_METALROUGH_LOCATION=2",
            "-DLIT_KIND=0",
            "-DLIT_TEXTURED=0",
            "-DLIT_MAPPED=0",
            str(legacy / "src/render/shaders/litFragment.glsl"),
            "-o",
            str(fragment),
        ],
        output,
        "compile-legacy",
    )
    description = json.loads((source / "comparison.json").read_text())
    names = [
        x["case"]
        for x in description["cases"]
        if "a0-" in x["case"] and "fixed" in x["case"]
    ]
    names += description["excluded_equal"]
    for name in names:
        target = output / f"legacy-{name}.f32"
        run(
            [
                str(binary),
                str(source / "masonry.vert.spv"),
                str(fragment),
                str(source / f"{name}-m1.bin"),
                str(target),
                "--uv1",
            ],
            output,
            "legacy-" + name,
        )
        old = np.fromfile(target, dtype="<f4").reshape(3, HEIGHT, WIDTH, 4)
        now = np.fromfile(source / f"{name}-m1.f32", dtype="<f4").reshape(
            3, HEIGHT, WIDTH, 4
        )
        np.testing.assert_array_equal(old, now, err_msg=name)
    report = {
        "cases_equal": names,
        "legacy_checkout": str(legacy),
        "scope": "Unflagged Facade1 versus the actual predecessor fragment, all MRT bytes.",
    }
    (output / "legacy.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--legacy", type=Path)
    args = parser.parse_args()
    source, output = args.input.resolve(), args.output.resolve()
    if output.is_relative_to(ROOT) or not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ):
        parser.error("use an explicit system-temp output")
    output.mkdir(parents=True, exist_ok=True)
    if args.legacy:
        legacy_controls(args.binary.resolve(), source, output, args.legacy.resolve())
    (output / "reference.json").unlink(missing_ok=True)
    checked_controls = controls()
    for mode in (0, 1):
        run(
            [
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
                "-DSUBJECT_WRITES_VELOCITY=0",
                f"-DMASONRY_DIAGNOSTIC={mode}",
                "test/experiments/render/masonry_facade_coordinates.frag",
                "-o",
                str(output / f"coordinates-{mode}.spv"),
            ],
            output,
            f"compile-{mode}",
        )
    colour_amplitude = amplitude(source_colour())
    records = []
    for item in json.loads((source / "comparison.json").read_text())["cases"]:
        name = item["case"]
        data = []
        for mode in (0, 1):
            target = output / f"{name}-d{mode}.f32"
            target.unlink(missing_ok=True)
            run(
                [
                    str(args.binary.resolve()),
                    str(source / "masonry.vert.spv"),
                    str(output / f"coordinates-{mode}.spv"),
                    str(source / f"{name}-m2.bin"),
                    str(target),
                    "--uv1",
                ],
                output,
                f"{name}-d{mode}",
            )
            data.append(np.fromfile(target, dtype="<f4").reshape(3, HEIGHT, WIDTH, 4))
        coordinates, components = data
        assert np.isfinite(coordinates).all() and np.isfinite(components).all()
        before, after = [
            np.fromfile(source / f"{name}-m{m}.f32", dtype="<f4").reshape(
                3, HEIGHT, WIDTH, 4
            )
            for m in (1, 2)
        ]
        observed = (after[1, :, :, 2] - before[1, :, :, 2]) / colour_amplitude[2]
        parity = float(np.max(np.abs(observed - components[0, :, :, 3])))
        assert parity < 2e-4, (name, parity)
        chosen = {(y, x) for y in range(4, HEIGHT, 12) for x in range(4, WIDTH, 20)}
        for channel in (0, 2):
            coverage = components[0, :, :, channel]
            candidates = np.argwhere((coverage > 0.001) & (coverage < 0.999))
            for y, x in candidates[:: max(1, len(candidates) // 24)][:24]:
                chosen.add((int(y), int(x)))
        errors = []
        for y, x in sorted(chosen):
            at, dx, dy = coordinates[:, y, x].astype(float)
            per_m = float(components[1, y, x, 0])
            fractions = components[1, y, x, 1:3].astype(float)
            reference = exact(at, dx, dy, fractions, per_m)
            error = abs(float(observed[y, x]) - reference) * float(
                np.max(np.abs(colour_amplitude))
            )
            errors.append(error)
        maximum = max(errors)
        assert maximum < 0.015, (name, maximum)
        record = {
            "case": name,
            "pixels": len(errors),
            "max_rgb_delta_error": maximum,
            "rms_rgb_delta_error": float(np.sqrt(np.mean(np.square(errors)))),
            "max_production_field_difference": parity,
        }
        records.append(record)
        print(record, flush=True)
    (output / "reference.json").write_text(
        json.dumps(
            {"scope": __doc__, "controls": checked_controls, "cases": records}, indent=2
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
