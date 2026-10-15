"""Independent state queries against complete building, furniture, frames and leaf solids."""

import numpy as np
from door_assembly import mesh, prepare
from furniture_collision import furnishing_solid
from structural_shell_geometry import NativeColumns, inspect_poses


def check(base, selected, low, high):
    fixed = prepare(selected)
    opened = base + furnishing_solid(mesh(selected, 90.0, fixed=fixed))
    closed = base + furnishing_solid(mesh(selected, 0.0, fixed=fixed))
    columns = NativeColumns(closed, low, high)
    records = []
    for row in selected:
        leaf = row["leaf"]
        centre, tangent, inward = map(
            np.asarray, (leaf.centre, leaf.tangent, leaf.inward)
        )
        rays = 0
        for across in np.linspace(
            -leaf.width_m / 2 + 0.01, leaf.width_m / 2 - 0.01, 13
        ):
            point = centre + tangent * across
            for height in (0.04, 1.04, leaf.height_m - 0.04):
                a = [*(point - inward * 0.5), leaf.floor_z_m + height]
                b = [*(point + inward * 0.5), leaf.floor_z_m + height]
                if not closed.ray_cast(a, b) or opened.ray_cast(a, b):
                    raise AssertionError(
                        "Complete assembled doorway contradicts open/closed ray state"
                    )
                rays += 1
        oblique = 0
        for across in np.linspace(
            -leaf.width_m / 2 + 0.001, leaf.width_m / 2 - 0.001, 13
        ):
            inner = centre + tangent * across + inward * leaf.wall_face_m
            for slope in (-4.0, -1.0, 0.0, 1.0, 4.0):
                start = inner - inward * 0.56 - tangent * 0.56 * slope
                finish = inner + inward * 0.44 + tangent * 0.44 * slope
                if not closed.ray_cast(
                    [*start, leaf.floor_z_m + 1.2], [*finish, leaf.floor_z_m + 1.2]
                ):
                    raise AssertionError(
                        "Complete closed doorway leaks a sampled oblique privacy ray"
                    )
                oblique += 1
        xy = centre + inward * (
            leaf.wall_face_m + leaf.stand_off_m + leaf.thickness_m / 2
        )
        try:
            inspect_poses(columns, np.array([[*xy, leaf.floor_z_m]]), feet=False)
        except ValueError as error:
            if "obstructs" not in str(error):
                raise
        else:
            raise AssertionError(
                "Actual closed door did not obstruct the sampled actor"
            )
        records.append(
            {
                "leaf_id": leaf.id,
                "paired_state_rays": rays,
                "closed_oblique_rays": oblique,
                "closed_actor_rejected": True,
            }
        )
    return opened, records
