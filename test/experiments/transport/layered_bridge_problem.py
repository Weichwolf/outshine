"""Conservative whole-region scalar derivative bounds across actual affine cells."""

import itertools
import time

import numpy as np
import shapely
from shared_bridge_geometry import corridor

RECIPES = {
    "motorway": {
        "long_grade": 0.06,
        "cross_grade": 0.04,
        "margin": 0.002,
        "deck_thickness_m": 0.5,
        "soil_gap_m": 4.5,
        "clearance_margin_m": 0.02,
    },
    "path": {
        "long_grade": 0.12,
        "cross_grade": 0.04,
        "margin": 0.002,
        "deck_thickness_m": 0.25,
        "soil_gap_m": 0.1,
        "clearance_margin_m": 0.01,
    },
}


def domain_of(rows):
    return shapely.union_all(
        [corridor({"xy": r["coordinates"], "width_m": r["width_m"]}) for r in rows]
    )


def signed_rate(gradient, axis, tangent, recipe):
    normal = np.array([-tangent[1], tangent[0]])
    frame = np.array(
        [
            tangent / (recipe["long_grade"] - recipe["margin"]),
            normal / (recipe["cross_grade"] - recipe["margin"]),
        ]
    )
    direction, ground = frame @ axis, frame @ gradient
    q, b, c = (
        float(direction @ direction),
        float(direction @ ground),
        float(ground @ ground) - 1,
    )
    discriminant = b * b - q * c
    if discriminant < 0:
        raise ValueError(
            "chosen scalar chart cannot satisfy a whole source cross-section"
        )
    return np.array([-b - np.sqrt(discriminant), -b + np.sqrt(discriminant)]) / q, frame


def patch_records(fixture, rows, domain, origin, axis, recipe):
    began = time.monotonic()
    xyz, faces = np.array(fixture["field_vertices"]), np.array(fixture["field_faces"])
    triangles = xyz[faces]
    cells = shapely.polygons(triangles[:, :, :2])
    if domain.difference(shapely.union_all(cells)).area > 1e-7:
        raise ValueError("whole profile outside the actual captured affine ground")
    records = []
    for row in rows:
        footprint = domain_of([row])
        xy = np.array(row["coordinates"])
        influences = []
        for a, b in itertools.pairwise(xy):
            direction = b - a
            if np.linalg.norm(direction) <= 1e-8:
                raise ValueError("zero-length candidate source segment")
            tangent = direction / np.linalg.norm(direction)
            # Miter-limit-two corner wedges are covered by these local influences.
            influence = footprint.intersection(
                shapely.LineString([a, b]).buffer(row["width_m"])
            )
            influences.append(influence)
            for i, (cell, tri) in enumerate(zip(cells, triangles, strict=True)):
                if time.monotonic() - began > 4 or len(records) >= 2048:
                    raise ValueError("layered profile derivative patch budget")
                region = cell.intersection(influence)
                if region.area <= 1e-10:
                    continue
                gradient = np.linalg.solve(
                    tri[1:, :2] - tri[0, :2], tri[1:, 2] - tri[0, 2]
                )
                rate, frame = signed_rate(gradient, axis, tangent, recipe)
                stations = (shapely.get_coordinates(region) - origin) @ axis
                records.append(
                    {
                        "source_id": row["source_id"],
                        "field_face": fixture["original_field_face_ids"][i],
                        "range_m": [float(stations.min()), float(stations.max())],
                        "rate": rate.tolist(),
                        "frame": frame.tolist(),
                        "region_wkb_hex": region.wkb_hex,
                    }
                )
        if footprint.difference(shapely.union_all(influences)).area > 1e-7:
            raise ValueError("source frame influences leave an unowned corner")
    return records


def band(row):
    return shapely.LineString(row["coordinates"]).buffer(
        row["width_m"] / 2, quad_segs=2, cap_style="round", join_style="bevel"
    )


