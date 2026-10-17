"""Retain matching wall/roof seams and scope-local front UVs on prepared products."""

import numpy as np
from elevation_mesh import binding_for
from outshine_ref.buildings.modelling import Scope, Shape
from outshine_ref.buildings.modelling.cut_geos import Slab
from outshine_ref.buildings.modelling.cut_source import prepare_source
from recipe_hall_operators import sealed
from tile_facade_recess import selection

FRONT, GLASS = 65535, 65534


def split(product, eave):
    mesh = product.mesh
    size = np.exp2(np.ceil(np.log2(np.maximum(1, abs(mesh["vertices"]).max(axis=0)))))
    source = prepare_source(Shape.from_mesh(dict(mesh), Scope(size=tuple(size))))
    chunks = []
    for low, high in (
        (float(mesh["vertices"][:, 2].min()), eave),
        (eave, float(mesh["vertices"][:, 2].max())),
    ):
        slab = Slab(source, 2, low, high, None, 0)
        slab.surfaces()
        value, _omitted = slab.emit()
        if value is None:
            raise ValueError("Dormer split lost a complete wall or roof product")
        data, _faces, _indices = value
        part = {k: data[k] for k in ("vertices", "normals", "uv", "faces", "materials")}
        part["facade_extent"] = np.zeros((len(part["vertices"]), 3))
        chunks.append(part)
    return tuple(chunks)


def marked(skin, dormers, *, recessed=False):
    xyz = skin["vertices"][skin["faces"]]
    normals = np.cross(xyz[:, 1] - xyz[:, 0], xyz[:, 2] - xyz[:, 0])
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
    used = np.zeros(len(xyz), dtype=bool)
    for p in dormers:
        frame = p.frame()
        mask, _basis, rectangular = selection(xyz, normals, frame, p.base_m)
        if not mask.any() or np.any(used & mask) or (not recessed and not rectangular):
            raise ValueError("Dormer lacks a unique complete rectangular front")
        used |= mask
        binding_for(skin, mask, frame, p.base_m, lambda _m, _f: FRONT, 1)
    return sealed(skin)


def bound(mesh, material, front, glass=None):
    values = mesh["materials"].copy()
    values[values == 1] = material
    values[values == FRONT] = front
    if glass is not None:
        values[values == GLASS] = glass
    elif np.any(values == GLASS):
        raise ValueError("Glass details require an explicit existing shader binding")
    return dict(mesh, materials=values)
