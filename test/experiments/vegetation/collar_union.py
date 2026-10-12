"""Simplify geometric joins before transferring per-source shading attributes."""

import manifold3d as manifold
import numpy as np
from collar_cleanup import remove_fragments
from spine_frames import unit
from surface_normal_transfer import restore_normals


def transfer_normals(raw, sources):
    vertices = np.asarray(raw.vert_properties)[:, :3]
    faces = np.asarray(raw.tri_verts, dtype=np.int64)
    normals = np.empty((len(faces), 3, 3))
    face_ids = np.asarray(raw.face_id, dtype=int)
    indices = np.asarray(raw.run_index, dtype=int) // 3
    for start, end, source_id in zip(
        indices[:-1], indices[1:], raw.run_original_id, strict=True
    ):
        source = sources[source_id]
        source_indices = face_ids[start:end]
        source_faces = source["faces"][source_indices]
        triangles = source["vertices"][source_faces]
        e1, e2 = triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
        n = np.cross(e1, e2)
        squared = np.sum(n**2, axis=1)[:, None]
        relative = vertices[faces[start:end]] - triangles[:, None, 0]
        u = np.sum(np.cross(relative, e2[:, None]) * n[:, None], axis=2) / squared
        v = np.sum(np.cross(e1[:, None], relative) * n[:, None], axis=2) / squared
        if "normals" in source:
            corners = source["normals"].reshape(-1, 3, 3)[source_indices]
            weights = np.stack((1 - u - v, u, v), axis=-1)
            interpolated = np.sum(weights[:, :, :, None] * corners[:, None], axis=2)
        else:
            interpolated = np.broadcast_to(n[:, None], (end - start, 3, 3))
        normals[start:end] = unit(interpolated)
    return {
        "vertices": vertices.copy(),
        "faces": faces.astype(np.int32),
        "normals": normals.reshape(-1, 3),
    }


def local_union(products, with_work=False):
    if (
        not products
        or len(products) > 4096
        or sum(len(m["faces"]) for m in products) > 1_000_000
    ):
        raise ValueError("bounded complete geometric union inputs required")
    solids, sources = [], {}
    for product in products:
        raw = manifold.Mesh64(
            np.array(product["vertices"], dtype=np.float64, order="C", copy=True),
            np.array(product["faces"], dtype=np.uint64, order="C", copy=True),
            face_id=np.arange(len(product["faces"]), dtype=np.uint64),
        )
        solid = manifold.Manifold(raw)
        if solid.status() != manifold.Error.NoError:
            raise ValueError("invalid geometric union source")
        source_ids = np.asarray(solid.to_mesh64().run_original_id)
        if len(source_ids) != 1:
            raise ValueError("one source identity per primitive required")
        sources[source_ids[0]] = product
        solids.append(solid)
    result = manifold.Manifold.batch_boolean(solids, manifold.OpType.Add).simplify(1e-8)
    if result.status() != manifold.Error.NoError or result.is_empty():
        raise ValueError("geometric union failed")
    raw = result.to_mesh64()
    if len(raw.merge_from_vert):
        raise ValueError("geometric output unexpectedly split property vertices")
    reference = transfer_normals(raw, sources)
    reset = result.as_original()
    regrouped = reset.to_mesh64()
    faces = np.asarray(regrouped.tri_verts, dtype=np.int64)
    dtype = np.dtype([("a", np.int64), ("b", np.int64), ("c", np.int64)])
    keys = (
        np.ascontiguousarray(np.sort(reference["faces"].astype(np.int64), axis=1))
        .view(dtype)
        .ravel()
    )
    wanted = np.ascontiguousarray(np.sort(faces, axis=1)).view(dtype).ravel()
    order = np.argsort(keys)
    mapping = order[np.searchsorted(keys[order], wanted)]
    np.testing.assert_array_equal(keys[mapping], wanted)
    corners = np.argmax(reference["faces"][mapping, :, None] == faces[:, None], axis=1)
    reference = {
        "vertices": np.asarray(regrouped.vert_properties)[:, :3],
        "faces": faces,
        "normals": reference["normals"]
        .reshape(-1, 3, 3)[mapping[:, None], corners]
        .reshape(-1, 3),
    }
    cleaned = reset.set_tolerance(1e-7).as_original().simplify(1e-7).to_mesh64()
    cleaned, cleanup_work = remove_fragments(cleaned)
    mesh, _work = restore_normals(reset, reference, cleaned)
    _work.update(cleanup_work)
    return (mesh, _work) if with_work else mesh
