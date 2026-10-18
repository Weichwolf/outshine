"""Independent known-camera controls for the mature shoot pixel measurements."""

from types import SimpleNamespace

import numpy as np
from mature_window_comparison import screen_extents


def check_projection():
    scene = {
        "camera": [0, 0, 0],
        "target": [0, 10, 0],
        "resolution": [1600, 1000],
        "fov_y": 90,
    }
    sites = {
        "ids": np.zeros((1, 4), np.uint64),
        "variants": np.zeros(1, int),
        "kinds": np.zeros(1, int),
        "positions": np.zeros((1, 3)),
    }
    case = {"kind": 0, "variant": 0}
    axis = SimpleNamespace(frame=np.eye(3), origin=np.zeros(3))
    for x in (0, 5):
        result = screen_extents(
            scene, sites, case, axis, np.array(((x - 1, 10, -1), (x + 1, 10, 1)))
        )
        np.testing.assert_allclose(
            result["max_axis_span_px_min_median_max"], [100] * 3, atol=5e-13
        )
    shift = np.array((3, 4, 5))
    shifted = dict(
        scene,
        camera=shift.tolist(),
        target=(np.array(scene["target"]) + shift).tolist(),
    )
    translated = dict(sites, positions=shift[None])
    result = screen_extents(
        shifted, translated, case, axis, np.array(((-1, 10, -1), (1, 10, 1)))
    )
    np.testing.assert_allclose(
        result["max_axis_span_px_min_median_max"], [100] * 3, atol=5e-13
    )
    try:
        screen_extents(scene, sites, case, axis, np.array(((-1, 0, -1), (1, 0, 1))))
    except ValueError:
        pass
    else:
        raise AssertionError("shoot crossing camera plane accepted")
    return {
        "known_2m_plane_at_10m_depth_px": 100,
        "off_axis_uses_depth_not_distance": True,
        "world_camera_translation_invariant": True,
        "camera_plane_crossing_rejects": True,
    }


if __name__ == "__main__":
    print(check_projection())
