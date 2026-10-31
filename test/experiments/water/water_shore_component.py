"""Extract a complete edge-connected native water surface with original lineage.

Boundary loops retain native vertex identities. They are not automatically
physical shores: source/capture cuts still need qualification before construction.
"""

import numpy as np
from water_body_domains import Components


def component(prepared, seed_face):
    faces, vertices = prepared["faces"][:, :3], prepared["vertices"]
    if type(seed_face) is not int or not 0 <= seed_face < len(faces):
        raise ValueError("Existing native seed face required")
    if not 0 < len(faces) <= 262144:
        raise ValueError("Bounded native component input required")
    oriented = faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2)
    _, inverse, counts = np.unique(
        np.sort(oriented, axis=1), axis=0, return_inverse=True, return_counts=True
    )
    order = np.argsort(inverse, kind="stable")
    groups = Components(range(len(faces)))
    offset = 0
    for count in counts:
        owners = order[offset : offset + count] // 3
        offset += count
        for face in owners[1:]:
            groups.join(int(owners[0]), int(face))
    chosen = np.array(
        [i for i in range(len(faces)) if groups.root(i) == groups.root(seed_face)],
        dtype="<u4",
    )
    selected_edges = np.flatnonzero(np.isin(np.arange(len(oriented)) // 3, chosen))
    if np.any(counts[inverse[selected_edges]] > 2):
        raise ValueError("Nonmanifold selected component requires original fallback")
    edges = oriented[selected_edges[counts[inverse[selected_edges]] == 1]]
    if (
        not len(edges)
        or len(np.unique(edges[:, 0])) != len(edges)
        or (set(map(int, edges[:, 0])) != set(map(int, edges[:, 1])))
    ):
        raise ValueError("Unambiguous closed boundary loops required")
    successor = dict(zip(map(int, edges[:, 0]), map(int, edges[:, 1]), strict=True))
    loops = []
    while successor:
        start = min(successor)
        loop, at = [], start
        while at in successor:
            loop.append(at)
            at = successor.pop(at)
        if at != start or len(loop) < 3:
            raise ValueError("Closed noncollapsed boundary required")
        loops.append(loop)
    used = np.unique(faces[chosen])
    local_faces = np.searchsorted(used, faces[chosen]).astype("<u4")
    xyz = vertices[used].copy()
    xy = xyz[local_faces, :2]
    cross = (xy[:, 1, 0] - xy[:, 0, 0]) * (xy[:, 2, 1] - xy[:, 0, 1]) - (
        xy[:, 1, 1] - xy[:, 0, 1]
    ) * (xy[:, 2, 0] - xy[:, 0, 0])
    if not np.isfinite(xyz).all() or np.any(cross <= 0):
        raise ValueError("Positive finite native height triangles required")
    loop_offsets = np.r_[0, np.cumsum([len(loop) for loop in loops])].astype("<u4")
    loop_vertices = np.searchsorted(used, np.concatenate(loops)).astype("<u4")
    offsets, sources = prepared["face_source_offsets"], prepared["face_sources"]
    rows = [sources[offsets[i] : offsets[i + 1]] for i in chosen]
    return {
        "anchor": prepared["anchor"].copy(),
        "original_face_ids": chosen,
        "original_vertex_ids": used,
        "vertices": xyz,
        "faces": local_faces,
        "loop_offsets": loop_offsets,
        "loop_vertices": loop_vertices,
        "face_source_offsets": np.r_[0, np.cumsum([len(r) for r in rows])].astype(
            "<u4"
        ),
        "face_sources": np.concatenate(rows),
    }
