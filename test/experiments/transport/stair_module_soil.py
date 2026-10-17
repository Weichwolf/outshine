"""Bounded local terrain replacement with exact cached boundary and pier parents."""

import numpy as np
import shapely
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import SurfaceSampler, conform_triangles, weld_vertices
from stair_blend_solve import solve
from stair_module_topology import check_surface
from stair_terrain_solid import retaining_body


def selection(state, footprint):
    plan = state["plan"]
    shapes = shapely.polygons(plan["xy"][plan["faces"]])
    tree = shapely.STRtree(shapes)
    selected = shapely.dwithin(shapes, footprint, 24)
    groups = []
    for low, _high in state["pier_contacts"]:
        foot = shapely.union_all(shapely.polygons(low.vertices[low.triangles, :2]))
        groups.append(tree.query(foot, predicate="intersects"))
    for _iteration in range(129):
        before = selected.copy()
        for group in groups:
            if selected[group].any():
                selected[group] = True
        if selected.sum() > 2048:
            raise ValueError("local terrain dependency face cap")
        if np.array_equal(before, selected):
            break
    else:
        raise ValueError("local pier dependency closure cap")
    affected = [g for g in groups if selected[g].any()]
    protected = np.unique(np.concatenate(affected)) if affected else np.empty(0, int)
    protected = protected[~plan["ground"][protected]]
    return selected, protected, shapes, len(affected)


def tessellate(old_xy, seed_ids, fixed_triangles, free_parts):
    triangles = shapely.get_parts(shapely.constrained_delaunay_triangles(free_parts))
    free = shapely.get_coordinates(triangles).reshape(-1, 4, 2)[:, :3]
    raw = np.vstack([fixed_triangles[:, :, :2].reshape(-1, 2), free.reshape(-1, 2)])
    unique, inverse = np.unique(raw, axis=0, return_inverse=True)
    seed = old_xy[seed_ids]
    xy, labels, weld = weld_vertices(
        np.vstack([seed, unique]), 1e-7, max_vertices=16384, max_pairs=65536
    )
    if not np.array_equal(xy[labels[: len(seed)]], seed):
        raise ValueError("local weld changed an immutable cached terrain vertex")
    faces = labels[len(seed) :][inverse].reshape(-1, 3)
    faces, note = conform_triangles(xy, faces, max_candidates=2000000, max_faces=32768)
    p = xy[faces]
    a, b = p[:, 1] - p[:, 0], p[:, 2] - p[:, 0]
    cross = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
    faces, cross = faces[cross != 0], cross[cross != 0]
    faces[cross < 0] = faces[cross < 0, ::-1]
    used, mapped = np.unique(faces, return_inverse=True)
    return (
        xy[used],
        mapped.reshape(-1, 3),
        {
            "weld": weld,
            "conformance": note,
            "raw_xy_bytes": raw.nbytes,
            "unique_xy_bytes": unique.nbytes,
            "raw_to_unique_bytes": inverse.nbytes,
            "unused_vertices_removed": len(xy) - len(used),
        },
    )


def sampled_pins(xy, references):
    values = np.full(len(xy), np.nan)
    for mesh in references:
        heights = (
            SurfaceSampler(mesh, xy_tolerance_m=1e-7)
            .sample(xy, max_queries=16384, max_candidates=2000000)
            .points_enu_m[:, 2]
        )
        good = np.isfinite(heights)
        both = good & np.isfinite(values)
        if np.max(abs(values[both] - heights[both]), initial=0) > 1e-7:
            raise ValueError("incompatible fixed terrain reference heights")
        values[good] = heights[good]
    return values


