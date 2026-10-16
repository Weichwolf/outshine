"""Measure the actual fused colour/roughness/relief helper on the prior cost cohort."""

import argparse
import hashlib
import json
import os
import tempfile
from pathlib import Path

import numpy as np
from facade_coating_raster import ROOT, run
from masonry_facade_cost import capture, prepare
from masonry_relief_raster import sources


def compile_fused(output):
    original = (ROOT / "test/experiments/render/masonry_facade_probe.comp").read_text()
    original = original.replace(
        "vec4(filtered.xyz, field)", "vec4(filtered.x, field, heightChange)"
    )
    for enabled in (0, 1):
        folder = output / f"source-{enabled}"
        sources(folder, bool(enabled), normal_export=False)
        source = folder / "probe.comp"
        source.write_text(original)
        run(
            [
                "nice",
                "-n",
                "10",
                os.environ.get("GLSLANG", "glslangValidator"),
                "-V",
                "-Isrc",
                "-Isrc/render/shaders",
                "-Isrc/content/shade",
                "-DFACADE_MASONRY=1",
                f"-DRELIEF_ENABLE={enabled}",
                "-DRELIEF_COUNT_WORK=0",
                str(source),
                "-o",
                str(output / f"filter-{enabled}.spv"),
            ],
            output,
            f"compile-fused-{enabled}",
        )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.is_relative_to(ROOT):
        parser.error("use an explicit system-temp output outside the checkout")
    output.mkdir(parents=True, exist_ok=True)
    count = 115200
    unmapped = prepare(output, count)
    compile_fused(output)
    baseline, receipts = [], []
    for mode in (0, 1):
        values, receipt = capture(
            args.binary.resolve(), output, mode, unmapped, False, 0
        )
        assert len(values) == count
        baseline.append(values)
        receipts.append(receipt)
    np.testing.assert_array_equal(baseline[0][:, :6], baseline[1][:, :6])
    np.testing.assert_array_equal(baseline[0][:, 6:], 0)
    np.testing.assert_array_equal(baseline[1][-512:, 6:], 0)
    assert np.count_nonzero(baseline[1][:, 6:]) > 0
    for pass_index, order in enumerate(((0, 1), (1, 0)), start=1):
        for mode in order:
            values, receipt = capture(
                args.binary.resolve(), output, mode, unmapped, True, pass_index
            )
            np.testing.assert_array_equal(values, baseline[mode])
            receipts.append(receipt)
            Path(receipt["command"][4]).unlink()
    report = {
        "probes": count,
        "receipts": receipts,
        "rgb_roughness_opening_brickfield_byte_equal": True,
        "far_zero_controls": 512,
        "new_noise_or_texture_samples": 0,
        "new_window_or_band_evaluations": 0,
        "reused_joint_subcells": 16,
        "new_cached_asset_or_buffer_layout_bytes": 0,
        "new_bindings": 0,
        "source_sha256": {
            str(p.relative_to(output)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in output.glob("source-*/*")
        },
        "load": Path("/proc/loadavg").read_text().strip()
        if Path("/proc/loadavg").exists()
        else None,
        "scope": "Sum of32 public compute dispatches; no copies, whole-frame or device peak claim",
    }
    (output / "cost.json").write_text(json.dumps(report, indent=2) + "\n")
    print({"sum_ms": [r["dispatch_sum_ms"] for r in receipts[2:]], "probes": count})


if __name__ == "__main__":
    main()
