"""Explicit shared ground junction pads derived from whole original end profiles."""

import numpy as np
import shapely
from shared_bridge_geometry import corridor
from stair_cycle_contacts import profile


def apply_pads(rows, nodes, declarations):
    if not isinstance(declarations, list) or not 1 <= len(declarations) <= 8:
        raise ValueError("one to eight explicit endpoint junction pads required")
    if any("junction_pads_xy_m" in row for row in rows):
        raise ValueError("junction pads must start from original source bands")
    keys = [r["source_id"] for r in rows]
    additions, notes, seen = {}, [], set()
    for declaration in declarations:
        profiles = declaration["profiles"]
        if len(profiles) != 2:
            raise ValueError("a corner pad needs two actual endpoint profiles")
        owners, ends = zip(*profiles, strict=True)
        if len(set(owners)) != 2 or any(end not in (0, -1) for end in ends):
            raise ValueError("distinct source endpoint profiles required")
        ids = [keys.index(key) for key in owners]
        signature = tuple(sorted(zip(owners, ends, strict=True)))
        if signature in seen:
            raise ValueError("duplicate corner pad")
        seen.add(signature)
        point = rows[ids[0]]["xy"][ends[0]]
        if not np.array_equal(point, rows[ids[1]]["xy"][ends[1]]):
            raise ValueError("corner pad requires coincident original endpoints")
        matches = [
            node
            for node in nodes
            if node["mvt_xy"] is not None
            and set(ids) <= set(node["incident"])
            and np.array_equal(point, node["xy"])
        ]
        if len(matches) != 1:
            raise ValueError("corner pad requires one actual chosen source node")
        for i in ids:
            props = rows[i]["original"]["source"]["feature"]["properties"]
            if (
                rows[i]["original"]["bridge"]
                or props.get("subclass") not in ("cycleway", "footway")
                or props.get("brunnel")
                or props.get("layer", 0) != 0
                or props.get("level", 0) != 0
                or props.get("indoor")
            ):
                raise ValueError("corner pad needs ordinary exterior ground paths")
        original = [corridor(rows[i]) for i in ids]
        lines = [profile(rows[i], end) for i, end in zip(ids, ends, strict=True)]
        hull = shapely.MultiPoint(np.vstack(lines)).convex_hull
        method = declaration["method"]
        if method not in ("profile_hull", "profile_box"):
            raise ValueError("known profile corner recipe required")
        pad = hull if method == "profile_hull" else hull.envelope
        width = max(rows[i]["width_m"] for i in ids)
        if (
            pad.geom_type != "Polygon"
            or not pad.is_valid
            or not 1e-6 < pad.area <= width**2
            or any(not pad.buffer(1e-8).covers(shapely.LineString(p)) for p in lines)
        ):
            raise ValueError("nondegenerate bounded whole-profile corner pad required")
        coords = np.array(pad.exterior.coords[:-1])
        for i in ids:
            additions.setdefault(i, []).append(coords.copy())
        notes.append(
            {
                "method": method,
                "profiles": profiles,
                "node_mvt_xy": matches[0]["mvt_xy"],
                "node_xy_m": point.tolist(),
                "pad_xy_m": coords.tolist(),
                "pad_area_m2": pad.area,
                "added_union_area_m2": pad.difference(shapely.union_all(original)).area,
                "whole_original_profile_xy_m": [line.tolist() for line in lines],
                "source_axes_and_attributes": "unchanged",
            }
        )
    for i, pads in additions.items():
        rows[i]["junction_pads_xy_m"] = pads
    return notes
