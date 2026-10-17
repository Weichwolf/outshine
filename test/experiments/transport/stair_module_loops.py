"""Explicit original-node contacts to an immutable cached source interior."""

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from outshine_ref.transport.junction_pads import profile_junction_pad
from stair_cycle_contacts import interior_profile_xy, profile
from stair_landing_pads import node_profiles
from stair_module_sections import line_pieces


def actual_profile(state, sources, old_key, vertex, *, interior):
    old_row = state.get("cached_module_rows", {}).get(old_key)
    if old_row is None or not np.array_equal(old_row["xy"], sources[old_key]["xy_m"]):
        raise ValueError(
            "interior contact needs its exact cached module axis and profile"
        )
    if type(vertex) is not int or not 0 <= vertex < len(old_row["xy"]):
        raise ValueError("one actual original source node required")
    if interior != (0 < vertex < len(old_row["xy"]) - 1):
        raise ValueError("declared cached profile kind differs from original node")
    node = sources[old_key]["mvt_xy"][vertex]
    if sum(p == node for p in sources[old_key]["mvt_xy"]) != 1:
        raise ValueError("ambiguous repeated original interior source node")
    xy = np.array(old_row["xy"])
    cross = (
        interior_profile_xy(old_row, vertex)
        if interior
        else profile(dict(old_row, xy=xy), 0 if vertex == 0 else -1)
    )
    owner = state["report"]["candidate_source_ids"].index(old_key)
    selected = (state["road"]["face_sources"] & (1 << owner)) != 0
    if not state["road_ground"][selected].all():
        raise ValueError("interior contact requires actual cached ground")
    cap = Mesh(state["road"]["body_vertices"], state["road"]["top_faces"][selected])
    sampled = (
        SurfaceSampler(cap, xy_tolerance_m=1e-7)
        .sample(cross, max_queries=3, max_candidates=65536)
        .points_enu_m
    )
    pieces, _parents = line_pieces(cap.vertices, cap.triangles, cross)
    if (
        not np.isfinite(sampled).all()
        or shapely.LineString(cross)
        .difference(shapely.union_all(pieces).buffer(1e-7))
        .length
        > 1e-7
    ):
        raise ValueError("actual cached interior profile has missing intervals")
    return {"mvt_xy": node, "xy": xy[vertex].tolist(), "profile": sampled, "cap": cap}


def interior_profile(state, sources, old_key, vertex):
    return actual_profile(state, sources, old_key, vertex, interior=True)


def endpoint_contacts(state, recipe, sources):
    result = []
    for chosen in recipe.get("endpoint_contacts", []):
        key, old_key = chosen["source_id"], chosen["cached_source_id"]
        vertex, old_vertex = chosen["vertex"], chosen["cached_vertex"]
        if key not in recipe["source_ids"] or old_key not in sources:
            raise ValueError("original new and cached endpoint sources required")
        if type(vertex) is not int or not 0 <= vertex < len(sources[key]["mvt_xy"]):
            raise ValueError("one actual original new source vertex required")
        contact = actual_profile(state, sources, old_key, old_vertex, interior=False)
        node = sources[key]["mvt_xy"][vertex]
        if node != contact["mvt_xy"]:
            raise ValueError("cached endpoint needs the exact shared original node")
        if not np.array_equal(sources[key]["xy_m"][vertex], contact["xy"]):
            raise ValueError("shared original endpoint has a different source frame")
        incident = [k for k in recipe["source_ids"] if node in sources[k]["mvt_xy"]]
        if any(sources[k]["mvt_xy"].count(node) != 1 for k in incident):
            raise ValueError("ambiguous repeated new source node")
        result.append(
            {
                **chosen,
                **contact,
                "cached_interior": False,
                "incident_source_ids": incident,
            }
        )
    return result


