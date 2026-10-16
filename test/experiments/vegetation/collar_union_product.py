"""Add demanded local collar solids to a finished crown without rebuilding its axes."""

import time

import manifold3d as manifold
import numpy as np
from collar_union import transfer_normals
from crown_union import flat_fragment
from spatial_crown_replay import check_mesh


def add_collars(base, collars, nodes):
    if (
        len(collars) != len(nodes)
        or len(collars) > 8
        or sum(len(m["faces"]) for m in [base, *collars]) > 262144
    ):
        raise ValueError("bounded complete cached wood and collar products required")
    if not collars:
        return base, {"union_s": 0, "collars": 0}
    start = time.perf_counter()
    solids, sources, ownership = [], {}, {}
    for index, mesh in enumerate([base, *collars]):
        check_mesh(mesh)
        raw = manifold.Mesh64(
            np.array(mesh["vertices"], dtype=np.float64, order="C"),
            np.array(mesh["faces"], dtype=np.uint64, order="C"),
            face_id=np.arange(len(mesh["faces"]), dtype=np.uint64),
        )
        solid = manifold.Manifold(raw)
        if solid.status() != manifold.Error.NoError or solid.volume() <= 0:
            raise ValueError("invalid cached wood or collar solid")
        keys = solid.to_mesh64().run_original_id
        if len(keys) != 1:
            raise ValueError("one source origin per input required")
        key = keys[0]
        sources[key] = mesh
        ownership[key] = index
        solids.append(solid)
    result = manifold.Manifold.batch_boolean(solids, manifold.OpType.Add)
    if result.status() != manifold.Error.NoError or result.is_empty():
        raise ValueError("local collar union failed")
    kept, fragments = [], []
    for part in result.decompose():
        fragment = flat_fragment(part.to_mesh64())
        if fragment is None:
            kept.append(part)
        else:
            fragments.append(fragment)
    if len(kept) != 1 or kept[0].volume() <= 0:
        raise ValueError("collared tree is not one positive connected body")
    raw = kept[0].to_mesh64()
    if len(raw.tri_verts) > 262144 or len(raw.merge_from_vert):
        raise ValueError("collared tree exceeds output capacity or splits positions")
    mesh = transfer_normals(raw, sources)
    kind = np.empty(len(mesh["faces"]), np.uint8)
    source_node = np.empty(len(mesh["faces"]), np.uint32)
    indices, ids = np.asarray(raw.run_index) // 3, np.asarray(raw.face_id)
    for first, last, key in zip(
        indices[:-1], indices[1:], raw.run_original_id, strict=True
    ):
        index = ownership[key]
        if index == 0:
            kind[first:last] = base["source_kind"][ids[first:last]]
            source_node[first:last] = base["source_node"][ids[first:last]]
        else:
            kind[first:last] = 2
            source_node[first:last] = nodes[index - 1]
    mesh.update(source_kind=kind, source_node=source_node)
    checked = check_mesh(mesh)
    return mesh, {
        "union_s": time.perf_counter() - start,
        "collars": len(collars),
        "discarded_flat_fragments": fragments,
        "volume_m3": kept[0].volume(),
        "genus": kept[0].genus(),
        "checks": checked,
    }
