"""Affine radius factoring of the existing fixed-length/taper curved-axis kernel."""

import numpy as np
from outshine_ref.vegetation.axis_surface import axis_mesh
from outshine_ref.vegetation.curve_frame import arc_frame


def radius_basis(length, bend):
    reference = length / 64
    mesh = axis_mesh(length, bend, reference, reference * 0.3, 3, 4)
    t = np.r_[np.repeat(np.linspace(0, 1, 4), 4), 0, 1]
    centre, _, _, normal = arc_frame(length, bend, 0, t)
    offset = mesh["vertices"] - centre
    radius = np.linalg.norm(offset, axis=1)
    radial = np.divide(
        offset, radius[:, None], out=np.zeros_like(offset), where=radius[:, None] > 0
    )
    side = mesh["faces"][:-8].ravel()
    unit = mesh["normals"][:-24]
    sin_phi = np.sum(radial[side] * normal[side], axis=1)
    axial = length - bend * reference * (1 - 0.7 * t[side]) * sin_phi
    scale = axial / np.sum(unit * radial[side], axis=1)
    base = mesh["normals"].copy()
    base[:-24] = length * radial[side]
    coefficient = np.zeros_like(base)
    coefficient[:-24] = (unit * scale[:, None] - base[:-24]) / reference
    return {
        "centres": centre,
        "position_coefficient": offset / reference,
        "normal_base": base,
        "normal_coefficient": coefficient,
        "faces": mesh["faces"],
        "maximum_radius": length / 8,
    }


def apply_basis(basis, radii):
    radii = np.asarray(radii, float)
    if (
        radii.ndim != 1
        or not np.isfinite(radii).all()
        or np.any((radii <= 0) | (radii > basis["maximum_radius"]))
    ):
        raise ValueError(
            "explicit positive slender source radii up to length/8 required"
        )
    vertices = (
        basis["centres"][None]
        + radii[:, None, None] * basis["position_coefficient"][None]
    )
    normals = (
        basis["normal_base"][None]
        + radii[:, None, None] * basis["normal_coefficient"][None]
    )
    normals /= np.linalg.norm(normals, axis=2, keepdims=True)
    return vertices, normals
