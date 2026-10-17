"""Existing architectural producer consumes actual height-adjusted source polygons."""

import numpy as np
import shapely
from context import attach
from outshine_ref.contracts import BuildingFootprint, HeightInterval, Mesh, Reservation
from provider_contract import attributes, hidden
from stair_consumer_inputs import root_contact, source_rows
from stair_occupancy_geometry import OccupancyError, polygonal_conflict
from tile_facades import FacadeDetails


def view_for(terrain):
    from stepped_source_probe import overview

    return overview(terrain.vertices, [1, -1, 0.7])


def generated(row, neighbors, view):
    owner = FacadeDetails([row, *neighbors], view)
    mesh, info = owner(row, attributes(row["properties"]), 1)
    vertices, faces = mesh["vertices"], mesh["faces"]
    if not np.isfinite(vertices).all() or not len(faces):
        raise ValueError("actual architecture producer returned no finite body")
    return Mesh(vertices, faces), {
        "producer": "tile_facades.FacadeDetails",
        "functional_status": info.get("functional_status", "see programme receipt"),
        "info": info,
        "rejected": owner.rejected,
        "room_rejected": owner.base.room_rejected,
    }


def projected_domain(mesh):
    triangles = mesh.vertices[mesh.triangles, :2]
    a, b = triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    projected = triangles[a[:, 0] * b[:, 1] != a[:, 1] * b[:, 0]]
    return polygonal_conflict(shapely.union_all(shapely.polygons(projected)))


def body_query(identity, mesh, dz):
    vertices = mesh.vertices + [0, 0, dz]
    world = Mesh(vertices, mesh.triangles)
    heights = HeightInterval(float(vertices[:, 2].min()), float(vertices[:, 2].max()))
    return world, BuildingFootprint(identity, projected_domain(world), heights)


def one(row, neighbors, data, query, view):
    contact = root_contact(data, row["polygon"])
    local_frame = contact["world_frame"] + ":flat-architecture:" + row["id"]
    dz = contact["root_enu_m"][2]
    original, original_info = generated(row, neighbors, view)
    before, requested = body_query(row["id"], original, dz)
    # Query the source footprint with heights measured from this actual recipe.
    footprint = BuildingFootprint(row["id"], row["polygon"], requested.heights)
    cut, cost = query.conflict(footprint)
    remaining = polygonal_conflict(row["polygon"].difference(cut))
    bodies, receipts, reservations = [], [], []
    for part, polygon in enumerate(shapely.get_parts(remaining)):
        if polygon.is_empty:
            continue
        actual, receipt = (
            (original, original_info)
            if polygon.equals_exact(row["polygon"], 0)
            else generated(row | {"polygon": polygon}, neighbors, view)
        )
        world, occupied = body_query(row["id"], actual, dz)
        residual, checked = query.conflict(occupied)
        if not residual.is_empty:
            raise OccupancyError("actual architecture body still enters transport")
        bodies.append(world)
        receipts.append(
            {
                "output_part": part,
                "footprint_wkb": polygon.wkb_hex,
                "body_heights_m": [occupied.heights.lower_m, occupied.heights.upper_m],
                "final_body_query": checked,
                "producer": receipt,
            }
        )
        reservations.append(
            Reservation(row["id"], occupied.geometry, occupied.heights, "building.body")
        )
    return (
        before,
        bodies,
        reservations,
        {
            "contact": contact,
            "local_recipe_frame": local_frame,
            "local_recipe_height_datum": "flat-local-ground0",
            "query_heights_m": [requested.heights.lower_m, requested.heights.upper_m],
            "source_footprint_wkb": row["polygon"].wkb_hex,
            "adjusted_footprint_wkb": remaining.wkb_hex,
            "removed_footprint_area_m2": row["polygon"].area - remaining.area,
            "query": cost,
            "original_producer": original_info,
            "products": receipts,
        },
    )


def buildings(data, query, *, maximum=128):
    rows, rejected = source_rows(data, "building")
    context = attach(
        rows,
        data["layers"],
        data["project"],
        data["context"]["vector"]["sha256"],
    )
    view = view_for(data["terrain"])
    before, after, reservations, ledger = [], [], [], rejected.copy()
    for index, row in enumerate(rows):
        retained = {k: v for k, v in row.items() if k != "polygon"}
        try:
            if hidden(row["properties"]):
                raise ValueError("source_hide_3d")
            if index >= maximum:
                raise ValueError("architecture source-part budget exhausted")
            neighbors = [r for r in rows if r["id"] != row["id"]]
            old, bodies, occupied, receipt = one(row, neighbors, data, query, view)
            before.append(old)
            after.extend(bodies)
            reservations.extend(occupied)
            ledger.append(retained | {"state": "prepared", "result": receipt})
        except (OccupancyError, ValueError, shapely.errors.GEOSException) as error:
            ledger.append(retained | {"state": "deferred", "reason": str(error)})
    return before, after, tuple(reservations), ledger, context
