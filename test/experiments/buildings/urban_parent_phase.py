"""Local recipe-phase quadrature for unresolved pixels; no baked lighting."""

import numpy as np
from urban_parent_conditional import grid, render_conditional


def phase_quadrature(side, fill):
    """Split at footprint edges; integrate gaps and occupied phase separately."""
    if type(side) is not int or not 3 <= side <= 16 or not 0 < fill < 1:
        raise ValueError("bounded phase quadrature required")
    roots, weights = np.polynomial.legendre.leggauss(side - 2)
    edge = (1 - fill) / 2
    axis = np.r_[edge / 2, 0.5 + roots * fill / 2, 1 - edge / 2]
    weights = np.r_[edge, weights * fill / 2, edge]
    x, y = np.meshgrid(axis, axis)
    return np.column_stack((x.ravel(), y.ravel())), np.outer(weights, weights).ravel()


def phase_rays(parent, origins, directions, side):
    """Keep macro view directions but integrate phase in the ground-hit cell."""
    if np.any(directions[:, 2] >= 0):
        raise ValueError("downward rays required for ground phase")
    product = grid(parent)
    pitch = product["header"]["pitch_m"]
    ground = origins - (origins[:, 2] / directions[:, 2])[:, None] * directions
    cell = (ground[:, :2] - product["reference_origin_m"][:2]) / pitch
    phase, weights = phase_quadrature(side, parent.frame[5])
    offset = pitch * (phase[None] - (cell - np.floor(cell))[:, None])
    shifted = np.repeat(origins[:, None], side**2, axis=1)
    shifted[:, :, :2] += offset
    return shifted.reshape(-1, 3), np.repeat(directions, side**2, axis=0), weights


def render_phase(
    parent, origins, directions, profile, *, side=4, batch=256, shadow_transfer=None
):
    if type(batch) is not int or not 1 <= batch <= 4096:
        raise ValueError("bounded macro packet required")
    colour, coverage = np.empty_like(origins), np.empty(len(origins))
    work = {
        "macro_samples": len(origins),
        "phase_rays": 0,
        "phase_packet_array_peak_bytes": 0,
    }
    for first in range(0, len(origins), batch):
        part = slice(first, first + batch)
        rays, directions_at, weights = phase_rays(
            parent, origins[part], directions[part], side
        )
        if shadow_transfer is not None:
            shadow_transfer.prepare(first, len(origins[part]))
            work["retained_visibility_buffer_peak_bytes"] = max(
                work.get("retained_visibility_buffer_peak_bytes", 0),
                shadow_transfer.visibility.nbytes,
            )
            if shadow_transfer.phase_side != side:
                raise ValueError("transfer and rendered phase layout must match")
        rgb, alpha, counts = render_conditional(
            parent, rays, directions_at, profile, approximation=shadow_transfer
        )
        colour[part] = np.einsum("nsc,s->nc", rgb.reshape(-1, side**2, 3), weights)
        coverage[part] = alpha.reshape(-1, side**2) @ weights
        work["phase_rays"] += len(rays)
        work["phase_packet_array_peak_bytes"] = max(
            work["phase_packet_array_peak_bytes"], rays.nbytes + directions_at.nbytes
        )
        for key, value in counts.items():
            work[key] = (
                max(work.get(key, 0), value)
                if key.endswith("peak_bytes")
                else work.get(key, 0) + value
            )
    return colour, coverage, work
