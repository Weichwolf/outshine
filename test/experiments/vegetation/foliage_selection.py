"""Camera-independent organ ranks and bounded area-compensated foliage trials."""

import numpy as np
from group_cloud import make_chart, organ_statistics
from raster_batch import rasterize
from shoot_plan import normal_frame


def directions(count=32):
    if type(count) is not int or not 4 <= count <= 64:
        raise ValueError("four to 64 uniform sphere directions required")
    index = np.arange(count)
    z = 1 - 2 * (index + 0.5) / count
    azimuth = index * np.pi * (3 - np.sqrt(5))
    radius = np.sqrt(1 - z * z)
    return np.column_stack((radius * np.cos(azimuth), radius * np.sin(azimuth), z))


def visible_area(source, pixels=64, views=32):
    if type(pixels) is not int or not 8 <= pixels <= 128:
        raise ValueError("bounded visibility raster size required")
    count = len(source["ids"])
    if not 1 <= count <= 256:
        raise ValueError("one to 256 source organs required")
    attributes = np.broadcast_to(
        source["organ_faces"][:, None, None] + 1, (len(source["faces"]), 3, 1)
    )
    scores = np.zeros(count)
    for direction in directions(views):
        frame = normal_frame(direction, np.array((0.0, 0.0, 1.0)))
        camera = np.column_stack((frame[:, 0], frame[:, 2], frame[:, 1]))
        positions = source["vertices"] @ camera
        lower, upper = positions[:, :2].min(axis=0), positions[:, :2].max(axis=0)
        padding = np.max(upper - lower) / pixels
        lower, upper = lower - padding, upper + padding
        mask, values = rasterize(
            positions, source["faces"], attributes, lower, upper, pixels
        )
        organs = np.rint(values[..., 0][mask]).astype(int) - 1
        scores += (
            np.bincount(organs, minlength=count) * np.prod(upper - lower) / pixels**2
        )
    return scores / views


def surface_areas(source):
    triangles = source["vertices"][source["faces"]]
    areas = (
        np.linalg.norm(
            np.cross(
                triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
            ),
            axis=1,
        )
        / 2
    )
    return np.bincount(
        source["organ_faces"], weights=areas, minlength=len(source["ids"])
    )


def organ_order(ids, centres, scores, method):
    count = len(ids)
    if not 1 <= count <= 256:
        raise ValueError("one to 256 source organs required")
    if method == "random":
        keys = np.random.default_rng(17).random(count)
        canonical = np.argsort(ids, kind="stable")
        return canonical[np.argsort(keys, kind="stable")]
    order = np.lexsort((ids, -scores))
    if method == "visible":
        return order
    if method != "spread":
        raise ValueError("declared foliage selection procedure required")
    distance = np.linalg.norm(centres[:, None] - centres[None], axis=-1)
    distance /= max(distance.max(), 1e-12)
    priority = scores / max(scores.max(), 1e-12)
    chosen, remaining = [int(order[0])], np.ones(count, dtype=bool)
    remaining[chosen] = False
    nearest = distance[:, chosen[0]].copy()
    while np.any(remaining):
        value = np.where(remaining, nearest * (0.25 + 0.75 * priority), -1)
        next_organ = int(np.lexsort((ids, -value))[0])
        chosen.append(next_organ)
        remaining[next_organ] = False
        nearest = np.minimum(nearest, distance[:, next_organ])
    return np.asarray(chosen)


def retained_cloud(source, count, method, scores=None, size=16):
    if type(count) is not int or not 1 <= count <= len(source["ids"]) <= 256:
        raise ValueError("bounded retained organ count required")
    centres, _ = organ_statistics(source)
    if scores is None:
        scores = np.zeros(len(centres)) if method == "random" else visible_area(source)
    scores = np.asarray(scores)
    if (
        scores.shape != (len(centres),)
        or not np.all(np.isfinite(scores))
        or np.any(scores < 0)
    ):
        raise ValueError("finite nonnegative visibility scores required")
    order = organ_order(source["ids"], centres, scores, method)
    selected = order[:count]
    area = surface_areas(source)
    scale = float(np.sqrt(area.sum() / area[selected].sum()))
    vertices = source["vertices"].copy()
    normals = []
    for organ in selected:
        indices = np.unique(source["faces"][source["organ_faces"] == organ])
        anchor = source["organ_anchors"][organ]
        vertices[indices] = anchor + scale * (vertices[indices] - anchor)
        positions = vertices[indices] - vertices[indices].mean(axis=0)
        normals.append(np.linalg.eigh(positions.T @ positions)[1][:, 0])
    grown = source | {"vertices": vertices}
    charts = tuple(
        make_chart(grown, np.array((organ,)), normal, size)
        for organ, normal in zip(selected, normals)
    )
    return charts, {
        "selected_ids": source["ids"][selected].tolist(),
        "order_ids": source["ids"][order].tolist(),
        "scale": scale,
        "source_area_m2": float(area.sum()),
        "retained_area_m2": float(surface_areas(grown)[selected].sum()),
    }
