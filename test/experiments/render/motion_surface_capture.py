"""Bounded multiview surface selection; whole original triangles preserve fields."""

import numpy as np
from current_visibility import box_bounds, mesh_boxes
from packed_normals import unpack
from primitive_bvh import build

FIELDS = ("positions", "uv", "normal", "colour", "metres")


def shared_mesh(source, selected):
    selected = np.asarray(selected)
    if (
        selected.ndim != 1
        or selected.dtype.kind not in "iu"
        or not len(selected)
        or len(selected) > 131072
        or selected.min() < 0
        or selected.max() >= len(source["indices"])
    ):
        raise ValueError("Bounded nonempty original triangle selection required")
    if len(np.unique(selected)) != len(selected):
        raise ValueError("Duplicate triangle selection")
    used, corners = np.unique(source["indices"][selected], return_inverse=True)
    fields = tuple(
        k
        for k in FIELDS
        if not (
            k == "colour"
            and np.all(source[k][used] == 1)
            or k == "metres"
            and not np.any(source[k][used])
        )
    )
    dtype = np.dtype([(k, source[k].dtype, source[k].shape[1:]) for k in fields])
    records = np.empty(len(used), dtype)
    for key in fields:
        records[key] = source[key][used]
    # Share only bit-identical full native attribute records, without welding.
    raw = records.view(np.dtype((np.void, records.dtype.itemsize)))
    _, first, inverse = np.unique(raw, return_index=True, return_inverse=True)
    vertices = records[first]
    result = {
        k: np.ascontiguousarray(vertices[k])
        if k in fields
        else np.empty((0, source[k].shape[1]), source[k].dtype)
        for k in FIELDS
    }
    result["indices"] = (
        inverse[corners]
        .reshape(-1, 3)
        .astype(np.uint16 if len(first) <= 65536 else np.uint32)
    )
    result["binding"] = source["binding"][selected].copy()
    result["original_faces"] = selected.astype(np.uint32)
    return result


def decoded(mesh):
    source = dict(mesh)
    count = len(source["positions"])
    for key, width, value in (("colour", 4, 1), ("metres", 2, 0)):
        if not len(source[key]):
            source[key] = np.broadcast_to(
                np.full(width, value, np.float32), (count, width)
            )
    source["vertex_normal"] = unpack(source["normal"])
    return source


def source_groups(source, maximum_sources=32):
    if type(maximum_sources) is not int or not 1 <= maximum_sources <= 64:
        raise ValueError("Bounded source group size required")
    identities, owners = np.unique(source["binding"][:, 1], return_inverse=True)
    low, high = mesh_boxes(
        source["positions"], source["indices"], owners, len(identities)
    )
    tree = build(np.stack((low, high), axis=1))
    counts = np.ones(len(tree.children), np.int32)
    for i in range(len(counts) - 1, -1, -1):
        left, right = tree.children[i]
        if left >= 0:
            counts[i] = counts[left] + counts[right]
    groups, boxes, pending = [], [], [0]
    while pending:
        node = pending.pop()
        left, right = tree.children[node]
        if counts[node] > maximum_sources:
            pending.extend((int(right), int(left)))
            continue
        leaves, stack = [], [node]
        while stack:
            a, b = tree.children[stack.pop()]
            if a < 0:
                leaves.append(int(b))
            else:
                stack.extend((int(b), int(a)))
        groups.append(np.flatnonzero(np.isin(owners, leaves)).astype(np.uint32))
        boxes.append(tree.bounds[node])
    return np.asarray(boxes), tuple(groups)


def select_groups(bounds, view):
    low, high = bounds[:, 0], bounds[:, 1]
    return np.flatnonzero(box_bounds((low, high), view)[4]).astype(np.uint32)


