"""Original part/vertex inputs at whole-source hits; no BRDF or World admission."""

from dataclasses import dataclass
from types import MappingProxyType

import numpy as np
from native_capture_meshes import fields_at
from triangle_columns import TriangleColumns, immutable, world_rays


@dataclass(frozen=True)
class MaterialInputs:
    geometry: TriangleColumns
    source: object
    part_ends: np.ndarray
    materials: np.ndarray

    @property
    def attribute_payload_bytes(self):
        return (
            sum(
                self.source[k].nbytes
                for k in ("vertex_normal", "uv", "metres", "colour")
            )
            + self.part_ends.nbytes
            + self.materials.nbytes
        )


def prepare(field, source):
    """Snapshot verified original attributes in the unchanged global vertex order."""
    if (
        not isinstance(field, TriangleColumns)
        or not np.array_equal(field.positions, source["positions"])
        or not np.array_equal(field.triangles, source["indices"])
    ):
        raise ValueError("Attributes must belong to the identical whole geometry")
    parts, data = source["parts"], source["attributes"]
    if not 1 <= len(parts) <= 256 or len(data) != len(parts):
        raise ValueError("Bounded original part/attribute inventory required")
    vertex = face = 0
    for part, attributes in zip(parts, data, strict=True):
        count, triangles = part["vertices"], part["triangles"]
        if (
            type(count) is not int
            or type(triangles) is not int
            or count < 1
            or triangles < 1
            or type(part["material"]) is not int
            or not 0 <= part["material"] < 2**32
        ):
            raise ValueError("Original counts and material indices required")
        local = field.triangles[face : face + triangles].astype(np.int64) - vertex
        if not local.size or local.min() < 0 or local.max() >= count:
            raise ValueError("Original part/vertex offsets changed")
        for name, width in (("normals", 3), ("uv", 2), ("uv1", 2), ("colours", 4)):
            a = attributes[name]
            if (
                a.dtype != np.float32
                or a.shape != (count, width)
                or not np.isfinite(a).all()
            ):
                raise ValueError("Original finite Float32 vertex attributes required")
        vertex += count
        face += triangles
    if vertex != len(field.positions) or face != len(field.triangles):
        raise ValueError("Complete original part coverage required")
    values = {"positions": field.positions, "indices": field.triangles}
    for target, original in (
        ("vertex_normal", "normals"),
        ("uv", "uv"),
        ("metres", "uv1"),
        ("colour", "colours"),
    ):
        values[target] = immutable(
            np.concatenate([d[original] for d in data]), np.float32
        )
    return MaterialInputs(
        field,
        MappingProxyType(values),
        immutable(np.cumsum([p["triangles"] for p in parts]), np.uint32),
        immutable([p["material"] for p in parts], np.uint32),
    )


def read(
    materials,
    world,
    points,
    raster,
    geometry_result,
    *,
    method="sparse",
    hits_most=4096,
):
    """Same fields_at interpolation; sparse selection precedes rays/output arrays.

    geometry_result must be the query of materials.geometry with these exact
    World/sample operands. This internal caller contract is not a live revision.
    Outputs are raw original-frame inputs, not decoded native material or radiance.
    """
    z, faces, evaluated, _ = geometry_result
    points = np.asarray(points, float)
    if (
        method not in ("sparse", "dense")
        or type(hits_most) is not int
        or not 0 <= hits_most <= 65536
        or points.ndim != 2
        or points.shape[1] != 2
        or len(points) > 65536
        or not np.isfinite(points).all()
        or z.shape != (len(points),)
        or faces.shape != z.shape
        or evaluated.shape != z.shape
        or faces.dtype.kind not in "iu"
        or evaluated.dtype != bool
        or not np.isfinite(z).all()
        or np.any((z < 0) | (z > 1))
        or np.any(faces < -1)
        or np.any(faces >= len(materials.geometry.triangles))
        or np.any(evaluated & ((faces >= 0) != (z > 0)))
    ):
        raise ValueError("Bounded coupled whole-source geometry results required")
    selected = np.flatnonzero(evaluated & (faces >= 0))
    work = {
        "requested_samples": len(points),
        "geometry_hits": len(selected),
        "unknown_geometry_samples": int((~evaluated).sum()),
        "requested_corner_rows": 3 * len(selected),
        "interpolated_corner_rows": 0,
        "attribute_payload_bytes": materials.attribute_payload_bytes,
        "geometry_payload_bytes": materials.geometry.payload_bytes,
        "runtime_admitted": False,
    }
    if len(selected) > hits_most:
        return {}, work | {
            "reason": "fragment_hit_budget",
            "unknown_attribute_hits": len(selected),
        }
    inverse = np.linalg.inv(world)
    hit_clip = (
        np.c_[
            points[selected] / np.asarray(raster) * [2, -2] + [-1, 1],
            z[selected],
            np.ones(len(selected)),
        ]
        @ inverse.T
    )
    if not np.isfinite(hit_clip).all() or np.any(hit_clip[:, 3] <= 0):
        return {}, work | {
            "reason": "invalid_hit_projection",
            "unknown_attribute_hits": len(selected),
        }
    positions = hit_clip[:, :3] / hit_clip[:, 3:]
    needed = selected if method == "sparse" else np.arange(len(points))
    origins, directions = world_rays(
        np.asarray(world), points[needed], np.asarray(raster)
    )
    if method == "sparse":
        distance = np.einsum("ij,ij->i", positions - origins, directions)
        source_faces = faces[selected]
    else:
        distance = np.full(len(points), np.inf)
        distance[selected] = np.einsum(
            "ij,ij->i", positions - origins[selected], directions[selected]
        )
        source_faces = np.where(evaluated, faces, -1)
    try:
        work["attempted_corner_rows"] = 3 * len(selected)
        fields = fields_at(
            materials.source, origins, directions, distance, source_faces, rgba=True
        )
    except ValueError:
        return {}, work | {
            "reason": "undefined_interpolated_normal",
            "unknown_attribute_hits": len(selected),
        }
    work["temporary_interpolated_payload_bytes"] = sum(
        v.nbytes for v in fields.values()
    )
    work["interpolated_corner_rows"] = 3 * len(selected)
    work["ray_rows"] = len(needed)
    if method == "dense":
        fields = {k: v[selected] for k, v in fields.items()}
    fields["uv1"] = fields.pop("metres")
    if not all(np.isfinite(v).all() for v in fields.values()):
        return {}, work | {
            "reason": "nonfinite_interpolated_input",
            "unknown_attribute_hits": len(selected),
        }
    part = np.searchsorted(materials.part_ends, faces[selected], side="right")
    result = {
        "sample_indices": selected,
        "original_triangle_ordinals": faces[selected].copy(),
        "part_ordinals": part,
        "material_indices": materials.materials[part],
        "position": positions,
        **fields,
    }
    work["result_payload_bytes"] = sum(v.nbytes for v in result.values())
    work["unknown_attribute_hits"] = 0
    return result, work
