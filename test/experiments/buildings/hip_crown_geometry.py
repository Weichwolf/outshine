"""Existing CGA hip solids explicitly converted from Y-up into core-local ENU."""

from dataclasses import dataclass
from types import MappingProxyType

import numpy as np
import shapely
from outshine_ref.buildings.modelling import Shape, hip_footprints, prepare_hip
from outshine_ref.buildings.modelling.mesh_fans import connected_fans
from outshine_ref.buildings.modelling.prism_union import frozen
from outshine_ref.buildings.rooflight_panels import triangle_surface
from structural_shell_geometry import native
from terraced_crown_plan import ROOF_MATERIAL


@dataclass(frozen=True)
class ClosedHip:
    mesh: object
    vertex_roots: object
    volume_m3: float
    area_m2: float
    hip_plan: object
    chart_origin_xy: tuple
    chart_basis_xy: tuple


def enu(values):
    points = np.asarray(values)
    result = np.empty_like(points, order="C")
    result[:, 0] = points[:, 0]
    np.negative(points[:, 2], out=result[:, 1])
    result[:, 2] = points[:, 1]
    return result


def chart(source):
    mesh, face = source.emit(), source.geometry.polygons[0]
    points = enu(mesh["vertices"])[list(face.outer), :2]
    edges = np.roll(points, -1, axis=0) - points
    tangent = edges[np.argmax(np.linalg.norm(edges, axis=1))]
    tangent /= np.linalg.norm(tangent)
    return tuple(points[0]), (tuple(tangent), (-tangent[1], tangent[0]))


def build(crown):
    polygon = shapely.from_wkb(crown.domain_wkb)
    rise = crown.top_m - crown.deck_m
    source = Shape.from_polygon(polygon, elevation=crown.deck_m, material=ROOF_MATERIAL)
    intrinsic = hip_footprints(source)[0]
    plan = prepare_hip(intrinsic, crown.method, crown.limits)
    roof = source.roof_hip(
        rise,
        mode="byHeight",
        limits=crown.limits,
        max_cells=65536,
        prepared_plans=(plan,),
    )
    raw = dict(roof.emit())
    raw["vertices"] = enu(raw["vertices"])
    points, faces, duplicates, targets = connected_fans(
        raw, np.arange(len(raw["faces"]))
    )
    roots = np.arange(len(points), dtype=np.uint64)
    roots[duplicates] = targets
    mesh = {
        "vertices": points[:, :3],
        "uv": points[:, 3:5],
        "faces": faces.astype(np.int32),
        "materials": raw["materials"],
    }
    solid = native(mesh, roots)
    if solid.volume() <= 0:
        raise ValueError("Hip crown lacks a positive closed volume")
    low, high = mesh["vertices"][:, 2].min(), mesh["vertices"][:, 2].max()
    if low < crown.deck_m - 1e-9 or abs(high - crown.top_m) > 1e-9:
        raise ValueError("Hip crown leaves its reserved source-height interval")
    origin, basis = chart(source)
    product = ClosedHip(
        MappingProxyType({k: frozen(v) for k, v in mesh.items()}),
        frozen(roots),
        float(solid.volume()),
        float(solid.surface_area()),
        plan,
        origin,
        basis,
    )
    xyz = mesh["vertices"][mesh["faces"]]
    cross = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
    skin = triangle_surface(
        dict(mesh, faces=mesh["faces"][cross[:, 2] > 0]), ROOF_MATERIAL
    )
    return (product,), skin
