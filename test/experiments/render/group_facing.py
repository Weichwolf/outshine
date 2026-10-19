"""Normal groups with membership masks retain the original cluster/index order."""

import collections

import numpy as np
from static_facing import bake, query


def bake_groups(triangles, sphere, partition="faces6"):
    triangles = np.asarray(triangles, dtype=float)
    if partition not in ("faces6", "octants8"):
        raise ValueError("explicit bounded normal partition required")
    if (
        triangles.ndim != 3
        or triangles.shape[1:] != (3, 3)
        or not 1 <= len(triangles) <= 128
    ):
        raise ValueError("one original cluster required")
    if not np.isfinite(triangles).all():
        raise ValueError("finite original triangles required")
    normals = np.cross(
        triangles[:, 1] - triangles[:, 0], triangles[:, 2] - triangles[:, 0]
    )
    if partition == "faces6":
        axis = np.argmax(np.abs(normals), axis=1)
        tags = 2 * axis + (normals[np.arange(len(normals)), axis] > 0)
    else:
        tags = ((normals > 0) * [1, 2, 4]).sum(axis=1)
    groups = []
    for tag in np.unique(tags):
        members = np.flatnonzero(tags == tag)
        groups.append(
            (sum(1 << int(i) for i in members), bake(triangles[members], sphere))
        )
    return {"triangles": len(triangles), "groups": groups, "partition": partition}


def select_groups(record, camera, *, counters=None, **eligibility):
    """Reference cone calls keep all shared original-sphere/eligibility fallbacks."""
    keep = (1 << record["triangles"]) - 1
    if counters is not None:
        counters.update(directory_bytes=8)
    for members, cone in record["groups"]:
        work = collections.Counter()
        visible, _unused_static_mask = query(cone, camera, counters=work, **eligibility)
        if counters is not None:
            counters.update(
                {key: value for key, value in work.items() if key.startswith("cone_")}
            )
        if not visible:
            keep &= ~members
            if counters is not None:
                counters.update(membership_read_bytes=16, membership_clear_words=4)
    if counters is not None:
        partial = keep not in (0, (1 << record["triangles"]) - 1)
        counters.update(
            visibility_write_bytes=16,
            triangle_bit_tests=record["triangles"] * int(partial),
            rank_popcount_upper_bound=4 * record["triangles"] * int(partial),
            compacted_index_write_bytes=12 * keep.bit_count(),
        )
    return keep
