"""Resolve mapped façade colours on building outlines and their visible parts."""

from string import hexdigits

import numpy as np
import shapely
from outshine_ref.buildings.structures import suppressed
from PIL import ImageColor

MIN_PARENT_COVERAGE = 0.9


def mapped_colour(properties):
    value = properties.get("colour")
    if not isinstance(value, str):
        return None
    value = value.strip()
    if len(value) == 6 and all(channel in hexdigits for channel in value):
        value = "#" + value
    try:
        rgb = ImageColor.getrgb(value)
    except ValueError:
        return None
    if len(rgb) == 4 and rgb[3] != 255:
        return None
    if not all(0 <= channel <= 255 for channel in rgb[:3]):
        return None
    return "#" + bytes(rgb[:3]).hex()


def facade_colours(geometries, properties):
    colours = [mapped_colour(p) for p in properties]
    parents = [
        index
        for index, p in enumerate(properties)
        if colours[index] is not None and suppressed(p)
    ]
    if not parents:
        return colours
    children = [
        index
        for index, p in enumerate(properties)
        if colours[index] is None and not suppressed(p)
    ]
    if not children:
        return colours
    shapes = np.asarray(geometries, dtype=object)
    index = shapely.STRtree(shapes[parents])
    child_ids, parent_ids = index.query(shapes[children], predicate="intersects")
    child_ids = np.asarray(children)[child_ids]
    parent_ids = np.asarray(parents)[parent_ids]
    child_shapes = shapes[child_ids]
    parent_shapes = shapes[parent_ids]
    child_area = shapely.area(child_shapes)
    overlap = shapely.area(shapely.intersection(child_shapes, parent_shapes))
    coverage = np.divide(
        overlap, child_area, out=np.zeros_like(overlap), where=child_area > 0
    )
    eligible = coverage >= MIN_PARENT_COVERAGE
    child_ids, parent_ids = child_ids[eligible], parent_ids[eligible]
    coverage = coverage[eligible]
    parent_area = shapely.area(shapes[parent_ids])
    owner = {}
    for child, parent, fraction, area in zip(
        child_ids, parent_ids, coverage, parent_area, strict=True
    ):
        rank = (-fraction, area)
        prior = owner.get(child)
        if prior is None or rank < prior[0]:
            owner[child] = (rank, colours[parent])
        elif rank == prior[0] and colours[parent] != prior[1]:
            owner[child] = (rank, None)
    for child, (_, colour) in owner.items():
        colours[child] = colour
    return colours


def wall_material(base, colour):
    return f"{base} {colour}" if colour else base
