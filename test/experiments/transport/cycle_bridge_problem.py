"""Whole-footprint contacts and mode-specific underpasses on one actual affine cell."""

import json
from pathlib import Path

import numpy as np
import shapely
from shared_bridge_geometry import corridor
from transport_travel import MODES, source_travel


def band(row):
    return shapely.LineString(row["coordinates"]).buffer(
        row["width_m"] / 2, quad_segs=2, cap_style="round", join_style="bevel"
    )


def plane(fixture, domain):
    xyz = np.asarray(fixture["field_vertices"], float)
    faces = np.asarray(fixture["field_faces"], int)
    if xyz.shape != (3, 3) or faces.shape != (1, 3) or not np.isfinite(xyz).all():
        raise ValueError(
            "this profile experiment requires one actual affine ground cell"
        )
    triangle = xyz[faces[0]]
    if not shapely.Polygon(triangle[:, :2]).buffer(1e-8).covers(domain):
        raise ValueError("actual affine ground cell does not cover the whole footprint")
    gradient = np.linalg.solve(
        triangle[1:, :2] - triangle[0, :2], triangle[1:, 2] - triangle[0, 2]
    )
    return triangle[0].copy(), gradient


def intervals(fixture, selected, origin, axis):
    parts = [
        corridor({"xy": r["coordinates"], "width_m": r["width_m"]}) for r in selected
    ]
    domain = shapely.union_all(parts)
    project = lambda shape: (shapely.get_coordinates(shape) - origin) @ axis
    extent = project(domain)
    records = [
        {
            "kind": "ground_joint",
            "range_m": [float(extent.min()), float(extent.min() + 1)],
            "source_id": "recipe:outer-port-start",
        },
        {
            "kind": "ground_joint",
            "range_m": [float(extent.max() - 1), float(extent.max())],
            "source_id": "recipe:outer-port-end",
        },
        {
            "kind": "deck",
            "range_m": [float(project(parts[0]).min()), float(project(parts[0]).max())],
            "source_id": selected[0]["source_id"],
        },
    ]
    for row in fixture["sources"]:
        if row["role"] in ("bridge", "approach"):
            continue
        overlap = band(row).intersection(domain)
        if overlap.area <= 1e-9:
            raise ValueError(
                "captured neighbour no longer overlaps the proposed footprint"
            )
        bridge = band(row).intersection(parts[0])
        expected = "underpass" if bridge.area > 1e-9 else "ground_joint"
        if expected != row["role"] or row["properties"].get("layer", 0):
            raise ValueError(
                "source topology/level disagrees with the proposed contact role"
            )
        if expected == "underpass":
            actual = shapely.from_wkb(
                bytes.fromhex(row["actual_ground_traffic_wkb_hex"])
            )
            if not actual.is_valid or actual.difference(parts[0]).area > 1e-8:
                raise ValueError(
                    "actual captured underpass footprint outside the bridge"
                )
            overlap = shapely.union_all([overlap, actual])
        values = project(overlap)
        records.append(
            {
                "kind": expected,
                "range_m": [float(values.min()), float(values.max())],
                "source_id": row["source_id"],
            }
        )
    return domain, records


def problem(fixture):
    rows = fixture["sources"]
    if not 3 <= len(rows) <= 64 or len({r["source_id"] for r in rows}) != len(rows):
        raise ValueError("bounded unique bridge source set required")
    for row in rows:
        xy = np.asarray(row["coordinates"], float)
        p = row["properties"]
        if (
            not 2 <= len(xy) <= 256
            or xy.shape != (len(xy), 2)
            or not np.isfinite(xy).all()
            or not 0 < row["width_m"] <= 8
            or p.get("class") != "path"
            or p.get("indoor")
            or p.get("level", 0)
        ):
            raise ValueError("bounded explicit outdoor nonmotorized source required")
    selected = [r for r in rows if r["role"] in ("bridge", "approach")]
    if len(selected) != 3 or selected[0]["role"] != "bridge":
        raise ValueError("one bridge and its two chosen approaches required")
    for row in selected:
        p = row["properties"]
        if (
            p.get("subclass") != "cycleway"
            or (
                row["role"] == "bridge"
                and (p.get("brunnel") != "bridge" or p.get("layer") != 1)
            )
            or (row["role"] == "approach" and (p.get("brunnel") or p.get("layer", 0)))
        ):
            raise ValueError(
                "explicit cycle bridge and chosen ground approaches required"
            )
    recipes = json.loads(
        Path(__file__).with_name("transport_network_recipes.json").read_text()
    )
    travel, restricted, access_notes = source_travel(selected, recipes["travelRecipes"])
    if not np.all(np.any(travel & MODES["cycle"], axis=1)):
        raise ValueError(
            "source access does not permit the proposed public cycle route"
        )
    xy = np.asarray(selected[0]["coordinates"], float)
    if len(xy) != 2:
        raise ValueError(
            "one straight bridge axis required for this profile experiment"
        )
    origin, axis = xy[0], xy[-1] - xy[0]
    axis /= np.linalg.norm(axis)
    domain, records = intervals(fixture, selected, origin, axis)
    reference, gradient = plane(fixture, domain)
    recipe = fixture["recipe"]
    grade, thickness, headroom = [
        float(recipe[k])
        for k in ("maximum_grade", "deck_thickness_m", "nonmotorized_headroom_m")
    ]
    if not 0 < grade <= 0.2 or not 0 < thickness <= 1 or not 2 <= headroom <= 5:
        raise ValueError("finite bounded explicit bridge recipe required")
    grade_margin, clearance_margin = [
        float(recipe[k]) for k in ("construction_grade_margin", "clearance_margin_m")
    ]
    if not 0 <= grade_margin < grade or not 0 <= clearance_margin <= 0.1:
        raise ValueError("bounded explicit construction margins required")
    fit_grade = grade - grade_margin
    parallel = float(gradient @ axis)
    perpendicular = gradient - parallel * axis
    remaining = fit_grade * fit_grade - float(perpendicular @ perpendicular)
    if remaining < 0:
        raise ValueError(
            "actual ground crossfall already exceeds the chosen whole-face grade"
        )
    rate = np.array([-parallel - np.sqrt(remaining), -parallel + np.sqrt(remaining)])
    stations = np.unique([v for r in records for v in r["range_m"]])
    if not 2 <= len(stations) <= 256 or np.min(np.diff(stations)) <= 1e-10:
        raise ValueError("bounded separated critical stations required")
    lower, upper = np.zeros(len(stations)), np.full(len(stations), np.inf)
    for record in records:
        lo, hi = record["range_m"]
        chosen = (stations >= lo) & (stations <= hi)
        if record["kind"] == "ground_joint":
            upper[chosen] = 0
        else:
            minimum = thickness + (
                headroom + clearance_margin if record["kind"] == "underpass" else 0
            )
            lower[chosen] = np.maximum(lower[chosen], minimum)
    return {
        "stations": stations,
        "lower": lower,
        "upper": upper,
        "rate": rate,
        "origin": origin,
        "axis": axis,
        "reference": reference,
        "gradient": gradient,
        "domain": domain,
        "records": records,
        "recipe": dict(recipe),
        "public_travel": travel,
        "restricted_travel": restricted,
        "access_notes": access_notes,
    }
