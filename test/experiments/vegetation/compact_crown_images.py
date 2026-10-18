"""Fixed finite coverage-image gates; collect rejected results before failing."""

import numpy as np


def compare(images):
    result = {}
    original = images["original"]
    if (
        original.dtype != np.dtype(np.uint8)
        or original.ndim != 3
        or original.shape[2] != 3
    ):
        raise ValueError("RGB8 reference image required")
    for name in ("compact64", "compact32"):
        candidate = images[name]
        if candidate.shape != original.shape or candidate.dtype != original.dtype:
            raise ValueError("equal-profile RGB8 comparison required")
        error = abs(candidate.astype(np.int16) - original)
        result[name] = {
            "mean_RGB8_error": float(error.mean()),
            "max_RGB8_error": int(error.max()),
            "different_pixels": int(np.any(error, axis=-1).sum()),
            "different_fraction": float(np.any(error, axis=-1).mean()),
        }
        result[name]["accepted"] = (
            result[name]["different_pixels"] == 0
            if name == "compact64"
            else result[name]["mean_RGB8_error"] <= 0.1
            and result[name]["different_fraction"] <= 0.005
        )
    return result