def bounds_records(fixture, rows, domain, origin, axis, recipe, profile):
    records = []
    decks = domain_of([r for r in rows if r["role"] == "deck"])

    def add(shape, kind, value, source):
        stations = (shapely.get_coordinates(shape) - origin) @ axis
        records.append(
            {
                "source_id": source,
                "kind": kind,
                "value_m": value,
                "range_m": [float(stations.min()), float(stations.max())],
            }
        )

    for row in rows:
        if row["role"] == "deck":
            add(
                domain_of([row]),
                "minimum",
                recipe["deck_thickness_m"]
                + recipe["soil_gap_m"]
                + recipe["clearance_margin_m"],
                row["source_id"],
            )
    if profile == "motorway":
        actual = shapely.from_wkb(bytes.fromhex(fixture["actual_traffic"]["wkb_hex"]))
        for part in shapely.get_parts(actual):
            add(
                part,
                "minimum",
                recipe["deck_thickness_m"] + 4.5 + recipe["clearance_margin_m"],
                "actual:old-ground-traffic",
            )
        raised = shapely.from_wkb(bytes.fromhex(fixture["actual_borders"]["wkb_hex"]))
        for part in shapely.get_parts(raised):
            if part.area > 1e-8:
                add(
                    part,
                    "minimum",
                    recipe["deck_thickness_m"]
                    + 4.5
                    + fixture["actual_borders"]["maximum_height_m"]
                    + recipe["clearance_margin_m"],
                    "actual:retained-raised-traffic",
                )
    for row in fixture["context"]:
        p = row["properties"]
        if (
            row["old_decision"]["status"] != "used"
            or p.get("brunnel")
            or p.get("layer", 0)
        ):
            continue
        overlap = band(row).intersection(domain)
        if overlap.area <= 1e-8:
            continue
        if profile == "motorway" and p.get("class") != "motorway":
            continue
        if profile == "path" and band(row).intersection(decks).area > 1e-8:
            add(
                overlap,
                "minimum",
                recipe["deck_thickness_m"] + 4.5 + recipe["clearance_margin_m"],
                row["source_id"],
            )
            continue
        add(overlap, "maximum", 0.0, row["source_id"])
    return records


def prepare(fixture, profile, extra=()):
    recipe = RECIPES[profile]
    rows = [r for r in fixture["sources"] if r["profile"] == profile]
    if not 1 <= len(rows) <= 64:
        raise ValueError("bounded actual profile sources required")
    root = next(r for r in fixture["sources"] if r["profile"] == "motorway")
    xy = np.array(root["coordinates"])
    origin, axis = xy[0], xy[-1] - xy[0]
    axis /= np.linalg.norm(axis)
    domain = domain_of(rows)
    patches = patch_records(fixture, rows, domain, origin, axis, recipe)
    bounds = bounds_records(fixture, rows, domain, origin, axis, recipe, profile) + [
        dict(r, range_m=list(r["range_m"])) for r in extra
    ]
    # Outward centimetre intervals retain every original whole-region condition.
    for row in patches + bounds:
        a, b = row["range_m"]
        row["actual_range_m"] = [a, b]
        row["range_m"] = [float(np.floor(a * 100) / 100), float(np.ceil(b * 100) / 100)]
    extent = (shapely.get_coordinates(domain) - origin) @ axis
    stations = np.unique(
        [
            np.floor(extent.min() * 100) / 100,
            np.ceil(extent.max() * 100) / 100,
            *[x for r in patches + bounds for x in r["range_m"]],
        ]
    )
    lower, upper = np.zeros(len(stations)), np.full(len(stations), np.inf)
    rate = np.tile(
        [-recipe["long_grade"], recipe["long_grade"]], (len(stations) - 1, 1)
    )
    for row in patches:
        lo, hi = row["range_m"]
        active = (stations[:-1] < hi) & (stations[1:] > lo)
        rate[active, 0] = np.maximum(rate[active, 0], row["rate"][0])
        rate[active, 1] = np.minimum(rate[active, 1], row["rate"][1])
    for row in bounds:
        lo, hi = row["range_m"]
        active = (stations >= lo) & (stations <= hi)
        if row["kind"] == "maximum":
            upper[active] = np.minimum(upper[active], row["value_m"])
        else:
            lower[active] = np.maximum(lower[active], row["value_m"])
    return {
        "stations": stations,
        "lower": lower,
        "upper": upper,
        "rate": rate,
        "domain": domain,
        "origin": origin,
        "axis": axis,
        "patches": patches,
        "bounds": bounds,
        "recipe": dict(recipe),
        "profile": profile,
    }
