"""Pitched roof surfaces within mapped building envelopes."""

from dataclasses import dataclass

import numpy as np
import shapely

from outshine.buildings.modelling import (
    SkeletonBudgetExceeded,
    SkeletonLimits,
    prepare_hip,
)
from outshine.buildings.section_roofs import RoofSection, section_surface

DEFAULT_PITCH = np.tan(np.deg2rad(28))
MIN_GABLE_AREA = 55.0
FLAT_FALLBACK_TYPES = {"garage", "industrial", "parking", "shed", "warehouse"}
HIP_LIMITS = SkeletonLimits(vertices=96, nodes=384)


@dataclass(frozen=True)
class Gable:
    eave: float
    section: RoofSection


@dataclass(frozen=True)
class PitchedRoof:
    eave: float
    surface: np.ndarray
    ends: np.ndarray


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
        or not MIN_GABLE_AREA <= footprint.area <= 1100
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


def hip_roof(footprint, bottom, top, tags):
    try:
        plan = prepare_hip(footprint, method="contour", limits=HIP_LIMITS)
    except (ValueError, SkeletonBudgetExceeded, shapely.GEOSException):
        return None
    try:
        rise = float(tags.get("roof:height", "nan"))
    except (TypeError, ValueError):
        rise = np.nan
    if not np.isfinite(rise):
        rise = min(plan.distances_m.max() * DEFAULT_PITCH, (top - bottom) * 0.27, 4.5)
    if not 0.55 <= rise <= top - bottom - 3.0:
        return None
    eave = top - rise
    triangles = shapely.get_parts(
        shapely.constrained_delaunay_triangles([facet.polygon for facet in plan.facets])
    )
    xy = (
        shapely.get_coordinates(shapely.orient_polygons(triangles))
        .reshape(-1, 4, 2)[:, :3]
        .reshape(-1, 2)
    )
    elevations = dict(
        zip(
            map(tuple, plan.positions_m),
            eave + plan.distances_m * plan.factor(rise, "byHeight"),
            strict=True,
        )
    )
    surface = np.column_stack((xy + plan.origin, [elevations[tuple(p)] for p in xy]))
    return PitchedRoof(eave, surface, np.empty((0, 3)))


def pitched_roof(footprint, bottom, top, tags, *, infer_hip=True):
    shape = tags.get("roof:shape", tags.get("roof_shape"))
    gable = gable_plan(footprint, bottom, top, tags)
    if gable is not None and (mesh := compile_gable(footprint, gable)) is not None:
        return PitchedRoof(gable.eave, *mesh)
    if shape == "hipped" or (
        shape is None
        and infer_hip
        and 10 <= top - bottom <= 27
        and 120 <= footprint.area <= 5500
        and tags.get("building", tags.get("class")) not in FLAT_FALLBACK_TYPES
    ):
        return hip_roof(footprint, bottom, top, tags)
    return None


def shared_roof_components(footprints, tops, tags, eligible, contacts):
    links = [[] for _ in footprints]
    for a, b in contacts:
        if not eligible[a] or not eligible[b] or abs(tops[a] - tops[b]) > 0.75:
            continue
        ta, tb = tags[a], tags[b]
        shape = ta.get("roof:shape", ta.get("roof_shape"))
        if shape not in (None, "hipped", "gabled", "gable") or shape != tb.get(
            "roof:shape", tb.get("roof_shape")
        ):
            continue
        if any(
            key.startswith("roof:") and key != "roof:shape"
            for record in (ta, tb)
            for key in record
        ):
            continue
        if any(
            t.get("building", t.get("class")) in FLAT_FALLBACK_TYPES for t in (ta, tb)
        ):
            continue
        x, y = footprints[a], footprints[b]
        if min(x.area, y.area) < MIN_GABLE_AREA:
            continue
        contact = x.intersection(y)
        if contact.length >= 1.2 and contact.area <= min(
            4.0, x.area * 0.05, y.area * 0.05
        ):
            links[a].append(b)
            links[b].append(a)
    seen = set()
    for i, adjacent in enumerate(links):
        if not adjacent or i in seen:
            continue
        group = [i]
        seen.add(i)
        for j in group:
            for k in links[j]:
                if k not in seen:
                    seen.add(k)
                    group.append(k)
        if 2 <= len(group) <= 12 and np.ptp(tops[group]) <= 0.75:
            yield group


def plan_roofs(
    footprints, bottoms, tops, tags, eligible, *, minimum_rise=0.0, infer_hip=True
):
    eaves = np.array(tops, dtype=float)
    tops = np.asarray(tops)
    bottoms = np.asarray(bottoms)
    eligible = np.asarray(eligible, dtype=bool)
    if not np.any(eligible):
        return eaves, []
    tree = shapely.STRtree(footprints)
    minimum_rise = np.broadcast_to(minimum_rise, tops.shape)
    roofs = []

    def place(footprint, group):
        i = group[0]
        top = tops[group].min()
        roof = pitched_roof(
            footprint,
            bottoms[group].max(),
            top,
            tags[i],
            infer_hip=infer_hip,
        )
        if roof is None or top - roof.eave < minimum_rise[group].max():
            return
        adjacent = tree.query(footprint, "intersects")
        adjacent = adjacent[(tops[adjacent] > roof.eave) & (bottoms[adjacent] < top)]
        for j in adjacent:
            if j in group:
                continue
            other = footprints[j]
            props = tags[j]
            shape = props.get("roof:shape", props.get("roof_shape"))
            flat_appendage = (
                bottoms[j] <= roof.eave
                and tops[j] >= top
                and shape in (None, "flat")
                and other.area < MIN_GABLE_AREA
            )
            if not flat_appendage and footprint.intersection(other).area > 1e-6:
                return
        eaves[group] = roof.eave
        roofs.append((i, roof))

    if infer_hip:
        pairs = tree.query(footprints, "intersects").T
        contacts = pairs[pairs[:, 0] < pairs[:, 1]]
        for group in shared_roof_components(footprints, tops, tags, eligible, contacts):
            footprint = shapely.union_all([footprints[i] for i in group])
            if isinstance(footprint, shapely.Polygon):
                place(footprint, group)
    for i in np.flatnonzero(eligible & (eaves == tops)):
        place(footprints[i], [i])
    return eaves, roofs
