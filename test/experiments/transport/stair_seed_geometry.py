"""Whole ordinary-ground seed geometry without an artificial frozen root."""

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.transport import HeightBudget, close_surface, fit_surface_heights
from shared_bridge_geometry import corridor
from stair_branch_inputs import surface_sampling, surface_welding
from stair_closed_height_rounding import correct
from stair_junction_layout import arrangement, landings
from stair_landing_pads import complete_landings
from stair_module_materials import surface_slots
from stair_module_node_heights import apply_heights, plan_heights
from stair_module_roads import face_frames
from stair_module_topology import check_surface
from stair_network_inputs import junctions
from stair_seed_axes import axis_form
from stair_seed_precision import GRID_M


def seed_contract(recipe, sources):
    seed = recipe.get("seed", {})
    if (
        recipe.get("contact_mode") != "seed"
        or recipe.get("junction_mode") != "level_box"
        or "port" in recipe
        or recipe.get("interior_contacts")
        or recipe.get("endpoint_contacts")
        or set(seed) != {"source_id", "end"}
        or seed["source_id"] not in recipe["source_ids"]
        or type(seed["end"]) is not int
        or seed["end"] not in (0, -1)
        or recipe.get("seed_grid_m") not in (None, GRID_M)
    ):
        raise ValueError(
            "explicit independent whole-source seed without cached contacts required"
        )
    for key in recipe["source_ids"]:
        axis_form(sources[key])


def prepared(rows, sources, ground):
    keys = [r["source_id"] for r in rows]
    nodes = junctions(keys, sources, [], rows)
    complete_landings(rows, nodes, [], max_pads=16, include_interiors=True)
    surface_sampling(rows, "source_knots")
    surface_welding(rows, 1e-7)
    original = [corridor(r) for r in rows]
    _, _, masks = landings(rows, original, "width", 0.75, nodes)
    old = shapely.union_all(shapely.polygons(ground.vertices[ground.triangles, :2]))
    if not old.is_empty and any(p.distance(old) < 0.2 for p in original):
        raise ValueError(
            "independent seed touches old ground traffic; explicit contact recipe required"
        )
    return nodes, original, original, masks, old


def plans_for(rows, nodes):
    plans = plan_heights(rows, nodes, [], include_interiors=True)
    plans.extend(
        {
            "mvt_xy": declaration["mvt_xy"],
            "height_m": declaration["height_m"],
            "shape": shapely.box(*declaration["bounds_xy_m"]),
        }
        for row in rows
        for declaration in row.get("pending_junctions", [])
    )
    grid = rows[0].get("source_grid_m")
    if grid:
        for plan in plans:
            bounds = np.array(plan["shape"].bounds) / grid
            plan["shape"] = shapely.box(
                *(np.r_[np.floor(bounds[:2]), np.ceil(bounds[2:])] * grid)
            )
    return plans


def build(rows, sources, ground, sample):
    planning = prepared(rows, sources, ground)
    nodes, _original, domains, _masks, _old = planning
    plans = plans_for(rows, nodes)
    xy, faces, owners, note = arrangement(
        rows, domains, [], constraints=[p["shape"].boundary for p in plans]
    )
    check_surface(xy, faces)
    targets = sample(xy)
    fixed, values = apply_heights(xy, np.zeros(len(xy), bool), targets, plans)
    frames = face_frames(rows, xy, faces, owners, reserve=0.0002)
    surface = Mesh(np.c_[xy, targets], faces)
    result = fit_surface_heights(
        surface,
        frames,
        fixed={int(i): float(values[i]) for i in np.flatnonzero(fixed)},
        budget=HeightBudget(max_vertices=4096, max_faces=8192),
    )
    if result.mesh is None:
        raise ValueError("bounded seed height fit failed: " + str(result.report))
    vertices = result.mesh.vertices.copy()
    vertices[fixed, 2] = values[fixed]
    closed, rounding = {}, None
    if (
        len(rows) == 1
        and axis_form(sources[rows[0]["source_id"]]) == "closed"
        and not plans
    ):
        closed["unrounded_closed_heights"] = vertices[:, 2].copy()
        vertices, rounding = correct(
            vertices, faces, face_frames(rows, xy, faces, owners)
        )
    surface = Mesh(vertices, faces)
    body = close_surface(surface, 0.3)
    data = {
        "surface_vertices": surface.vertices,
        "surface_faces": faces,
        "owners": owners,
        "targets": targets,
        "limits": np.min(
            np.where(owners, [r["gradient"] for r in rows], np.inf), axis=1
        ),
        "fixed": fixed,
        "fixed_heights": values[fixed],
        "grade_frames": frames,
        "surface_materials": surface_slots(rows, owners),
        "body_vertices": body.vertices,
        "body_faces": body.triangles,
        **closed,
    }
    return (
        data,
        {
            "arrangement": note,
            "fit": dict(result.report),
            **({"closed_height_rounding": rounding} if rounding else {}),
        },
        planning,
    )
