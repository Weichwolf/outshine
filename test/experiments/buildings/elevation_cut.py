"""Use the existing open-surface slab cutter with exact original wall provenance."""

import numpy as np
from outshine_ref.buildings.modelling import Scope, Shape
from outshine_ref.buildings.modelling.cut_geos import cut_geos
from outshine_ref.buildings.modelling.cut_source import prepare_source
from tile_facade_recess import selection


def walls(mesh, frames, bottom):
    triangles = mesh["vertices"][mesh["faces"]]
    normals = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    normals /= np.linalg.norm(normals, axis=1, keepdims=True)
    owners = np.full(len(triangles), -1, dtype=np.int32)
    for index, frame in enumerate(frames):
        mask, _basis, _rectangular = selection(triangles, normals, frame, bottom)
        if not mask.any() or np.any(owners[mask] >= 0):
            raise ValueError("Elevation needs a unique nonempty actual wall owner")
        owners[mask] = index
    chosen = owners >= 0
    raw = {k: mesh[k] for k in ("vertices", "normals", "uv")}
    raw.update(faces=mesh["faces"][chosen], materials=mesh["materials"][chosen])
    # Binary scale and zero origin preserve source doubles through scope emission.
    # A tight decimal bounding box would change some retained cap edges by one ULP.
    size = np.exp2(np.ceil(np.log2(np.maximum(1, abs(raw["vertices"]).max(axis=0)))))
    source = prepare_source(Shape.from_mesh(raw, Scope(size=tuple(size))))
    if source.closed:
        raise ValueError("Facade wall cutter requires open wall sheets")
    maximum = float(triangles[chosen, :, 2].max())
    return source, owners[chosen], chosen, maximum


def cut(source, owners, low, high):
    origin = source.shape.scope.origin[2]
    result, _omitted = cut_geos(source, 2, low - origin, high - origin)
    if result is None:
        return None
    data, _faces, indices = result
    original = np.asarray(indices)[data["parts"]]
    mesh = {k: data[k] for k in ("vertices", "faces", "normals", "uv", "materials")}
    mesh["facade_extent"] = np.zeros((len(mesh["vertices"]), 3))
    return mesh, owners[original]
