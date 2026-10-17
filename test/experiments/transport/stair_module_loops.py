"""Explicit original-node contacts to an immutable cached source interior."""

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from outshine_ref.transport.junction_pads import profile_junction_pad
from stair_cycle_contacts import profile
from stair_module_sections import line_pieces


def interior_profile(state, sources, old_key, vertex):
    old_row = state.get("cached_module_rows", {}).get(old_key)
    if old_row is None or not np.array_equal(old_row["xy"], sources[old_key]["xy_m"]):
        raise ValueError(
            "interior contact needs its exact cached module axis and profile"
        )
    if type(vertex) is not int or not 1 <= vertex < len(old_row["xy"]) - 1:
        raise ValueError("one actual original interior source node required")
    node = sources[old_key]["mvt_xy"][vertex]
    if sum(p == node for p in sources[old_key]["mvt_xy"]) != 1:
        raise ValueError("ambiguous repeated original interior source node")
    xy = np.array(old_row["xy"])
    incoming, outgoing = xy[vertex] - xy[vertex - 1], xy[vertex + 1] - xy[vertex]
    tangent = incoming / np.linalg.norm(incoming) + outgoing / np.linalg.norm(outgoing)
    if np.linalg.norm(tangent) < 1e-6:
        raise ValueError("interior source cusp has no bounded shared profile")
    tangent /= np.linalg.norm(tangent)
    normal = np.array([-tangent[1], tangent[0]])
    cross = xy[vertex] + np.array([-1, 0, 1])[:, None] * normal * old_row["width_m"] / 2
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


def frozen_contacts(state, recipe, sources):
    declarations = recipe.get("interior_contacts", [])
    if len(declarations) > 4 or (
        declarations and recipe.get("contact_mode") != "profile"
    ):
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
    if len({tuple(r["mvt_xy"]) for r in result}) != len(result):
        raise ValueError("duplicate cached interior contact node")
    return result


def prepare_contacts(rows, nodes, contacts):
    keys = [r["source_id"] for r in rows]
    for contact in contacts:
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
        profiles = [contact["profile"][:, :2]]
        ends = []
        for i in incident:
            for end in (0, -1):
                if np.array_equal(rows[i]["xy"][end], contact["xy"]):
                    profiles.append(profile(rows[i], end))
                    ends.append(i)
        pad = profile_junction_pad(profiles)
        for i in ends:
            rows[i].setdefault("junction_pads_xy_m", []).append(
                np.array(pad.exterior.coords[:-1])
            )


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
