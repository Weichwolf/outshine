"""Choose source-owned free elevations before any camera/detail selection."""

import numpy as np
import shapely
from elevation_plan import plan
from provider_contract import number, seed_for


def choose(row, parameters, facades, family, entry, entrances):
    if (
        entry is None
        or row.get("clipped")
        or parameters["minHeight"] != 0
        or family not in ("residential", "mixed-use", "commercial", "institutional")
        or not 800 < row["polygon"].area <= 6000
        or len(shapely.get_coordinates(row["polygon"])) > 80
        or any(
            k.startswith("window") and v is not None
            for k, v in row["properties"].items()
        )
    ):
        return (), "outside ordinary elevation programme scope"
    levels = next(
        (
            number(row["properties"][k])
            for k in ("building:levels", "building_levels", "levels")
            if row["properties"].get(k) is not None
        ),
        None,
    )
    if levels is not None and any(f["rows"] != levels for f in facades):
        return (), "supplied level count differs from the existing source grid"
    seed = seed_for(row["id"] + "/elevation-programme")
    ribbon = family in ("commercial", "institutional") or (
        family == "mixed-use" and seed % 2 == 0
    )
    kind = "horizontal-ribbons" if ribbon else "vertical-punched"
    result = []
    for f in facades:
        if f["id"] == entry["frame"]["id"] or f["width_m"] < 12:
            continue
        try:
            candidate = plan(f, parameters["minHeight"], kind, seed)
        except ValueError:
            continue
        a, b = np.asarray(f["a"]), np.asarray(f["b"])
        tangent = (b - a) / f["width_m"]
        out = np.array([tangent[1], -tangent[0]]) * 0.25
        support = shapely.Polygon([a, b, b + out, a + out])
        if support.intersection(row["polygon"]).area > 1e-8:
            continue
        if entrances.clear(
            row,
            support,
            parameters["minHeight"] + f["height_m"],
            low_m=parameters["minHeight"],
        ):
            result.append(candidate)
    if not result:
        return (), "no free wide secondary elevation with safe storey scopes"
    selected = {p.facade_id for p in result}
    count = sum(f["opening_count"] for f in facades if f["id"] not in selected)
    count += sum(f["opening_count"] for p in result for f in p.frames())
    if count > 4096:
        return (), "whole-source logical window count exceeds4096"
    return tuple(result), "source-stable base/body/top elevation programme"
