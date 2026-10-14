"""A bounded cut-and-cover subway recipe attached to an explicitly chosen surface."""

import time

import numpy as np
import shapely
from bridge_continuation_recipe import unchanged_surface
from outshine_ref.contracts import Mesh
from outshine_ref.geometry import (
    ProjectedSurfaceBudget,
    SurfaceSampler,
    projected_height_surface,
)
from outshine_ref.transport import plan_vertical_transition, sample_vertical_transition
from overpass_clearance import vertical_clearance
from retained_surface import flat_domain, old_contacts, stitch, surface_domain

DEFAULT_BUDGET = ProjectedSurfaceBudget(time_limit_s=4.0)
HEADROOM_M = 4.5
WALL_M = 0.35
ROOF_M = 0.35
COVER_M = 2.0


def reference(fixture, previous):
    sources = fixture["sources"]
    if not 1 <= len(sources) <= 8 or len({s["source_id"] for s in sources}) != len(
        sources
    ):
        raise ValueError("bounded unique tunnel sources required")
    near, far, shapes = [], [], []
    sampler = SurfaceSampler(previous[0])
    for source in sources:
        props = source["properties"]
        if (
            props.get("subclass") != "subway"
            or props.get("brunnel") != "tunnel"
            or type(props.get("layer")) is not int
            or props["layer"] != -1
            or props.get("indoor")
        ):
            raise ValueError("explicit outside subway tunnel semantics required")
        if not 2 <= len(source["xy"]) <= 256 or not 3.0 <= source["width_m"] <= 8.0:
            raise ValueError("tunnel coordinate/clear-section budget")
        xy = np.asarray(source["xy"], float)
        if not np.isfinite(xy).all() or np.any(
            np.linalg.norm(np.diff(xy, axis=0), axis=1) <= 1e-6
        ):
            raise ValueError("finite noncollapsed original axes required")
        contact = source["contact"]
        index = contact["endpoint_index"]
        if (
            index not in (0, len(xy) - 1)
            or np.linalg.norm(xy[index] - contact["xyz_m"][:2]) > 1e-7
        ):
            raise ValueError("original endpoint contact required")
        selected = sampler.sample(xy[[index]])
        if (
            any(selected.reasons)
            or np.max(abs(selected.points_enu_m[0] - contact["xyz_m"])) > 1e-7
            or np.linalg.norm(selected.normals_enu[0, :2]) > 1e-10
        ):
            raise ValueError("chosen flat previous floor contact changed")
        near.append(xy[index])
        far.append(xy[-1 if index == 0 else 0])
        shapes.append(
            shapely.LineString(xy).buffer(
                source["width_m"] / 2,
                cap_style="flat",
                join_style="mitre",
                mitre_limit=2,
            )
        )
    origin = np.mean(near, axis=0)
    axis = np.mean(far, axis=0) - origin
    axis /= np.linalg.norm(axis)
    for source in sources:
        xy = np.asarray(source["xy"])
        if source["contact"]["endpoint_index"] != 0:
            xy = xy[::-1]
        if np.any(np.diff((xy - origin) @ axis) <= 1e-6):
            raise ValueError("folded tunnel needs a different reference recipe")
    inner = shapely.union_all(shapes)
    outer = shapely.union_all(
        [
            shapely.LineString(s["xy"]).buffer(
                s["width_m"] / 2 + WALL_M,
                cap_style="flat",
                join_style="mitre",
                mitre_limit=2,
            )
            for s in sources
        ]
    )
    heights = [s["contact"]["xyz_m"][2] for s in sources]
    if np.ptp(heights) > 1e-7:
        raise ValueError("one shared selected tunnel contact height required")
    return origin, axis, inner, outer, float(np.mean(heights))


def half_plane(origin, axis, station, domain):
    xy = shapely.get_coordinates(domain)
    lateral = (xy - origin) @ np.array([-axis[1], axis[0]])
    maximum = float(((xy - origin) @ axis).max()) + 1
    cross = np.array([-axis[1], axis[0]])
    return shapely.Polygon(
        [
            origin + s * axis + t * cross
            for s, t in (
                (station, lateral.min() - 1),
                (maximum, lateral.min() - 1),
                (maximum, lateral.max() + 1),
                (station, lateral.max() + 1),
            )
        ]
    )


