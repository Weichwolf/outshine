"""Clip only new ground against existing ground and fit the frozen flat datum."""

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler
from outshine_ref.transport import HeightBudget, close_surface
from outshine_ref.transport.junction_pads import profile_junction_pad
from outshine_ref.transport.weighted_heights import fit_weighted_heights
from shared_bridge_geometry import corridor
from stair_branch_inputs import surface_sampling, surface_welding
from stair_cycle_contacts import profile
from stair_junction_layout import arrangement, landings
from stair_landing_pads import complete_landings
from stair_network_inputs import junctions


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
    if int(ground.sum()) != int(plan["ground"].sum()):
        raise ValueError("cached original ground facets are not exact source bottoms")
    key = recipe["port"]["source_id"]
    owner = state["report"]["after"]["source_ids"].index(key)
    selected = (road["face_sources"] & (1 << owner)) != 0
    if not ground[selected].all():
        raise ValueError("frozen endpoint must belong to actual ground")
    faces = road["top_faces"][selected]
    xyz = road["body_vertices"][faces]
    if not np.array_equal(
        road["body_vertices"][road["bottom_faces"][selected], 2], xyz[:, :, 2] - 0.3
    ):
        raise ValueError("cached source depth is incompatible with this module family")
    if np.ptp(xyz[:, :, 2]) != 0:
        raise ValueError("first module family requires a truly flat cached source")
    proposal = [
        p
        for p in state["report"]["after"]["proposed_outer_profiles"]
        if p["source_id"] == key
    ]
    if len(proposal) != 1:
        raise ValueError("one unambiguous cached outer endpoint required")
    line = np.array(proposal[0]["proposed_vertices_enu_m"])
    end = recipe["port"]["end"]
    source = sources[key]
    width = float(np.linalg.norm(line[-1, :2] - line[0, :2]))
    row = {
        "xy": np.array(source["xy_m"]),
        "width_m": width,
        "source_id": key,
        "port_end": end,
    }
    if (
        end not in (0, -1)
        or line.shape != (3, 3)
        or not np.allclose(profile(row, end), line[:, :2], rtol=0, atol=1e-8)
        or not np.all(line[:, 2] == xyz[0, 0, 2])
    ):
        raise ValueError("cached whole outer profile is shifted or incompatible")
    cap = Mesh(road["body_vertices"], faces)
    sampled = SurfaceSampler(cap).sample(
        line[:, :2], max_queries=3, max_candidates=65536
    )
    if not np.allclose(sampled.points_enu_m, line, rtol=0, atol=1e-7):
        raise ValueError(
            "actual cached source does not support its whole outer profile"
        )
    ground_mesh = Mesh(old_xyz, plan["faces"][plan["ground"]])
    upper = Mesh(road["body_vertices"], road["bottom_faces"][~ground])
    return row, line, ground_mesh, upper


def prepared(rows, sources, frozen_row, line, ground):
    keys = [r["source_id"] for r in rows]
    nodes = junctions(keys, sources, [], rows)
    anchor = sources[frozen_row["source_id"]]["mvt_xy"][frozen_row["port_end"]]
    root = [n for n in nodes if n["mvt_xy"] == anchor]
    if len(root) != 1 or not np.array_equal(root[0]["xy"], line[1, :2]):
        raise ValueError("original shared source node required at frozen endpoint")
    profiles = [line[:, :2]]
    ends = []
    for i in root[0]["incident"]:
        for end in (0, -1):
            if np.array_equal(rows[i]["xy"][end], line[1, :2]):
                profiles.append(profile(rows[i], end))
                ends.append(i)
    if not ends:
        raise ValueError("new module has no original endpoint at its frozen port")
    pad = profile_junction_pad(profiles)
    for i in ends:
        rows[i]["junction_pads_xy_m"] = [np.array(pad.exterior.coords[:-1])]
    complete_landings(rows, nodes, [], max_pads=16)
    surface_sampling(rows, "source_knots")
    surface_welding(rows, 1e-7)
    original = [corridor(r) for r in rows]
    _, _, masks = landings(rows, original, "width", 0.75, nodes)
    old = shapely.union_all(shapely.polygons(ground.vertices[ground.triangles, :2]))
    domains = [p.difference(old) for p in original]
    if any(p.is_empty or not p.is_valid for p in domains):
        raise ValueError("whole selected source requires a valid nonempty addition")
    return nodes, original, domains, masks, old


def build(rows, sources, frozen_row, line, ground, sample):
    nodes, original, domains, masks, old = prepared(
        rows, sources, frozen_row, line, ground
    )
    xy, faces, owners, arrangement_note = arrangement(rows, domains, masks)
    targets = sample(xy)
    fixed = shapely.dwithin(old, shapely.points(xy), 1e-7)
    existing = SurfaceSampler(ground, xy_tolerance_m=1e-7).sample(
        xy[fixed], max_queries=4096, max_candidates=65536
    )
    if not np.allclose(
        existing.points_enu_m[:, 2], line[1, 2] - 0.3, rtol=0, atol=1e-7
    ):
        raise ValueError("new addition touches nonflat or incompatible existing ground")
    for mask in masks:
        fixed |= shapely.dwithin(mask, shapely.points(xy), 1e-8)
    limits = np.min(np.where(owners, [r["gradient"] for r in rows], np.inf), axis=1)
    frames = np.eye(2)[None] / (limits[:, None, None] - 0.0002)
    fit = fit_weighted_heights(
        Mesh(np.c_[xy, targets], faces),
        frames,
        reference_datum_m=float(line[1, 2]),
        fixed_vertices=np.flatnonzero(fixed),
        budget=HeightBudget(max_vertices=4096, max_faces=8192),
    )
    if fit.mesh is None:
        raise ValueError("bounded module height fit failed: " + str(fit.report))
    body = close_surface(fit.mesh, 0.3)
    data = {
        "surface_vertices": fit.mesh.vertices,
        "surface_faces": faces,
        "owners": owners,
        "targets": targets,
        "limits": limits,
        "fixed": fixed,
        "body_vertices": body.vertices,
        "body_faces": body.triangles,
    }
    return (
        data,
        {"arrangement": arrangement_note, "fit": dict(fit.report)},
        (nodes, original, domains, masks, old),
    )
