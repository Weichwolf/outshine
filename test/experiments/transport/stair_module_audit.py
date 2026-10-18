"""Independent saved geometry admission; whole old assets stay referenced unchanged."""

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from stair_module_clearance import source_clearances
from stair_module_contacts import interfaces, profiles
from stair_module_geometry import contact_pins
from stair_module_loops import reuse as profile_reuse
from stair_module_node_heights import apply_heights, inspect_heights, plan_heights
from stair_module_profiles import varying_profiles
from stair_module_roads import inspect_frames
from stair_module_topology import check_surface
from stair_network_geometry import vertex_links
from stair_road_vehicle import inspect_curves
from stair_shared_boxes import verify_masks
from stair_shared_paving import inspect_paving
from stair_source_owners import source_selection
from stair_surface_contacts import contact_regions
from stair_surface_portals import inspect_contacts
from surface_height_checks import normal_gradients
from surface_solid_checks import audit_closed


def module_body(data, rows, domains, *, source_tolerance_m=0.0):
    if not np.isfinite(source_tolerance_m) or not 0 <= source_tolerance_m <= 0.003:
        raise ValueError("bounded source geometry policy required")
    vertices, faces = data["surface_vertices"], data["surface_faces"]
    owners = data["owners"]
    n, m = len(vertices), len(faces)
    if (
        not 0 < n <= 4096
        or not 0 < m <= 8192
        or owners.shape != (m, len(rows))
        or owners.dtype.kind != "b"
        or not owners.any(axis=1).all()
    ):
        raise ValueError("bounded owned module surface required")
    body_v, body_f = data["body_vertices"], data["body_faces"]
    if not (
        np.array_equal(body_v[:n], vertices)
        and len(body_v) == 2 * n
        and np.array_equal(body_v[n:], vertices - [0, 0, 0.3])
        and np.array_equal(body_f[:m], faces)
        and np.array_equal(body_f[m : 2 * m], faces[:, ::-1] + n)
    ):
        raise ValueError("saved module differs from its actual surface and bottom")
    shapes = shapely.polygons(vertices[faces, :2])
    if not shapely.coverage_is_valid(shapes):
        raise ValueError("actual module surface coverage invalid")
    if "grade_frames" in data:
        check_surface(vertices[:, :2], faces)
    source_bounds = []
    for i, domain in enumerate(domains):
        actual = shapely.union_all(shapes[owners[:, i]])
        if source_tolerance_m:
            from stair_seed_precision import outward_contract

            source_bounds.append(outward_contract(actual, domain, source_tolerance_m))
        elif actual.symmetric_difference(domain).area > 1e-7:
            raise ValueError("whole source addition lost or gained positive area")
    limits = np.min(np.where(owners, [r["gradient"] for r in rows], np.inf), axis=1)
    if not np.array_equal(limits, data["limits"]):
        raise ValueError("source profile limits changed")
    original = body_v[body_f]
    reference_normals = np.cross(
        original[:, 1] - original[:, 0], original[:, 2] - original[:, 0]
    )
    proof = []
    for dtype in (np.float64, np.float32):
        top = vertices.astype(dtype).astype(float)
        slopes = np.linalg.norm(normal_gradients(top[:, :2], faces, top[:, 2]), axis=1)
        if np.any(slopes > limits):
            raise ValueError("actual module source gradient exceeded")
        body = Mesh(body_v.astype(dtype), body_f)
        xyz = body.vertices[body.triangles]
        normals = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
        if np.any(np.linalg.norm(normals, axis=1) == 0) or np.any(
            np.einsum("ij,ij->i", reference_normals, normals) <= 0
        ):
            raise ValueError("actual module body face collapses or inverts")
        volume = audit_closed(body)
        links = vertex_links(body.vertices, body.triangles)
        if volume <= 0:
            raise ValueError("nonpositive module volume")
        proof.append(
            {
                "dtype": str(np.dtype(dtype)),
                "volume_m3": volume,
                "maximum_normalized_grade": float(np.max(slopes / limits)),
                **links,
            }
        )
    return {
        "surface_vertices": n,
        "surface_faces": m,
        "body_faces": len(body_f),
        "checks": proof,
        **({"whole_source_outward_models": source_bounds} if source_bounds else {}),
    }


def traffic(data, upper, piers, *, clearances=None):
    result = []
    if len(piers) > 128:
        raise ValueError("old pier traffic cap")
    for cast in (False, True):
        vertices = (
            data["surface_vertices"].astype("f4").astype(float)
            if cast
            else data["surface_vertices"]
        )
        footprint = shapely.union_all(
            shapely.polygons(vertices[data["surface_faces"], :2])
        )
        high = upper.vertices.astype("f4").astype(float) if cast else upper.vertices
        upper_shape = shapely.union_all(shapely.polygons(high[upper.triangles, :2]))
        area = footprint.intersection(upper_shape).area
        if area > 1e-7 and clearances is None:
            raise ValueError("first ground module family intersects an existing bridge")
        distances = []
        for pier in piers:
            v = pier.vertices.astype("f4").astype(float) if cast else pier.vertices
            p = shapely.union_all(shapely.polygons(v[pier.triangles, :2]))
            distances.append(footprint.distance(p))
        if distances and min(distances) < 0.2:
            raise ValueError("new module violates old pier foreign-traffic distance")
        result.append(
            {
                "float32": cast,
                "bridge_overlap_area_m2": float(area),
                "minimum_whole_pier_xy_distance_m": min(distances)
                if distances
                else None,
            }
        )
    return result


