"""Bind geometric junction regions to the shared installed Float32 height core."""

import numpy as np
from stair_joint_height_fit import level_groups


def fit_rounded_levels(surface, frames, plans, budget):
    from outshine_ref.transport.level_heights import fit_level_heights

    groups = level_groups(surface.vertices[:, :2], plans)
    result = fit_level_heights(surface, frames, groups, budget=budget)
    levels = (
        None
        if result.mesh is None
        else np.array([result.mesh.vertices[g[0], 2] for g in groups])
    )
    return result, levels
