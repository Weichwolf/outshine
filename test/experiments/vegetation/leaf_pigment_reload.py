"""Fresh-process validation of cached optical maps without the reference model."""

import hashlib
import json
from pathlib import Path

import numpy as np
from PIL import Image


def main():
    folder = Path("build/experiments/vegetation/pigment-leaves")
    products = []
    for index in range(3):
        recipe = json.loads((folder / f"developed-{index}-material.json").read_text())
        assert (
            recipe["version"] == "procedural-leaf-material-3"
            and recipe["symmetric_optics"]
        )
        for key in ("pigment_table", "optical_reference"):
            assert (
                hashlib.sha256((folder / recipe[key]).read_bytes()).hexdigest()
                == recipe[key + "_sha256"]
            )
        maps = {}
        for key, filename in recipe["maps"].items():
            with Image.open(folder / filename) as image:
                assert image.size == (512, 512)
                maps[key] = np.asarray(image)[::-1] / 255
        w = maps["transmission_weight"].ravel()[:, None]
        reflection = maps["colour"].reshape(-1, 3) * (1 - w)
        transmission = maps["transmittance"].reshape(-1, 3) * w
        assert np.max(reflection + transmission) <= 1 + 1e-14
        with np.load(folder / recipe["optical_reference"]) as saved:
            error = float(
                np.max(
                    abs(np.column_stack((reflection, transmission)) - saved["optical"])
                )
            )
            assert error <= 1 / 255 + 1e-14
        products.append(
            {
                "quantization_max_error": error,
                "max_rgb_energy": float(np.max(reflection + transmission)),
            }
        )
    report = {"products": products, "optics_generation_repeated": False}
    (folder / "optics-reload.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report))


if __name__ == "__main__":
    main()
