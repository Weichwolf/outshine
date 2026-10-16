"""Thin material-preserving far faces, independent of complete physical wall relief."""

import numpy as np
import shapely
from industrial_relief_mesh import placed, prefix
from industrial_relief_products import ArticulatedDetails, demanded
from outshine_ref.buildings.facade_frames import front
from outshine_ref.buildings.modelling.scope import scalar
from outshine_ref.buildings.rooflight_panels import triangle_surface


def cap_prefix(part):
    count = len(part.full["faces"]) - len(part.outer["faces"])
    if not 0 < count < len(part.outer["faces"]):
        raise ValueError("Prepared relief requires matching front/back cap topology")
    return prefix(part.full, count)


def direct_caps(plan, *, offset_m=None):
    offset = plan.depth_m if offset_m is None else scalar(offset_m)
    if not 0 <= offset <= plan.depth_m:
        raise ValueError("Far cap offset must lie inside its physical relief depth")
    caps = []
    for polygon in shapely.get_parts(plan.region):
        cap = front(shapely.orient_polygons(polygon))
        cap[:, :, 2] = offset
        caps.append(cap)
    points = np.concatenate(caps).reshape(-1, 3)
    raw = triangle_surface(
        {
            "vertices": points,
            "faces": np.arange(len(points), dtype=np.uint32).reshape(-1, 3),
        },
        10,
    )
    return placed(raw, plan)


class SignalDetails(ArticulatedDetails):
    def __init__(self, prepared, view, *, method="front", **kwargs):
        if method not in ("front", "wall"):
            raise ValueError("Far relief method requires front or wall")
        super().__init__(prepared, view, **kwargs)
        self.method = method
        self.far_parts = {
            identifier: tuple(
                cap_prefix(p)
                if method == "front"
                else direct_caps(p.plan, offset_m=0.004)
                for p in parts
            )
            for identifier, parts in self.reliefs.items()
        }

    def draw_parts(self, identifier):
        return tuple(
            p.outer if demanded(p, self.view) else cap
            for p, cap in zip(
                self.reliefs[identifier], self.far_parts[identifier], strict=True
            )
        )

    def report(self):
        return super().report() | {"distant_relief": self.method}
