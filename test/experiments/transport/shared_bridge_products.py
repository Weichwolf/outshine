"""Existing Plan contracts plus precise source coverage and explicit unresolved floors."""

import hashlib

import numpy as np
import shapely
from outshine_ref.contracts import (
    HeightInterval,
    LocalFrame,
    Reservation,
    SourceFeature,
)
from outshine_ref.transport import Plan
from shared_bridge_geometry import corridor


def products(
    sources,
    meshes,
    report,
    *,
    frame="Vienna-ENU-48.21-16.37",
    top_roles=("deck",),
    top_kind="deck",
):
    identity = hashlib.sha256(
        "\n".join(sorted(s["source_id"] for s in sources)).encode()
    ).hexdigest()[:16]
    owner = "shared-bridge-" + identity
    reservations, cells = [], []
    for kind, roles, mesh, depth in (
        (top_kind, top_roles, meshes[0], 0.35),
        ("ground_traffic", ("ground",), meshes[1], 0.25),
    ):
        if mesh is None:
            continue
        candidates = [
            (s["source_id"], corridor(s)) for s in sources if s["role"] in roles
        ]
        shapes = np.array([p for _, p in candidates], object)
        tree = shapely.STRtree(shapes)
        for face, triangle in enumerate(mesh.vertices[mesh.triangles]):
            shape = shapely.Polygon(triangle[:, :2])
            covered = [
                candidates[i][0]
                for i in sorted(tree.query(shape, predicate="intersects"))
                if shape.intersection(shapes[i]).area > 0
            ]
            if not covered:
                raise ValueError("product face lost its source binding")
            low, high = float(triangle[:, 2].min()), float(triangle[:, 2].max())
            for purpose, floor, ceiling in (
                ("transport_structure", low - depth, high),
                ("traffic_clearance", low, high + 4.5),
            ):
                reservations.append(
                    Reservation(owner, shape, HeightInterval(floor, ceiling), purpose)
                )
                cells.append(
                    {
                        "component_owner": owner,
                        "source_ids": covered,
                        "surface_kind": kind,
                        "face": face,
                        "purpose": purpose,
                        "bounds_m": [floor, ceiling],
                        "xy": triangle[:, :2].tolist(),
                    }
                )
    pending = [
        {
            "source_id": s["source_id"],
            "reason": s["omission_reason"],
            "xy": s["xy"],
            "corridor_wkb_hex": corridor(s).wkb_hex,
            "properties": s["properties"],
            "missing_contract": "Chosen indoor floor/roof surface in the same ENU frame; level alone is not metres",
        }
        for s in sources
        if s["role"] == "deferred"
    ]
    originals = tuple(
        SourceFeature(s["source_id"], shapely.LineString(s["xy"]), s["properties"])
        for s in sources
    )
    plan = Plan(
        LocalFrame(frame),
        reservations=tuple(reservations),
        meshes=tuple(m for m in meshes if m is not None),
    )
    return plan, {
        "frame": frame,
        "component_owner": owner,
        "cells": cells,
        "source_originals": [
            {
                "source_id": s.source_id,
                "xy": shapely.get_coordinates(s.geometry).tolist(),
                "properties": dict(s.attributes),
            }
            for s in originals
        ],
        "source_surface_roles": [
            {"source_id": s["source_id"], "role": s["role"]} for s in sources
        ],
        "mesh_order": [
            {"kind": top_kind, "present": meshes[0] is not None},
            {"kind": "ground_traffic", "present": meshes[1] is not None},
        ],
        "pending_buildings_contacts": pending,
        "report": report,
        "scope": "One source-coherent surface per role, conservative face prisms. All originals retained; indoor floors remain unresolved and no complete neighboring-world clearance is claimed. Provenance describes positive-area face contributions, not whole-face exclusive ownership. No public API change.",
    }
