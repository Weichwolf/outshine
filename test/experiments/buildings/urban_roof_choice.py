"""Bounded urban hip alternatives, inferred without location or building-name recipes."""

from dataclasses import replace

import numpy as np
from provider_contract import number
from recipes import choose as facade_recipe


def extend(row, parameters, previous):
    properties, polygon = row["properties"], row["polygon"]
    if previous.kind != "flat" or any(
        key in properties for key in ("roof:shape", "roof_shape")
    ):
        return previous
    height, area = parameters["bodyHeight"], polygon.area
    if (
        row["clipped"]
        or parameters["hide3d"]
        or parameters["minHeight"] != 0.0
        or not 150 <= area <= 2500
        or not 8 <= height <= 36
        or (area <= 350 and height <= 15)
        or sum(len(r.coords) - 1 for r in (polygon.exterior, *polygon.interiors)) > 80
        or 4 * np.pi * area / polygon.exterior.length**2 > 0.85
    ):
        return previous
    recipe = facade_recipe(
        properties,
        row.get("context", {}),
        polygon,
        parameters["buildingHeight"],
        parameters["minHeight"],
    )
    if recipe["family"] not in ("residential", "mixed-use", "unknown"):
        return previous
    corners = np.asarray(polygon.minimum_rotated_rectangle.exterior.coords)
    lengths = np.linalg.norm(np.diff(corners, axis=0), axis=1)
    width, length = float(lengths.min()), float(lengths.max())
    if width < 8 or length / width > 6:
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
    rise = min(3.5, width * 0.25, height * 0.22, height - levels * 2.64)
    if rise < 0.5:
        return previous
    return replace(
        previous,
        kind="hip",
        rise_m=rise,
        eave_m=parameters["buildingHeight"] - rise,
        origin="procedural urban size/height estimate",
        reason="Ordinary mid-rise source envelope; hipped form is inferred, courtyard and delivered top retained; no measured roof type or architecture-family claim",
    )
