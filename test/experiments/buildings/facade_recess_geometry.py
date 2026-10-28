"""One bounded closed recess assembly shared by compact drawing and physical detail."""

from collections import Counter
from dataclasses import dataclass
from types import MappingProxyType

import numpy as np
from opening_details import expand
from outshine_ref.buildings.modelling.prism_union import (
    frozen,
    primitive_mesh,
    triangle_bound,
    union_prisms,
)

MAX_TRIANGLES = 8192
DOOR_FRAME_TRIANGLES = 3 * 12


@dataclass(frozen=True)
class RecessGeometry:
    descriptors: tuple
    mesh: object
    owners: tuple


def build(recesses, *, method="parts"):
    recesses = tuple(recesses)
    if not recesses or len(recesses) > 24 or method not in ("parts", "union"):
        raise ValueError("Bounded nonempty recess assembly and known method required")
    if len({r.identifier for r in recesses}) != len(recesses):
        raise ValueError("Duplicate recess source identifier")
    if (
        sum(triangle_bound(v) for r in recesses for v in r.volumes)
        + DOOR_FRAME_TRIANGLES * len(recesses)
        > MAX_TRIANGLES
    ):
        raise ValueError("recess_assembly_input_triangle_budget")
    vertices, faces, materials, parts, owners = [], [], [], [], []
    offset = 0
    for recess in recesses:
        if method == "parts":
            products = [(*primitive_mesh(v), v.identifier) for v in recess.volumes]
        else:
            product = union_prisms(recess.volumes, max_triangles=MAX_TRIANGLES)
            products = [
                (
                    product.mesh["vertices"],
                    product.mesh["faces"],
                    recess.identifier + "/walls",
                )
            ]
        for points, triangles, identifier in products:
            vertices.append(points)
            faces.append(triangles.astype(np.uint32) + offset)
            materials.append(np.full(len(triangles), 12, dtype=np.uint16))
            parts.append(np.full(len(triangles), len(owners), dtype=np.int32))
            owners.append(identifier)
            offset += len(points)
    doors = expand(
        [
            {
                "id": r.door.identifier,
                "kind": "terrace",
                "centre": r.door.centre,
                "tangent": r.door.tangent,
                "width_m": r.door.width_m,
                "height_m": r.door.height_m,
                "bottom_z_m": r.door.bottom_m,
                "storey": r.door.storey,
                "sill": False,
            }
            for r in recesses
        ]
    )
    if len(doors["faces"]) > DOOR_FRAME_TRIANGLES * len(recesses):
        raise ValueError("recess_door_frame_triangle_contract")
    vertices.append(doors["vertices"])
    faces.append(doors["faces"] + offset)
    materials.append(doors["materials"])
    parts.append(doors["parts"] + len(owners))
    local_parts = Counter()
    for part in range(int(doors["parts"].max()) + 1):
        rows = doors["parts"] == part
        indices = np.unique(doors["opening_indices"][rows])
        if len(indices) != 1:
            raise ValueError("Recess door component has ambiguous source ownership")
        opening = int(indices[0])
        owners.append(
            recesses[opening].door.identifier + f"/frame/{local_parts[opening]}"
        )
        local_parts[opening] += 1
    mesh = {
        "vertices": np.concatenate(vertices),
        "faces": np.concatenate(faces),
        "materials": np.concatenate(materials),
        "parts": np.concatenate(parts),
    }
    if len(mesh["faces"]) > MAX_TRIANGLES:
        raise ValueError("recess_assembly_triangle_budget")
    return RecessGeometry(
        recesses,
        MappingProxyType({k: frozen(v) for k, v in mesh.items()}),
        tuple(owners),
    )


def resolved(prepared, components):
    if components is None:
        return build(prepared.recesses)
    if components.descriptors != prepared.recesses:
        raise ValueError("Recess assembly belongs to a different immutable facade plan")
    return components


def attach(frames, additions):
    part_offset = int(frames["parts"].max()) + 1 if len(frames["parts"]) else 0
    extra = additions.mesh
    return {
        "vertices": np.r_[frames["vertices"], extra["vertices"]],
        "faces": np.r_[frames["faces"], extra["faces"] + len(frames["vertices"])],
        "materials": np.r_[frames["materials"], extra["materials"]],
        "parts": np.r_[frames["parts"], extra["parts"] + part_offset],
    }
