"""Existing opening frames plus closed procedural panes and exterior accent members."""

from itertools import pairwise

import numpy as np
import shapely

from building_layout import rectangle
from building_mesh import solid
from opening_details import expand


def details(mass, facade):
    frames = expand(
        [
            dict(
                id=w.identifier,
                kind="window",
                centre=w.centre,
                tangent=w.tangent,
                width_m=w.width_m,
                height_m=w.height_m,
                bottom_z_m=w.bottom_m,
                storey=w.storey,
                sill=True,
            )
            for w in facade.windows
        ]
    )
    storage = [], [], [], []
    for w in facade.windows:
        pane = rectangle(w.centre, w.tangent, w.width_m - 0.11, -0.079, 0.087)
        solid(storage, pane, w.bottom_m + 0.055, w.bottom_m + w.height_m - 0.055, 3)
        if w.width_m > 2:
            mullion = rectangle(w.centre, w.tangent, 0.045, -0.043, 0.118)
            solid(
                storage,
                mullion,
                w.bottom_m + 0.055,
                w.bottom_m + w.height_m - 0.055,
                10,
            )
        if mass.kind == "paired-wings":
            tangent = np.asarray(w.tangent)
            centre = np.asarray(w.centre) + tangent * (w.width_m / 2 + 0.22)
            pier = rectangle(centre, tangent, 0.16, 0.14, 0.015)
            storey = mass.storeys[w.storey]
            solid(storage, pier, storey.bottom_m + 0.06, storey.top_m - 0.06, 9)
    if mass.kind == "cascade":
        for storey in mass.storeys:
            p = shapely.orient_polygons(shapely.from_wkb(storey.footprint_wkb))
            for ring in (p.exterior, *p.interiors):
                for a, b in pairwise(np.asarray(ring.coords)):
                    vector = b - a
                    length = float(np.linalg.norm(vector))
                    if length <= 0.20:
                        continue
                    band = rectangle((a + b) / 2, vector / length, length, 0.10, 0.015)
                    solid(
                        storage,
                        band,
                        storey.bottom_m + 0.025,
                        storey.bottom_m + 0.20,
                        9,
                    )
    points = np.asarray(storage[0], dtype=np.float64).reshape(-1, 3)
    triangles = np.asarray(storage[1], dtype=np.uint32).reshape(-1, 3)
    part_offset = int(frames["parts"].max()) + 1 if len(frames["parts"]) else 0
    return {
        "vertices": np.r_[frames["vertices"], points],
        "faces": np.r_[frames["faces"], triangles + len(frames["vertices"])],
        "materials": np.r_[
            frames["materials"], np.asarray(storage[2], dtype=np.uint16)
        ],
        "parts": np.r_[
            frames["parts"], np.asarray(storage[3], dtype=np.int32) + part_offset
        ],
    }
