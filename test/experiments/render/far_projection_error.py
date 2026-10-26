"""Projected point-displacement bound; no opacity or native admission."""

import numpy as np
from facing_intervals import length, point, sub


def projection_error(
    box, observer, *, displacement_m, focal_px, width, height, near_m=0.1
):
    """Project an independently proved two-sided point displacement bound.

    box must enclose both surfaces. The scalar is not derived from bounds,
    vertex samples, opacity, source membership or a different representation.
    """
    box, observer = np.asarray(box, float), np.asarray(observer, float)
    if box.shape != (2, 3) or observer.shape != (2, 3):
        raise ValueError("two metric boxes required")
    if (
        not np.isfinite(box).all()
        or not np.isfinite(observer).all()
        or (box[0] > box[1]).any()
        or (observer[0] > observer[1]).any()
        or not np.isfinite(focal_px)
        or focal_px <= 0
        or type(width) is not int
        or type(height) is not int
        or not 1 <= width <= 16384
        or not 1 <= height <= 16384
        or not np.isfinite(near_m)
        or near_m <= 0
        or not np.isfinite(displacement_m)
        or displacement_m < 0
    ):
        raise ValueError(
            "finite ordered boxes and explicit perspective profile required"
        )
    first, second = (
        sub(point(box[0]), point(observer[1])),
        sub(point(observer[0]), point(box[1])),
    )
    gap = (
        np.maximum(np.maximum(first[0], second[0]), 0),
        np.maximum(np.maximum(first[1], second[1]), 0),
    )
    distance = length(gap)[0]
    ratios = np.array(
        [
            1.0,
            np.nextafter(width / (2 * focal_px), np.inf),
            np.nextafter(height / (2 * focal_px), np.inf),
        ]
    )
    factor = length(point(ratios))[1]
    depth = np.nextafter(distance / factor, -np.inf)
    denominator = np.nextafter(depth - displacement_m, -np.inf)
    if denominator <= near_m or not np.isfinite(denominator):
        return np.inf
    if displacement_m == 0:
        return 0.0
    numerator = np.nextafter(
        np.nextafter(focal_px * factor, np.inf) * displacement_m, np.inf
    )
    return float(np.nextafter(numerator / denominator, np.inf))
