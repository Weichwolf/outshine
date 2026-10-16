"""Metrical rectangular/chamfered window bays with open backs and three glazed sides."""

import math
from dataclasses import dataclass

import numpy as np
import shapely

from .modelling.prism_union import (
    PrismVolume,
    primitive_mesh,
    triangle_bound,
    union_prisms,
)


@dataclass(frozen=True, slots=True)
class WindowBay:
    width_m: float
    height_m: float
    depth_m: float = 0.65
    front_fraction: float = 0.65
    wall_m: float = 0.14
    back_m: float = 0.24
    floor_m: float = 0.16
    ceiling_m: float = 0.18
    sill_m: float = 0.85
    head_m: float = 2.25
    glass_m: float = 0.012

    def __post_init__(self):
        values = (
            self.width_m,
            self.height_m,
            self.depth_m,
            self.front_fraction,
            self.wall_m,
            self.back_m,
            self.floor_m,
            self.ceiling_m,
            self.sill_m,
            self.head_m,
            self.glass_m,
        )
        if any(
            isinstance(v, (bool, str, np.bool_)) or not math.isfinite(v) or v <= 0
            for v in values
        ):
            raise ValueError("Window bay dimensions require positive finite metres")
        if not (
            0.5 <= self.front_fraction <= 1
            and self.glass_m < self.wall_m < self.depth_m / 3
            and self.floor_m
            < self.sill_m
            < self.head_m
            < self.height_m - self.ceiling_m
            and self.height_m - self.ceiling_m - self.floor_m >= 1.9
            and self.width_m - 2 * self.wall_m > 0.6
        ):
            raise ValueError(
                "Window bay leaves insufficient wall, glazing or clear height"
            )
        area = (
            self.width_m * (self.back_m + self.depth_m)
            - self.width_m * (1 - self.front_fraction) * self.depth_m / 2
        )
        if not math.isfinite(area * self.height_m) or not (
            self.width_m / 2 - self.wall_m < self.width_m / 2
            and -self.depth_m < -self.depth_m + self.wall_m
            and self.height_m - self.ceiling_m < self.height_m
        ):
            raise ValueError(
                "Window bay dimensions are unresolved at their coordinate scale"
            )


def sections(plan):
    """Local ENU: open rear at Y=back, outside at negative Y, up Z; no source footprint."""
    if not isinstance(plan, WindowBay):
        raise TypeError("WindowBay required")
    half, front = plan.width_m / 2, plan.width_m * plan.front_fraction / 2
    path = np.array(
        [
            [-half, plan.back_m],
            [-half, 0],
            [-front, -plan.depth_m],
            [front, -plan.depth_m],
            [half, 0],
            [half, plan.back_m],
        ]
    )
    outer = shapely.Polygon(path)
    wall = shapely.LineString(path).buffer(plan.wall_m, single_sided=True, join_style=2)
    if not outer.is_valid or not wall.is_valid or wall.difference(outer).area > 1e-10:
        raise ValueError("Window bay wall offset is unresolved")
    cuts, panes = [], []
    for index in (1, 2, 3):
        a, b = path[index : index + 2]
        delta = b - a
        length = float(np.linalg.norm(delta))
        tangent = delta / length
        inward = np.array([-tangent[1], tangent[0]])
        margin = 2 * plan.wall_m
        if length <= 2 * margin + 0.04:
            raise ValueError("Window bay side is too short for positive framed glazing")
        lo, hi = a + margin * tangent, b - margin * tangent
        cuts.append(
            shapely.Polygon(
                [
                    lo - plan.wall_m * inward,
                    hi - plan.wall_m * inward,
                    hi + 2 * plan.wall_m * inward,
                    lo + 2 * plan.wall_m * inward,
                ]
            )
        )
        low, high = (plan.wall_m - plan.glass_m) / 2, (plan.wall_m + plan.glass_m) / 2
        panes.append(
            shapely.Polygon(
                [
                    lo + low * inward,
                    hi + low * inward,
                    hi + high * inward,
                    lo + high * inward,
                ]
            )
        )
    posts = wall.difference(shapely.union_all(cuts))
    inside = outer.difference(wall)
    if inside.is_empty or not inside.is_valid or posts.is_empty or not posts.is_valid:
        raise ValueError("Window bay has no resolved interior or frame posts")
    if any(
        p.difference(outer).area > 1e-10 or p.intersection(posts).area > 1e-10
        for p in panes
    ):
        raise ValueError("Window bay glazing leaves its framed opening")
    return {
        "path": path,
        "outer": outer,
        "wall": wall,
        "posts": posts,
        "inside": inside,
        "panes": tuple(panes),
    }


def volumes(plan):
    """Touching closed components; roles0 wall/frame,1 floor,2 ceiling,3 glazing."""
    shape = sections(plan)
    levels = (
        ("floor", shape["outer"], 0.0, plan.floor_m, 1),
        ("sill", shape["wall"], plan.floor_m, plan.sill_m, 0),
        ("posts", shape["posts"], plan.sill_m, plan.head_m, 0),
        ("head", shape["wall"], plan.head_m, plan.height_m - plan.ceiling_m, 0),
        ("ceiling", shape["outer"], plan.height_m - plan.ceiling_m, plan.height_m, 2),
    )
    result = [PrismVolume(*values) for values in levels]
    result.extend(
        PrismVolume(f"glass-{i}", p, plan.sill_m, plan.head_m, 3)
        for i, p in enumerate(shape["panes"])
    )
    return tuple(result)


