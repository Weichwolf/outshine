"""Two broad facade compositions on existing upper window/storey axes."""

from dataclasses import dataclass

import numpy as np
import shapely
from facade_frame_programme import Placement
from facade_frame_programme import placement as original
from opening_modules import template as window_module
from provider_contract import seed_for


@dataclass(frozen=True)
class CladPlacement(Placement):
    region: object
    protected: object

    @property
    def depth_m(self):
        return self.frame.depth_m

    def triangles(self, mode):
        if mode not in ("front", "outer", "closed"):
            raise ValueError("Clad product requires front, outer or closed")
        total = 0
        for p in shapely.get_parts(self.region):
            n = len(p.exterior.coords) - 1 + sum(len(r.coords) - 1 for r in p.interiors)
            front = n + 2 * len(p.interiors) - 2
            total += front if mode == "front" else front + 2 * n
            if mode == "closed":
                total += front
        return total


def protected(facade, current, bottom):
    cell = facade["width_m"] / facade["columns"]
    floor = facade["height_m"] / facade["rows"]
    width, height = np.array(facade["fractions"]) * [cell, floor]
    raw = window_module(width=width, height=height)
    points = raw["vertices"][:, [0, 2]]
    lo, hi = points.min(axis=0) - 0.04, points.max(axis=0) + 0.04
    margin = np.linalg.norm(np.asarray(current.origin_xy) - facade["a"])
    return shapely.union_all(
        [
            shapely.box(*(origin + lo), *(origin + hi))
            for c in range(facade["columns"])
            for r in range(1, facade["rows"])
            for origin in (
                np.array(
                    [
                        (c + 0.5) * cell - margin,
                        bottom + (r + 0.5) * floor - height / 2 - current.base_m,
                    ]
                ),
            )
        ]
    )


def placement(facade, bottom, family, kind):
    if kind not in ("broad-piers", "broad-spandrels"):
        raise ValueError("Unknown clad facade programme")
    base = original(facade, bottom, family)
    cell = facade["width_m"] / facade["columns"]
    floor = facade["height_m"] / facade["rows"]
    gap_u, gap_v = np.array([cell, floor]) * (1 - np.array(facade["fractions"]))
    pier = min(1.0, gap_u * 0.78) if kind == "broad-piers" else 0.12
    beam = min(0.9, gap_v * 0.78) if kind == "broad-spandrels" else 0.14
    if min(pier, beam) < 0.10:
        raise ValueError("Existing apertures leave no architectural band width")
    w, h = base.frame.width_m, base.frame.height_m
    vertical = base.frame.vertical
    horizontal = (0.0, h)
    if kind == "broad-spandrels":
        horizontal = (
            0.0,
            *(bottom + r * floor - base.base_m for r in range(2, facade["rows"])),
            h,
        )
    bars = [shapely.box(c - pier / 2, 0, c + pier / 2, h) for c in vertical]
    bars.extend(shapely.box(0, c - beam / 2, w, c + beam / 2) for c in horizontal)
    reserved = protected(facade, base, bottom)
    region = shapely.union_all(bars).intersection(shapely.box(0, 0, w, h))
    region = shapely.orient_polygons(region.difference(reserved))
    if (
        region.is_empty
        or not region.is_valid
        or region.geom_type not in ("Polygon", "MultiPolygon")
        or region.area < 0.025 * w * h
        or len(shapely.get_coordinates(region)) > 2048
    ):
        raise ValueError("Clad region is empty, invalid, negligible or over capacity")
    result = CladPlacement(
        base.facade_id,
        kind,
        base.frame,
        base.origin_xy,
        base.tangent,
        base.base_m,
        region,
        reserved,
    )
    if result.triangles("closed") > 4096:
        raise ValueError("Clad closed reference exceeds 4096 triangles")
    return result


def choose(row, parameters, facades, family, entry):
    if (
        family not in ("residential", "mixed-use", "commercial", "institutional")
        or row.get("clipped")
        or parameters["minHeight"] != 0
        or entry is None
    ):
        return (), []
    kind = ("broad-piers", "broad-spandrels")[seed_for(row["id"] + "/cladding") % 2]
    candidates = sorted(
        (f for f in facades if f["id"] != entry["frame"]["id"]),
        key=lambda f: (-f["width_m"], f["id"]),
    )
    accepted, rejected, remaining = [], [], 4096
    for facade in candidates[:4]:
        try:
            current = placement(facade, parameters["minHeight"], family, kind)
            count = current.triangles("outer")
            if count > remaining:
                raise ValueError("Clad whole-source outer capacity exceeded")
            remaining -= count
            accepted.append(current)
        except ValueError as error:
            rejected.append(
                {"id": row["id"], "facade_id": facade["id"], "reason": str(error)}
            )
    return tuple(accepted), rejected
