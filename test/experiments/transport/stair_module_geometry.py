"""Clip new ground and fit whole contacts against the actual frozen old surfaces."""

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from outshine_ref.transport import HeightBudget, close_surface, fit_surface_heights
from outshine_ref.transport.weighted_heights import fit_weighted_heights
from shared_bridge_geometry import corridor
from stair_branch_inputs import surface_sampling, surface_welding
from stair_junction_layout import arrangement, landings
from stair_landing_pads import complete_landings
from stair_module_loops import frozen_contacts, prepare_contacts
from stair_module_materials import surface_slots
from stair_module_node_heights import apply_heights, plan_heights
from stair_module_roads import face_frames
from stair_module_roots import bind_root_pad, profile_masks, root_profile
from stair_module_sections import (
    cap_edge_constraints,
    profile_levels,
    whole_line_pieces,
)
from stair_module_topology import check_surface
from stair_network_inputs import junctions
from stair_source_owners import source_selection


def old_surfaces(state, recipe, sources):
    road, plan = state["road"], state["plan"]
    old_xyz = np.c_[plan["xy"], state["heights"]]
    ground_xyz = old_xyz[plan["faces"][plan["ground"]]]
    known = {triangle.tobytes() for triangle in ground_xyz}
    ground = np.array(
        [
            triangle.tobytes() in known
            for triangle in road["body_vertices"][road["bottom_faces"]]
        ]
    )
    if "road_ground" in state:
        ground = state["road_ground"]
    elif int(ground.sum()) != int(plan["ground"].sum()):
        raise ValueError("cached original ground facets are not exact source bottoms")
    key = recipe["port"]["source_id"]
    varying = recipe.get("contact_mode") == "profile"
    selected = source_selection(state, key, state["report"]["after"]["source_ids"])
    if not ground[selected].all():
        raise ValueError("frozen endpoint must belong to actual ground")
    faces = road["top_faces"][selected]
    xyz = road["body_vertices"][faces]
    if not np.array_equal(
        road["body_vertices"][road["bottom_faces"][selected], 2], xyz[:, :, 2] - 0.3
    ):
        raise ValueError("cached source depth is incompatible with this module family")
    row, line = root_profile(state, recipe, sources)
    cap = Mesh(road["body_vertices"], faces)
    levels = profile_levels(cap.vertices, cap.triangles, line[:, :2])
    if not len(levels) or (not varying and np.max(abs(levels - line[1, 2])) > 1e-7):
        raise ValueError("whole actual cached profile must be locally flat")
    sampled = SurfaceSampler(cap).sample(
        line[:, :2], max_queries=3, max_candidates=65536
    )
    if not np.allclose(sampled.points_enu_m, line, rtol=0, atol=1e-7):
        raise ValueError(
            "actual cached source does not support its whole outer profile"
        )
    if varying:
        whole_line_pieces(cap.vertices, cap.triangles, line[:, :2])
        row["cached_cap"] = cap
    state["module_contacts"] = frozen_contacts(state, recipe, sources)
    row["additional_contacts"] = state["module_contacts"]
    row["include_interiors"] = row["root_interior"] or bool(
        recipe.get("endpoint_contacts")
    )
    ground_mesh = Mesh(old_xyz, plan["faces"][plan["ground"]])
    upper = Mesh(road["body_vertices"], road["bottom_faces"][~ground])
    return row, line, ground_mesh, upper


def prepared(rows, sources, frozen_row, line, ground):
    keys = [r["source_id"] for r in rows]
    nodes = junctions(keys, sources, [], rows)
    anchor = sources[frozen_row["source_id"]]["mvt_xy"][frozen_row["port_vertex"]]
    root = [n for n in nodes if n["mvt_xy"] == anchor]
    if not root:
        incident = [
            i
            for i, key in enumerate(keys)
            if any(sources[key]["mvt_xy"][end] == anchor for end in (0, -1))
        ]
        if incident:
            node = {
                "mvt_xy": anchor,
                "xy": list(frozen_row["xy"][frozen_row["port_vertex"]]),
                "incident": incident,
            }
            nodes.append(node)
            root = [node]
    if len(root) != 1 or not np.array_equal(root[0]["xy"], line[1, :2]):
        raise ValueError("original shared source node required at frozen endpoint")
    contacts = frozen_row.get("additional_contacts", [])
    at_root = [c for c in contacts if c["mvt_xy"] == anchor]
    bind_root_pad(
        rows,
        root[0],
        line,
        include_interiors=frozen_row["root_interior"],
        cached_profiles=[c["profile"][:, :2] for c in at_root],
    )
    prepare_contacts(rows, nodes, [c for c in contacts if c["mvt_xy"] != anchor])
    complete_landings(
        rows, nodes, [], max_pads=16, include_interiors=frozen_row["include_interiors"]
    )
    surface_sampling(rows, "source_knots")
    surface_welding(rows, 1e-7)
    original = [corridor(r) for r in rows]
    _, _, masks = landings(rows, original, "width", 0.75, nodes)
    if "cached_cap" in frozen_row:
        masks = profile_masks(nodes, masks, line, contacts)
    old = shapely.union_all(shapely.polygons(ground.vertices[ground.triangles, :2]))
    domains = [p.difference(old) for p in original]
    if any(p.is_empty or not p.is_valid for p in domains):
        raise ValueError("whole selected source requires a valid nonempty addition")
    return nodes, original, domains, masks, old


