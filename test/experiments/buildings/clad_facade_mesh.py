"""Emit only demanded frame front/sides; the closed reference is explicit."""

import numpy as np
import shapely
from clad_facade_plan import choose
from facade_frame_geometry import FrameDetails, placed
from outshine_ref.buildings.facade_frames import front, sides
from outshine_ref.buildings.rooflight_panels import triangle_surface
from provider_contract import attributes
from recipe_hall_operators import sealed
from tile_facades import FacadeDetails


def surface(plan, mode):
    count = plan.triangles(mode)
    pieces = []
    for polygon in shapely.get_parts(plan.region):
        cap = front(polygon)
        cap[:, :, 2] = plan.depth_m
        pieces.append(cap)
        if mode != "front":
            edge, _uv = sides(polygon, plan.depth_m)
            pieces.append(edge)
        if mode == "closed":
            back = cap[:, ::-1].copy()
            back[:, :, 2] = 0
            pieces.append(back)
    points = np.concatenate(pieces).reshape(-1, 3)
    if len(points) != count * 3:
        raise ValueError("Clad triangulation differs from exact preflight topology")
    return triangle_surface(
        {
            "vertices": points,
            "faces": np.arange(len(points), dtype=np.uint32).reshape(-1, 3),
        },
        10,
    )


class CladFrames(FrameDetails):
    def programmes(self, row, parameters, facades, family, entry):
        return choose(row, parameters, facades, family, entry)

    def product(self, current, resolved, material):
        mode = "outer" if resolved else "front"
        key = (current.region.wkb, current.depth_m, mode)
        if key not in self.templates:
            self.templates[key] = sealed(surface(current, mode))
        return placed(self.templates[key], current, 10), mode

    def description(self, current):
        return super().description(current) | {
            "composition": {
                "region_wkb": current.region.wkb_hex,
                "protected_wkb": current.protected.wkb_hex,
                "origin": "procedural broad bands on original window/storey axes",
                "material": 10,
            }
        }

    def logical_info(self, count):
        if not count:
            return {}
        keys = (
            "id",
            "facade_id",
            "kind",
            "origin_xy_m",
            "tangent",
            "base_m",
            "composition",
        )
        return {
            "facade_clad_plan": [
                {k: actual[k] for k in keys}
                | {
                    "source_frame": actual["frame"],
                    "origin": "procedural clad region on unchanged source frame axes",
                }
                for actual in self.completed[-count:]
            ]
        }


class CladFacades(FacadeDetails):
    def __init__(self, rows, view, **kwargs):
        super().__init__(rows, view, **kwargs)
        self.frames = CladFrames(self.entrances)


def build(row, neighbors, view):
    owner = CladFacades([row, *neighbors], view)
    mesh, info = owner(row, attributes(row["properties"]), 12)
    return mesh, info, owner
