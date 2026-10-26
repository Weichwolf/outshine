"""Bind constant per-triangle facade extents without nested shape dispatch."""

import mitsuba as mi
import numpy as np


def bind(bsdf, data, indices):
    def uses_extent(node):
        return isinstance(node, dict) and (
            node.get("type") == "outshine_facade_finish"
            or any(uses_extent(value) for value in node.values())
        )

    if not uses_extent(bsdf):
        return bsdf
    values = np.asarray(data["facade_extent"])[indices]
    if not np.array_equal(values, np.broadcast_to(values[:, :1], values.shape)):
        raise ValueError("Facade extent must be constant on each source triangle")
    fields = {
        "extent_" + axis: mi.Float(values[:, 0, index].astype(np.float32))
        for index, axis in enumerate("xyz")
    }

    def replace(node):
        if not isinstance(node, dict):
            return node
        result = {key: replace(value) for key, value in node.items()}
        if result.get("type") == "outshine_facade_finish":
            result.update(fields)
        return result

    return replace(bsdf)
