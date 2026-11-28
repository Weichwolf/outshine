"""Parent-relative circular radii and conservative bounds before axis placement."""


def radius_limits(levels):
    limits = []
    for depth, level in enumerate(levels):
        if level.radius_parent_ratio is None:
            limits.append((level.base_radius,) * level.branches)
            continue
        parent = levels[depth - 1]
        maximum = max(limits[-1])
        taper = parent.tip_radius / parent.base_radius - 1
        limits.append(
            tuple(
                min(
                    level.base_radius,
                    float(level.radius_parent_ratio)
                    * maximum
                    * (1 + taper * level.attachment(slot, -1)),
                )
                for slot in range(level.branches)
            )
        )
    return tuple(limits)


def axis_radii(plan, path):
    level = plan.levels[len(path)]
    if level.radius_parent_ratio is None:
        return level.base_radius, level.tip_radius
    axis = plan.axis(path)
    parent_base, parent_tip = axis_radii(plan, path[:-1])
    parent_radius = parent_base + (parent_tip - parent_base) * axis.parent_parameter
    base = min(level.base_radius, float(level.radius_parent_ratio) * parent_radius)
    tip = base * (level.tip_radius / level.base_radius)
    if not 0 < tip <= base:
        raise ValueError("physical radius underflow")
    return base, tip
