"""Merge touching junction regions before assigning a single coherent level."""

import numpy as np


def merge_levels(plans):
    if not 1 <= len(plans) <= 20:
        raise ValueError("bounded nonempty joint level plans required")
    groups = []
    for plan in plans:
        members = [plan]
        region = plan["shape"]
        for _iteration in range(20):
            touched = [
                i
                for i, (shape, _members) in enumerate(groups)
                if region.intersects(shape)
            ]
            if not touched:
                break
            for index in reversed(touched):
                shape, old = groups.pop(index)
                region = region.union(shape)
                members.extend(old)
        else:
            raise ValueError("joint level cluster closure budget exhausted")
        groups.append((region, members))
    result = []
    for region, members in groups:
        target = float(np.mean([p["height_m"] for p in members]))
        height = float(np.float32(target))
        if not np.isfinite(height) or abs(height - target) > 0.005:
            raise ValueError("joint level Float32 rounding exceeds 5 mm")
        result.append(
            {
                "shape": region,
                "height_m": height,
                "mvt_xy": members[0]["mvt_xy"],
                "members": [
                    {"mvt_xy": p["mvt_xy"], "height_prior_m": p["height_m"]}
                    for p in members
                ],
            }
        )
    return result
