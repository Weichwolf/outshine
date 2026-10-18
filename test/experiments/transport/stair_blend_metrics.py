"""Integrate the complete piecewise-affine height difference on the same mesh."""

import numpy as np


def earthwork(layout, heights):
    faces, xy = layout["faces"], layout["xy"]
    p = xy[faces]
    u, v = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
    areas = abs(u[:, 0] * v[:, 1] - u[:, 1] * v[:, 0]) / 2
    delta = np.sort(heights[faces] - layout["targets"][faces], axis=1)
    a, b, c = delta.T
    signed = areas * delta.mean(axis=1)
    fill, cut = np.zeros(len(faces)), np.zeros(len(faces))
    fill[a >= 0] = signed[a >= 0]
    cut[c <= 0] = -signed[c <= 0]
    one = (a < 0) & (c > 0) & (b <= 0)
    two = (a < 0) & (b > 0)
    fill[one] = (
        areas[one]
        / 3
        * c[one]
        * (c[one] / (c[one] - a[one]))
        * (c[one] / (c[one] - b[one]))
    )
    cut[one] = fill[one] - signed[one]
    cut[two] = (
        areas[two]
        / 3
        * (-a[two])
        * (-a[two] / (b[two] - a[two]))
        * (-a[two] / (c[two] - a[two]))
    )
    fill[two] = cut[two] + signed[two]
    roundoff = 16 * np.finfo(float).eps * areas * abs(delta).max(axis=1)
    if np.any(fill < -roundoff) or np.any(cut < -roundoff):
        raise ValueError("earthwork integral has more than arithmetic negative volume")
    fill, cut = np.maximum(fill, 0), np.maximum(cut, 0)
    return {
        "fill_m3": float(fill.sum()),
        "cut_m3": float(cut.sum()),
        "signed_change_m3": float(signed.sum()),
        "mass_balance_residual_m3": float(fill.sum() - cut.sum() - signed.sum()),
        "faces": len(faces),
        "mixed_sign_faces": int(np.count_nonzero(one | two)),
        "maximum_height_change_m": float(abs(heights - layout["targets"]).max()),
        "scope": "Complete affine model integral, split at exact linear zero crossings; not full bilinear DEM or measured Earth volumes. Terrain closure-base depth does not enter the result.",
    }