def contact_pins(xy, old, ground, masks, datum, mode):
    if mode not in ("flat", "piecewise", "profile"):
        raise ValueError("known module contact mode required")
    touching = shapely.dwithin(old, shapely.points(xy), 1e-7)
    existing = (
        SurfaceSampler(ground, xy_tolerance_m=1e-7)
        .sample(xy[touching], max_queries=4096, max_candidates=65536)
        .points_enu_m[:, 2]
        + 0.3
    )
    if not np.isfinite(existing).all():
        raise ValueError("new module contact lacks actual cached ground")
    if mode == "flat" and not np.allclose(existing, datum, rtol=0, atol=1e-7):
        raise ValueError("new addition touches nonflat or incompatible existing ground")
    fixed = touching.copy()
    if mode != "profile":
        for mask in masks:
            fixed |= shapely.dwithin(mask, shapely.points(xy), 1e-8)
    values = np.full(len(xy), datum)
    if mode != "flat":
        values[touching] = existing
    return fixed, values


def build(
    rows,
    sources,
    frozen_row,
    line,
    ground,
    sample,
    *,
    contact_mode="flat",
    junction_mode="free",
):
    nodes, original, domains, masks, old = prepared(
        rows, sources, frozen_row, line, ground
    )
    constraints = ()
    if junction_mode not in ("free", "level_box") or (
        junction_mode != "free" and contact_mode != "profile"
    ):
        raise ValueError("known bounded new junction-height mode required")
    plans = []
    if contact_mode == "profile":
        caps = [frozen_row["cached_cap"]] + [
            c["cap"] for c in frozen_row["additional_contacts"]
        ]
        constraints = cap_edge_constraints(caps, domains)
        if junction_mode == "level_box":
            cached_points = [
                line[1, :2],
                *[c["xy"] for c in frozen_row["additional_contacts"]],
            ]
            plans = plan_heights(
                rows,
                nodes,
                cached_points,
                include_interiors=frozen_row["include_interiors"],
            )
            constraints.extend(p["shape"].boundary for p in plans)
    xy, faces, owners, arrangement_note = arrangement(
        rows,
        domains,
        [] if contact_mode == "profile" else masks,
        constraints=constraints,
    )
    if contact_mode == "profile":
        check_surface(xy, faces)
    targets = sample(xy)
    fixed, values = contact_pins(xy, old, ground, masks, line[1, 2], contact_mode)
    fixed, values = apply_heights(xy, fixed, values, plans)
    limits = np.min(np.where(owners, [r["gradient"] for r in rows], np.inf), axis=1)
    frames = np.eye(2)[None] / (limits[:, None, None] - 0.0002)
    if contact_mode == "profile":
        frames = face_frames(rows, xy, faces, owners, reserve=0.0002)
    surface = Mesh(np.c_[xy, targets], faces)
    budget = HeightBudget(max_vertices=4096, max_faces=8192)
    if contact_mode == "flat":
        fit = fit_weighted_heights(
            surface,
            frames,
            reference_datum_m=float(line[1, 2]),
            fixed_vertices=np.flatnonzero(fixed),
            budget=budget,
        )
    else:
        fit = fit_surface_heights(
            surface,
            frames,
            fixed={int(i): float(values[i]) for i in np.flatnonzero(fixed)},
            budget=budget,
        )
    if fit.mesh is None:
        raise ValueError("bounded module height fit failed: " + str(fit.report))
    vertices = fit.mesh.vertices
    if contact_mode == "profile":
        vertices = vertices.copy()
        vertices[fixed, 2] = values[fixed]
    surface = Mesh(vertices, faces)
    body = close_surface(surface, 0.3)
    data = {
        "surface_vertices": surface.vertices,
        "surface_faces": faces,
        "owners": owners,
        "targets": targets,
        "limits": limits,
        "fixed": fixed,
        "body_vertices": body.vertices,
        "body_faces": body.triangles,
    }
    if contact_mode != "flat":
        data["fixed_heights"] = values[fixed]
    if contact_mode == "profile":
        data["grade_frames"] = frames
        data["surface_materials"] = surface_slots(rows, owners)
    return (
        data,
        {
            "arrangement": arrangement_note,
            "fit": dict(fit.report),
            **(
                {
                    "new_node_heights": [
                        {k: v for k, v in p.items() if k != "shape"} for p in plans
                    ]
                }
                if plans
                else {}
            ),
        },
        (nodes, original, domains, masks, old),
    )
