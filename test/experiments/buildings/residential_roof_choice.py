"""Bounded, explicitly inferred house roofs beyond the original tiny-body controls."""

import math
from dataclasses import replace

import numpy as np
import shapely
from building_layout import frame
from provider_contract import number
from recipes import choose as facade_recipe


def extend(row, parameters, previous):
    """Retain supplied/legacy choices; infer only additional small residential roofs."""
    properties, polygon = row["properties"], row["polygon"]
    if previous.kind != "flat" or any(
        key in properties for key in ("roof:shape", "roof_shape")
    ):
        return previous
    if (
        row["clipped"]
        or parameters["hide3d"]
        or parameters["minHeight"] != 0.0
        or not 70 <= polygon.area <= 350
        or not 3.5 <= parameters["bodyHeight"] <= 15
        or sum(len(r.coords) - 1 for r in (polygon.exterior, *polygon.interiors)) > 32
    ):
        return previous
    recipe = facade_recipe(
        properties,
        row.get("context", {}),
        polygon,
        parameters["buildingHeight"],
        parameters["minHeight"],
    )
    if recipe["family"] != "residential":
        return previous
    corners = np.asarray(polygon.minimum_rotated_rectangle.exterior.coords)[:4]
    lengths = np.linalg.norm(np.roll(corners, -1, axis=0) - corners, axis=1)
    width, length = float(lengths.min()), float(lengths.max())
    if width < 5.0 or length / width > 5.0:
        return previous
    try:
        levels = next(
            (
                number(properties[key])
                for key in ("building:levels", "levels")
                if key in properties
            ),
            1.0,
        )
    except ValueError:
        return previous
    if levels < 1 or not levels.is_integer():
        return previous
    rise = min(3.2, width * 0.30, parameters["bodyHeight"] - levels * 2.64)
    if rise < 0.5:
        return previous
    transform, _ = frame(polygon)
    axis = np.asarray(transform[:2])
    ring = np.asarray(shapely.orient_polygons(polygon).exterior.coords)
    edges = np.diff(ring, axis=0)
    edge_index = int(
        np.argmax(abs((edges / np.linalg.norm(edges, axis=1, keepdims=True)) @ axis))
    )
    return replace(
        previous,
        kind="ridge" if length / width >= 1.4 else "shed",
        rise_m=rise,
        eave_m=parameters["buildingHeight"] - rise,
        ridge_degrees=math.degrees(math.atan2(axis[1], axis[0])),
        edge_index=edge_index,
        origin="procedural residential size/height estimate",
        reason="Small residential source or site context; source roof type unproven, roof reserved before room planning",
    )
