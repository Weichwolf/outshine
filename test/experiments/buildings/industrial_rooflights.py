"""Bounded roof-facet replacements and actual shafts into existing provider halls."""

import math
from functools import lru_cache

import numpy as np
import shapely
from facade_parts import append
from industrial_hall_details import products, region_solid
from industrial_hall_shell import metric, structure
from outshine_ref.buildings.modelling import PrismVolume
from outshine_ref.buildings.section_roofs import RoofSection
from outshine_ref.contracts import immutable

MAX_FIELDS = 64
FRAME_M = 0.08
DEPTH_M = 0.12
GLASS_INSET_M = 0.04
GLASS_M = 0.006


def plan(hall, section, *, max_fields=MAX_FIELDS):
    """Camera/state-independent logical rectangles; never expand a mesh here."""
    if type(max_fields) is not int or not 1 <= max_fields <= MAX_FIELDS:
        raise ValueError("Rooflight capacity requires 1..64 fields")
    if not isinstance(section, RoofSection):
        raise TypeError("Rooflights require an immutable metrical RoofSection")
    interior = shapely.from_wkb(hall["interior_wkb"])
    clear = interior.buffer(-0.35, join_style=2)
    local = shapely.transform(
        clear, lambda xy: (xy - section.origin_xy) @ section.axes.T
    )
    if local.is_empty:
        return ()
    _, low, _, high = local.bounds
    count = min(8, max(1, math.floor((high - low) / 4.3)))
    cell = (high - low) / count
    length = min(4.0, cell - 0.60)
    if length < 1:
        return ()
    output = []
    for interval, (first, last) in enumerate(zip(section.knots, section.knots[1:])):
        start, z0 = first
        end, z1 = last
        slope = (z1 - z0) / (end - start)
        if slope >= -0.1:
            continue
        margin = max(0.25, (end - start) * 0.18)
        a, b = start + margin, end - margin
        if b - a < 0.45 or z1 < hall["ceiling_m"] + DEPTH_M + 0.02:
            continue
        for column in range(count):
            centre = low + (column + 0.5) * cell
            bounds = (a, centre - length / 2, b, centre + length / 2)
            patch = shapely.box(*bounds)
            if not local.covers(patch):
                continue
            if len(output) == max_fields:
                raise ValueError("Rooflight capacity exceeded before mesh expansion")
            world = shapely.transform(
                patch, lambda xy: xy @ section.axes + section.origin_xy
            )
            output.append(
                {
                    "id": hall["id"] + f":roof-interval-{interval}:light-{column}",
                    "interval": interval,
                    "column": column,
                    "bounds": bounds,
                    "slope": slope,
                    "intercept_m": z0 - slope * start,
                    "axes": section.axes.tolist(),
                    "origin_xy": section.origin_xy,
                    "footprint_wkb": world.wkb_hex,
                    "ceiling_m": hall["ceiling_m"],
                    "roof_top_m": max(p[1] for p in section.knots),
                    "origin": "Procedural roof glazing; no measured provider rooflights",
                }
            )
    return immutable(output)


def cuts(fields, material):
    return [
        PrismVolume(
            item["id"] + ":lightwell",
            shapely.from_wkb(item["footprint_wkb"]),
            item["ceiling_m"] - 0.02,
            item["roof_top_m"] + 0.10,
            material,
        )
        for item in fields
    ]


@lru_cache(maxsize=32)
def templates(width, length, slope, method="ring"):
    if method not in ("ring", "bars"):
        raise ValueError("Unknown rooflight frame construction")
    inset = FRAME_M / math.hypot(1, slope)
    outside = shapely.box(0, 0, width, length)
    inside = shapely.box(inset, FRAME_M, width - inset, length - FRAME_M)
    if inside.is_empty or width <= 2 * inset or length <= 2 * FRAME_M:
        raise ValueError("Rooflight has no clear glazing area")
    if method == "ring":
        regions = [outside.difference(inside)]
    else:
        regions = [
            shapely.box(0, 0, width, FRAME_M),
            shapely.box(0, length - FRAME_M, width, length),
            shapely.box(0, FRAME_M, inset, length - FRAME_M),
            shapely.box(width - inset, FRAME_M, width, length - FRAME_M),
        ]
    components = []
    for region, depth, top, material in [
        *((r, DEPTH_M, 0.0, 10) for r in regions),
        (inside, GLASS_M, -GLASS_INSET_M, 3),
    ]:
        raw = region_solid(region, depth, material)
        raw["vertices"][:, 2] += slope * raw["vertices"][:, 0] + top - depth
        value = metric(raw, material)
        for array in value.values():
            array.flags.writeable = False
        components.append(value)
    return tuple(components)


def components(fields, *, method="ring"):
    output = []
    for item in fields:
        a, low, b, high = item["bounds"]
        axes = np.asarray(item["axes"])
        transform = np.eye(3)
        transform[:2, :2] = axes
        origin = np.r_[
            np.array([a, low]) @ axes + item["origin_xy"],
            item["intercept_m"] + item["slope"] * a,
        ]
        for part in templates(b - a, high - low, item["slope"], method):
            output.append(
                dict(
                    part,
                    vertices=part["vertices"] @ transform + origin,
                    normals=part["normals"] @ transform,
                )
            )
    return output


def assemble(hall, envelope, fields, material, opened, view, *, method="ring"):
    body, shell = structure(hall, envelope, material, extra_cuts=cuts(fields, material))
    prior, detailed = products(hall, material, opened, view)
    added = components(fields, method=method)
    return append(body, [*prior, *added]), shell, body, added, detailed
