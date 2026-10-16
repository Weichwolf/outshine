"""Check actual public-API normal texture and masonry gradient composition."""

import argparse
import hashlib
import json
import re
import subprocess
import tempfile
from pathlib import Path

import numpy as np
from facade_coating_raster import ROOT
from masonry_relief_scene import chart

# Three binary16 normal readbacks participate in the reconstruction.
# Each component rounds by at most 2**-12 on [-1,1]; division by dot(n,bump)
# adds conditioning up to sqrt(3) for this bounded two-axis unit-slope recipe.
# 0.2 degrees conservatively covers their combined angular uncertainty.
NORMAL_COMPOSITION_LIMIT_DEG = 0.2


def unit(value):
    return value / np.linalg.norm(value, axis=-1, keepdims=True)


def dot(a, b):
    return np.sum(a * b, axis=-1, keepdims=True)


def read(path):
    values = np.fromfile(path, dtype="<f4").reshape(720, 1280, 4).astype(float)
    assert np.isfinite(values).all()
    return values[:, :, :3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--fixture", type=Path, required=True)
    parser.add_argument("--scene", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not output.is_relative_to(
        Path(tempfile.gettempdir()).resolve()
    ) or output.is_relative_to(ROOT):
        parser.error("use an explicit system-temp output outside the checkout")
    output.mkdir(parents=True, exist_ok=True)
    from matplotlib import pyplot as plt

    normals, images, receipts = [], [], []
    for side in ("before", "after"):
        runtime = args.scene.resolve() / side
        prefix = output / side
        command = [
            str(args.binary.resolve()),
            str(args.fixture.resolve()),
            str(prefix),
            "3",
        ]
        result = subprocess.run(
            command, cwd=runtime, capture_output=True, text=True, check=False
        )
        text = result.stdout + result.stderr
        (output / f"{side}.log").write_text(text)
        assert result.returncode == 0 and re.search(
            r"CHECKS \d+ FAILURES 0 SKIPPED 0 UNPREPARED 0 PARTIAL 0", text
        )
        normals.append(read(prefix.with_suffix(".normal.f32")))
        images.append(plt.imread(prefix.with_suffix(".png")))
        receipts.append(
            {"command": command, "cwd": str(runtime), "exit": result.returncode}
        )
    n = unit(read(args.scene / "before/scene.normal.f32"))
    bumped = unit(read(args.scene / "after/scene.normal.f32"))
    mapped = unit(normals[0])
    assert dot(n, bumped).min() >= 1 / np.sqrt(3) - 0.001
    gradient = n - bumped / dot(n, bumped)
    expected = unit(mapped - gradient * dot(n, mapped))
    actual = unit(normals[1])
    errors = np.rad2deg(
        2 * np.arcsin(np.minimum(np.linalg.norm(actual - expected, axis=-1) / 2, 1))
    )
    assert errors.max() < NORMAL_COMPOSITION_LIMIT_DEG, float(errors.max())
    flat = np.max(np.abs(n - bumped), axis=-1) == 0
    assert flat.sum() > 0
    np.testing.assert_array_equal(normals[0][flat], normals[1][flat])
    assert np.max(np.abs(mapped - n)) > 0.1
    assert np.max(np.abs(actual - mapped)) > 0.1
    report = {
        "pixels": int(errors.size),
        "flat_normals_byte_equal": int(flat.sum()),
        "max_normal_error_deg": float(errors.max()),
        "rms_normal_error_deg": float(np.sqrt(np.mean(errors * errors))),
        "limit_deg": NORMAL_COMPOSITION_LIMIT_DEG,
        "receipts": receipts,
        "binary_sha256": hashlib.sha256(args.binary.read_bytes()).hexdigest(),
        "fixture_sha256": hashlib.sha256(args.fixture.read_bytes()).hexdigest(),
        "scope": "Actual authored texture/tangent upload and material composition; no whole-world claim",
    }
    (output / "comparison.json").write_text(json.dumps(report, indent=2) + "\n")
    chart(output, images)
    print({k: v for k, v in report.items() if k != "receipts"})


if __name__ == "__main__":
    main()
