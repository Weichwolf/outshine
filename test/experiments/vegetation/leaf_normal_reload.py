"""Fresh cache reload: validate every normal level without regenerating its surface."""

import hashlib
import json
import time
from pathlib import Path

import numpy as np
import tifffile
from leaf_normal_moments import resolve_moments
from PIL import Image


def main():
    folder = Path("build/experiments/vegetation/normal-leaves")
    cases = []
    for i in range(3):
        start = time.perf_counter()
        recipe = json.loads((folder / f"developed-{i}-material.json").read_text())
        assert recipe["version"] == "procedural-leaf-material-4"
        assert (
            hashlib.sha256((folder / recipe["normal_moments"]).read_bytes()).hexdigest()
            == recipe["normal_moments_sha256"]
        )
        worst_normal, worst_rough = 0.0, 0.0
        with np.load(folder / recipe["normal_moments"]) as moments:
            assert len(moments.files) == 10
            for level, files in enumerate(recipe["normal_levels"]):
                for name in files.values():
                    assert (
                        hashlib.sha256((folder / name).read_bytes()).hexdigest()
                        == recipe["normal_level_sha256"][name]
                    )
                expected, rough, _ = resolve_moments(
                    moments[f"level_{level}"].astype(float)
                )
                size = 512 // 2**level
                image = tifffile.imread(folder / files["normal"])
                assert image.dtype == np.uint16 and image.shape == (size, size, 3)
                actual = image[::-1] / 65535 * 2 - 1
                actual /= np.linalg.norm(actual, axis=2)[..., None]
                angle = float(
                    np.max(np.arccos(np.clip(np.sum(actual * expected, axis=2), -1, 1)))
                )
                with Image.open(folder / files["roughness"]) as image:
                    assert image.size == (size, size)
                    error = float(np.max(abs(np.asarray(image)[::-1] / 255 - rough)))
                assert angle < 6e-5 and error <= 0.5 / 255 + 1e-6
                worst_normal = max(worst_normal, angle)
                worst_rough = max(worst_rough, error)
        cases.append(
            {
                "levels": 10,
                "load_and_check_seconds": time.perf_counter() - start,
                "normal_angle_error_radians": worst_normal,
                "roughness_quantization_error": worst_rough,
            }
        )
    report = {"products": cases, "normal_generation_repeated": False}
    (folder / "normal-reload.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
