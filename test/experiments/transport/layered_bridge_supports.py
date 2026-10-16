"""Compare fixed and bounded shifted supports against the same whole lower spaces."""

import itertools
import time

import numpy as np
import shapely
from layered_bridge_problem import band, domain_of
from outshine_ref.contracts import Mesh
from outshine_ref.transport.supports import SupportVolumeBuilder


def excluded(fixture, profile, lower=None):
    shapes = [
        band(r).buffer(2.251)
        for r in fixture["context"]
        if r["old_decision"]["status"] == "used"
        and not r["properties"].get("brunnel")
        and not r["properties"].get("layer", 0)
    ]
    if profile == "motorway":
        decks = domain_of(
            [
                r
                for r in fixture["sources"]
                if r["profile"] == "motorway" and r["role"] == "deck"
            ]
        )
        shapes = [shape.difference(decks) for shape in shapes]
        shapes.append(
            shapely.from_wkb(bytes.fromhex(fixture["actual_traffic"]["wkb_hex"]))
        )
    shapes.append(shapely.from_wkb(bytes.fromhex(fixture["actual_borders"]["wkb_hex"])))
    if lower is not None:
        shapes.append(lower[0]["domain"].buffer(0.2))
    return shapely.union_all(shapes).buffer(0.2)


def axes(fixture, problem, chains):
    if problem["profile"] == "motorway":
        return chains
    merged = shapely.line_merge(
        shapely.union_all(
            [
                shapely.LineString(r["coordinates"])
                for r in fixture["sources"]
                if r["profile"] == "path"
            ]
        )
    )
    if merged.geom_type != "LineString":
        raise ValueError("lower path does not form one source chain")
    return {0: {"xy": shapely.get_coordinates(merged)}}


def air_intervals(line, problem, table):
    xy = np.array(line.coords)
    segment = np.linalg.norm(np.diff(xy, axis=0), axis=1)
    arc = np.r_[0, np.cumsum(segment)]
    projected = (xy - problem["origin"]) @ problem["axis"]
    depth = problem["recipe"]["deck_thickness_m"]
    points = list(arc)
    for i, (a, b) in enumerate(itertools.pairwise(projected)):
        if abs(b - a) < 1e-10:
            continue
        for s in table[:, 0]:
            t = (s - a) / (b - a)
            if 0 < t < 1:
                points.append(float(arc[i] + t * segment[i]))
    points = np.unique(points)
    coordinates = shapely.get_coordinates(shapely.line_interpolate_point(line, points))
    height = np.interp(
        (coordinates - problem["origin"]) @ problem["axis"], table[:, 0], table[:, 1]
    )
    active = (height[:-1] > depth + 1e-8) | (height[1:] > depth + 1e-8)
    merged = []
    for a, b in zip(points[:-1][active], points[1:][active], strict=True):
        if merged and abs(merged[-1][1] - a) < 1e-8:
            merged[-1] = (merged[-1][0], float(b))
        else:
            merged.append((float(a), float(b)))
    return merged


def layout(fixture, candidate, chains, *, lower=None, method="shifted"):
    if method not in ("fixed", "shifted"):
        raise ValueError("known support comparison required")
    problem, _heights, ribbon, note = candidate
    profile = problem["profile"]
    field = Mesh(np.array(fixture["field_vertices"]), np.array(fixture["field_faces"]))
    builder = SupportVolumeBuilder(field, ribbon.lower)
    forbidden = excluded(fixture, profile, lower)
    spacing, max_span = (24.0, 36.0) if profile == "motorway" else (8.0, 12.0)
    shifts = (0,) if method == "fixed" else (0, -3, 3, -6, 6)
    lateral = (0,) if method == "fixed" or profile == "path" else (0, -3, 3, -4.5, 4.5)
    half = np.array([0.8, 0.6]) if profile == "motorway" else np.array([0.3, 0.2])
    products, shapes, records, spans = [], [], [], []
    began = time.monotonic()
    trials = 0
    for key, row in axes(fixture, problem, chains).items():
        line = shapely.LineString(row["xy"])
        accepted = []
        for arc in np.arange(spacing / 2, line.length, spacing):
            point = line.interpolate(arc)
            s = float((np.array(point.coords[0]) - problem["origin"]) @ problem["axis"])
            if (
                np.interp(s, problem["stations"], candidate[1])
                <= problem["recipe"]["deck_thickness_m"] + 1e-8
            ):
                continue
            record = {
                "chain": key,
                "nominal_m": float(arc),
                "product": None,
                "trials": [],
            }
            for shift in shifts:
                for side in lateral:
                    trials += 1
                    if trials > 4096 or time.monotonic() - began > 12:
                        raise ValueError("layered support time/trial budget")
                    distance = arc + shift
                    if not 0 < distance < line.length:
                        continue
                    centre = np.array(line.interpolate(distance).coords[0])
                    a = np.array(line.interpolate(max(0, distance - 0.01)).coords[0])
                    b = np.array(
                        line.interpolate(min(line.length, distance + 0.01)).coords[0]
                    )
                    tangent = (b - a) / np.linalg.norm(b - a)
                    normal = np.array([-tangent[1], tangent[0]])
                    xy = np.array([[-1, -1], [1, -1], [1, 1], [-1, 1]]) * half
                    shape = shapely.Polygon(
                        xy @ np.array([tangent, normal]) + centre + side * normal
                    )
                    reason = ""
                    if not ribbon.shape.covers(shape):
                        reason = "outside_whole_deck"
                    elif shape.intersects(forbidden):
                        reason = "lower_traffic_or_joint"
                    elif shapes and np.any(shapely.intersects(shape, shapes)):
                        reason = "other_pier"
                    else:
                        product, proof = builder.build(shape, minimum_height_m=0.1)
                        if product is None:
                            reason = proof["reason"]
                    record["trials"].append(
                        {"shift_m": shift, "side_m": side, "reason": reason}
                    )
                    if not reason:
                        record.update(
                            product=len(products),
                            arc_m=float(distance),
                            footprint_wkb_hex=shape.wkb_hex,
                            geometry=proof,
                        )
                        products.append(product)
                        shapes.append(shape)
                        accepted.append(distance)
                        break
                if record["product"] is not None:
                    break
            records.append(record)
        for a, b in air_intervals(line, problem, np.array(note["geometry"]["table"])):
            split = sorted({a, b, *[x for x in accepted if a < x < b]})
            spans.append(max(np.diff(split)))
    worst = float(max(spans, default=0))
    return products, {
        "valid": bool(products) and worst <= max_span + 1e-8,
        "maximum_air_span_m": worst,
        "maximum_allowed_span_m": max_span,
        "method": method,
        "trials": trials,
        "stations": records,
        "seconds": time.monotonic() - began,
        "scope": "Whole-contact procedural support rhythm; no structural calculation or subsurface foundation claim.",
    }
