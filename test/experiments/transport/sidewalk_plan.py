"""Finite outside-road sidewalk proposals bound to actual accepted curb footprints."""

import numpy as np
import shapely

WIDTH_M = 2.0
REACH_M = 1.82


def dilation(shapes, method):
    if not 0 < len(shapes) <= 256:
        raise ValueError("bounded actual curb footprint set required")
    if method == "combined":
        return shapely.union_all(shapes).buffer(REACH_M, quad_segs=4)
    if method == "individual":
        return shapely.union_all(shapely.buffer(shapes, REACH_M, quad_segs=4))
    raise ValueError("unknown sidewalk mask method")


def select(curbs, rows, traffic, world, excluded, method="combined"):
    curb_domain = shapely.union_all(curbs)
    outside = traffic.buffer(WIDTH_M, quad_segs=4)
    blocked = shapely.union_all([traffic, curb_domain, excluded])
    mask = dilation(curbs, method).intersection(outside)
    mask = mask.difference(blocked).intersection(world.buffer(-0.01))
    parts = list(shapely.get_parts(mask))
    if len(parts) > 8192 or shapely.get_num_coordinates(mask) > 1048576:
        raise ValueError("sidewalk proposal polygon/coordinate budget")
    parts.sort(key=lambda s: (-s.area, *s.bounds))
    tree = shapely.STRtree(curbs)
    candidates = []
    for index, shape in enumerate(parts):
        reason = ""
        if shape.geom_type != "Polygon" or not shape.is_valid:
            reason = "invalid_sidewalk_candidate"
        elif shape.area < 0.1:
            reason = "sidewalk_candidate_below_0.1_m2"
        elif shapely.get_num_coordinates(shape) > 1024:
            reason = "sidewalk_footprint_coordinate_budget"
        hits = tree.query(shape, predicate="intersects")
        joint = []
        owners = []
        for hit in hits:
            line = shape.boundary.intersection(curbs[hit].boundary)
            if line.length > 1e-6:
                joint.append(line)
                owners.append(int(hit))
        if not owners and not reason:
            reason = "sidewalk_without_actual_curb_joint"
        candidates.append(
            {
                "candidate": index,
                "shape": shape,
                "curb_products": sorted(owners),
                "sources": sorted({s for i in owners for s in rows[i]["sources"]}),
                "joint_wkb_hex": shapely.to_wkb(shapely.union_all(joint), hex=True),
                "joint_length_m": sum(s.length for s in joint),
                "area_m2": shape.area,
                "reason": reason,
            }
        )
    return (
        candidates,
        blocked,
        outside,
        {
            "method": method,
            "maximum_width_xy_m": WIDTH_M,
            "maximum_curb_reach_m": REACH_M,
            "curb_footprint_coordinates": int(
                np.sum(shapely.get_num_coordinates(curbs))
            ),
            "mask_coordinates": int(shapely.get_num_coordinates(mask)),
            "candidates": len(candidates),
            "candidate_area_m2": sum(s.area for s in parts),
        },
    )
