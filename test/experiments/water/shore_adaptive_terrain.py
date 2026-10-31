"""Stitch actual mixed-zoom native postings in their integer posting lattice.

Only original sample heights are used. Fine edge nodes subdivide adjacent coarse
facets; coarse interpolation there changes, not its original Float32 postings.
ENU positions remain the existing WGS84 Double reference, not native GPU words.
"""

import numpy as np
import shapely
from outshine_ref.geometry import conform_triangles
from outshine_ref.water.geodetic import local_points
from shore_bank_inputs import grid_faces


def seam_gaps(keys, raw, vertices):
    edges = np.unique(
        np.sort(raw[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1), axis=0
    )
    points = keys[edges]
    candidates = edges[
        np.all(points.sum(axis=1) % 2 == 0, axis=1)
        & np.any(abs(points[:, 1] - points[:, 0]) > 1, axis=1)
    ]
    lookup = {tuple(p): i for i, p in enumerate(keys)}
    differences = []
    for a, b in candidates:
        midpoint = lookup.get(tuple((keys[a] + keys[b]) // 2))
        if midpoint is not None:
            differences.append(vertices[midpoint] - (vertices[a] + vertices[b]) / 2)
    differences = np.array(differences).reshape(-1, 3)
    return {
        "shared_fine_midpoints": len(differences),
        "original_coarse_fine_up_gap_max_m": float(
            abs(differences[:, 2]).max(initial=0)
        ),
        "original_coarse_fine_xy_gap_max_m": float(
            np.linalg.norm(differences[:, :2], axis=1).max(initial=0)
        ),
    }


def parents(keys, faces, pages, zoom):
    centroid_sum = keys[faces].sum(axis=1)
    result = np.full(len(faces), -1, np.int64)
    for ordinal, page in enumerate(pages):
        step = 1 << (zoom - page["zoom"])
        origin = np.array([page["x"], page["y"]]) * 32 * step
        local = centroid_sum - 3 * origin
        inside = np.all(local > 0, axis=1) & np.all(local < 3 * 32 * step, axis=1)
        ids = np.flatnonzero(inside)
        if np.any(result[ids] != -1):
            raise ValueError("overlapping original adaptive pages")
        cells, remainder = np.divmod(local[ids], 3 * step)
        if np.any(remainder[:, 0] == remainder[:, 1]):
            raise ValueError("adaptive child centroid lies on its parent diagonal")
        upper = remainder[:, 0] > remainder[:, 1]
        result[ids] = ordinal * 2048 + cells[:, 1] * 32 + cells[:, 0] + upper * 1024
    if np.any(result < 0):
        raise ValueError("adaptive child has no original page/facet owner")
    return result


def load(root, anchor, manifest):
    pages = manifest["neighbor_pages"]["pages"]
    zoom = manifest["neighbor_pages"]["window_zoom"]
    if not 1 <= len(pages) <= 8 or not 0 <= zoom <= 23:
        raise ValueError("bounded supplied adaptive native pages required")
    lookup, keys, words, original_faces, page_ids, boxes = {}, [], [], [], [], []
    with np.load(root / "deformed-neighbors.npz", allow_pickle=False) as arrays:
        for page in pages:
            if (
                page["zoom"] not in (zoom - 1, zoom)
                or page["halo"] != 1
                or page["side"] != 33
            ):
                raise ValueError("original adjacent native zooms and halo required")
            field = arrays[page["array"]]
            if (
                field.shape != (35, 35)
                or field.dtype != np.dtype("<f4")
                or not np.isfinite(field).all()
            ):
                raise ValueError("original finite Float32 halo words required")
            step = 1 << (zoom - page["zoom"])
            x, y = np.meshgrid(
                (page["x"] * 32 + np.arange(33)) * step,
                (page["y"] * 32 + np.arange(33)) * step,
            )
            ids = []
            for key, word in zip(
                zip(x.ravel(), y.ravel(), strict=True),
                field[1:34, 1:34].ravel().view("<u4"),
                strict=True,
            ):
                key = tuple(map(int, key))
                if key in lookup:
                    index = lookup[key]
                    if words[index] != word:
                        raise ValueError(
                            "shared original adaptive posting words differ"
                        )
                else:
                    index = len(keys)
                    lookup[key] = index
                    keys.append(key)
                    words.append(word)
                ids.append(index)
            original_faces.append(np.array(ids)[grid_faces(33, 33)])
            page_ids.append(int(page["page"]))
            low = np.array([page["x"], page["y"]]) * 32 * step
            boxes.append(shapely.box(*low, *(low + 32 * step)))
    keys = np.array(keys, np.int64)
    words = np.array(words, dtype="<u4")
    raw = np.concatenate(original_faces)
    faces, work = conform_triangles(keys, raw, max_faces=16384, max_candidates=1048576)
    parent = parents(keys, faces, pages, zoom)
    # Integer half-plane containment and exact twice-area conservation per parent.
    outer = keys[raw[parent]]
    child = keys[faces]
    for i in range(3):
        a, b = outer[:, i], outer[:, (i + 1) % 3]
        cross = (b[:, None, 0] - a[:, None, 0]) * (child[:, :, 1] - a[:, None, 1]) - (
            b[:, None, 1] - a[:, None, 1]
        ) * (child[:, :, 0] - a[:, None, 0])
        if np.any(cross > 0):
            raise ValueError("adaptive subdivision leaves its exact original parent")

    def areas(triangles):
        a, b = triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
        return a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]

    area = areas(child)
    total = np.zeros(len(raw), np.int64)
    np.add.at(total, parent, area)
    if np.any(area >= 0) or not np.array_equal(total, areas(keys[raw])):
        raise ValueError("adaptive subdivision changes exact parent area")
    boundary, counts = np.unique(
        np.sort(faces[:, [[0, 1], [1, 2], [2, 0]]].reshape(-1, 2), axis=1),
        axis=0,
        return_counts=True,
    )
    domain = shapely.union_all(boxes)
    if np.any(counts > 2) or not np.all(
        shapely.covers(
            domain.boundary, shapely.linestrings(keys[boundary[counts == 1]])
        )
    ):
        raise ValueError("adaptive stitching leaves internal boundary edges")
    geo = np.c_[
        keys[:, 0] / (32 * 2**zoom) * 360 - 180,
        np.rad2deg(np.arctan(np.sinh(np.pi * (1 - 2 * keys[:, 1] / (32 * 2**zoom))))),
    ]
    vertices = local_points(anchor, geo, words.view("<f4"))
    projected = shapely.polygons(vertices[faces, :2])
    if not shapely.coverage_is_valid(projected) or np.any(
        areas(vertices[faces, :2]) <= 0
    ):
        raise ValueError("projected adaptive mesh is folded or nonconforming")
    return {
        "vertices": vertices,
        "faces": faces,
        "posting_keys": keys,
        "height_words": words,
        "original_faces": raw,
        "source_parent_faces": parent.astype("<u4"),
        "source_pages": np.array(page_ids, dtype="<u4")[parent // 2048],
        "source_local_faces": (parent % 2048).astype("<u4"),
    }, dict(
        work,
        **seam_gaps(keys, raw, vertices),
        pages=len(pages),
        input_faces=len(raw),
        output_faces=len(faces),
        original_unique_postings=len(keys),
        exact_parent_area_preserved=True,
    )
