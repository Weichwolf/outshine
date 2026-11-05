"""Model lookup by canonical vertex ID; retain native indexed vertex fetch.

Source ranges must be disjoint or identical. Shared geometry with different
placements and instanced draws retain the existing draw path. This models opaque,
order-independent compatible draw groups, not measured GPU performance.
"""

from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True)
class Draw:
    first: int
    count: int
    model: int
    group: int
    instances: int = 1


def batch(vertex_count, draws):
    if vertex_count < 0:
        raise ValueError("negative vertex capacity")
    ranges = {}
    missing = np.iinfo(np.uint32).max
    for at, draw in enumerate(draws):
        if (draw.first < 0 or draw.count <= 0 or draw.first + draw.count > vertex_count
                or not 0 <= draw.model < missing or draw.instances <= 0):
            raise ValueError("invalid source range or placement")
        ranges.setdefault((draw.first, draw.count), []).append(at)
    previous = 0
    models = np.full(vertex_count, missing, dtype=np.uint32)
    groups = {}
    fallback = []
    for (first, count), members in sorted(ranges.items()):
        if first < previous:
            raise ValueError("partially overlapping source ranges require the original draw path")
        previous = first + count
        placements = {draws[at].model for at in members}
        if len(placements) != 1 or any(draws[at].instances != 1 for at in members):
            fallback.extend(members)
            continue
        models[first:first + count] = placements.pop()
        for at in members:
            groups.setdefault(draws[at].group, []).append(at)
    return models, groups, sorted(fallback)
