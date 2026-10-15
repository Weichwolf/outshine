"""Unique organ depth bands from the same complete source, composed per target view."""

import numpy as np
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


def render_layers(layers, source, direction, size=64, grid=1):
    images = [reproject(layer, source, direction, size, grid=grid) for layer in layers]
    rgba = np.stack([image["rgba"] for image in images])
    depth = np.stack([image["depth"] for image in images])
    normal = np.stack([image["normal"] for image in images])
    order = np.argsort(
        -depth[..., 0] / np.maximum(rgba[..., 3], 1e-20), axis=0, kind="stable"
    )
    output = np.zeros_like(rgba[0])
    out_depth = np.zeros_like(depth[0])
    out_normal = np.zeros_like(normal[0])
    for index in range(len(images)):
        selected = order[index]
        y, x = np.indices(selected.shape)
        colour = rgba[selected, y, x]
        transmission = 1 - output[..., 3, None]
        output += transmission * colour
        out_depth += transmission * depth[selected, y, x]
        out_normal += transmission * normal[selected, y, x]
    return dict(images[0], rgba=output, depth=out_depth, normal=out_normal)
