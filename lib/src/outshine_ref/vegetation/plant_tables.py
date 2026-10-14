"""Complete immutable canonical rows for requested plant assets and cache replay."""

from types import MappingProxyType

import numpy as np

from .plan_arrays import immutable_array


def canonical_tables(plan):
    detail = plan.expand(sum(plan.axis_counts), sum(plan.leaf_counts))
    axes = detail["axes"]
    paths = np.full((len(axes), 7), -1, dtype=np.int16)
    parents = np.full(len(axes), -1, dtype=np.int64)
    leaves, owners = [], []
    for row, axis in enumerate(axes):
        paths[row, : len(axis.path)] = axis.path
        if axis.path:
            parents[row] = plan.axis(axis.path[:-1]).id
        group = plan.leaves(axis.path, sum(plan.leaf_counts))
        if group is not None:
            leaves.append(group)
            owners.extend([axis.id] * len(group["ids"]))
    result = {
        "axis_ids": np.array([axis.id for axis in axes], dtype=np.uint64),
        "axis_parent_ids": parents,
        "axis_paths": paths,
        "axis_origins": np.stack([axis.origin for axis in axes]),
        "axis_frames": np.stack([axis.frame for axis in axes]),
        "axis_lengths": np.array([axis.length for axis in axes]),
        "axis_bends": np.array([axis.bend for axis in axes]),
        "axis_parent_parameters": np.array([axis.parent_parameter for axis in axes]),
        "axis_radii": np.array(
            [
                [
                    plan.levels[len(axis.path)].base_radius,
                    plan.levels[len(axis.path)].tip_radius,
                ]
                for axis in axes
            ]
        ),
        "leaf_axis_ids": np.array(owners, dtype=np.uint64),
    }
    for name, shape, dtype in (
        ("ids", (0,), np.uint64),
        ("variants", (0,), np.uint8),
        ("origins", (0, 3), np.float64),
        ("nodes", (0, 3), np.float64),
        ("frames", (0, 3, 3), np.float64),
        ("stem_parameter", (0,), np.float64),
    ):
        result["leaf_" + name] = (
            np.concatenate([group[name] for group in leaves])
            if leaves
            else np.empty(shape, dtype=dtype)
        )
    return MappingProxyType(
        {key: immutable_array(value) for key, value in result.items()}
    )
