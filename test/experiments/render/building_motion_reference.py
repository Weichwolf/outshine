"""Physical translation and fixed-screen residual changes; no temporal filter."""

import numpy as np
from triangle_image_reference import projected_bounds


def trajectory(camera, local_velocity, duration, frames):
    velocity = np.asarray(local_velocity, float)
    basis = np.asarray(camera[1], float)
    if (
        velocity.shape != (3,)
        or not np.isfinite(velocity).all()
        or basis.shape != (3, 3)
        or not np.isfinite(basis).all()
        or not np.allclose(basis @ basis.T, np.eye(3), rtol=0, atol=1e-12)
        or not np.isfinite(camera[0]).all()
        or not np.isfinite(duration)
        or duration <= 0
        or not isinstance(frames, int)
        or not 2 <= frames <= 61
    ):
        raise ValueError("finite velocity, positive seconds and2..61 frames required")
    world_velocity = velocity @ basis
    times = np.linspace(-duration / 2, duration / 2, frames)
    return times, [(camera[0] + t * world_velocity, *camera[1:]) for t in times]


def motion_bounds(positions, cameras):
    bounds = np.array([projected_bounds(positions, camera) for camera in cameras])
    box = np.array(
        [
            np.maximum(np.floor(bounds[:, 0].min(axis=0)) - 1, [0, 0]),
            np.minimum(np.ceil(bounds[:, 1].max(axis=0)) + 1, cameras[0][3:]),
        ],
        int,
    )
    if np.any(box[1] <= box[0]):
        raise ValueError("motion reference must remain visible")
    return box


def residuals(images, background=0.1):
    reference = images["original"]
    return {
        name: {
            "RGB": image["C"]
            + background * image["T"]
            - (reference["C"] + background * reference["T"]),
            "T": image["T"] - reference["T"],
        }
        for name, image in images.items()
        if name != "original"
    }


def residual_change(previous, current, seconds):
    if not np.isfinite(seconds) or seconds <= 0 or previous.keys() != current.keys():
        raise ValueError("matching candidates and positive interval required")
    result = {}
    for name in current:
        if any(previous[name][k].shape != current[name][k].shape for k in ("RGB", "T")):
            raise ValueError("same fixed screen domain required")
        result[name] = {
            key + "_residual_delta_l1": float(
                abs(current[name][key] - previous[name][key]).sum()
            )
            for key in ("RGB", "T")
        }
        result[name]["interval_seconds"] = float(seconds)
    return result
