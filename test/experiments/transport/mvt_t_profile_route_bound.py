"""Necessary finite-prior error from an explicit fixed point and supported route."""

import numpy as np
import shapely
from mvt_t_profile_layout import prior_values


def route_prior_bound(plan, contact, junction, target_source_id, *, max_vertices=4096):
    if contact.get("binding") != "fixed" or len(contact["xyz_m"]) != 1:
        raise ValueError("one explicitly fixed source-owned point required")
    owners = {row["source_id"]: i for i, row in enumerate(plan["rows"])}
    first, target = owners[contact["source_id"]], owners[target_source_id]
    if first == target or not {first, target} <= set(plan["incident"][junction]):
        raise ValueError(
            "explicit shared construction node between both owners required"
        )
    fixed = np.asarray(contact["xyz_m"][0], dtype=float)
    if fixed.shape != (3,) or not np.isfinite(fixed).all():
        raise ValueError("finite fixed XYZ point required")
    via = np.asarray(plan["nodes"][junction])
    vertices = np.unique(plan["parts"][target])
    if (
        type(max_vertices) is not int
        or not 0 <= max_vertices <= 4096
        or len(vertices) > max_vertices
    ):
        raise ValueError(
            "finite route-bound vertex budget exhausted before geometry work"
        )
    first_route = shapely.LineString([fixed[:2], via])
    if not plan["prepared_domains"][first].covers(first_route):
        raise ValueError("first route lacks complete actual source-domain coverage")
    xy = plan["xy"][vertices]
    routes = shapely.linestrings(np.stack([np.broadcast_to(via, xy.shape), xy], axis=1))
    supported = np.flatnonzero(shapely.covers(plan["prepared_domains"][target], routes))
    if not len(supported):
        raise ValueError("no completely supported route to a finite prior vertex")
    gradients = [plan["rows"][i]["gradient"] for i in (first, target)]
    if any(not np.isfinite(g) or g <= 0 for g in gradients):
        raise ValueError("positive finite whole-surface gradient bounds required")
    lengths = shapely.length(routes[supported])
    priors = prior_values(plan["rows"][target], xy[supported])
    bounds = (
        abs(priors - fixed[2])
        - first_route.length * gradients[0]
        - lengths * gradients[1]
    )
    best = int(np.argmax(bounds))
    hit = int(supported[best])
    return {
        "necessary_prior_error_m": max(0.0, float(bounds[best])),
        "fixed_contact_id": contact["contact_id"],
        "fixed_source_id": contact["source_id"],
        "fixed_xyz_m": fixed.tolist(),
        "shared_junction": junction,
        "via_xy_m": via.tolist(),
        "target_source_id": target_source_id,
        "target_vertex": int(vertices[hit]),
        "target_xy_m": xy[hit].tolist(),
        "target_prior_height_m": float(priors[best]),
        "route_lengths_m": [float(first_route.length), float(lengths[best])],
        "whole_gradient_limits": gradients,
        "vertices_tested": len(vertices),
        "supported_routes": len(supported),
        "route_coordinate_bytes": len(vertices) * 4 * 8,
        "scope": "Necessary error for this fixed point, actual supported two-owner route, shared continuous node and declared gradient bounds. Triangle inequality on finite mesh-vertex priors; no global-network or different-region impossibility claim. Floating geometry/arithmetic, not an exact interval certificate.",
    }
