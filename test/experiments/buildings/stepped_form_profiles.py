"""Metric half-plane profiles on the unchanged source polygon and its holes."""

import numpy as np
import shapely


def profile_for(identifier):
    from provider_contract import seed_for

    return ("legacy", "corner", "diagonal")[
        seed_for(str(identifier) + "/upper-form") % 3
    ]


def outlines(plan, axis, side, distances):
    polygon = plan["footprint"]
    origin = np.asarray(polygon.centroid.coords[0])
    points = shapely.get_coordinates(polygon) - origin
    tangent = np.array([-axis[1], axis[0]])
    projection = points @ axis
    lo, hi = projection.min(), projection.max()
    cross = points @ tangent
    low, high = cross.min() - 1.0, cross.max() + 1.0
    result = []
    for distance in distances:
        if distance == 0:
            result.append(polygon)
            continue
        a, b = (lo + distance, hi + 1.0) if side < 0 else (lo - 1.0, hi - distance)
        vertices = [
            origin + axis * x + tangent * y
            for x, y in ((a, low), (b, low), (b, high), (a, high))
        ]
        result.append(polygon.intersection(shapely.Polygon(vertices)))
    return result


def requests(plan, axes, distances, profile):
    if profile == "legacy":
        yield "all-sides", None, 0, None
        for index, axis in enumerate(axes):
            for side in (-1, 1):
                yield (
                    "axis-" + str(index),
                    axis,
                    side,
                    outlines(plan, axis, side, distances),
                )
        return
    if profile not in ("corner", "diagonal"):
        raise ValueError("Unknown stepped form profile")
    for a in (-1, 1):
        for b in (-1, 1):
            if profile == "corner":
                x = outlines(plan, axes[0], a, distances)
                y = outlines(plan, axes[1], b, distances)
                proposed = [u.intersection(v) for u, v in zip(x, y, strict=True)]
            else:
                axis = (axes[0] * a + axes[1] * b) / np.sqrt(2)
                proposed = outlines(plan, axis, 1, distances)
            yield f"{profile}-{a:+d}-{b:+d}", None, 0, proposed
