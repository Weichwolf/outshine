"""Complete bounded physical charts; blend field values instead of chart coordinates."""

import numpy as np
from bark_coordinates import smooth_step
from scipy.spatial import cKDTree


def bark_layers(axes, mesh, method="indexed", max_layers=8):
    points, faces = mesh["vertices"], mesh["faces"]
    if (
        method not in ("dense", "indexed")
        or type(max_layers) is not int
        or not 1 <= max_layers <= 8
    ):
        raise ValueError("declared chart method and one through eight layers required")
    if not axes or len(axes) > max_layers or len(points) * len(axes) > 1_000_000:
        return None
    if len(faces) > 1_000_000 or not np.isfinite(points).all():
        raise ValueError("bounded finite complete wood product required")
    lookup = {a.path: a for a in axes}
    roots = [a for a in axes if a.path[:-1] not in lookup]
    if len(roots) != 1 or axes[0] is not roots[0]:
        raise ValueError("one connected parent-first wood body required")
    tree = cKDTree(points) if method == "indexed" else None
    weights = np.zeros((len(points), len(axes)))
    weights[:, 0] = 1
    coordinates = np.zeros((len(points), len(axes), 3))
    radii = np.zeros_like(weights)
    offsets, evaluations = {}, 0
    for column, axis in enumerate(axes):
        parent = lookup.get(axis.path[:-1])
        indices = np.arange(len(points))
        if parent is None:
            offsets[axis.path] = np.zeros(3)
        else:
            inherited, origin_radius, _ = parent.chart(axis.pose.anchor[None])
            offsets[axis.path] = offsets[parent.path] + inherited[0]
            if tree is not None:
                centre, bound = axis.bounds()
                margin = 64 * np.finfo(float).eps * max(np.max(abs(centre)), bound, 1)
                indices = np.asarray(
                    tree.query_ball_point(centre, bound + margin), dtype=int
                )
        if not len(indices):
            continue
        chart, radius, distance = axis.chart(points[indices])
        evaluations += len(indices)
        coordinates[indices, column] = chart + offsets[axis.path]
        radii[indices, column] = radius
        if parent is not None:
            span = min(2.5 * origin_radius[0], axis.pose.length / 4)
            weight = smooth_step(chart[:, 2] / span) * smooth_step(
                (2 * radius - distance) / radius
            )
            weights[indices, :column] *= 1 - weight[:, None]
            weights[indices, column] = weight
    # Complete chart values at zero-weight corners of every contributing triangle.
    # Otherwise interpolating from a zero placeholder bends a still-visible field.
    completed = 0
    for column, axis in enumerate(axes[1:], 1):
        active = weights[:, column] > 0
        touched = faces[active[faces].any(axis=1)]
        needed = np.unique(touched)
        if tree is None or not len(needed):
            continue
        chart, radius, _ = axis.chart(points[needed])
        coordinates[needed, column] = chart + offsets[axis.path]
        radii[needed, column] = radius
        evaluations += len(needed)
        completed += len(needed)
    if (
        not np.isfinite(weights).all()
        or np.any(weights < 0)
        or np.any(abs(weights.sum(axis=1) - 1) > 64 * np.finfo(float).eps)
    ):
        raise ValueError("finite nonnegative complete chart partition required")
    attributes = {}
    for column in range(len(axes)):
        attributes[f"bark_chart_{column}"] = coordinates[:, column].copy()
        attributes[f"bark_weight_{column}"] = weights[:, column].copy()
        attributes[f"bark_radius_{column}"] = radii[:, column].copy()
    return attributes, {
        "layers": len(axes),
        "point_axis_evaluations": evaluations,
        "completed_triangle_corners": completed,
        "attribute_bytes": sum(a.nbytes for a in attributes.values()),
        "paths": [a.path for a in axes],
    }
