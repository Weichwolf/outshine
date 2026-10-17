"""Bounded source-cell industrial relief, with unchanged openings and roof envelope."""

import hashlib
from dataclasses import dataclass

import numpy as np
import shapely


@dataclass(frozen=True, slots=True)
class Relief:
    id: str
    kind: str
    region: object
    keepouts: object
    origin: tuple
    tangent: tuple
    depth_m: float
    triangles: int


def openings(plan, facade):
    tangent = (np.asarray(facade["b"]) - facade["a"]) / facade["width_m"]
    gates = [dict(plan["gate"], margin_m=0.38)]
    door = plan["entry"]["door"]
    doors = [
        dict(
            door,
            facade_id=plan["entry"]["frame"]["id"],
            width_m=1.1,
            height_m=2.15,
            bottom_m=0.16,
            margin_m=0.20,
        )
    ]
    windows = [dict(w, margin_m=0.18) for w in plan["windows"]]
    regions = []
    for opening in [*gates, *doors, *windows]:
        if opening["facade_id"] != facade["id"]:
            continue
        centre = float((np.asarray(opening["centre"]) - facade["a"]) @ tangent)
        margin = opening["margin_m"]
        width = opening["width_m"] / 2 + margin
        regions.append(
            shapely.box(
                centre - width,
                opening["bottom_m"] - margin,
                centre + width,
                opening["bottom_m"] + opening["height_m"] + margin,
            )
        )
    return shapely.union_all(regions)


def placement(plan, facade, variant):
    width, height, count = (
        facade["width_m"],
        plan["ceiling_m"] + 0.10,
        facade["columns"],
    )
    if not 2 <= width <= 200 or not 1.8 <= height <= 18 or not 1 <= count <= 32:
        raise ValueError("Relief needs a bounded existing hall facade")
    cadence, pier, depth, kind = (
        (1, 0.30, 0.20, "portal-bays"),
        (2, 0.38, 0.28, "paired-bays"),
        (3, 0.24, 0.14, "ribbon-bays"),
    )[variant]
    cell = width / count
    border = 0.04
    centres = (
        border + pier / 2,
        *(c * cell for c in range(cadence, count, cadence)),
        width - border - pier / 2,
    )
    rectangles = [shapely.box(c - pier / 2, 0, c + pier / 2, height) for c in centres]
    rectangles.extend(
        (
            shapely.box(border, 0, width - border, min(0.55, height * 0.14)),
            shapely.box(
                border, height - min(0.48, height * 0.12), width - border, height
            ),
        )
    )
    if kind == "ribbon-bays":
        sill = (
            min(
                (
                    w["bottom_m"]
                    for w in plan["windows"]
                    if w["facade_id"] == facade["id"]
                ),
                default=height * 0.45,
            )
            - 0.20
        )
        if sill > 0.80:
            rectangles.append(shapely.box(border, sill - 0.18, width - border, sill))
    keepouts = shapely.set_precision(openings(plan, facade), 1e-8)
    region = shapely.difference(
        shapely.union_all(rectangles, grid_size=1e-8), keepouts, grid_size=1e-8
    ).simplify(0)
    pieces = list(shapely.get_parts(region))
    if not pieces or any(
        p.geom_type != "Polygon" or p.is_empty or not p.is_valid for p in pieces
    ):
        raise ValueError("Relief has no valid positive-area polygon domain")
    triangles = sum(
        4 * (len(shapely.get_coordinates(p)) - 1 - len(p.interiors))
        + 4 * len(p.interiors)
        - 4
        for p in pieces
    )
    tangent = (np.asarray(facade["b"]) - facade["a"]) / width
    return Relief(
        facade["id"] + ":industrial-relief",
        kind,
        region,
        keepouts,
        tuple(facade["a"]),
        tuple(tangent),
        depth,
        triangles,
    )


def footprint(plan):
    a, t = np.asarray(plan.origin), np.asarray(plan.tangent)
    normal = np.array([t[1], -t[0]])
    low, _, high, _ = plan.region.bounds
    start, end = a + t * low, a + t * high
    return shapely.Polygon(
        [start, end, end + normal * plan.depth_m, start + normal * plan.depth_m]
    )


def choose(asset, row, clear, *, max_triangles=4096):
    if type(max_triangles) is not int or max_triangles < 0:
        raise ValueError("Relief capacity requires a nonnegative integer")
    facades = asset.run.outputs["programme"].value.facades
    if len(facades) > 32:
        raise ValueError("Relief facade capacity exceeded before region construction")
    variant = int(hashlib.sha256(asset.plan["id"].encode()).hexdigest()[:8], 16) % 3
    result, rejected, remaining = [], [], max_triangles
    for facade in facades:
        try:
            current = placement(asset.plan, facade, variant)
            if current.triangles > remaining:
                raise ValueError(
                    "Relief triangle capacity exceeded before mesh construction"
                )
            support = footprint(current)
            low, high = current.region.bounds[1], current.region.bounds[3]
            if support.intersection(row["polygon"]).area > 1e-8:
                raise ValueError("Relief intersects another wing of its source")
            if not clear(row, support, high, low_m=low):
                raise ValueError(
                    "Relief intersects a known height-overlapping neighbor"
                )
            result.append(current)
            remaining -= current.triangles
        except ValueError as error:
            rejected.append({"id": facade["id"], "reason": str(error)})
    return tuple(result), tuple(rejected)
