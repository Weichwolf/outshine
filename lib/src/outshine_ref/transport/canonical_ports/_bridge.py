"""Source-derived landing planes and C1 profiles for narrow straight bridge chains."""

import math
from itertools import pairwise

import numpy as np

from ...contracts import Mesh
from ..endpoint_grades import plan_endpoint_transition
from ._bridge_lift import clear_coarse_observations, source_observations
from ._geodesy import ground_geographic_at, positions_in_frame
from ._ground import strip_frame
from ._queries import CoarseDomain


class BridgeField:
    def __init__(self, domain, profile, origin, tangent, left, s0, s1, planes, cross):
        self.domain, self.profile, self.origin = domain, profile, origin
        self.tangent, self.left = tangent, left
        self.s0, self.s1, self.planes, self.cross = s0, s1, planes, cross

    def evaluate(self, xy):
        self.domain.part_at(xy)
        s = float((xy - self.origin) @ self.tangent)
        v = float((xy - self.origin) @ self.left)
        if s < self.s0:
            a, b = self.planes[0]
            h = a + b * s
            grade = b
        elif s > self.s1:
            a, b = self.planes[1]
            h = a + b * s
            grade = b
        else:
            at = [s - self.s0]
            h = float(self.profile.evaluate(at)[0])
            grade = float(self.profile.evaluate(at, 1)[0])
        return h + self.cross * v, grade * self.tangent + self.cross * self.left

    @property
    def queries(self):
        return self.domain.queries

    @property
    def face_bounds_tests(self):
        return self.domain.face_bounds_tests


def landing_planes(
    anchor, axis, west, east, width, tangent, left, terrain, quality, budget
):
    samples = []
    owners = []
    for side, indices in enumerate((range(west + 1), range(east, len(axis)))):
        for i in indices:
            samples.extend(
                axis[i] + np.array([-width / 2, 0, width / 2])[:, None] * left
            )
            owners.extend([side] * 3)
    if len(samples) + 4 > budget.terrain_samples:
        raise ValueError("terrain_sample_budget")
    samples = np.array(samples)
    owners = np.array(owners)
    at = ground_geographic_at(anchor, samples)
    h = np.asarray(terrain(at), float)
    if h.shape != (len(at),) or not np.isfinite(h).all() or np.any(abs(h) > 1e6):
        raise ValueError("invalid_landing_model_heights")
    xyz = positions_in_frame(anchor, at, h)
    s = (xyz[:, :2] - axis[0]) @ tangent
    v = (xyz[:, :2] - axis[0]) @ left
    centres = [float(s[owners == i].mean()) for i in (0, 1)]
    design = np.c_[
        owners == 0,
        (owners == 0) * (s - centres[0]),
        owners == 1,
        (owners == 1) * (s - centres[1]),
        v,
    ]
    fit = np.linalg.lstsq(design, xyz[:, 2], rcond=None)[0]
    # Each landing plane envelopes its bound ground samples within the same quality limit.
    residual = xyz[:, 2] - design @ fit
    for i in (0, 1):
        fit[2 * i] += max(0.0, float(residual[owners == i].max()))
    cross = float(fit[4])
    planes = [
        (float(fit[2 * i] - fit[2 * i + 1] * centres[i]), float(fit[2 * i + 1]))
        for i in (0, 1)
    ]
    error = float(abs(design @ fit - xyz[:, 2]).max())
    if error > quality:
        raise ValueError("landing DEM plane error")
    return planes, cross, xyz, error


def domain_mesh(axis, geo, anchor, width, terrain):
    # Preserve original per-span domain; only terminal centres acquire actual DEM XY.
    outer_geo = geo[[0, -1]]
    outer_xyz = positions_in_frame(anchor, outer_geo, np.asarray(terrain(outer_geo)))
    axis = axis.copy()
    axis[[0, -1]] = outer_xyz[:, :2]
    _, _, miter = strip_frame(axis, width)
    xy = (
        axis[:, None, :]
        + np.array([-width / 2, width / 2])[None, :, None] * miter[:, None, :]
    ).reshape(-1, 2)
    first = 2 * np.arange(len(axis) - 1)
    faces = np.vstack(
        (np.c_[first, first + 2, first + 1], np.c_[first + 2, first + 3, first + 1])
    )
    mesh = Mesh(np.c_[xy, np.zeros(len(xy))], faces)
    return mesh, outer_xyz


