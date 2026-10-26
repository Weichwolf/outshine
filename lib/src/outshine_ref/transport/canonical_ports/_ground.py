"""Whole normalized ground chains, full-width DEM observations and bounded fields."""

import math

import numpy as np
import shapely

from ...contracts import Mesh
from ..port_heights import PortHeightInput, plan_port_heights
from ._affine import AffineGround
from ._geodesy import ground_geographic_at, positions_in_frame
from ._queries import CoarseFieldQuery


def strip_frame(xy, width):
    if not 2 <= len(xy) <= 256 or not 0 < width <= 30:
        raise ValueError("source_strip_recipe_domain")
    delta = np.diff(xy, axis=0)
    lengths = np.linalg.norm(delta, axis=1)
    if np.any(lengths < 1e-6):
        raise ValueError("collapsed_source_axis")
    tangent = delta / lengths[:, None]
    normal = np.c_[-tangent[:, 1], tangent[:, 0]]
    if len(tangent) > 1:
        products = np.sum(tangent[:-1] * tangent[1:], axis=1)
        if np.any(products < np.cos(np.pi / 6)):
            raise ValueError("source_turn_exceeds_coarse_strip_recipe")
        middle = (normal[:-1] + normal[1:]) / (1 + products)[:, None]
        miter = np.vstack((normal[0], middle, normal[-1]))
    else:
        miter = np.vstack((normal[0], normal[-1]))
    sides = (
        xy[:, None, :]
        + np.array([-width / 2, width / 2])[None, :, None] * miter[:, None, :]
    )
    polygon = shapely.Polygon(np.vstack((sides[:, 0], sides[::-1, 1])))
    if not polygon.is_valid or polygon.is_empty:
        raise ValueError("folded_source_strip")
    return np.r_[0.0, np.cumsum(lengths)], tangent, miter


def plane_bands(original):
    end = original[-1]
    if end < 20:
        raise ValueError("source_too_short_for_coarse_plane_bands")
    if math.ceil(end / 20) - 1 > 256:
        raise ValueError("coarse_control_budget")
    controls = np.unique(np.r_[original, np.arange(20, end, 20)])
    if len(controls) > 256:
        raise ValueError("coarse_control_budget")
    boundaries, roles = [], []
    for i, station in enumerate(controls):
        left = controls[i - 1] if i else station
        right = controls[i + 1] if i + 1 < len(controls) else station
        before = station - left if i else right - station
        after = right - station if i + 1 < len(controls) else station - left
        half = min(2.0, 0.2 * min(before, after))
        boundaries.append(0.0 if i == 0 else float(station - half))
        boundaries.append(
            float(end) if i + 1 == len(controls) else float(station + half)
        )
        roles.append(1)
        if i + 1 < len(controls):
            roles.append(0)
    return np.array(boundaries), np.array(roles)


def coarse_input(axis, width, maximum_gradient, terrain_positions, budget):
    original, tangent, miter = strip_frame(axis, width)
    boundaries, roles = plane_bands(original)
    stations = np.unique(np.r_[boundaries, original])
    if 2 * len(stations) > budget.terrain_samples:
        raise ValueError("terrain_sample_budget")
    centres = np.column_stack(
        [np.interp(stations, original, axis[:, i]) for i in (0, 1)]
    )
    miters = np.column_stack(
        [np.interp(stations, original, miter[:, i]) for i in (0, 1)]
    )
    xy = (
        centres[:, None, :]
        + np.array([-width / 2, width / 2])[None, :, None] * miters[:, None, :]
    ).reshape(-1, 2)
    first = 2 * np.arange(len(stations) - 1)
    faces = np.vstack(
        (np.c_[first, first + 2, first + 1], np.c_[first + 2, first + 3, first + 1])
    )
    midpoints = (stations[:-1] + stations[1:]) * 0.5
    owners = np.tile(np.searchsorted(boundaries, midpoints, side="right") - 1, 2)
    indices = np.searchsorted(stations, boundaries[1:-1])
    ports = np.c_[2 * indices, 2 * indices + 1]
    links = np.c_[np.arange(len(roles) - 1), np.arange(1, len(roles))]
    edge = np.clip(
        np.searchsorted(original, midpoints, side="right") - 1, 0, len(original) - 2
    )
    directions = tangent[edge]
    normals = np.c_[-directions[:, 1], directions[:, 0]]
    frames = np.tile(
        np.stack([directions / maximum_gradient, normals / 0.025], axis=1), (2, 1, 1)
    )
    xyz = terrain_positions(xy)
    return PortHeightInput(Mesh(xyz, faces), owners, roles, ports, links, frames)


def prepare_ground(chain, terrain, budget):
    geographic, _order, width, maximum_gradient = chain
    geographic = np.asarray(geographic, float)
    anchor = tuple(geographic[0])
    axis = positions_in_frame(anchor, geographic, np.zeros(len(geographic)))[:, :2]
    samples = 0

    def sample_positions(xy):
        nonlocal samples
        samples += len(xy)
        if samples > budget.terrain_samples:
            raise ValueError("terrain_sample_budget")
        at = ground_geographic_at(anchor, xy)
        heights = np.asarray(terrain(at), float)
        if heights.shape != (len(at),) or not np.isfinite(heights).all():
            raise ValueError("missing_or_invalid_bound_terrain_samples")
        if np.any(abs(heights) > 1e6):
            raise ValueError("model_height_domain_exceeded")
        return positions_in_frame(anchor, at, heights)

    inputs = coarse_input(axis, width, maximum_gradient, sample_positions, budget)
    return anchor, inputs, samples


def fit_ground(inputs, error_limit_m, method, budget):
    affine_report = None
    if method in ("auto", "affine"):
        affine = AffineGround(inputs)
        affine_report = affine.report
        if affine_report["maximum_original_prior_error_m"] <= error_limit_m:
            return affine, {"selected_method": "affine", **affine_report}
        if method == "affine":
            return None, {"reason": "ground_prior_error_limit", **affine_report}
    fit = plan_port_heights(inputs, budget=budget.surface, method="cone")
    report = dict(fit.report)
    report.update(selected_method="cone", affine_control=affine_report)
    if fit.plan is None:
        return None, report
    if report["maximum_original_prior_error_m"] > error_limit_m:
        report["reason"] = "ground_prior_error_limit"
        return None, report
    return CoarseFieldQuery(inputs, fit.plan), report