def bay_surface(plan, *, method="parts", max_triangles=1024):
    """Owned closed component/union surfaces; no placement, room planner or LOD admission."""
    if (
        type(max_triangles) is not int
        or max_triangles < 1
        or method not in ("union", "parts")
    ):
        raise ValueError("Declared bay method and positive triangle capacity required")
    sources = volumes(plan)
    if sum(triangle_bound(source) for source in sources) > max_triangles:
        raise ValueError("Window bay input exceeds triangle capacity before expansion")
    if method == "union":
        import manifold3d as manifold

        result = union_prisms(sources, max_triangles=max_triangles)
        product = {
            "vertices": result.mesh["vertices"].copy(),
            "faces": result.mesh["faces"].astype(np.uint32),
            "roles": result.mesh["materials"].astype(np.uint8),
        }
        _, first, inverse = np.unique(
            result.vertex_roots, return_index=True, return_inverse=True
        )
        targets = first[inverse]
        ids = np.arange(len(targets), dtype=np.uint64)
        selected = ids != targets
        replay = manifold.Manifold(
            manifold.Mesh64(
                product["vertices"],
                product["faces"].astype(np.uint64),
                merge_from_vert=ids[selected],
                merge_to_vert=targets[selected].astype(np.uint64),
            )
        )
        if (
            replay.status() != manifold.Error.NoError
            or abs(replay.volume() - result.volume_m3) > 1e-9
        ):
            raise ValueError(
                "Window bay union loses topology or volume on mesh roundtrip"
            )
        return product
    positions, faces, roles, parts, offset = [], [], [], [], 0
    for index, source in enumerate(sources):
        points, indices = primitive_mesh(source)
        positions.append(points)
        faces.append(indices + offset)
        roles.append(np.full(len(indices), source.material, dtype=np.uint8))
        parts.append(np.full(len(indices), index, dtype=np.uint8))
        offset += len(points)
    return {
        "vertices": np.concatenate(positions),
        "faces": np.concatenate(faces).astype(np.uint32),
        "roles": np.concatenate(roles),
        "parts": np.concatenate(parts),
    }


def bay_skin(plan, *, max_triangles=128):
    """External render surfaces only; open back, no inner walls/floor or collision claim.

    Retained surfaces coincide with the physical components, including outer
    glazing and reveals. Dropped interior surfaces can affect light/shadows;
    this is no radiance/visibility or general LOD admission.
    """
    if type(max_triangles) is not int or max_triangles < 86:
        raise ValueError("Bay skin needs86 triangle capacity before expansion")
    shape = sections(plan)
    points, roles = [], []

    def quad(p, role):
        q = np.asarray(p)
        points.extend(q[[0, 1, 2, 0, 2, 3]])
        roles.extend((role, role))

    for index, (a, b) in enumerate(
        zip(shape["path"][:-1], shape["path"][1:], strict=True)
    ):
        length = float(np.linalg.norm(b - a))
        t = (b - a) / length
        inward = np.array([-t[1], t[0]])

        def point(s, z, depth=0, a=a, t=t, inward=inward):
            return np.r_[a + t * s + inward * depth, z]

        def panel(lo, hi, bottom, top, role, depth=0):
            quad(
                [
                    point(lo, bottom, depth),
                    point(hi, bottom, depth),
                    point(hi, top, depth),
                    point(lo, top, depth),
                ],
                role,
            )

        for low, high, role in (
            (0, plan.floor_m, 1),
            (plan.floor_m, plan.height_m - plan.ceiling_m, 0),
            (plan.height_m - plan.ceiling_m, plan.height_m, 2),
        ):
            if index in (1, 2, 3) and role == 0:
                margin = 2 * plan.wall_m
                panel(0, length, low, plan.sill_m, role)
                panel(0, length, plan.head_m, high, role)
                panel(0, margin, plan.sill_m, plan.head_m, role)
                panel(length - margin, length, plan.sill_m, plan.head_m, role)
                depth = (plan.wall_m - plan.glass_m) / 2
                panel(margin, length - margin, plan.sill_m, plan.head_m, 3, depth)
                for z, reverse in ((plan.sill_m, False), (plan.head_m, True)):
                    p = [
                        point(margin, z),
                        point(length - margin, z),
                        point(length - margin, z, depth),
                        point(margin, z, depth),
                    ]
                    quad(p[::-1] if reverse else p, 0)
                for s, reverse in ((margin, True), (length - margin, False)):
                    p = [
                        point(s, plan.sill_m),
                        point(s, plan.head_m),
                        point(s, plan.head_m, depth),
                        point(s, plan.sill_m, depth),
                    ]
                    quad(p[::-1] if reverse else p, 0)
            else:
                panel(0, length, low, high, role)
    for triangle in shapely.get_parts(
        shapely.constrained_delaunay_triangles(shape["outer"])
    ):
        xy = np.asarray(triangle.exterior.coords)[:3]
        u, v = xy[1] - xy[0], xy[2] - xy[0]
        if u[0] * v[1] - u[1] * v[0] < 0:
            xy = xy[::-1]
        for z, p, role in ((0, xy[::-1], 1), (plan.height_m, xy, 2)):
            points.extend(np.c_[p, np.full(3, z)])
            roles.append(role)
    return {
        "vertices": np.asarray(points),
        "faces": np.arange(len(points), dtype=np.uint32).reshape(-1, 3),
        "roles": np.asarray(roles, dtype=np.uint8),
    }
