"""Unique organ depth bands from the same complete source, composed per target view."""

import numpy as np
from needle_composition import composite
from needle_group_reprojection import reproject
from needle_group_views import projection, render_view


def depth_layers(source, direction, count=4, size=64):
    if type(count) is not int or count not in (1, 2, 4, 8):
        raise ValueError("bounded organ depth bands required")
    frame, lower, upper = projection(source["vertices"], direction)
    triangles = source["vertices"][source["faces"]]
    areas = np.linalg.norm(
        np.cross(triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]),
        axis=1,
    )
    depth = triangles.mean(axis=1) @ frame[:, 2]
    weights = np.bincount(
        source["organ_faces"], weights=areas, minlength=len(source["ids"])
    )
    centres = (
        np.bincount(
            source["organ_faces"], weights=areas * depth, minlength=len(source["ids"])
        )
        / weights
    )
    labels = np.minimum(
        np.floor(
            (centres - centres.min()) / max(np.ptp(centres), 1e-20) * count
        ).astype(int),
        count - 1,
    )
    layers = []
    for index in range(count):
        selected = labels[source["organ_faces"]] == index
        if not selected.any():
            continue
        subset = dict(source)
        for field in ("faces", "normals", "colour", "roughness", "organ_faces"):
            subset[field] = source[field][selected]
        layers.append(render_view(subset, direction, size, (lower, upper), samples=4))
    if len(labels) != len(source["ids"]):
        raise AssertionError("each complete source organ assigned once")
    return layers


def render_layers(layers, source, direction, size=64, grid=1, samples=4):
    images = [
        reproject(layer, source, direction, size * samples, samples=1, grid=grid)
        for layer in layers
    ]
    return composite(images, samples)
