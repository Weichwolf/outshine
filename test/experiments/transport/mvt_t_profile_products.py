"""Shared navigation, mesh-port and terrain-reservation views of one solved plan."""

from collections import defaultdict
from itertools import pairwise

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler


def polygon_parts(shape):
    return [
        {
            "outer_xy_m": [list(p) for p in part.exterior.coords[:-1]],
            "holes_xy_m": [
                [list(p) for p in ring.coords[:-1]] for ring in part.interiors
            ],
        }
        for part in shapely.get_parts(shape)
    ]


def whole_section(line, triangles, sampler, extra_xy):
    crossings = shapely.intersection(line, shapely.boundary(triangles))
    xy = np.unique(np.vstack([shapely.get_coordinates(crossings), extra_xy]), axis=0)
    order = np.argsort(shapely.line_locate_point(line, shapely.points(xy)))
    sampled = sampler.sample(xy[order])
    if any(sampled.reasons):
        raise ValueError("whole profile lacks retained surface support")
    return sampled.points_enu_m.tolist()


def interface_ports(plan, heights):
    ports = []
    for landing, (patch, owners) in enumerate(
        zip(plan["masks"], plan["incident"], strict=True)
    ):
        for owner in owners:
            faces = plan["parts"][owner]
            inside = shapely.covers(
                patch, shapely.point_on_surface(shapely.polygons(plan["xy"][faces]))
            )
            edges = defaultdict(list)
            for face, flat in zip(faces, inside, strict=True):
                for a, b in (
                    (face[0], face[1]),
                    (face[1], face[2]),
                    (face[2], face[0]),
                ):
                    edges[tuple(sorted((int(a), int(b))))].append(bool(flat))
            for edge, sides in sorted(edges.items()):
                if len(sides) == 2 and sides[0] != sides[1]:
                    v = np.array(edge)
                    ports.append(
                        {
                            "source_id": plan["rows"][owner]["source_id"],
                            "landing": landing,
                            "vertex_ids": list(edge),
                            "profile_xyz_m": np.c_[plan["xy"][v], heights[v]].tolist(),
                        }
                    )
    return ports


def native_external_profiles(plan, samplers):
    owners = {row["source_id"]: i for i, row in enumerate(plan["rows"])}
    result = []
    for profile in plan["native_cut_context"]["profiles"]:
        owner = owners[profile["source_id"]]
        xy = np.array(profile["profile_xyz_m"])[:, :2]
        xyz = whole_section(
            shapely.LineString(xy),
            shapely.polygons(plan["xy"][plan["parts"][owner]]),
            samplers[owner],
            xy,
        )
        result.append(
            {
                "source_id": profile["source_id"],
                "source_station_m": profile["source_station_m"],
                "profile_xyz_m": xyz,
                "boundary_role": "artificial_native_mesh_cut",
                "profile_status": "sampled_local_body_pending_native_outside_integration",
                "station_scope": "Original source arclength; native navstation remains unqualified.",
            }
        )
    return result


def external_profiles(plan, samplers):
    if "native_cut_context" in plan:
        return native_external_profiles(plan, samplers)
    result = []
    roles = {
        (r["source_id"], r["endpoint"]): r["status"]
        for r in plan.get("region_context", {}).get("endpoint_roles", [])
    }
    for owner, row in enumerate(plan["rows"]):
        if (
            row["original"]["source"]["feature"]["properties"]["subclass"] == "steps"
            and not roles
        ):
            continue
        triangles = shapely.polygons(plan["xy"][plan["parts"][owner]])
        for end, adjacent in ((0, 1), (-1, -2)):
            role = roles.get((row["source_id"], end))
            if role in ("joint_observed_contact", "joint_construction_proposal"):
                continue
            point = row["xy"][end]
            tangent = point - row["xy"][adjacent]
            tangent /= np.linalg.norm(tangent)
            normal = np.array([-tangent[1], tangent[0]]) * row["width_m"] / 2
            line = shapely.LineString([point - normal, point + normal])
            xyz = whole_section(line, triangles, samplers[owner], point[None])
            result.append(
                {
                    "source_id": row["source_id"],
                    "endpoint": end,
                    "profile_xyz_m": xyz,
                    "profile_status": "proposed_pending_actual_neighbour_contact",
                    **({"boundary_role": role} if role is not None else {}),
                }
            )
    return result


def stair_level_ranges(plan, body_data):
    owners = body_data.get("stair_owners", np.array([plan["stairs"][0][0]]))
    offsets = body_data.get(
        "stair_level_offsets", np.array([0, len(body_data["levels_m"])])
    )
    return [
        {
            "source_id": plan["rows"][int(owner)]["source_id"],
            "begin": int(begin),
            "end": int(end),
        }
        for owner, begin, end in zip(owners, offsets[:-1], offsets[1:], strict=True)
    ]


