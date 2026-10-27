"""Explicit coarse primary proxy from an already resident canonical caster field."""

import numpy as np
from urban_opaque_receivers import has_prism_domain, prism_grid_offset
from urban_parent_conditional import grid


def select_caster_proxy(parent, camera, caster, view_regions, caster_regions, enabled):
    """Borrow geometry only for Camera-Pending in this bounded alternative.

    The caller separately certifies the same source/world class and the existing
    uniform urban parent appearance. Canonical geometry metadata alone proves
    neither source/material equivalence nor native primary-resource readiness.
    The caster field must already be restricted to its complete declared domain.
    Existing camera products/empty-domain declarations win; partial cases
    retain their previous path. Selection alone renders nothing: the consumer
    must really query/shade this proxy in each moment/background phase.
    """
    if (
        not enabled
        or camera is not None
        or caster is None
        or (view_regions is not None and len(view_regions))
    ):
        return camera, view_regions, None
    product = grid(parent)
    if not has_prism_domain(product, caster, caster_regions):
        return camera, view_regions, None
    regions = np.asarray(caster_regions)
    offset = prism_grid_offset(product, caster)
    low, high = regions[:, 0].min(axis=0) - offset, regions[:, 1].max(axis=0) - offset
    first, size = low - caster["low"], high - low
    x, y = first
    proxy = caster | {
        "low": low,
        "shape": size,
        "lookup": caster["lookup"][y : y + size[1], x : x + size[0]],
    }
    product["primary_covered"] = (
        np.empty((0, 2, 2), np.int64) if view_regions is None else view_regions
    )
    return proxy, caster_regions, product