def inspect_pins(data, fixed, values, mode):
    actual = data["surface_vertices"][fixed, 2]
    pin_ok = (
        np.array_equal(actual, values[fixed])
        if mode == "flat"
        else np.allclose(actual, values[fixed], rtol=0, atol=1e-7)
    )
    if not np.array_equal(fixed, data["fixed"]) or not pin_ok:
        raise ValueError("frozen module heights or pin ownership changed")
    if mode != "flat" and not np.array_equal(data["fixed_heights"], values[fixed]):
        raise ValueError("saved contact heights differ from immutable reference")


def junction_pins(data, recipe, rows, nodes, line, contacts, fixed, values):
    node_checks = []
    if recipe.get("junction_mode", "free") in ("level_box", "shared_box"):
        cached_points = [line[1, :2], *[c["xy"] for c in contacts]]
        plans = plan_heights(
            rows,
            nodes,
            cached_points,
            include_interiors="vertex" in recipe["port"]
            or bool(recipe.get("endpoint_contacts")),
        )
        fixed, values = apply_heights(
            data["surface_vertices"][:, :2],
            fixed,
            values,
            plans,
        )
        node_checks = inspect_heights(data, plans)
    return fixed, values, node_checks


def audit(state, recipe, data, rows, planning, line, upper):
    nodes, original, domains, masks, old = planning
    if recipe.get("junction_mode") == "shared_box":
        verify_masks(rows, nodes, original, masks, line, state["module_contacts"])
    selected = source_selection(
        state, recipe["port"]["source_id"], state["report"]["after"]["source_ids"]
    )
    cap = Mesh(state["road"]["body_vertices"], state["road"]["top_faces"][selected])
    old_cap = shapely.union_all(shapely.polygons(cap.vertices[cap.triangles, :2]))
    additions = shapely.union_all(domains)
    reuse = []
    mode = recipe.get("contact_mode", "flat")
    if mode != "profile":
        for i, shape in enumerate(original):
            borrowed = shape.intersection(old_cap)
            if shape.difference(domains[i].union(borrowed)).area > 1e-7:
                raise ValueError("source reuse needs an unrelated old asset")
            reuse.append(
                {
                    "source_id": rows[i]["source_id"],
                    "new_area_m2": domains[i].area,
                    "reused_area_m2": borrowed.area,
                    "reused_source_id": recipe["port"]["source_id"],
                }
            )
    contact_cap = cap
    if mode == "profile":
        contact_cap, old_cap, reuse = profile_reuse(
            state, recipe, rows, original, domains, cap
        )
    contact = interfaces(
        state["road"],
        data,
        old_cap,
        domains,
        float(line[1, 2]),
        reference_cap=contact_cap if mode in ("piecewise", "profile") else None,
    )
    for component in shapely.get_parts(additions):
        if (
            component.boundary.intersection(old_cap.boundary.buffer(1e-7)).length
            <= 1e-7
        ):
            raise ValueError("detached new module component")
    ground = Mesh(
        np.c_[state["plan"]["xy"], state["heights"]],
        state["plan"]["faces"][state["plan"]["ground"]],
    )
    fixed, values = contact_pins(
        data["surface_vertices"][:, :2], old, ground, masks, line[1, 2], mode
    )
    constructed = contact_regions(state.get("surface_contacts", []), original)
    fixed, values, node_checks = junction_pins(
        data, recipe, rows, nodes, line, state["module_contacts"], fixed, values
    )
    inspect_pins(data, fixed, values, mode)
    profile_note = (
        varying_profiles(
            data,
            rows,
            nodes,
            masks,
            original,
            cap,
            line,
            contacts=state.get("module_contacts", []),
            root_interior="vertex" in recipe["port"],
            constructed=constructed,
            shared_box=recipe.get("junction_mode") == "shared_box",
        )
        if mode == "profile"
        else profiles(
            data, rows, nodes, masks, original, cap, line, piecewise=mode == "piecewise"
        )
    )
    clearances = None
    if recipe.get("traffic_mode") == "source_clearance":
        caps = [cap] + [c["cap"] for c in state.get("module_contacts", [])]
        caps.extend(c["cap"] for c in constructed)
        clearances = source_clearances(data, rows, original, caps, upper)
    elif recipe.get("traffic_mode") is not None:
        raise ValueError("known explicit module traffic mode required")
    return {
        "body": module_body(data, rows, domains),
        **(
            {"axis_detour_bindings": state["axis_detour_bindings"]}
            if state.get("axis_detour_bindings")
            else {}
        ),
        **(
            {"borrowed_road_profiles": inspect_paving(state, rows, original, cap)}
            if any(c["kind"] == "minor_footway_node" for c in constructed)
            else {}
        ),
        **(
            {"surface_contacts": inspect_contacts(data, rows, constructed)}
            if constructed
            else {}
        ),
        "reuse": reuse,
        "interfaces": contact,
        **({"new_junction_heights": node_checks} if node_checks else {}),
        "profiles": profile_note,
        **(
            {
                "road_frames": [
                    inspect_frames(data, rows, cast) for cast in (False, True)
                ]
            }
            if mode == "profile"
            else {}
        ),
        "traffic": traffic(data, upper, state["piers"], clearances=clearances),
        **({"source_clearances": clearances} if clearances is not None else {}),
        **(
            {"horizontal_curves": inspect_curves(data, rows)}
            if any("horizontal_curve_plan" in row for row in rows)
            else {}
        ),
    }