def _consumer_samplers(plan, heights):
    tolerance = plan.get("consumer_sampling_tolerance_m", 0)
    if not np.isfinite(tolerance) or not 0 <= tolerance <= 1e-12:
        raise ValueError(
            "bounded explicit numeric consumer sampling allowance required"
        )
    if tolerance:
        for owner, row in enumerate(plan["rows"]):
            axis = row.get("navigation_axis_xy_m", row["xy"])
            queries = [
                *axis,
                *[
                    xy
                    for xy, incident in zip(
                        plan["nodes"], plan["incident"], strict=True
                    )
                    if owner in incident
                ],
            ]
            if not shapely.covers(
                plan["domains"][owner], shapely.points(queries)
            ).all():
                raise ValueError(
                    "numeric sampling cannot extend an axis outside its input domain"
                )
    samplers = [
        SurfaceSampler(Mesh(np.c_[plan["xy"], heights], f), xy_tolerance_m=tolerance)
        for f in plan["parts"]
    ]
    return samplers, tolerance


def shared_products(plan, heights, body_data):
    samplers, tolerance = _consumer_samplers(plan, heights)
    axes, profiles, graph_nodes, graph_edges = {}, {}, {}, []
    for owner, row in enumerate(plan["rows"]):
        axis = row.get("navigation_axis_xy_m", row["xy"])
        sampled = samplers[owner].sample(axis)
        if any(sampled.reasons):
            raise ValueError(
                "retained navigation axis has an unsupported surface point"
            )
        xyz = sampled.points_enu_m
        axes[row["source_id"]] = xyz.tolist()
        profiles[row["source_id"]] = whole_section(
            shapely.LineString(axis),
            shapely.polygons(plan["xy"][plan["parts"][owner]]),
            samplers[owner],
            axis,
        )
        for a, b in pairwise(xyz):
            keys = []
            for point in (a, b):
                key = tuple(point[:2])
                if key in graph_nodes and abs(graph_nodes[key][2] - point[2]) > 1e-7:
                    raise ValueError(
                        "navigation contact has conflicting shared heights"
                    )
                graph_nodes.setdefault(key, point.tolist())
                keys.append(key)
            graph_edges.append((keys, row["source_id"]))
    ordered = sorted(graph_nodes)
    indices = {key: i for i, key in enumerate(ordered)}
    landings = []
    for index, (xy, owners, patch) in enumerate(
        zip(plan["nodes"], plan["incident"], plan["masks"], strict=True)
    ):
        values = [samplers[owner].sample([xy]) for owner in owners]
        if any(any(v.reasons) for v in values):
            raise ValueError("construction node has no actual shared source support")
        zs = [v.points_enu_m[0, 2] for v in values]
        if np.ptp(zs) > 1e-7:
            raise ValueError("construction node lacks one common prepared height")
        landings.append(
            {
                "construction_id": plan["construction_contacts"][index][
                    "construction_id"
                ],
                "xy_m": list(xy),
                "height_m": float(zs[0]),
                "source_ids": [plan["rows"][i]["source_id"] for i in owners],
                "reservation_parts": polygon_parts(patch),
            }
        )
    return {
        "format_version": 1,
        "status": "local_2.5d_proposal_pending_external_ports_and_native_admission",
        "coordinate_frame": plan["coordinate_frame"],
        "source_metadata": plan["source_metadata"],
        **(
            {"native_cut_context": plan["native_cut_context"]}
            if "native_cut_context" in plan
            else {}
        ),
        "source_id_scope": "fixture native parts; no provider-node identity claim",
        **({"consumer_sampling_tolerance_m": tolerance} if tolerance else {}),
        **(
            {"compact_flights": plan["compact_flights"]}
            if plan.get("compact_flights")
            else {}
        ),
        **(
            {"region_context": plan["region_context"]}
            if "region_context" in plan
            else {}
        ),
        "stair_flights": [
            {
                "source_id": flight["source_id"],
                "origin_xy_m": flight["origin_xy_m"].tolist(),
                "tangent_xy": flight["tangent"].tolist(),
                "start_landing": flight["start_node"],
                "end_landing": flight["end_node"],
                "start_cut_m": flight["start_cut_m"],
                "end_cut_m": flight["end_cut_m"],
                "height_rule": "flat_linear_flat",
            }
            for flight in plan.get("stair_lofts", [])
        ],
        "landings": landings,
        "interface_ports": interface_ports(plan, heights),
        "navigation": {
            "nodes_xyz_m": [graph_nodes[k] for k in ordered],
            "edges": [
                {"nodes": [indices[k] for k in keys], "source_id": source}
                for keys, source in graph_edges
            ],
            "axes_xyz_m": axes,
            "axis_profiles_xyz_m": profiles,
            "stair_levels_m": body_data["levels_m"].tolist(),
            "stair_level_ranges": stair_level_ranges(plan, body_data),
            "scope": "height-envelope graph with explicit stair levels; no full swept walking route",
        },
        "terrain_reservations": [
            {
                "source_id": r["source_id"],
                "footprint_parts": polygon_parts(domain),
                "surface_face_ids": f.tolist(),
            }
            for r, domain, f in zip(
                plan["rows"], plan["prepared_domains"], plan["parts"], strict=True
            )
        ],
        "external_contacts": external_profiles(plan, samplers),
    }
