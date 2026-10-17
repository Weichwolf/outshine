"""Small planar pads for explicitly chosen, coincident whole end profiles."""

import numpy as np
import shapely


def profile_junction_pad(profiles_xy_m, *, method="profile_hull"):
    """Return an immutable Polygon; caller owns connectivity, height and clearance."""
    if method not in ("profile_hull", "profile_box"):
        raise ValueError("known profile corner recipe required")
    if not 2 <= len(profiles_xy_m) <= 4 or any(
        len(p) != 3 or any(len(point) != 2 for point in p) for p in profiles_xy_m
    ):
        raise ValueError("two to four left-centre-right XY profiles required")
    points = np.asarray(profiles_xy_m, float)
    if (
        points.shape != (len(points), 3, 2)
        or not np.isfinite(points).all()
        or abs(points).max() > 1e6
    ):
        raise ValueError("finite metric endpoint profiles required")
    if not np.array_equal(
        points[:, 1], np.broadcast_to(points[0, 1], (len(points), 2))
    ):
        raise ValueError("exactly coincident profile centres required")
    scale = max(1.0, float(abs(points).max()))
    error = 64 * np.finfo(float).eps * scale
    if np.max(abs((points[:, 0] + points[:, 2]) / 2 - points[:, 1])) > error:
        raise ValueError("each whole endpoint profile must be centred")
    widths = np.linalg.norm(points[:, 2] - points[:, 0], axis=1)
    if np.any(widths <= 0) or np.any(widths > 30 + 2 * error):
        raise ValueError("positive endpoint widths at most 30 metres required")
    hull = shapely.MultiPoint(points.reshape(-1, 2)).convex_hull
    pad = hull if method == "profile_hull" else hull.envelope
    if (
        pad.geom_type != "Polygon"
        or not pad.is_valid
        or not 1e-6 < pad.area <= widths.max() ** 2
        or any(
            not pad.buffer(1e-8).covers(shapely.LineString(profile))
            for profile in points
        )
    ):
        raise ValueError("nondegenerate bounded whole-profile corner pad required")
    return pad
