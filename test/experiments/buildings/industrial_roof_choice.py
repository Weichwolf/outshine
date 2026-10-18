"""Inferred repeated hall roofs inside ordinary industrial source height budgets."""

import math
from dataclasses import asdict, replace

import numpy as np
from building_layout import frame
from outshine_ref.buildings.section_roofs import RoofSection
from outshine_ref.buildings.structures import classify_structure
from provider_contract import number
from recipes import choose as facade_recipe

KINDS = ("folded", "sawtooth")


def extent(polygon):
    forward, _ = frame(polygon)
    axis = np.asarray(forward[2:4])
    origin = np.asarray(polygon.centroid.coords[0])
    run = (np.asarray(polygon.exterior.coords) - origin) @ axis
    width = float(np.ptp(run))
    return axis, origin + axis * run.min(), width


def extend(row, parameters, previous):
    props, polygon = row["properties"], row["polygon"]
    if previous.kind != "flat" or any(k.startswith(("roof:", "roof_")) for k in props):
        return previous
    count = sum(len(r.coords) - 1 for r in (polygon.exterior, *polygon.interiors))
    height = parameters["bodyHeight"]
    if (
        row["clipped"]
        or parameters["hide3d"]
        or parameters["minHeight"] != 0
        or not 200 <= polygon.area <= 15000
        or not 4.5 <= height <= 18
        or count > 80
        or 4 * np.pi * polygon.area / polygon.exterior.length**2 > 0.9
    ):
        return previous
    recipe = facade_recipe(props, row.get("context", {}), polygon, height, 0)
    if recipe["family"] != "industrial":
        return previous
    if (
        classify_structure(
            polygon,
            props,
            industrial_coverage=1.0,
            coordinate_step_m=row.get("coordinate_step_m", 0.001),
            clipped=False,
        ).kind
        != "ordinary"
    ):
        return previous
    rectangle = polygon.minimum_rotated_rectangle
    sides = np.linalg.norm(
        np.diff(np.asarray(rectangle.exterior.coords), axis=0), axis=1
    )
    width, length = float(sides.min()), float(sides.max())
    if (
        not 10 <= width <= 80
        or length / width > 6
        or polygon.area / rectangle.area < 0.7
    ):
        return previous
    try:
        levels = number(props.get("building:levels", props.get("levels", 1)))
    except ValueError:
        return previous
    if levels < 1 or not levels.is_integer():
        return previous
    bays = min(8, max(2, math.floor(width / 7 + 0.5)))
    rise = min(
        2.4, width / bays * 0.24, height * 0.28, height - max(3.15, levels * 2.64)
    )
    if rise < 0.5:
        return previous
    return replace(
        previous,
        kind="folded" if bays <= 3 else "sawtooth",
        rise_m=rise,
        eave_m=parameters["buildingHeight"] - rise,
        origin="procedural industrial size/context estimate",
        reason="Repeated closed hall roof; source roof type unproven, no glazed clerestory or industrial room programme",
    )


def section_for(row, choice):
    if choice.kind not in KINDS:
        raise ValueError("No repeated industrial section for this roof kind")
    axis, origin, width = extent(row["polygon"])
    bays = min(8, max(2, math.floor(width / 7 + 0.5)))
    peak = 0.5 if choice.kind == "folded" else 0.8
    knots = [(0.0, choice.eave_m)]
    for i in range(bays):
        knots.extend(
            (
                ((i + peak) * width / bays, choice.eave_m + choice.rise_m),
                ((i + 1) * width / bays, choice.eave_m),
            )
        )
    return RoofSection(tuple(knots), tuple(axis), tuple(origin))


def metadata(row, choice):
    section = section_for(row, choice)
    return asdict(section) | {
        "bays": (len(section.knots) - 1) // 2,
        "origin": choice.origin,
        "status": "logical section; closed unglazed roof, no measured source roof",
    }
