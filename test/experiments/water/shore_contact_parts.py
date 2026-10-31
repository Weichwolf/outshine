"""Select ready terrain parts from one canonical bank-contact plan.

The body remains one separately owned mesh. No independent clipping, interpolation
or new cache/World ABI is introduced by a part selection.
"""

from types import MappingProxyType

import numpy as np
from outshine_ref.contracts import Mesh
from outshine_ref.geometry.surface_patches import SurfacePatch, SurfacePatches


def cell_groups(original_faces, shape, cells):
    if (
        len(shape) != 2
        or any(type(v) is not int or not 2 <= v <= 129 for v in shape)
        or type(cells) is not int
        or not 1 <= cells <= 64
    ):
        raise ValueError("bounded posting shape and positive part-cell span required")
    columns = shape[1]
    if original_faces.min() < 0 or original_faces.max() >= shape[0] * columns:
        raise ValueError("original posting references outside the supplied window")
    rows, cols = original_faces // columns, original_faces % columns
    if np.any(np.ptp(rows, axis=1) != 1) or np.any(np.ptp(cols, axis=1) != 1):
        raise ValueError(
            "original source triangles must belong to single posting cells"
        )
    return np.c_[rows.min(axis=1) // cells, cols.min(axis=1) // cells]


def prepare(products, original_faces, shape, frame, *, cells=8):
    groups = cell_groups(original_faces, shape, cells)
    source = products["terrain_source_faces"]
    if source.min() < 0 or source.max() >= len(original_faces):
        raise ValueError("canonical terrain has missing original facet ownership")
    patch = SurfacePatch(
        "shore-terrain",
        frame,
        "terrain_top",
        Mesh(products["terrain_vertices"], products["terrain_faces"]),
        {"source_face": source, "canonical_face": np.arange(len(source), dtype="<u4")},
    )
    owners = groups[source]
    parts = {
        tuple(map(int, key)): tuple(
            (patch.patch_id, int(i))
            for i in np.flatnonzero(np.all(owners == key, axis=1))
        )
        for key in np.unique(owners, axis=0)
    }
    body_links = {}
    for row in products["contacts"]:
        terrain, body = int(row["terrain"]), int(row["body"])
        point = products["terrain_vertices"][terrain]
        if point.tobytes() != products["body_vertices"][body].tobytes():
            raise ValueError("canonical terrain/body position words differ")
        key = point.tobytes()
        if key in body_links and body_links[key] != body:
            raise ValueError("coincident body nodes need explicit ownership")
        body_links[key] = body
    return SurfacePatches(patch), MappingProxyType(parts), MappingProxyType(body_links)


def select(field, parts, body_links, keys):
    if not isinstance(keys, (tuple, list)) or len(set(keys)) != len(keys):
        raise ValueError("unique explicit ready-part keys required")
    if any(key not in parts for key in keys):
        raise ValueError("requested ready terrain part is missing")
    chosen = [face for key in keys for face in parts[key]]
    query = field.faces(chosen, max_faces=65536)
    if query.mesh is None:
        raise ValueError("ready terrain selection is unresolved: " + query.reason)
    pairs = [
        (i, body_links[p.tobytes()])
        for i, p in enumerate(query.mesh.vertices)
        if p.tobytes() in body_links
    ]
    return {
        "vertices": query.mesh.vertices,
        "faces": query.mesh.triangles,
        "source_faces": query.face_data["source_face"],
        "canonical_faces": query.face_data["canonical_face"],
        "body_pairs": np.array(pairs, dtype="<u4").reshape(-1, 2),
    }