def frozen_contacts(state, recipe, sources):
    declarations = recipe.get("interior_contacts", [])
    count = len(declarations) + len(recipe.get("endpoint_contacts", []))
    if count > 4 or (count and recipe.get("contact_mode") != "profile"):
        raise ValueError("bounded explicit varying-profile interior contacts required")
    result = []
    for chosen in declarations:
        key, old_key, end = (
            chosen["source_id"],
            chosen["cached_source_id"],
            chosen["end"],
        )
        if key not in recipe["source_ids"] or end not in (0, -1):
            raise ValueError("interior contact must identify a new original endpoint")
        if old_key not in sources:
            raise ValueError("cached contact must retain an original source identity")
        node = sources[key]["mvt_xy"][end]
        positions = [i for i, p in enumerate(sources[old_key]["mvt_xy"]) if p == node]
        if len(positions) != 1:
            raise ValueError("one actual original interior source node required")
        contact = interior_profile(state, sources, old_key, positions[0])
        incident = [
            k
            for k in recipe["source_ids"]
            if any(sources[k]["mvt_xy"][e] == node for e in (0, -1))
        ]
        result.append(
            {
                "source_id": key,
                "cached_source_id": old_key,
                "end": end,
                **contact,
                "incident_source_ids": incident,
            }
        )
    result.extend(endpoint_contacts(state, recipe, sources))
    if len({(r["cached_source_id"], tuple(r["mvt_xy"])) for r in result}) != len(
        result
    ):
        raise ValueError("duplicate cached source contact node")
    return result


def prepare_contacts(rows, nodes, contacts):
    keys = [r["source_id"] for r in rows]
    groups = {}
    for contact in contacts:
        groups.setdefault(tuple(contact["mvt_xy"]), []).append(contact)
    for group in groups.values():
        contact = group[0]
        if any(
            c["incident_source_ids"] != contact["incident_source_ids"]
            or not np.array_equal(c["xy"], contact["xy"])
            for c in group
        ):
            raise ValueError(
                "shared cached node has conflicting source membership/frame"
            )
        incident = [keys.index(k) for k in contact["incident_source_ids"]]
        choices = [n for n in nodes if n["mvt_xy"] == contact["mvt_xy"]]
        if not choices:
            nodes.append(
                {"mvt_xy": contact["mvt_xy"], "xy": contact["xy"], "incident": incident}
            )
        elif len(choices) != 1 or choices[0]["incident"] != sorted(incident):
            raise ValueError(
                "cached interior contact conflicts with original source node"
            )
        node = next(n for n in nodes if n["mvt_xy"] == contact["mvt_xy"])
        chosen = node_profiles(rows, node, include_interiors=True)
        pad = profile_junction_pad(
            [*[c["profile"][:, :2] for c in group], *[p for _i, _v, p in chosen]]
        )
        for i, vertex, _p in chosen:
            rows[i].setdefault("junction_pads_xy_m", []).append(
                np.array(pad.exterior.coords[:-1])
            )
            if 0 < vertex < len(rows[i]["xy"]) - 1:
                indices = rows[i].setdefault("junction_pad_vertices", [])
                if vertex not in indices:
                    indices.append(vertex)


def reuse(state, recipe, rows, original, domains, root_cap):
    caps = [root_cap] + [c["cap"] for c in state.get("module_contacts", [])]
    faces = np.unique(np.vstack([c.triangles for c in caps]), axis=0)
    complete = Mesh(root_cap.vertices, faces)
    footprint = shapely.union_all(shapely.polygons(complete.vertices[faces, :2]))
    result = []
    for i, shape in enumerate(original):
        allowed = [(recipe["port"]["source_id"], root_cap)]
        allowed.extend(
            (c["cached_source_id"], c["cap"])
            for c in state.get("module_contacts", [])
            if rows[i]["source_id"] in c["incident_source_ids"]
        )
        pieces = [
            (
                key,
                shape.intersection(
                    shapely.union_all(shapely.polygons(cap.vertices[cap.triangles, :2]))
                ),
            )
            for key, cap in allowed
        ]
        borrowed = shapely.union_all([p for _key, p in pieces])
        if shape.difference(domains[i].union(borrowed)).area > 1e-7:
            raise ValueError("source reuse needs an undeclared cached asset")
        result.append(
            {
                "source_id": rows[i]["source_id"],
                "new_area_m2": domains[i].area,
                "reused_area_m2": borrowed.area,
                "reused_sources": [
                    {"source_id": key, "area_m2": p.area}
                    for key, p in pieces
                    if p.area > 1e-7
                ],
            }
        )
    return complete, footprint, result
