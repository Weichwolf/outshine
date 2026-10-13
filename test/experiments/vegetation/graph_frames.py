"""One parent skeleton convention for circular, organ and spatial generator axes."""

import numpy as np
from curve_frame import arc_frame
from spatial_axis import SpatialAxis


def parent_frame(node, pose, parameter):
    if isinstance(node.shape, SpatialAxis):
        sample = node.shape.field.sample(parameter)
        return (
            sample["centre"] * pose.scale,
            sample["tangent"],
            sample["lateral"],
            sample["normal"],
        )
    if pose.length:
        twist = node.shape.twist if node.kind == "blade" else 0
        centre, tangent, lateral, normal = arc_frame(
            pose.length, pose.bend, twist, parameter
        )
        if node.kind == "frond":
            angle = node.shape.guide.roll.evaluate(parameter)
            lateral, normal = (
                np.cos(angle) * lateral + np.sin(angle) * normal,
                -np.sin(angle) * lateral + np.cos(angle) * normal,
            )
        return centre, tangent, lateral, normal
    return np.zeros(3), np.array((0, 1, 0)), np.array((1, 0, 0)), np.array((0, 0, 1))


def parent_frames(node, pose, parameters):
    if not isinstance(node.shape, SpatialAxis):
        raise TypeError("batched parent transport is for spatial axes")
    sample = node.shape.field.sample(parameters)
    return np.stack(
        (
            sample["centre"] * pose.scale,
            sample["tangent"],
            sample["lateral"],
            sample["normal"],
        ),
        axis=1,
    )
