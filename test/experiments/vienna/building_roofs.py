"""Compile restrained gables within the mapped height of a building envelope."""

from dataclasses import dataclass

import numpy as np
import shapely
from outshine_ref.buildings.section_roofs import RoofSection, section_surface

DEFAULT_PITCH = np.tan(np.deg2rad(28))
FLAT_FALLBACK_TYPES = {"garage", "industrial", "parking", "shed", "warehouse"}


@dataclass(frozen=True)
class Gable:
    eave: float
    section: RoofSection


def gable_plan(footprint, bottom, top, tags):
    shape = tags.get("roof:shape", tags.get("roof_shape"))
    if shape not in (None, "gable", "gabled") or footprint.interiors:
        return None
    height = top - bottom
    if height < 5.5:
        return None
    if shape is None and (
        tags.get("building", tags.get("class")) in FLAT_FALLBACK_TYPES
        or not 6 <= height <= 27
        or not 55 <= footprint.area <= 1100
    ):
        return None
    with np.errstate(invalid="ignore", divide="ignore"):
        envelope = shapely.minimum_rotated_rectangle(footprint.convex_hull)
    if not envelope.area:
        return None
    corners = shapely.get_coordinates(envelope)[:4]
    edges = np.roll(corners, -1, axis=0) - corners
    widths = np.linalg.norm(edges, axis=1)
    short, long = min(widths), max(widths)
    if shape is None and (
        footprint.area < 0.86 * envelope.area or short < 4.5 or long < 1.16 * short
    ):
        return None
    orientation = tags.get("roof:orientation")
    edge = np.argmax(widths) if orientation == "across" else np.argmin(widths)
    axis = edges[edge] / widths[edge]
    try:
        direction = float(tags.get("roof:direction", "nan"))
    except (TypeError, ValueError):
        direction = np.nan
    if np.isfinite(direction):
        bearing = np.deg2rad(direction)
        axis = np.array([np.sin(bearing), np.cos(bearing)])
    if axis[0] < 0 or (axis[0] == 0 and axis[1] < 0):
        axis = -axis
    xy = shapely.get_coordinates(footprint)
    run = xy[:, 0] * axis[0] + xy[:, 1] * axis[1]
    first, last = run.min(), run.max()
    try:
        rise = float(tags.get("roof:height", "nan"))
    except (TypeError, ValueError):
        rise = np.nan
    if not np.isfinite(rise):
        rise = min((last - first) * 0.5 * DEFAULT_PITCH, height * 0.27, 4.5)
    if not 0.55 <= rise <= height - 3.0:
        return None
    middle = (first + last) / 2
    eave = top - rise
    return Gable(
        eave, RoofSection(((first, eave), (middle, top), (last, eave)), tuple(axis))
    )


def gable_ends(footprint, plan):
    axis = np.asarray(plan.section.axis_xy)
    middle = plan.section.knots[1][0]
    knots = np.asarray(plan.section.knots)
    xy = np.asarray(shapely.orient_polygons(footprint).exterior.coords)
    run = xy[:, 0] * axis[0] + xy[:, 1] * axis[1]
    eave = plan.eave
    triangles = []
    for a, b, u, v in zip(xy[:-1], xy[1:], run[:-1], run[1:], strict=True):
        if min(u, v) < middle < max(u, v):
            crossing = a + (b - a) * ((middle - u) / (v - u))
            sections = [(a, crossing, u, middle), (crossing, b, middle, v)]
        else:
            sections = [(a, b, u, v)]
        for left, right, start, stop in sections:
            za, zb = np.interp([start, stop], knots[:, 0], knots[:, 1])
            lower_a, lower_b = [*left, eave], [*right, eave]
            upper_b, upper_a = [*right, zb], [*left, za]
            if zb > eave + 1e-8:
                triangles.append((lower_a, lower_b, upper_b))
            if za > eave + 1e-8:
                triangles.append((lower_a, upper_b, upper_a))
    return np.asarray(triangles).reshape(-1, 3)


def compile_gable(footprint, plan):
    try:
        roof = section_surface(footprint, plan.section)
    except (ValueError, shapely.GEOSException):
        return None
    return roof["vertices"][roof["faces"]].reshape(-1, 3), gable_ends(footprint, plan)