def prepare(state, data, ground, recipe, sample):
    old = state["plan"]
    old_xyz = np.c_[old["xy"], state["heights"]]
    new_bed = data["surface_vertices"] - [0, 0, 0.3]
    new_tri = new_bed[data["surface_faces"]]
    footprint = shapely.union_all(shapely.polygons(new_tri[:, :, :2]))
    chosen, protected_ids, shapes, affected = selection(state, footprint)
    if not chosen.any():
        raise ValueError("new module lies outside cached terrain")
    region = shapely.union_all(shapes[chosen])
    if footprint.difference(region).area > 1e-7:
        raise ValueError("local terrain does not contain the whole new module")
    old_ground_ids = np.flatnonzero(chosen & old["ground"])
    protected = old_xyz[old["faces"][protected_ids]]
    protected_shape = shapely.union_all(shapely.polygons(protected[:, :, :2]))
    if protected_shape.intersection(footprint).area > 1e-7:
        raise ValueError("new module intersects immutable old pier ground")
    old_ground = old_xyz[old["faces"][old_ground_ids]]
    fixed_tri = np.vstack([old_ground, new_tri, protected])
    fixed_union = shapely.union_all(shapely.polygons(fixed_tri[:, :, :2]))
    free = shapely.get_parts(shapely.difference(shapes[chosen], fixed_union))
    xy, faces, work = tessellate(
        old["xy"], np.unique(old["faces"][chosen]), fixed_tri, free
    )
    polygons = shapely.polygons(xy[faces])
    if (
        not shapely.coverage_is_valid(polygons)
        or region.symmetric_difference(shapely.union_all(polygons)).area > 1e-7
    ):
        raise ValueError("local terrain lost original coverage")
    check_surface(xy, faces)
    actual_ground = shapely.union_all(
        shapely.polygons(np.vstack([old_ground, new_tri])[:, :, :2])
    )
    kinds = shapely.covers(actual_ground, shapely.point_on_surface(polygons))
    references = [
        Mesh(old_xyz, old["faces"][np.r_[old_ground_ids, protected_ids]]),
        Mesh(new_bed, data["surface_faces"]),
    ]
    values = sampled_pins(xy, references)
    boundary = shapely.dwithin(region.boundary, shapely.points(xy), 1e-7)
    original_ids = {tuple(p): i for i, p in enumerate(old["xy"])}
    try:
        boundary_ids = [original_ids[tuple(p)] for p in xy[boundary]]
    except KeyError as error:
        raise ValueError("new vertex on immutable outer terrain boundary") from error
    values[boundary] = state["heights"][boundary_ids]
    if not np.isfinite(values[np.unique(faces[kinds])]).all():
        raise ValueError("whole new or retained ground lacks fixed heights")
    plan = {
        "xy": xy,
        "faces": faces,
        "ground": kinds,
        "targets": sample(xy),
        "bed": values,
        "boundary": boundary,
        "precision_mode": np.array("legacy"),
        "variable_mode": np.array("fixed_displacement"),
        "bridge_reserve_m": np.array(recipe["soil_bridge_reserve_m"]),
    }
    note = {
        "old_selected_faces": int(chosen.sum()),
        "protected_parent_faces": len(protected_ids),
        "affected_old_piers": affected,
        "points": len(xy),
        "faces": len(faces),
        "boundary_vertices": int(boundary.sum()),
        "finite_pins": int(np.isfinite(values).sum()),
        "work": work,
    }
    return plan, chosen, note


def assemble(state, local, heights, chosen):
    old, old_h = state["plan"], state["heights"]
    if heights.min() < old_h.min():
        raise ValueError("new field would change the cached terrain bottom datum")
    count = len(old_h)
    xy, lookup = np.unique(
        np.vstack([old["xy"], local["xy"]]), axis=0, return_inverse=True
    )
    before, after = lookup[:count], lookup[count:]
    h, target, bed = np.empty(len(xy)), np.empty(len(xy)), np.full(len(xy), np.nan)
    h[before], target[before], bed[before] = old_h, old["targets"], old["bed"]
    h[after], target[after], bed[after] = heights, local["targets"], local["bed"]
    faces = np.vstack([before[old["faces"][~chosen]], after[local["faces"]]])
    kinds = np.r_[old["ground"][~chosen], local["ground"]]
    used, mapped = np.unique(faces, return_inverse=True)
    xy, h, target, bed, faces = (
        xy[used],
        h[used],
        target[used],
        bed[used],
        mapped.reshape(-1, 3),
    )
    original = np.c_[old["xy"], old_h][old["faces"][~chosen]]
    replacement = np.c_[xy, h][faces[: len(original)]]
    if original.tobytes() != replacement.tobytes():
        raise ValueError("local replacement changed an outside cached facet")
    if len(xy) > 16384 or len(faces) > 32768:
        raise ValueError("assembled terrain diagnostic cap")
    plan = {
        "xy": xy,
        "faces": faces,
        "ground": kinds,
        "targets": target,
        "bed": bed,
        "bridge_reserve_m": local["bridge_reserve_m"],
    }
    body, parts = retaining_body(dict(plan, targets=h))
    return (
        plan,
        h,
        body,
        parts,
        {
            "outside_xyz_triangles_byte_exact": len(original),
            "explicit_global_xy_label_bytes": lookup.nbytes + xy.nbytes,
            "explicit_outside_xyz_compare_bytes": original.nbytes + replacement.nbytes,
        },
    )


def fit(plan, upper, footprint):
    heights, roles, note = solve(plan, upper, footprint)
    if heights is None:
        raise ValueError("bounded local terrain solve failed: " + str(note))
    return heights, roles, note
