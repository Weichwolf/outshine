"""Bounded actual source-bound end decks; no whole-bridge or World authority."""

from dataclasses import asdict, dataclass

import numpy as np
import shapely
from bound_bridge_end_surface import build_end_surface
from bridge_end_body_space import occupied_space
from outshine_ref.contracts import Mesh, identifier
from outshine_ref.transport import close_surface
from outshine_ref.transport.surface import _sorted_boundary


@dataclass(frozen=True)
class BodyRecipe:
    depth_m: float = 0.25
    headroom_m: float = 0.0
    maximum_vertices: int = 512
    maximum_faces: int = 2048
    maximum_volumes: int = 256

    def __post_init__(self):
        if (
            not np.isfinite(self.depth_m)
            or not 0.05 <= self.depth_m <= 2
            or not np.isfinite(self.headroom_m)
            or not 0 <= self.headroom_m <= 8
            or any(
                type(v) is not int or not 1 <= v <= cap
                for v, cap in (
                    (self.maximum_vertices, 16384),
                    (self.maximum_faces, 8192),
                    (self.maximum_volumes, 8192),
                )
            )
        ):
            raise ValueError(
                "Explicit bounded vertical deck depth/headroom and capacities required"
            )


def body_geometry(surface, recipe):
    if (
        not isinstance(surface, Mesh)
        or not len(surface.triangles)
        or 2 * len(surface.vertices) > recipe.maximum_vertices
        or 2 * len(surface.triangles) > recipe.maximum_faces
    ):
        raise ValueError("Deck body input/output budget")
    if abs(surface.vertices).max() > 1e6:
        raise ValueError("Bounded metric deck source frame required")
    boundary = _sorted_boundary(surface.triangles)
    if 2 * (len(surface.triangles) + len(boundary)) > recipe.maximum_faces:
        raise ValueError("Deck closed face budget")
    points = surface.vertices[surface.triangles]
    normals = np.cross(points[:, 1] - points[:, 0], points[:, 2] - points[:, 0])
    if (normals[:, 2] <= 0).any():
        raise ValueError("Upward finite deck faces required")
    polygons = shapely.polygons(points[:, :, :2])
    if not shapely.coverage_is_valid(polygons):
        raise ValueError("Conforming nonoverlapping deck coverage required")
    domain = shapely.coverage_union_all(polygons)
    if domain.geom_type != "Polygon" or not domain.is_valid or domain.area <= 0:
        raise ValueError("One valid connected deck domain required")
    body = close_surface(surface, recipe.depth_m)
    n, f = len(surface.vertices), len(surface.triangles)
    if (
        body.vertices[:n].tobytes() != surface.vertices.tobytes()
        or body.triangles[:f].tobytes() != surface.triangles.tobytes()
    ):
        raise ValueError("Deck closure changed original top surface")
    return (
        body,
        domain,
        {
            "top_vertices": n,
            "top_faces": f,
            "boundary_edges": len(boundary),
            "body_vertices": len(body.vertices),
            "body_faces": len(body.triangles),
            "body_array_bytes": body.vertices.nbytes + body.triangles.nbytes,
            "domain_area_m2": domain.area,
            "minimum_actual_vertical_depth_m": float(
                (body.vertices[:n, 2] - body.vertices[n:, 2]).min()
            ),
            "nominal_minimum_normal_thickness_m": float(
                (recipe.depth_m * normals[:, 2] / np.linalg.norm(normals, axis=1)).min()
            ),
            "depth_scope": "Vertical source-ENU recipe depth, not surveyed/structurally qualified thickness; upper XYZ/faces unchanged.",
        },
    )


def build_body(surface, owner, *, source_id, recipe=None, method="whole"):
    recipe = BodyRecipe() if recipe is None else recipe
    identifier(owner)
    identifier(source_id)
    if not isinstance(surface, Mesh):
        raise TypeError("One immutable deck surface required")
    if method not in ("whole", "faces"):
        raise ValueError("Known deck occupancy method required")
    # Reject face-volume budget before closing or querying the geometry.
    if method == "faces" and len(surface.triangles) > recipe.maximum_volumes:
        raise ValueError("Deck occupancy budget exhausted")
    body, domain, geometry = body_geometry(surface, recipe)
    rows, origins, records, work = occupied_space(
        surface,
        body,
        domain,
        owner,
        source_id,
        recipe.headroom_m,
        method,
        recipe.maximum_volumes,
    )
    return {"surface": surface, "body": body, "occupancies": rows, "domain": domain}, {
        "recipe": asdict(recipe),
        "geometry": geometry,
        "occupancy_work": work,
        "occupancy_sources": origins,
        "occupancy_records": records,
    }


def build_end_body(
    prepared, ground, index, group, *, recipe=None, method="whole", budget=None
):
    surface, report = build_end_surface(prepared, ground, index, group, budget=budget)
    cap = next(
        v["source_terminal"]
        for v in ground["terminal_values"]
        if v["source_terminal"]["group"] == group
    )
    members = [
        m
        for m in index.groups_by_node[cap["source_node"]].members
        if (m.edge[:2], m.physical_end, m.domain_node) == report["bridge_member"]
    ]
    if len(members) != 1:
        raise ValueError("Bridge body original source way is ambiguous")
    way = members[0].source_way
    product, body = build_body(
        surface,
        "bridge-end:" + report["group"],
        source_id=f"{report['group']}:StreetWay:{way}",
        recipe=recipe,
        method=method,
    )
    return product, report | {
        "state": "MetricEndBody",
        "body": body,
        "bridge_source_way": way,
        "pending": tuple(v for v in report["pending"] if v != "body_cross_section"),
        "scope": "Local closed source-bound deck and conservative physical/declared-use occupancy only. Terrain/crossing clearance, opposite side, whole bridge, supports/statics, Source/World/device admission remain open.",
    }