def prepare_bridge(index, selected, chain, terrain, quality, second, budget):
    _nodes, spans, _incidence = index
    geographic, ordered_nodes, width, grade = chain
    if not 2 <= len(ordered_nodes) <= 256 or not 0 < width <= 30:
        raise ValueError("source_strip_recipe_domain")
    by_nodes = {
        tuple(sorted(spans[identity].nodes)): spans[identity] for identity in selected
    }
    ordered_edges = [by_nodes[tuple(sorted(pair))] for pair in pairwise(ordered_nodes)]
    flags = np.array([e.bridge for e in ordered_edges])
    bridge = np.flatnonzero(flags)
    if (
        not len(bridge)
        or bridge[0] == 0
        or bridge[-1] == len(flags) - 1
        or not flags[bridge[0] : bridge[-1] + 1].all()
    ):
        raise ValueError("one_bridge_band_with_both_ground_connections_required")
    geo = np.asarray(geographic, float)
    anchor = tuple(map(float, geo[0]))
    axis = positions_in_frame(anchor, geo, np.zeros(len(geo)))[:, :2]
    tangent = axis[-1] - axis[0]
    length = float(np.linalg.norm(tangent))
    if length < 1e-6:
        raise ValueError("collapsed_source_axis")
    tangent /= length
    left = np.array([-tangent[1], tangent[0]])
    _stations, directions, _miters = strip_frame(axis, width)
    if np.any(np.diff((axis - axis[0]) @ tangent) <= 1e-6):
        raise ValueError("nonmonotone_bridge_source_axis")
    if abs((axis - axis[0]) @ left).max() > 1e-4 * width:
        raise ValueError("straight projected source recipe required")
    theta = math.radians(float(abs(geo - geo[0]).max(axis=0).sum())) + width / 6.3e6
    if theta >= min(math.atan(grade), math.atan(0.025)):
        raise ValueError("source-frame physical-grade reserve exhausted")
    source_grade = math.tan(math.atan(grade) - theta)
    west, east = int(bridge[0]), int(bridge[-1] + 1)
    planes, cross, xyz, error = landing_planes(
        anchor, axis, west, east, width, tangent, left, terrain, quality, budget
    )
    if abs(cross) > math.tan(math.atan(0.025) - theta) or abs(cross) >= source_grade:
        raise ValueError("cross-grade reserve exhausted")
    longitudinal_limit = math.sqrt(source_grade**2 - cross**2)
    source_cross_bound = abs(cross) + longitudinal_limit * float(
        abs(directions @ left).max()
    )
    physical_cross_bound = math.tan(math.atan(source_cross_bound) + theta)
    if physical_cross_bound > 0.025:
        raise ValueError("physical_source_direction_cross_grade_reserve_exhausted")
    # Actual landing centre positions include their DEM model heights.
    landing_geo = geo[[west, east]]
    landing_xyz = positions_in_frame(
        anchor, landing_geo, np.asarray(terrain(landing_geo))
    )
    s0, s1 = (landing_xyz[:, :2] - axis[0]) @ tangent
    heights = [
        planes[i][0] + planes[i][1] * station for i, station in enumerate((s0, s1))
    ]
    profile, report = plan_endpoint_transition(
        *heights,
        float(s1 - s0),
        planes[0][1],
        planes[1][1],
        grade=longitudinal_limit,
        second_derivative=second,
        method="envelope",
    )
    if profile is None:
        raise ValueError(report["reason"])
    coarse = source_observations(
        anchor, axis, west, east, width, tangent, left, terrain, budget
    )
    profile, lift_report = clear_coarse_observations(
        profile,
        coarse,
        axis[0],
        tangent,
        left,
        float(s0),
        float(s1),
        planes,
        cross,
        longitudinal_limit,
        second,
    )
    mesh, outer_xyz = domain_mesh(axis, geo, anchor, width, terrain)
    domain = CoarseDomain(mesh, np.zeros(len(mesh.triangles), int))
    field = BridgeField(
        domain, profile, axis[0], tangent, left, float(s0), float(s1), planes, cross
    )
    return (
        anchor,
        field,
        {
            "profile": report,
            "coarse_DEM_lift": lift_report,
            "sampled_ground_vertices": len(xyz),
            "landing_maximum_DEM_plane_error_m": error,
            "maximum_source_grade": source_grade,
            "maximum_frame_rotation_bound_rad": theta,
            "maximum_physical_cross_grade_bound": physical_cross_bound,
            "ground_planes_source_ENU": planes,
            "source_landing_positions": landing_xyz.tolist(),
            "selected_method": "bridge_endpoint_envelope",
            "terrain_samples": len(xyz) + 4 + len(coarse),
            "input_vertices": len(mesh.vertices),
            "input_faces": len(mesh.triangles),
            "input_payload_bytes": sum(
                a.nbytes
                for a in (
                    mesh.vertices,
                    mesh.triangles,
                    field.domain.parts,
                    xyz,
                    landing_xyz,
                    outer_xyz,
                    coarse,
                )
            ),
            "field_domain_payload_bytes": sum(
                a.nbytes for a in (mesh.vertices, mesh.triangles, field.domain.parts)
            ),
            "numeric_observation_bytes": sum(
                a.nbytes for a in (xyz, landing_xyz, outer_xyz, coarse)
            ),
            "scope": "Straight uniform exhausted source component; common-cross-grade landing planes and C1 source field. Crossing/deck-clearance qualification remains separate and unproved.",
        },
    )
