"""Complete canonical needle and short-shoot rows reusing the wood plan tables."""

from types import MappingProxyType

import numpy as np

from .plan_arrays import immutable_array


def canonical_tables(plan):
    wood = plan.wood.tables(sum(plan.wood.axis_counts), 0)
    groups = [plan.foliage(path, sum(plan.needle_counts)) for path in plan.wood.paths()]
    groups = [g for g in groups if g is not None]
    result = {k: v for k, v in wood.items() if k.startswith("axis_")}
    for key in groups[0]:
        result["foliage_" + key] = immutable_array(
            np.concatenate([g[key] for g in groups])
        )
    return MappingProxyType(result)
