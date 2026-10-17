"""Bounded original-knot-preserving corridor replacement around one actual pier."""

import hashlib

import numpy as np
import shapely

MODEL = "preserve-knots-pier-detour/1"
GUARD_M = 4.0
APPROACH_M = 10.0
MAX_OFFSET_M = 3.0
MAX_EXTRA_M = 1.0
CLEARANCE_M = 0.2
RESERVE_M = 0.1


def insert(original, segment, centre, offset):
    a, b = original[segment : segment + 2]
    length = float(np.linalg.norm(b - a))
    tangent = (b - a) / length
    normal = np.array([-tangent[1], tangent[0]])
    if (
        not np.isfinite([centre, offset]).all()
        or not GUARD_M < centre - APPROACH_M < centre + APPROACH_M < length - GUARD_M
        or not 0 < abs(offset) <= MAX_OFFSET_M
    ):
        raise ValueError(
            "bounded interior pier detour with straight endpoint runs required"
        )
    points = np.array(
        [
            a + GUARD_M * tangent,
            a + (centre - APPROACH_M) * tangent + offset * normal,
            a + (centre + APPROACH_M) * tangent + offset * normal,
            b - GUARD_M * tangent,
        ]
    )
    extra = float(
        np.linalg.norm(np.diff(np.vstack([a, points, b]), axis=0), axis=1).sum()
        - length
    )
    if extra > MAX_EXTRA_M:
        raise ValueError("local pier detour extra-length budget exceeded")
    return np.vstack([original[: segment + 1], points, original[segment + 1 :]]), extra


def checked_axis(row):
    original = np.asarray(row["xy"], float)
    planned = np.asarray(row.get("surface_axis_xy_m"), float)
    note = row["source_axis_detour"]
    segment = note["segment"]
    if (
        note.get("model") != MODEL
        or type(segment) is not int
        or original.ndim != 2
        or original.shape[1] != 2
        or not 4 <= len(original) <= 252
        or not np.isfinite(original).all()
        or not 1 <= segment < len(original) - 2
        or note["original_axis_sha256"]
        != hashlib.sha256(original.tobytes()).hexdigest()
    ):
        raise ValueError(
            "bound original source axis and known local detour model required"
        )
    expected, _extra = insert(
        original, segment, note["centre_station_m"], note["offset_m"]
    )
    if not np.array_equal(planned, expected):
        raise ValueError(
            "planned detour changed original knots or straight endpoint runs"
        )
    return planned


def proposal(row, declaration):
    if set(declaration) != {
        "segment",
        "pier_index",
        "pier_xyz_sha256",
        "pier_footprint_wkb",
    }:
        raise ValueError("explicit actual-pier detour binding required")
    segment, index = declaration["segment"], declaration["pier_index"]
    original = np.asarray(row["xy"], float)
    if (
        original.ndim != 2
        or original.shape[1] != 2
        or not np.isfinite(original).all()
        or np.any(np.linalg.norm(np.diff(original, axis=0), axis=1) < 1e-6)
        or type(segment) is not int
        or not 1 <= segment < len(original) - 2
        or not 4 <= len(original) <= 252
        or type(index) is not int
        or not 0 <= index < 128
        or not isinstance(declaration["pier_xyz_sha256"], str)
        or len(declaration["pier_xyz_sha256"]) != 64
        or not isinstance(declaration["pier_footprint_wkb"], str)
        or len(declaration["pier_footprint_wkb"]) > 16384
        or not 0 < row["width_m"] <= 4
    ):
        raise ValueError(
            "bounded original segment, pier and walking/cycling width required"
        )
    obstacle = shapely.from_wkb(declaration["pier_footprint_wkb"])
    if (
        obstacle.geom_type != "Polygon"
        or obstacle.is_empty
        or not obstacle.is_valid
        or len(shapely.get_coordinates(obstacle)) > 64
        or not np.isfinite(shapely.get_coordinates(obstacle)).all()
    ):
        raise ValueError("bounded valid whole pier footprint required")
    a, b = original[segment : segment + 2]
    tangent = (b - a) / np.linalg.norm(b - a)
    normal = np.array([-tangent[1], tangent[0]])
    positions = (shapely.get_coordinates(obstacle) - a) @ np.c_[tangent, normal]
    centre = float((positions[:, 0].min() + positions[:, 0].max()) / 2)
    half = row["width_m"] / 2
    low = float(positions[:, 1].min() - half - CLEARANCE_M - RESERVE_M)
    high = float(positions[:, 1].max() + half + CLEARANCE_M + RESERVE_M)
    offset = min((low, high), key=abs)
    planned, extra = insert(original, segment, centre, offset)
    old = shapely.LineString(original).buffer(
        half, cap_style="flat", join_style="mitre", mitre_limit=2
    )
    current = shapely.LineString(planned).buffer(
        half, cap_style="flat", join_style="mitre", mitre_limit=2
    )
    if (
        old.distance(obstacle) >= CLEARANCE_M
        or current.distance(obstacle) < CLEARANCE_M
    ):
        raise ValueError("actual blocked corridor and whole clear detour required")
    note = {
        **declaration,
        "model": MODEL,
        "original_axis_sha256": hashlib.sha256(original.tobytes()).hexdigest(),
        "centre_station_m": centre,
        "offset_m": offset,
        "extra_length_m": extra,
        "added_knots": 4,
        "original_area_removed_m2": float(old.difference(current).area),
        "planned_area_added_m2": float(current.difference(old).area),
        "original_pier_distance_m": float(old.distance(obstacle)),
        "planned_pier_distance_m": float(current.distance(obstacle)),
        "scope": "Physical corridor replacement; original provider axis/knots remain archived. Other traffic, whole D/Q body and terrain still require admission.",
    }
    return planned, note


def apply(rows, recipe):
    declared = recipe.get("source_axis_detours", {})
    if isinstance(declared, dict) and not declared:
        return
    if not isinstance(declared, dict) or not set(declared) <= {
        r["source_id"] for r in rows
    }:
        raise ValueError("detours may bind only selected original sources")
    if declared and (
        recipe.get("contact_mode") != "profile" or recipe.get("seed_grid_m")
    ):
        raise ValueError("pier detours require the actual attached profile family")
    for row in rows:
        if row["source_id"] not in declared:
            continue
        props = row["original"]["source"]["feature"]["properties"]
        if (
            props.get("subclass") not in ("footway", "cycleway")
            or "surface_axis_xy_m" in row
        ):
            raise ValueError("one walking/cycling source interpretation required")
        axis, note = proposal(row, declared[row["source_id"]])
        row.update(surface_axis_xy_m=axis, source_axis_detour=note)


def bind(piers, recipe):
    result = []
    for key, declared in recipe.get("source_axis_detours", {}).items():
        index = declared["pier_index"]
        if type(index) is not int or not 0 <= index < len(piers):
            raise ValueError("declared actual pier is unavailable")
        pier = piers[index]
        xyz = pier.vertices[pier.triangles]
        sha = hashlib.sha256(xyz.tobytes()).hexdigest()
        footprint = shapely.union_all(shapely.polygons(xyz[:, :, :2]))
        if sha != declared["pier_xyz_sha256"] or not footprint.equals(
            shapely.from_wkb(declared["pier_footprint_wkb"])
        ):
            raise ValueError("actual pier geometry differs from detour input")
        result.append(
            {"source_id": key, "pier_index": index, "actual_pier_xyz_sha256": sha}
        )
    return result
