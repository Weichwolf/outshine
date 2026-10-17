"""Clip source facets to requested pixels before sampling; GEOS owns polygon work."""

import numpy as np
import shapely
from plant_atlas import projection
from raster_rows import groups


def projected_source(source, view, size, pair_budget, *, two_sided=False):
    if type(two_sided) is not bool:
        raise ValueError("explicit surface sidedness required")
    frame, lower, upper, _ = projection(source["vertices"], view)
    if np.any(upper <= lower):
        return None
    triangles = (source["vertices"] @ frame)[source["faces"]]
    a, b = triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    signed = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
    faces = np.flatnonzero(signed != 0 if two_sided else signed > 0)
    screen = (triangles[faces, :, :2] - lower) / (upper - lower) * size
    first = np.maximum(np.floor(screen.min(1)).astype(int), 0)
    last = np.minimum(np.ceil(screen.max(1)).astype(int), size)
    span = np.maximum(last - first, 0)
    counts = span[:, 0] * span[:, 1]
    if int(counts.sum()) > pair_budget:
        raise ValueError("projected facet/pixel work exceeds declared pair budget")
    return frame, lower, upper, triangles, faces, screen, first, span, counts


def fans(pieces, face_ids, pixels):
    lengths = shapely.get_num_coordinates(pieces) - 1
    offsets = np.r_[0, np.cumsum(lengths + 1)[:-1]]
    count = lengths - 2
    owner = np.repeat(np.arange(len(pieces)), count)
    offset = np.arange(len(owner)) - np.repeat(np.cumsum(count) - count, count)
    coordinates = shapely.get_coordinates(pieces)
    xy = np.stack(
        (
            coordinates[offsets[owner]],
            coordinates[offsets[owner] + offset + 1],
            coordinates[offsets[owner] + offset + 2],
        ),
        axis=1,
    )
    return xy, face_ids[owner], pixels[owner]


def clipped_batches(
    source, view, size, work, pair_budget=262144, *, pixel_mask=None, two_sided=False
):
    if type(size) is not int or size not in (1, 4, 16, 64):
        raise ValueError("explicit native1/4/16/64 footprint required")
    if pixel_mask is not None and not pixel_mask.any():
        return
    data = projected_source(source, view, size, pair_budget, two_sided=two_sided)
    if data is None:
        return
    _, lower, upper, triangles, faces, screen, first, span, counts = data
    work.update(
        front_facets=len(faces),
        candidate_pairs=int(counts.sum()),
        clipped_polygons=0,
        integration_triangles=0,
        projected_triangle_bytes=triangles.nbytes,
        maximum_pair_batch=0,
        clipped_projected_area_m2=0.0,
        requested_candidate_pairs=0,
    )
    pixel_area = float(np.prod((upper - lower) / size))
    for start, stop in groups(counts, 8192):
        local = np.repeat(np.arange(start, stop), counts[start:stop])
        offsets = np.arange(len(local)) - np.repeat(
            np.cumsum(counts[start:stop]) - counts[start:stop], counts[start:stop]
        )
        x = first[local, 0] + offsets % span[local, 0]
        y = first[local, 1] + offsets // span[local, 0]
        work["maximum_pair_batch"] = max(work["maximum_pair_batch"], len(local))
        if pixel_mask is not None:
            requested = pixel_mask[y, x]
            local, x, y = (values[requested] for values in (local, x, y))
        work["requested_candidate_pairs"] += len(local)
        if not len(local):
            continue
        pieces = shapely.intersection(
            shapely.polygons(screen[local]), shapely.box(x, y, x + 1, y + 1)
        )
        selected = shapely.area(pieces) > 0
        if not selected.any():
            continue
        pieces = pieces[selected]
        work["clipped_polygons"] += len(pieces)
        work["clipped_projected_area_m2"] += float(
            shapely.area(pieces).sum() * pixel_area
        )
        xy, ids, pixels = fans(
            pieces, faces[local[selected]], y[selected] * size + x[selected]
        )
        xy = lower + xy / size * (upper - lower)
        base = triangles[ids, :, :2]
        a, b = base[:, 1] - base[:, 0], base[:, 2] - base[:, 0]
        det = a[:, 0] * b[:, 1] - a[:, 1] * b[:, 0]
        relative = xy - base[:, None, 0, :]
        u = (
            relative[:, :, 0] * b[:, None, 1] - relative[:, :, 1] * b[:, None, 0]
        ) / det[:, None]
        v = (
            a[:, None, 0] * relative[:, :, 1] - a[:, None, 1] * relative[:, :, 0]
        ) / det[:, None]
        weights = np.stack((1 - u - v, u, v), axis=-1)
        if np.min(weights) < -1e-6:
            raise ValueError("projected clipping lost source-facet support")
        points = np.einsum(
            "nki,nij->nkj", weights, source["vertices"][source["faces"][ids]]
        )
        edge_a, edge_b = xy[:, 1] - xy[:, 0], xy[:, 2] - xy[:, 0]
        area = (
            abs(edge_a[:, 0] * edge_b[:, 1] - edge_a[:, 1] * edge_b[:, 0])
            / 2
            / pixel_area
        )
        work["integration_triangles"] += len(ids)
        yield points, ids, pixels, area
