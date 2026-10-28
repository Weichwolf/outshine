"""Cut-prism topology without corner expansion on simple four-vertex rings."""

import numpy as np

from .mesh_fans import connected_fans


def fan_topology(_polygon, vertices, faces):
    points, triangles, duplicates, targets = connected_fans(
        {"vertices": vertices, "faces": faces, "uv": np.zeros((len(vertices), 2))},
        np.arange(len(faces)),
    )
    roots = np.arange(len(points), dtype=np.uint64)
    roots[duplicates] = targets
    return {"vertices": points[:, :3], "faces": triangles}, roots


def topology(polygon, vertices, faces):
    if (
        polygon.geom_type == "Polygon"
        and not polygon.interiors
        and len(polygon.exterior.coords) == 5
    ):
        points, inverse = np.unique(vertices, axis=0, return_inverse=True)
        if len(points) == 8:
            return (
                {"vertices": points, "faces": inverse[faces].astype(np.uint64)},
                np.arange(8, dtype=np.uint64),
            )
    return fan_topology(polygon, vertices, faces)
