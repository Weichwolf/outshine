"""Whole required headroom above new and explicitly borrowed source cap facets."""

import json

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import vertical_clearance
from stair_context_inputs import PROFILES


def source_clearances(data, rows, original, caps, upper):
    profiles = {
        p["kind"]: p
        for t in json.loads(PROFILES.read_text())["templates"]
        for p in t.get("osm", [])
        if p.get("layer") == "streets"
    }
    result = []
    for cast in (False, True):
        high = upper.vertices.astype("f4").astype(float) if cast else upper.vertices
        underside = Mesh(high, upper.triangles)
        checks = []
        for i, row in enumerate(rows):
            if "traffic_profile" in row:
                required = row["traffic_profile"]["clearance_m"]
            else:
                kind = row["original"]["source"]["feature"]["properties"].get(
                    "subclass"
                )
                required = profiles[kind]["clearanceM"]
            pieces = [
                data["surface_vertices"][data["surface_faces"][data["owners"][:, i]]]
            ]
            for cap in caps:
                p = cap.vertices[cap.triangles]
                selected = (
                    shapely.area(
                        shapely.intersection(shapely.polygons(p[:, :, :2]), original[i])
                    )
                    > 1e-12
                )
                if selected.any():
                    pieces.append(p[selected])
            triangles = np.concatenate(pieces)
            if cast:
                triangles = triangles.astype("f4").astype(float)
            lower = Mesh(
                triangles.reshape(-1, 3), np.arange(triangles.size // 3).reshape(-1, 3)
            )
            if len(upper.triangles):
                proof = vertical_clearance(lower, underside)
            else:
                proof = {
                    "valid": False,
                    "reason": "no_positive_area_overlap",
                    "checks": 0,
                }
            if not proof["valid"] and proof["reason"] != "no_positive_area_overlap":
                raise ValueError("whole source headroom unresolved: " + str(proof))
            if proof["valid"] and proof["minimum_clearance_m"] < required:
                raise ValueError(
                    "whole source headroom below declared profile: " + str(proof)
                )
            checks.append(
                {
                    "source_id": row["source_id"],
                    "required_clearance_m": required,
                    "lower_faces": len(lower.triangles),
                    **proof,
                }
            )
        result.append({"float32": cast, "sources": checks})
    return result
