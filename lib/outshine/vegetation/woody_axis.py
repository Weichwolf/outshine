"""Build tapered, perturbed woody tubes along a sampled axis."""

import numpy as np
from scipy.interpolate import BPoly, CubicSpline

from outshine.vegetation.spine_frames import transport_frames
from outshine.vegetation.tube_topology import close_rings
from outshine.vegetation.woodland_stem import root_flare


def woody_axis(
    meshes,
    controls,
    base,
    tip,
    rng,
    *,
    stations=8,
    sides=8,
    trunk=False,
    wander=0.11,
    material,
):
    curve = BPoly(np.asarray(controls)[:, None, :], (0, 1))
    knots = np.array([0, 0.17, 0.36, 0.57, 0.78, 1])
    points = curve(knots)
    drift = rng.uniform(-wander, wander, (len(knots) - 2, 3))
    drift[:, 2] *= 0.44
    points[1:-1] += drift
    slopes = curve.derivative()
    curve = CubicSpline(knots, points, bc_type=((1, slopes(0)), (1, slopes(1))))
    t = np.linspace(0, 1, stations)
    if trunk:
        t = np.sort(np.r_[t, 0.018, 0.047, 0.085])
    centres = curve(t)
    tangent = curve.derivative()(t)
    initial = np.eye(3)[np.argmin(abs(tangent[0]))]
    lateral, tangent, normal = transport_frames(centres, tangent, initial).transpose(
        1, 0, 2
    )
    angles = np.linspace(0, 2 * np.pi, sides, endpoint=False)
    phase = rng.uniform(0, 2 * np.pi)
    radius = (tip + (base - tip) * (1 - t) ** 1.34)[:, None]
    radius = radius * (
        1
        + 0.11 * np.sin(3 * angles + 2.4 * t[:, None] + phase)
        + 0.08 * np.cos(2 * angles - 3.7 * t[:, None])
    )
    if trunk:
        radius += root_flare(angles, phase, base, centres[:, 2] + 0.31, False, rng)
    radial = (
        np.cos(angles)[None, :, None] * lateral[:, None, :]
        + np.sin(angles)[None, :, None] * normal[:, None, :]
    )
    surface = centres[:, None, :] + radius[..., None] * radial
    axial = np.gradient(surface, t, axis=0)
    around = np.roll(surface, -1, axis=1) - np.roll(surface, 1, axis=1)
    normals = np.cross(axial, around)
    normals /= np.linalg.norm(normals, axis=2, keepdims=True)
    mesh = close_rings(surface, normals, centres, tangent)
    meshes.add(material, mesh["vertices"], mesh["faces"])
    return curve
