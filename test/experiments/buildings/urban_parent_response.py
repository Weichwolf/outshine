"""Transient projected response of this fixed-material, single-light study.

Seven categories (roof, four walls, ground, sky), each with visible mass M,
visible-and-lit mass J, and unit-light diffuse/specular responses D/S. Individual
normals and receiver positions are shaded before integration. This is neither a
general BRDF representation nor a persistent lit-colour asset. View, light
geometry, roughness or Fresnel changes require rebuilding the response.
"""

from dataclasses import dataclass

import numpy as np
from field_sampling import bilinear
from urban_parent_conditional import grid, ground_stops, trace_transport
from urban_parent_light import shade, surface_radiance_upper
from urban_parent_phase import phase_quadrature, phase_rays


def light_geometry(profile):
    key = "position" if profile["kind"] == "point" else "direction"
    return profile["kind"], tuple(profile[key])


def centre_phase_rays(parent, origins, directions, side):
    """Shift XY phase at the projected field centre depth, including horizon rays.

    A local stationary approximation, not an exact pixel-footprint integral.
    The reference depth avoids the singular ground projection at the horizon.
    """
    product = grid(parent)
    extent = parent.frame[2] * parent.frame[3]
    target = np.array([extent / 2, extent / 2, parent.heights @ parent.probabilities])
    depth = np.maximum(np.einsum("ij,ij->i", target - origins, directions), 0.1)
    points = origins + depth[:, None] * directions
    pitch = product["header"]["pitch_m"]
    cell = (points[:, :2] - product["reference_origin_m"][:2]) / pitch
    phase, weights = phase_quadrature(side, parent.frame[5])
    shifted = np.repeat(origins[:, None], side**2, axis=1)
    shifted[:, :, :2] += pitch * (phase[None] - (cell - np.floor(cell))[:, None])
    return shifted.reshape(-1, 3), np.repeat(directions, side**2, axis=0), weights


def collect_response(
    parent,
    origins,
    directions,
    profile,
    *,
    side=6,
    batch=256,
    phase="ground",
    transmission_tolerance=0.0,
):
    """Build cost still includes exact categorical cell/height/shadow queries."""
    if type(batch) is not int or not 1 <= batch <= 4096:
        raise ValueError("bounded response packet required")
    if phase not in {"ground", "centre"}:
        raise ValueError("explicit phase projection required")
    origins, directions = np.asarray(origins, float), np.asarray(directions, float)
    if (
        origins.ndim != 2
        or origins.shape[1] != 3
        or origins.shape != directions.shape
        or len(origins) == 0
        or not np.isfinite(origins).all()
        or not np.isfinite(directions).all()
        or np.any(origins[:, 2] < 0)
        or not np.allclose(np.linalg.norm(directions, axis=1), 1, rtol=0, atol=1e-12)
    ):
        raise ValueError("finite unit rays above ground required")
    surface_radiance_upper(profile, parent.heights[-1])
    moments = np.zeros((len(origins), 7, 4))
    unit = profile | {"intensity": 1.0, "ambient": 0.0}
    phase_query = phase_rays if phase == "ground" else centre_phase_rays
    work = {
        "anchor_samples": len(origins),
        "phase_rays": 0,
        "response_bytes": moments.nbytes,
        "phase_packet_array_peak_bytes": 0,
        "received_surface_samples": 0,
        "unit_shader_samples": 0,
    }
    for first in range(0, len(origins), batch):
        part = slice(first, first + batch)
        rays, directions_at, quadrature = phase_query(
            parent, origins[part], directions[part], side
        )

        def receive(
            ids,
            slots,
            points,
            normal,
            weight,
            visibility,
            first=first,
            quadrature=quadrature,
            directions_at=directions_at,
        ):
            anchor = first + ids // side**2
            category = np.where(slots == len(parent.heights) * 5, 5, slots % 5)
            weight = weight * quadrature[ids % side**2]
            specular = shade(points, normal, -directions_at[ids], 0.0, visibility, unit)
            total = shade(points, normal, -directions_at[ids], 1.0, visibility, unit)
            for channel, values in enumerate(
                (
                    weight,
                    weight * visibility,
                    weight * (total[:, 0] - specular[:, 0]),
                    weight * specular[:, 0],
                )
            ):
                np.add.at(moments[..., channel], (anchor, category), values)
            work["received_surface_samples"] += len(ids)
            work["unit_shader_samples"] += 2 * len(ids)

        coverage, counts = trace_transport(
            parent,
            rays,
            directions_at,
            profile,
            receive,
            transmission_tolerance=transmission_tolerance,
        )
        sky = ~np.isfinite(ground_stops(rays, directions_at))
        mass = ((1 - coverage) * sky).reshape(-1, side**2) @ quadrature
        moments[part, 6, 0] = mass
        moments[part, 6, 1] = mass
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
    return moments, work


@dataclass
class ProjectedResponse:
    """Per-view temporary field; caller owns view identity and invalidation."""

    moments: np.ndarray
    geometry: tuple

    def __post_init__(self):
        if (
            self.moments.ndim != 4
            or self.moments.shape[2:] != (7, 4)
            or min(self.moments.shape[:2]) < 1
            or not np.isfinite(self.moments).all()
            or np.any(self.moments < -1e-14)
        ):
            raise ValueError("finite nonnegative projected response required")

    def sample(self, uv):
        uv = np.asarray(uv, float)
        if uv.ndim != 2 or uv.shape[1] != 2 or not np.isfinite(uv).all():
            raise ValueError("finite two-dimensional coordinates required")
        height, width = self.moments.shape[:2]
        edge = 0.5 / np.array([width, height])
        sampled = bilinear(
            self.moments.reshape(height, width, 28), np.clip(uv, edge, 1 - edge)
        )
        return sampled.reshape(-1, 7, 4)

    def evaluate(self, uv, profile, *, sky=0.1):
        if light_geometry(profile) != self.geometry:
            raise ValueError("changed light geometry requires response rebuild")
        if (
            not np.isfinite([profile["intensity"], profile["ambient"], sky]).all()
            or min(profile["intensity"], profile["ambient"], sky) < 0
        ):
            raise ValueError("finite nonnegative lighting required")
        terms = self.sample(uv)
        albedo = np.array([0.6] * 5 + [0.2])
        value = (
            profile["intensity"] * (albedo * terms[:, :6, 2] + terms[:, :6, 3])
            + profile["ambient"] * albedo * terms[:, :6, 0]
        ).sum(axis=1) + sky * terms[:, 6, 0]
        colour = np.broadcast_to(value[:, None], (len(value), 3)).copy()
        coverage = terms[:, :5, 0].sum(axis=1)
        return colour, coverage
