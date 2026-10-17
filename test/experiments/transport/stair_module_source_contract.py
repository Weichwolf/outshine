"""Verify saved shared nodes and pad hulls against original axes and frozen caps."""

from collections import defaultdict

import numpy as np
import shapely
from stair_landing_pads import node_profiles


def shared_nodes(rows, sources, recipe, contacts):
    positions = defaultdict(list)
    for i, row in enumerate(rows):
        source = sources[row["source_id"]]
        for vertex, mvt in enumerate(source["mvt_xy"]):
            positions[tuple(mvt)].append((i, vertex))
    expected = {}
    for mvt, found in positions.items():
        incident = sorted({i for i, _v in found})
        endpoint = any(v in (0, len(rows[i]["xy"]) - 1) for i, v in found)
        if len(incident) > 1 and endpoint:
            expected[mvt] = incident
    if recipe.get("contact_mode") == "seed":
        if "port" in recipe or contacts:
            raise ValueError("independent seed cannot claim cached source contacts")
        return expected, positions, None
    port = recipe["port"]
    index = port["vertex"] if "vertex" in port else port["end"]
    root = tuple(sources[port["source_id"]]["mvt_xy"][index])
    expected[root] = sorted({i for i, _v in positions[root]})
    for contact in contacts:
        mvt = tuple(contact["mvt_xy"])
        expected[mvt] = sorted({i for i, _v in positions[mvt]})
    if not expected[root]:
        raise ValueError("frozen profile lacks an original new source node")
    return expected, positions, root


def verify_source_contract(rows, sources, recipe, contacts, frozen_line, nodes):
    port = recipe.get("port", {})
    include_interiors = (
        recipe.get("contact_mode") == "seed"
        or "vertex" in port
        or bool(recipe.get("endpoint_contacts"))
    )
    expected, positions, root = shared_nodes(rows, sources, recipe, contacts)
    if (
        len(nodes) != len(expected)
        or {tuple(n["mvt_xy"]) for n in nodes} != expected.keys()
    ):
        raise ValueError("saved graph differs from actual original source contacts")
    allowed = defaultdict(list)
    for node in nodes:
        mvt = tuple(node["mvt_xy"])
        i, vertex = positions[mvt][0]
        if node["incident"] != expected[mvt] or not np.array_equal(
            node["xy"], rows[i]["xy"][vertex]
        ):
            raise ValueError(
                "saved node membership or location differs from original sources"
            )
        profiles = [
            p
            for _i, _v, p in node_profiles(
                rows, node, include_interiors=include_interiors
            )
        ]
        if mvt == root:
            profiles.append(frozen_line[:, :2])
        profiles.extend(
            c["profile"][:, :2] for c in contacts if tuple(c["mvt_xy"]) == mvt
        )
        if len(profiles) < 2:
            continue
        hull = shapely.MultiPoint(np.vstack(profiles)).convex_hull
        if hull.geom_type != "Polygon" or hull.area <= 0:
            continue
        for j in node["incident"]:
            allowed[j].append(hull)
    count = 0
    for i, row in enumerate(rows):
        pads = row.get("junction_pads_xy_m", [])
        for vertex in row.get("junction_pad_vertices", []):
            if (
                not include_interiors
                or type(vertex) is not int
                or not 0 < vertex < len(row["xy"]) - 1
                or i
                not in expected.get(
                    tuple(sources[row["source_id"]]["mvt_xy"][vertex]), []
                )
            ):
                raise ValueError(
                    "interior pad anchor is not a shared original source vertex"
                )
        if len(pads) > 16:
            raise ValueError("saved source pad count exhausted")
        for coordinates in pads:
            pad = shapely.Polygon(coordinates)
            if not pad.is_valid or not any(
                pad.hausdorff_distance(hull) <= 1e-7
                and pad.symmetric_difference(hull).area <= 1e-7
                for hull in allowed[i]
            ):
                raise ValueError("saved pad is not an actual incident-profile hull")
            count += 1
    return {
        "original_shared_nodes": len(expected),
        "validated_source_pad_uses": count,
        "cached_interior_nodes": sum(c.get("cached_interior", True) for c in contacts),
        **(
            {"cached_endpoint_nodes": len(recipe["endpoint_contacts"])}
            if recipe.get("endpoint_contacts")
            else {}
        ),
    }
