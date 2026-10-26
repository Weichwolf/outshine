"""Replay physical geometry, whole clearances and actual new source coverage."""

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import vertical_clearance
from retained_surface import surface_domain
from surface_height_checks import normal_gradients
from transport_gallery_wien import replay


def near(surface, bounds):
    xy = surface.vertices[surface.triangles, :2]
    low, high = xy.min(axis=1), xy.max(axis=1)
    selected = np.all(high >= bounds[:2], axis=1) & np.all(low <= bounds[2:], axis=1)
    return Mesh(surface.vertices, surface.triangles[selected])


def clearance(lower, upper):
    low_xy, high_xy = (
        lower.vertices[lower.triangles, :2],
        upper.vertices[upper.triangles, :2],
    )
    low_bounds = np.r_[low_xy.min(axis=(0, 1)), low_xy.max(axis=(0, 1))]
    high_bounds = np.r_[high_xy.min(axis=(0, 1)), high_xy.max(axis=(0, 1))]
    return vertical_clearance(
        near(lower, high_bounds), near(upper, low_bounds), max_faces=65536, seconds=8
    )


def verify(mesh, road, traffic, domain, fixture, previous, model, note, raw, ledger):
    shape = surface_domain(mesh)
    actual_traffic = surface_domain(Mesh(mesh.vertices, mesh.triangles[road]))
    grade = np.linalg.norm(
        normal_gradients(mesh.vertices[:, :2], mesh.triangles, mesh.vertices[:, 2]),
        axis=1,
    )
    normalized = grade / np.where(road, 0.025, 2 / 3)
    bridge = clearance(
        Mesh(mesh.vertices, mesh.triangles[road]),
        Mesh(previous[0].vertices - [0, 0, 0.35], previous[0].triangles),
    )
    soil = clearance(
        mesh, Mesh(previous[0].vertices - [0, 0, 0.35], previous[0].triangles)
    )
    cover = clearance(model["roof"], Mesh(mesh.vertices - [0, 0, 0.25], mesh.triangles))
    errors = [
        shape.symmetric_difference(domain).area,
        actual_traffic.symmetric_difference(traffic).area,
        surface_domain(previous[1]).difference(actual_traffic).area,
    ]
    if (
        not np.isfinite(normalized).all()
        or normalized.max() > 1 + 1e-7
        or max(errors) > 1e-7
        or any(not r["valid"] for r in (bridge, soil, cover))
        or bridge["minimum_clearance_m"] < 4.5 - 1e-7
        or soil["minimum_clearance_m"] < 0.02 - 1e-7
        or cover["minimum_clearance_m"] < 2 - 1e-7
    ):
        raise ValueError("actual whole network violates geometry or separation")
    identities = {r["source_id"] for r in ledger if r["status"] != "not_used"}
    rows = [r for r in raw["roads"] if r["source_id"] in identities]
    buffered = traffic.buffer(1e-6)
    shapely.prepare(buffered)
    if not np.all(
        shapely.covers(buffered, [shapely.LineString(r["coordinates"]) for r in rows])
    ):
        raise ValueError("new used source axis missing from actual traffic footprint")
    return {
        "frame": fixture["origin_lat_lon"],
        "whole_bridge_clearance": bridge,
        "whole_soil_gap": soil,
        "roof_cover_to_soil_underside": cover,
        "maximum_normalized_grade": float(normalized.max()),
        "domain_error_m2": errors[0],
        "traffic_error_m2": errors[1],
        "old_traffic_missing_m2": errors[2],
        "complete_new_source_axes": len(rows),
        "old_regression": replay(model, previous, note),
    }
