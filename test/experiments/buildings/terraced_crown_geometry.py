"""Shared roof plan, direct far skin and lazy closed near cap above open floors."""

import math
from itertools import pairwise

import numpy as np
import shapely
from demand import displacement_error
from outshine_ref.buildings.facade_frames import front
from outshine_ref.buildings.modelling import union_prisms
from outshine_ref.buildings.rooflight_panels import triangle_surface
from terraced_crown_plan import ROOF_MATERIAL
from terraced_facade_prepare import positions
from terraced_facade_surface import joined
from terraced_mass_frame import placed
from terraced_terrace_geometry import TerraceProducts
from terraced_terrace_plan import plan as terrace_plan
from tile_terraced import indices


def flat(polygon, height, material, *, reverse=False):
    points = np.concatenate(
        [
            front(shapely.orient_polygons(p))
            for p in shapely.get_parts(polygon)
            if p.geom_type == "Polygon" and not p.is_empty
        ]
    )
    points[:, :, 2] = height
    if reverse:
        points = points[:, ::-1]
    points = points.reshape(-1, 3)
    return triangle_surface(
        {
            "vertices": points,
            "faces": np.arange(len(points), dtype=np.uint32).reshape(-1, 3),
        },
        material,
    )


def direct(crown):
    polygon = shapely.from_wkb(crown.domain_wkb)
    coping = next(p.volume.polygon for p in crown.parts if p.role == "coping")
    pieces = [
        flat(coping, crown.top_m, 10),
        flat(polygon.difference(coping), crown.top_m, ROOF_MATERIAL),
    ]
    points = []
    for ring in (polygon.exterior, *polygon.interiors):
        for a, b in pairwise(np.asarray(ring.coords)):
            quad = np.array(
                [
                    [*a, crown.deck_m],
                    [*b, crown.deck_m],
                    [*b, crown.top_m],
                    [*a, crown.top_m],
                ]
            )
            points.extend(quad[[0, 1, 2, 0, 2, 3]])
    points = np.asarray(points)
    pieces.append(
        triangle_surface(
            {
                "vertices": points,
                "faces": np.arange(len(points), dtype=np.uint32).reshape(-1, 3),
            },
            12,
        )
    )
    return joined(tuple(pieces))


def visible(crown, products):
    pieces = [flat(shapely.from_wkb(crown.deck_wkb), crown.deck_m, ROOF_MATERIAL)]
    for descriptor, product in zip(crown.parts, products, strict=True):
        if descriptor.role == "slab":
            continue
        mesh = product.mesh
        xyz = mesh["vertices"][mesh["faces"]]
        vertical = np.ptp(xyz[:, :, 2], axis=1) > 1e-10
        cross = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
        keep = vertical | (
            (cross[:, 2] > 0) if descriptor.role in ("coping", "service-cap") else False
        )
        pieces.append(
            triangle_surface(
                dict(mesh, faces=mesh["faces"][keep]), descriptor.volume.material
            )
        )
    coping = next(p.volume for p in crown.parts if p.role == "coping")
    pieces.append(
        flat(
            shapely.from_wkb(crown.coping_underside_wkb), coping.low_m, 10, reverse=True
        )
    )
    return joined(tuple(pieces))


def world_surface(mesh, core):
    result = dict(mesh, vertices=positions(mesh["vertices"], core))
    normals = np.array(mesh["normals"], copy=True)
    normals[:, :2] = normals[:, :1] * np.asarray(core.across_xy) + normals[
        :, 1:2
    ] * np.asarray(core.along_xy)
    result["normals"] = normals
    return result


def capless(mesh, height):
    triangles = mesh["vertices"][mesh["faces"]]
    cross = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    cap = (np.max(abs(triangles[:, :, 2] - height), axis=1) <= 1e-9) & (cross[:, 2] > 0)
    if not cap.any():
        raise ValueError("Expected highest body cap is missing from the composed draw")
    return dict(mesh, faces=indices(mesh)[~cap], materials=mesh["materials"][~cap])


class RoofedProducts:
    def __init__(self, plan):
        self.plan = plan
        self.draws = {}
        self.body_draws = {}
        self.body = TerraceProducts(terrace_plan(plan.body))
        self.roof_components, self.roof_skin = None, None
        self.coarse_roof = (
            world_surface(direct(plan.crown), plan.body.core)
            if plan.crown is not None
            else None
        )
        self.bounds = np.array(self.body.facade.bounds, copy=True)
        self.bounds[self.bounds[:, 2] == plan.body.top_m, 2] = plan.source_top_m

    def physical_roof(self):
        if self.plan.crown is None:
            return None
        if self.roof_components is None:
            local = tuple(union_prisms((p.volume,)) for p in self.plan.crown.parts)
            self.roof_skin = world_surface(
                visible(self.plan.crown, local), self.plan.body.core
            )
            self.roof_components = tuple(placed(p, self.plan.body.core) for p in local)
        return self.roof_components

    def select(self, view):
        decision = self.body.facade.select(view)
        if self.plan.crown is None:
            return decision
        radius = self.plan.source_top_m - self.plan.body.top_m
        coordinates = (self.bounds - view.eye) @ view.basis.T
        margin = (view.movement_m + max(0.35, radius)) * np.linalg.norm(
            view.planes[:, :3], axis=1
        )
        visible = not np.any(
            (coordinates @ view.planes[:, :3].T + view.planes[:, 3]).max(axis=0)
            + margin
            < 0
        )
        error = displacement_error(self.bounds, view, radius) if visible else None
        inside = self.plan.body.bottom_m <= view.eye[
            2
        ] <= self.plan.source_top_m and self.body.facade.source_polygon.covers(
            shapely.Point(view.eye[:2])
        )
        body_near = inside or decision["mode"] == "near"
        roof_near = inside or (visible and error > min(0.5, view.detail_error_px))
        return decision | {
            "mode": "near"
            if body_near and roof_near
            else "compact"
            if not body_near and not roof_near
            else "mixed",
            "body_mode": "near" if body_near else "compact",
            "roof_mode": "near" if roof_near else "compact",
            "possibly_visible": bool(visible),
            "roof_position_bound_px": error
            if error is not None and math.isfinite(error)
            else None,
            "unbounded_roof_position_error": error is not None
            and not math.isfinite(error),
            "inside_source": bool(inside),
        }

    def parts(self, body_near, roof_near):
        if body_near not in self.body_draws:
            body = self.body.draw(body_near)
            self.body_draws[body_near] = (
                capless(body, self.plan.body.top_m)
                if self.plan.crown is not None
                else dict(body, faces=indices(body))
            )
        body = self.body_draws[body_near]
        if self.plan.crown is None:
            return (body,)
        if roof_near:
            self.physical_roof()
        return body, self.roof_skin if roof_near else self.coarse_roof

    def resolve(self, view):
        decision = self.select(view)
        body_near = decision.get("body_mode", decision["mode"]) == "near"
        roof_near = decision.get("roof_mode", decision["mode"]) == "near"
        return self.parts(body_near, roof_near), decision

    def draw(self, near):
        if near not in self.draws:
            self.draws[near] = joined(self.parts(near, near))
        return self.draws[near]