def open_end_void(sources):
    shapes = []
    for source in sources:
        xy = np.asarray(source["xy"])
        if source["contact"]["endpoint_index"] != 0:
            xy = xy[::-1]
        direction = xy[-1] - xy[-2]
        direction /= np.linalg.norm(direction)
        extended = np.vstack((xy, xy[-1] + direction))
        shapes.append(
            shapely.LineString(extended).buffer(
                source["width_m"] / 2,
                cap_style="flat",
                join_style="mitre",
                mitre_limit=2,
            )
        )
    return shapely.union_all(shapes)


def field(fixture, previous, *, method, budget):
    origin, axis, inner, outer, contact_height = reference(fixture, previous)
    if (
        len(fixture["terrain"]["vertices"]) > 4096
        or len(fixture["terrain"]["triangles"]) > 8192
    ):
        raise ValueError("terrain input budget")
    terrain = Mesh(fixture["terrain"]["vertices"], fixture["terrain"]["triangles"])
    if (
        not shapely.dwithin(surface_domain(terrain), outer, 1e-8)
        or outer.difference(surface_domain(terrain)).area > 1e-7
    ):
        raise ValueError("selected terrain must cover the entire lining")
    floor_height = min(
        contact_height,
        float(terrain.vertices[:, 2].min()) - HEADROOM_M - ROOF_M - COVER_M,
    )
    xy = shapely.get_coordinates(outer)
    stations = (xy - origin) @ axis
    shared = outer.intersection(surface_domain(previous[0]))
    if shared.area <= 0:
        raise ValueError("positive-area existing floor interface required")
    start = max(
        float(((shapely.get_coordinates(shared) - origin) @ axis).max()) + 2, 2.0
    )
    available = float(stations.max()) - start
    length = available * 0.6
    if available - length < 60:
        raise ValueError("insufficient buried length after portal")
    profile, report = plan_vertical_transition(
        contact_height,
        floor_height,
        length,
        grade=0.024,
        second_derivative=0.002,
        method=method,
    )
    if profile is None:
        raise ValueError("tunnel_" + report["reason"])
    sample, sampling = sample_vertical_transition(
        profile, height_error_m=0.009, max_vertices=budget.max_stations - 2
    )
    if sample is None:
        raise ValueError(sampling["reason"])
    table = np.vstack(
        (
            [stations.min() - 1, contact_height],
            np.c_[sample[:, 0] + start, sample[:, 1]],
            [stations.max() + 1, floor_height],
        )
    )
    buried = outer.intersection(half_plane(origin, axis, start + length, outer))
    if buried.area <= 0:
        raise ValueError("empty portal lining domain")
    return (origin, axis, inner, outer, terrain, profile, table, buried), {
        "profile": report,
        "sampling": sampling,
        "contact_height_m": contact_height,
        "floor_height_m": floor_height,
        "transition_start_m": start,
        "transition_length_m": length,
        "roof_start_m": start + length,
        "minimum_cover_m": COVER_M,
        "clear_height_m": HEADROOM_M,
        "wall_thickness_m": WALL_M,
        "roof_thickness_m": ROOF_M,
        "far_clip_is_floor_contact": False,
        "analytic_height_continuity": "C1",
        "finite_face_continuity": "C0",
    }


def mesh_field(domain, origin, axis, table, budget):
    station = (shapely.get_coordinates(domain) - origin) @ axis
    selected = table[(table[:, 0] >= station.min()) & (table[:, 0] <= station.max())]
    heights = np.r_[
        np.interp([station.min(), station.max()], table[:, 0], table[:, 1]),
        selected[:, 1],
    ]
    if np.ptp(heights) <= 1e-10:
        flat = flat_domain(domain, budget=budget)
        mesh = Mesh(flat.vertices + [0, 0, float(heights[0])], flat.triangles)
        error = float(surface_domain(mesh).symmetric_difference(domain).area)
        boundary = float(surface_domain(mesh).hausdorff_distance(domain))
        if error > 1e-7 or boundary > 1e-8:
            raise ValueError("constant lining domain changed")
        note = {
            "valid": True,
            "constant_height_domain": True,
            "coverage_error_m2": error,
            "boundary_hausdorff_m": boundary,
            "faces": len(mesh.triangles),
        }
    else:
        mesh, note = projected_height_surface(
            domain, origin, axis, table, budget=budget
        )
    if mesh is None:
        raise ValueError(note["reason"] + ": " + note.get("detail", ""))
    xyz = mesh.vertices[mesh.triangles]
    gradients = np.linalg.solve(
        xyz[:, 1:, :2] - xyz[:, :1, :2], (xyz[:, 1:, 2] - xyz[:, :1, 2])[..., None]
    )[..., 0]
    maximum = float(np.linalg.norm(gradients, axis=1).max())
    if maximum > 0.024 + 1e-7:
        raise ValueError("whole tunnel floor grade exceeded")
    note["independent_maximum_gradient"] = maximum
    return mesh, note