def intrinsic_cell_box(positions, anchor):
    """One camera-independent tangent-frame bound; reuse the existing clip test."""
    positions, anchor = np.asarray(positions, float), np.asarray(anchor, float)
    if (
        positions.ndim != 2
        or positions.shape[1:] != (3,)
        or not 0 < len(positions) <= 393216
        or not np.isfinite(positions).all()
        or np.max(abs(positions)) > 1e8
        or anchor.shape != (3,)
        or not np.isfinite(anchor).all()
        or not np.any(anchor)
    ):
        raise ValueError("Bounded finite cell geometry and nonzero anchor required")
    up = anchor / np.max(abs(anchor))
    up /= np.linalg.norm(up)
    east = np.cross([0.0, 0.0, 1.0], up)
    if np.linalg.norm(east) == 0:
        east = np.cross([0.0, 1.0, 0.0], up)
    east /= np.linalg.norm(east)
    axes = np.column_stack((east, np.cross(up, east), up))
    points = positions @ axes
    bounds = np.array([points.min(0), points.max(0)])
    margin = 64 * np.finfo(float).eps * np.maximum(abs(bounds).max(0), 1)
    return axes, bounds + np.array([-1, 1])[:, None] * margin


def select_intrinsic_cell(axes, bounds, view):
    local_view = view[0] @ axes, view[1] @ axes, *view[2:]
    return bool(len(select_groups(np.asarray(bounds)[None], local_view)))


def front_layers(source, prepared, origins, directions, layers=2):
    """Keep exact sample depths; never replace distinct depths by an epsilon tie."""
    if (
        type(layers) is not int
        or not 1 <= layers <= 4
        or not 0 < len(origins) <= 32768
        or np.shape(origins) != np.shape(directions)
        or np.shape(origins)[1:] != (3,)
        or not np.isfinite(origins).all()
        or not np.isfinite(directions).all()
        or np.any(np.linalg.norm(directions, axis=1) == 0)
    ):
        raise ValueError("Bounded finite layered rays required")
    if prepared["source"] is not source:
        raise ValueError("Layered index belongs to another source")
    normals, mesh = prepared["normal"], prepared["mesh"]
    kept, records = [], []
    work = {
        "rays": len(origins),
        "triangle_ray_hits": 0,
        "front_hits": 0,
        "overflow_rays": 0,
        "retained_layer_hits": 0,
    }
    for begin in range(0, len(origins), 32):
        o, d = origins[begin : begin + 32], directions[begin : begin + 32]
        faces, rays = prepared["ray"].intersects_id(
            o, d, return_locations=False, multiple_hits=True
        )
        work["triangle_ray_hits"] += len(faces)
        den = np.einsum("ij,ij->i", normals[faces], d[rays])
        valid = den < -1e-12
        faces, rays, den = faces[valid], rays[valid], den[valid]
        depth = (
            np.einsum(
                "ij,ij->i",
                mesh.vertices[mesh.faces[faces, 0]] - o[rays],
                normals[faces],
            )
            / den
        )
        valid = depth > 0.1
        order = np.lexsort((faces[valid], depth[valid], rays[valid]))
        faces, rays, depth = (a[valid][order] for a in (faces, rays, depth))
        work["front_hits"] += len(faces)
        if not len(faces):
            continue
        unique = np.r_[True, (rays[1:] != rays[:-1]) | (depth[1:] != depth[:-1])]
        faces, rays, depth = faces[unique], rays[unique], depth[unique]
        first = np.maximum.accumulate(
            np.where(np.r_[True, rays[1:] != rays[:-1]], np.arange(len(rays)), 0)
        )
        rank = np.arange(len(rays)) - first
        work["overflow_rays"] += int(np.count_nonzero(rank == layers))
        take = rank < layers
        kept.append(faces[take])
        records.append(np.c_[rays[take] + begin, rank[take], faces[take], depth[take]])
        work["retained_layer_hits"] += int(take.sum())
    faces = (
        np.unique(np.concatenate(kept)).astype(np.uint32)
        if kept
        else np.empty(0, np.uint32)
    )
    record = np.concatenate(records) if records else np.empty((0, 4))
    return faces, record, work
