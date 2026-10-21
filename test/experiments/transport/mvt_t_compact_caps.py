"""Explicit compact straight-flight patches and provisional, unbound end caps."""

import copy
import math

import numpy as np
from mvt_t_connection_geometry import endpoint_nodes


def _frame(source, proposal, depth):
    xy = np.asarray(proposal["axes_xy_m"][source["source_id"]], float)
    tangent = xy[-1] - xy[0]
    length = float(np.linalg.norm(tangent))
    if not np.isfinite(length) or length <= 2 * depth + 2**-9:
        raise ValueError("compact flight needs a positive run beyond both end patches")
    tangent /= length
    normal = np.array([-tangent[1], tangent[0]])
    if np.max(abs((xy - xy[0]) @ normal)) > 1e-8:
        raise ValueError("compact cap requires a straight planned flight")
    return xy, tangent, normal, length


def _span(source, contact, sources, tangent):
    keys = contact["source_ids"]
    if len(keys) > 2:
        raise ValueError("compact cap needs a separate multi-leg patch recipe")
    for key in keys:
        if key == source["source_id"]:
            continue
        other = sources[key]
        endpoint = contact.get("source_endpoints", {}).get(key)
        if endpoint not in (0, -1):
            raise ValueError(
                "compact shared patch requires an observed neighbour endpoint"
            )
        xy = np.asarray(other["xy_m"], float)
        direction = xy[1] - xy[0] if endpoint == 0 else xy[-1] - xy[-2]
        direction /= np.linalg.norm(direction)
        if abs(tangent[0] * direction[1] - tangent[1] * direction[0]) > 0.25:
            raise ValueError("compact shared patch needs a near-collinear neighbour")
    return 2 * max(sources[key]["half_width_m"] for key in keys)


def compact_caps(sources, proposal, source_ids, *, cap_depth_m=0.3):
    if (
        not math.isfinite(cap_depth_m)
        or not 0.28 < cap_depth_m <= 1
        or not 1 <= len(source_ids) <= 16
        or len(set(source_ids)) != len(source_ids)
    ):
        raise ValueError(
            "bounded distinct compact flights and support-sized cap depth required"
        )
    rows = {s["source_id"]: s for s in sources}
    if any(key not in rows or rows[key]["kind"] != "steps" for key in source_ids):
        raise ValueError("explicit retained stair sources required for compact caps")
    result = copy.deepcopy(proposal)
    result.setdefault("boundary_caps", [])
    result.setdefault("compact_flights", [])
    roles = {
        (r["source_id"], r["endpoint"]): r["status"]
        for r in result["region_context"]["endpoint_roles"]
    }
    for key in source_ids:
        source = rows[key]
        xy, tangent, normal, length = _frame(source, result, cap_depth_m)
        contacts = (
            result["connections"]
            + result.get("observed_junctions", [])
            + result["boundary_caps"]
        )
        found = endpoint_nodes(contacts, key)
        caps = []
        for endpoint in (0, -1):
            if endpoint in found:
                contact = contacts[found[endpoint]]
            else:
                if roles.get((key, endpoint)) != "unknown_neighbour_not_free_end":
                    raise ValueError(
                        "outside or conflicting contact needs binding or joint expansion"
                    )
                contact = {
                    "construction_id": ["unbound-original-cap", key, endpoint],
                    "source_ids": [key],
                    "source_endpoints": {key: endpoint},
                    "xy_m": xy[endpoint].tolist(),
                    "boundary_role": roles[key, endpoint],
                }
                result["boundary_caps"].append(contact)
            if contact["xy_m"] != xy[endpoint].tolist() or "flat_patch_xy_m" in contact:
                raise ValueError(
                    "compact patch needs one unchanged endpoint and height owner"
                )
            half = _span(source, contact, rows, tangent)
            contact["flat_patch_xy_m"] = [
                (xy[endpoint] + tangent * a + normal * b).tolist()
                for a, b in (
                    (-cap_depth_m, -half),
                    (cap_depth_m, -half),
                    (cap_depth_m, half),
                    (-cap_depth_m, half),
                )
            ]
            caps.append(
                {
                    "endpoint": endpoint,
                    "normal_half_span_m": half,
                    "boundary_role": roles[key, endpoint],
                }
            )
        result["compact_flights"].append(
            {
                "source_id": key,
                "length_m": length,
                "cap_depth_m": cap_depth_m,
                "minimum_tread_depth_m": 0.3,
                "riser_height_budget_m": 0.17,
                "caps": caps,
            }
        )
    result["float32_edge_cleanup"] = True
    return result