def build(fixture, previous, *, method="cubic", budget=DEFAULT_BUDGET):
    began = time.monotonic()
    try:
        if budget.time_limit_s == 0:
            raise ValueError("tunnel_time_budget")
        model, field_note = field(fixture, previous, method=method, budget=budget)
        origin, axis, inner, outer, terrain, _profile, table, buried = model
        full_floor, floor_note = mesh_field(outer, origin, axis, table, budget)
        addition_domain = outer.difference(surface_domain(previous[0]))
        addition, _note = mesh_field(addition_domain, origin, axis, table, budget)
        top, stitch_note = stitch(
            previous[0],
            addition,
            outer.union(surface_domain(previous[0])),
            budget=budget,
        )
        xy = top.vertices[:, :2]
        pins, old_heights = old_contacts(previous[0], xy)
        heights = np.interp((xy - origin) @ axis, table[:, 0], table[:, 1])
        heights[pins] = old_heights[pins]
        top = Mesh(np.c_[xy, heights], top.triangles)
        preservation = unchanged_surface(previous[0], top)
        lining_domain = buried.difference(open_end_void(fixture["sources"]))
        walls, wall_note = mesh_field(lining_domain, origin, axis, table, budget)
        roof_floor, roof_note = mesh_field(buried, origin, axis, table, budget)
        roof = Mesh(
            roof_floor.vertices + [0, 0, HEADROOM_M + ROOF_M], roof_floor.triangles
        )
        headroom = vertical_clearance(
            full_floor, Mesh(roof.vertices - [0, 0, ROOF_M], roof.triangles)
        )
        if not headroom["valid"] or headroom["minimum_clearance_m"] < HEADROOM_M - 1e-7:
            raise ValueError("whole_tunnel_headroom")
        cover = vertical_clearance(roof, terrain)
        if not cover["valid"] or cover["minimum_clearance_m"] < COVER_M - 1e-7:
            raise ValueError("whole_roof_terrain_cover")
        if time.monotonic() - began > budget.time_limit_s * 6 + 1:
            raise ValueError("tunnel_total_time_budget")
        return {
            "top": top,
            "ground": previous[1],
            "floor": full_floor,
            "walls": walls,
            "roof": roof,
            "terrain": terrain,
            "inner": inner,
            "outer": outer,
            "buried": buried,
            "open_trench": outer.difference(buried),
            "field": (origin, axis, table),
        }, {
            "valid": True,
            "reason": "",
            "method": method,
            "preservation": preservation,
            "cover": cover,
            "headroom": headroom,
            "field": field_note,
            "floor": floor_note,
            "walls": wall_note,
            "roof": roof_note,
            "stitch": stitch_note,
            "total_ms": (time.monotonic() - began) * 1000,
            "used_source_ids": [s["source_id"] for s in fixture["sources"]],
            "scope": "Two pinned subway tunnel sources attach to explicitly chosen existing floors. Own cut-and-cover portal/lining recipe against selected coarse terrain; whole roof coverage and previous affine surfaces verified. Unplanned indoors, outside-world contacts, train routes, rail hardware and structural loads remain unproven.",
        }
    except (ValueError, shapely.errors.GEOSException) as failure:
        return {"top": previous[0], "ground": previous[1]}, {
            "valid": False,
            "valid_partial_base": True,
            "reason": str(failure),
            "used_source_ids": [],
            "method": method,
            "total_ms": (time.monotonic() - began) * 1000,
        }
