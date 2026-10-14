"""Bind local recipe meshes and conservative cells to existing shared contracts."""

import shapely
from outshine_ref.contracts import HeightInterval, LocalFrame, Reservation
from outshine_ref.transport import Plan


def products(
    case,
    meshes,
    *,
    frame="Vienna-ENU-48.21-16.37",
    upper_depth=0.35,
    lower_depth=0.25,
    headroom=4.5,
):
    reservations, cells = [], []
    for kind, source, mesh, depth in (
        ("deck", case["upper"], meshes[0], upper_depth),
        ("ground_traffic", case["lower"], meshes[1], lower_depth),
    ):
        positions = mesh.vertices[mesh.triangles]
        for face, triangle in enumerate(positions):
            geometry = shapely.Polygon(triangle[:, :2])
            low, high = float(triangle[:, 2].min()), float(triangle[:, 2].max())
            for purpose, floor, ceiling in (
                ("transport_structure", low - depth, high),
                ("traffic_clearance", low, high + headroom),
            ):
                reservations.append(
                    Reservation(
                        source["source_id"],
                        geometry,
                        HeightInterval(floor, ceiling),
                        purpose,
                    )
                )
                cells.append(
                    {
                        "source_id": source["source_id"],
                        "surface_kind": kind,
                        "face": face,
                        "purpose": purpose,
                        "bounds_m": [floor, ceiling],
                        "xy": triangle[:, :2].tolist(),
                    }
                )
    plan = Plan(
        LocalFrame(frame), reservations=tuple(reservations), meshes=tuple(meshes)
    )
    binding = {
        "frame": frame,
        "mesh_order": [
            {"kind": "deck", "source_id": case["upper"]["source_id"]},
            {"kind": "ground_traffic", "source_id": case["lower"]["source_id"]},
        ],
        "cells": cells,
        "scope": "Existing Plan/Reservation contracts; conservative per-face height prisms, not exact swept volumes. Explicit chosen surface/source binding remains experiment metadata, no highest-hit selection.",
    }
    return plan, binding
