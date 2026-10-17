"""A source-stable closed roof product; complete near pockets are prepared on demand."""

from dataclasses import dataclass

import shapely

from broken_hip import envelope, recipe
from dormer_geometry import union
from dormer_plan import plan
from dormer_present import marked, split
from dormer_windows import details, pockets
from recipe_hall_operators import sealed
from tile_roof_envelopes import roof_surface


@dataclass(frozen=True)
class Products:
    physical: object
    body: object
    skin: object
    details: object = None


@dataclass
class Asset:
    dormers: tuple
    original: object
    profile: object
    eave_m: float
    full: Products
    rejected: list
    near: object = None

    def near_products(self):
        if self.near is None:
            closed = pockets(self.full.physical, self.dormers)
            body, skin = split(closed, self.eave_m)
            self.near = Products(
                closed,
                sealed(body),
                marked(skin, self.dormers, recessed=True),
                details(self.dormers),
            )
        return self.near


def prepare(row, parameters, choice, *, clear=None):
    top, _wedge, _report = roof_surface(row, choice)
    original, profile = envelope(
        top, parameters, 1, choice, recipe(row, parameters, choice)
    )
    original_region = shapely.from_wkb(original.footprint_wkb)
    if row["polygon"].symmetric_difference(original_region).area > 1e-7:
        raise ValueError("Original roof violates its existing source-footprint gate")
    dormers, rejected = plan(row, original, choice, clear=clear)
    if not dormers:
        raise ValueError("No actual source roof front accepts a useful dormer")
    physical = union(original, dormers)
    body, skin = split(physical, choice.eave_m)
    products = Products(physical, sealed(body), marked(skin, dormers))
    return Asset(dormers, original, profile, choice.eave_m, products, rejected)
