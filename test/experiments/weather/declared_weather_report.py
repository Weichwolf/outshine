"""Check declared native captures against FP64 and compare known zero with missing in PDF."""

import argparse
import json
import tempfile
from pathlib import Path

import matplotlib
import numpy as np
from cloud_shape_raster import judge
from cloud_sky_reference import read

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--captures", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(Path(tempfile.gettempdir()).resolve()):
        parser.error("private reports belong in system temp")
    output.mkdir(parents=True, exist_ok=True)
    names = ("cloudy", "clear", "missing-low")
    labels = (
        "Explicit low cover0.65: procedural CumulusV1",
        "Known low cover0: transparent cloud product",
        "Unknown low cover: no cloud product",
    )
    actual = []
    rows = []
    fig, axes = plt.subplots(3, 1, figsize=(12, 17), layout="constrained")
    for name, label, axis, low in zip(names, labels, axes, (0.65, 0.0, None)):
        path = args.captures / name / "actual.sky-broken.json"
        rows.append(judge(path, 32, 12, expected_profile=1))
        info, images, _, _, _ = read(path)
        if info["cloud_low"] != low or info["snapshot_utc_s"] != 1792022400:
            raise AssertionError(
                "the original native declaration must remain distinguishable"
            )
        actual.append(images[1])
        rgb = np.maximum(
            images[1, :, :3].reshape(info["height"], info["width"], 3) * 4, 0
        )
        axis.imshow(rgb / (1 + rgb))
        axis.set_title(f"{label}; {info['width']}×{info['height']}")
        axis.set_axis_off()
        print(json.dumps({"case": name, "fp64": rows[-1]}), flush=True)
    np.testing.assert_array_equal(actual[1], actual[2])
    changed = np.any(actual[0][:, :3] != actual[1][:, :3], axis=1)
    if not np.any(changed):
        raise AssertionError(
            "the actual declared low cover must change the native image"
        )
    fig.suptitle(
        "Production scenario parser → Engine::weather → native Vulkan cloud consumer\n"
        "UTC2026-10-15 00:00Z; same controlled geometry, air and light; display4x/(1+4x)\n"
        "Missing remains unknown despite matching the clear image; no Place/device budget claim"
    )
    fig.savefig(output / "declared-weather-native.pdf")
    plt.close(fig)
    (output / "receipt.json").write_text(
        json.dumps(
            {
                "cases": rows,
                "cloud_changed_pixels": int(np.count_nonzero(changed)),
                "clear_missing_images_identical": True,
                "reference_batch_pixels": 16384,
                "scope": "Actual native declaration publication and raster, FP64 agreement; productive stages, dynamic weather and device budgets remain open",
            },
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    main()
