"""Complete source meshes and physical material fields for composed generators."""

from types import SimpleNamespace

import numpy as np
from axis_surface import axis_mesh, profiled_axis_mesh
from bark_coordinates import axis_chart
from blade_surface import Blade
from generator_specs import FrondShape, TubeShape
from needle_section import section_vertex_count
from needle_surface import Needle
from spatial_axis import SpatialAxis

SOURCE_GEOMETRY_VERSION = 3


def tube_resolution(shape, detail):
    segments, sides = (48, 12) if detail == "near" else (12, 6)
    if (
        isinstance(shape, TubeShape)
        and shape.bend == 0
        and np.all(shape.radius.table == shape.radius.table[0])
    ):
        segments = 2
    return segments, sides


def source_triangles(shape, detail="near"):
    if detail not in ("near", "coarse"):
        raise ValueError("declared source detail required")
    fine = detail == "near"
    if isinstance(shape, Needle):
        return (
            2
            * section_vertex_count(shape.section, 16 if fine else 8)
            * (32 if fine else 16)
        )
    if isinstance(shape, (TubeShape, SpatialAxis)):
        segments, sides = tube_resolution(shape, detail)
        return 2 * sides * (segments + 1)
    if isinstance(shape, Blade):
        segments, across = (48, 4) if fine else (12, 1)
        extra = int(shape.petiole > 0 and shape.base_fill > 0)
        return 4 * across * (segments + extra) - 2 * across * (
            1 + int(shape.base_fill == 0)
        )
    if isinstance(shape, FrondShape):
        segments, across = (48, 4) if fine else (12, 1)
        leaves = (2 * shape.pattern.pairs + 1) * (4 * across * segments - 2 * across)
        wood = 2 * (8 if fine else 6) * (segments + 1)
        return leaves + wood
    raise ValueError("implemented source geometry required")


def tube_fields(shape, mesh):
    state = SimpleNamespace(
        origin=np.zeros(3), frame=np.eye(3), length=shape.length, bend=shape.bend
    )
    radius = SimpleNamespace(base_radius=1.0, tip_radius=1.0)
    chart = axis_chart(state, radius, mesh["vertices"])[0]
    return dict(mesh) | {
        "bark_coordinates": chart,
        "bark_radius": shape.radius.evaluate(chart[:, 2] / shape.length),
    }


def source_parts(shape, detail="near"):
    if detail not in ("near", "coarse"):
        raise ValueError("declared source detail required")
    fine = detail == "near"
    if isinstance(shape, Needle):
        return {"leaf": shape.mesh(32 if fine else 16, 16 if fine else 8)}
    if isinstance(shape, SpatialAxis):
        return {"wood": shape.mesh(48 if fine else 12, 12 if fine else 6)}
    if isinstance(shape, TubeShape):
        mesh = profiled_axis_mesh(
            shape.length,
            shape.bend,
            1.0,
            shape.radius,
            *tube_resolution(shape, detail),
        )
        return {"wood": tube_fields(shape, mesh)}
    if isinstance(shape, Blade):
        return {"leaf": shape.mesh(48 if fine else 12, 4 if fine else 1)}
    if isinstance(shape, FrondShape):
        leaf = shape.pattern.mesh(48 if fine else 12, 4 if fine else 1)
        if leaf is None:
            raise ValueError("complete frond source exceeds the mesh budget")
        source = axis_mesh(
            shape.length, 0, 0.004, 0.0007, 48 if fine else 12, 8 if fine else 6
        )
        wood = shape.guide.deform(source)
        vertices = source["vertices"]
        t = vertices[:, 1] / shape.length
        width, height = shape.guide.width.evaluate(t), shape.guide.height.evaluate(t)
        q = vertices[:, 0] * width
        d = vertices[:, 2] * height + shape.guide.fold_per_m * q**2
        wood["bark_coordinates"] = np.column_stack((q, d, vertices[:, 1]))
        wood["bark_radius"] = (0.004 + t * (0.0007 - 0.004)) * np.sqrt(width * height)
        return {"leaf": shape.guide.deform(leaf), "wood": wood}
    raise ValueError("implemented source geometry required")
