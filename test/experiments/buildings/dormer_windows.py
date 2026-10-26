"""Real bounded pocket cuts and existing metrical frame/glass components."""

import numpy as np
import shapely
from dormer_present import GLASS
from facade_entry_geometry import basis_for, expand, local_quads
from facade_parts import append
from opening_modules import place, template
from outshine_ref.buildings.modelling.prism_cut import subtract_prisms
from outshine_ref.buildings.modelling.prism_union import PrismVolume
from recipe_hall_operators import sealed

DEPTH_M = 0.22


def aperture(p):
    frame = p.frame()
    width, height = p.width_m * 0.72, frame["height_m"] * 0.64
    centre = (np.asarray(p.a) + p.b) / 2
    bottom = p.base_m + (frame["height_m"] - height) / 2
    return centre, width, height, bottom


def pockets(product, dormers):
    cuts = []
    for p in dormers:
        centre, w, h, z = aperture(p)
        u = (np.asarray(p.b) - p.a) / p.width_m
        inward = np.array([-u[1], u[0]])
        a, b = centre - u * w / 2, centre + u * w / 2
        polygon = shapely.Polygon(
            [
                a - inward * 0.04,
                b - inward * 0.04,
                b + inward * DEPTH_M,
                a + inward * DEPTH_M,
            ]
        )
        cuts.append(PrismVolume(p.identifier + "/pocket", polygon, z, z + h, 1))
    return subtract_prisms("dormer-roof", product, tuple(cuts), max_triangles=131072)


def details(dormers):
    pieces = []
    for p in dormers:
        centre, w, h, z = aperture(p)
        u = (np.asarray(p.b) - p.a) / p.width_m
        raw = place(template(width=w, height=h, sill_enabled=False), centre, u, z)
        pieces.append(expand(raw, 10))
        frame = p.frame()
        x = (p.width_m - w) / 2
        y = z - p.base_m
        pieces.append(
            local_quads(
                [(x, y, x + w, y + h, -0.18)],
                basis_for(frame),
                np.r_[p.a, p.base_m],
                [p.width_m, frame["height_m"]],
                [1, 1, 0],
                GLASS,
            )
        )
    return sealed(append(pieces[0], pieces[1:]))
