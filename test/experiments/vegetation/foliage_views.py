"""Complete unlit spherical view sets; draw selection leaves the cached plan intact."""

import numpy as np
from foliage_selection import directions
from group_cloud import make_chart


def bake_views(source, count=32, size=16):
    if type(size) is not int or size not in (8, 16, 32, 64, 128):
        raise ValueError("bounded view resolution required")
    if not 1 <= len(source["ids"]) <= 256:
        raise ValueError("one to 256 organs per view set required")
    members = np.arange(len(source["ids"]))
    return tuple(
        make_chart(source, members, direction, size) for direction in directions(count)
    )


def select_view(charts, direction):
    direction = np.asarray(direction, dtype=float)
    if (
        direction.shape != (3,)
        or not np.all(np.isfinite(direction))
        or np.linalg.norm(direction) == 0
    ):
        raise ValueError("finite nonzero view direction required")
    if not charts:
        raise ValueError("complete view set required")
    direction = direction / np.linalg.norm(direction)
    normals = np.array([chart.frame[:, 2] for chart in charts])
    index = int(np.argmax(normals @ direction))
    return index, charts[index]
