"""Plan-first flat height profiles; footprint gaps and source identity stay explicit."""

from dataclasses import dataclass

import numpy as np
import shapely
from building_positions import building_positions, millimetres
from building_roof_domains import triangle_orientation
from column_roof_patches import bottom_m, roof_patches
from packed_normals import pack
from prepared_material_fields import at_local


@dataclass(frozen=True)
class HeightPart:
    source: int
    part: int
    surface: dict
    shape: dict
    footprint: object
    patches: tuple
    bottom: float
    roof_low: float
    roof_high: float

    @property
    def height_change_m(self):
        return self.roof_high - self.roof_low


def plan_source(source, surface):
    """Read original forms only; either all parts are known or the source delegates."""
    if surface is None or not surface["shapes"]:
        raise ValueError("Original source has no supported nonempty form")
    result = []
    for part, shape in enumerate(surface["shapes"]):
        polygon = shapely.Polygon(shape["ring"], shape["holes"])
        if polygon.is_empty or not polygon.is_valid or polygon.area <= 0:
            raise ValueError("Invalid original footprint")
        patches = tuple(roof_patches(shape))
        if not patches:
            raise ValueError("No supported original roof domains")
        coverage = shapely.union_all([p for p, _ in patches])
        if shapely.area(shapely.symmetric_difference(coverage, polygon)) > 1e-7 * max(
            polygon.area, 1
        ):
            raise ValueError("Original planar domains do not cover the footprint")
        values = np.concatenate(
            [
                shapely.get_coordinates(p) @ plane[:2] + plane[2]
                for p, plane in patches
                if p.area > 0
            ]
        )
        bottom = bottom_m(surface, shape)
        low, high = float(values.min()), float(values.max())
        if not np.isfinite([bottom, low, high]).all() or low <= bottom:
            raise ValueError("Nonpositive or nonfinite original height profile")
        result.append(
            HeightPart(
                source, part, surface, shape, polygon, patches, bottom, low, high
            )
        )
    return tuple(result)


def cap_triangles(polygon):
    """No vertex deletion, hole filling or convex hull approximation."""
    result = []
    for region in shapely.get_parts(polygon):
        if region.geom_type != "Polygon" or region.area <= 0:
            continue
        for triangle in shapely.get_parts(
            shapely.constrained_delaunay_triangles(region)
        ):
            if not region.covers(triangle):
                raise ValueError("Cap triangulation leaves its original footprint")
            xy = np.asarray(triangle.exterior.coords)[:3]
            cross = triangle_orientation(xy)
            if cross == 0:
                raise ValueError("Degenerate cap triangle")
            result.append(xy if cross > 0 else xy[[0, 2, 1]])
    return np.asarray(result).reshape(-1, 3, 2)


class ProfileMesh:
    """Full diagnostic draw fields, separate from compact native storage accounting."""

    def __init__(self):
        self.rows = {
            key: [] for key in ["positions", "uv", "normal", "colour", "metres"]
        }
        self.bindings = []

    def append(self, part, local, normal, face, ring=-1, edge=-1):
        local = np.asarray(local, float).reshape(-1, 3, 3)
        snapped = millimetres(local)
        crosses = np.cross(snapped[:, 1] - snapped[:, 0], snapped[:, 2] - snapped[:, 0])
        if (
            not np.isfinite(local).all()
            or (np.linalg.norm(crosses, axis=1) < 1e-12).any()
        ):
            raise ValueError("Profile triangle collapses under original local rounding")
        surface = part.surface
        frame = np.array([surface[k] for k in ["east", "north", "up"]])
        positions = (
            building_positions(local, surface, recipe="snapped-f32")
            .astype(np.float32)
            .reshape(-1, 3)
        )
        uv, metres = at_local(part.shape, face, local.reshape(-1, 3), gable=False)
        normal = np.asarray(normal) @ frame
        words = np.repeat(pack(normal.reshape(-1, 3)), 3)
        if len(words) == 3 and len(local) > 1:
            words = np.tile(words, len(local))
        colour = surface["wall_colour"] if face != 1 else None
        colour = (
            np.ones(4, np.float32)
            if colour is None
            else np.r_[np.asarray(colour, np.float32), np.float32(1)]
        )
        values = {
            "positions": positions,
            "uv": uv,
            "normal": words,
            "colour": np.tile(colour, (len(positions), 1)),
            "metres": metres,
        }
        for key, value in values.items():
            self.rows[key].append(value)
        self.bindings.extend([[part.source, part.part, face, ring, edge]] * len(local))

    def finish(self):
        if not self.bindings:
            raise ValueError("No generated profile triangles")
        fields = {key: np.concatenate(values) for key, values in self.rows.items()}
        # Share complete vertex words, retaining categorical and UV seams.
        dtype = np.dtype(
            [
                ("positions", "<f4", 3),
                ("uv", "<f4", 2),
                ("normal", "<u4"),
                ("colour", "<f4", 4),
                ("metres", "<f4", 2),
            ]
        )
        rows = np.empty(len(fields["positions"]), dtype=dtype)
        for key, value in fields.items():
            rows[key] = value
        unique, inverse = np.unique(
            rows.view(np.dtype((np.void, dtype.itemsize))), return_inverse=True
        )
        records = unique.view(dtype)
        return {key: records[key].copy() for key in fields} | {
            "indices": inverse.astype(np.uint32).reshape(-1, 3),
            "profile_binding": np.asarray(self.bindings, np.int32),
        }


def emit_part(mesh, part):
    top_triangles = cap_triangles(part.footprint)
    bottom_triangles = top_triangles[:, [0, 2, 1]]
    floor = np.dstack(
        (bottom_triangles, np.full(bottom_triangles.shape[:2], part.bottom))
    )
    mesh.append(part, floor, [0, 0, -1], 0)
    roof = np.dstack((top_triangles, np.full(top_triangles.shape[:2], part.roof_high)))
    mesh.append(part, roof, [0, 0, 1], 1)
    face = 2
    for ring_index, values in enumerate([part.shape["ring"], *part.shape["holes"]]):
        ring = np.asarray(values, float)
        signed = np.sum(
            ring[:, 0] * np.roll(ring[:, 1], -1) - ring[:, 1] * np.roll(ring[:, 0], -1)
        )
        for edge, (a, b) in enumerate(
            zip(ring, np.roll(ring, -1, axis=0), strict=True)
        ):
            if np.linalg.norm(b - a) == 0:
                raise ValueError("Zero-length original wall edge")
            quad = np.array(
                [
                    [*a, part.bottom],
                    [*b, part.bottom],
                    [*b, part.roof_high],
                    [*a, part.roof_high],
                ]
            )
            triangles = quad[[[0, 1, 2], [0, 2, 3]]]
            if (signed > 0) != (ring_index == 0):
                triangles = triangles[:, [0, 2, 1]]
            normal = np.cross(
                triangles[0, 1] - triangles[0, 0], triangles[0, 2] - triangles[0, 0]
            )
            mesh.append(part, triangles, normal, face, ring_index, edge)
            face += 1


def emit(parts):
    mesh = ProfileMesh()
    for part in parts:
        emit_part(mesh, part)
    return mesh.finish()
