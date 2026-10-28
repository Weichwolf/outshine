"""Exposed faces of the existing storey slabs and exterior wall thickness."""

import numpy as np
import shapely
from building_layout import WALL_M
from building_plan import CEILING_M, FLOOR_M


def slabs(writer, prepared):
    mass = prepared.local
    opening = shapely.from_wkb(mass.core.opening_wkb)
    for index in sorted({cell.outer.storey for cell in prepared.recesses}):
        storey = mass.storeys[index]
        polygon = shapely.from_wkb(storey.footprint_wkb)
        floor = polygon.difference(opening) if index else polygon
        if index < len(mass.storeys) - 1:
            ceiling = polygon.difference(opening)
        elif prepared.roof_access is not None:
            ceiling = polygon.difference(
                shapely.from_wkb(prepared.roof_access.opening_wkb)
            )
        else:
            ceiling = polygon
        for name, height, reverse in (
            ("floor", storey.bottom_m + FLOOR_M, False),
            ("soffit", storey.top_m - CEILING_M, True),
        ):
            writer.polygon(
                shapely.orient_polygons(ceiling if reverse else floor),
                np.array([0, 0, height]),
                np.array([1, 0, 0]),
                np.array([0, 1, 0]),
                12,
                storey.identifier + "/recess-visible-" + name,
                reverse=reverse,
            )


def write(writer, prepared):
    slabs(writer, prepared)
    for cell in prepared.recesses:
        aperture = cell.outer
        tangent = np.asarray(aperture.tangent)
        inward = np.array([-tangent[1], tangent[0]])
        for name, side in (("left-reveal", -1), ("right-reveal", 1)):
            centre = np.asarray(aperture.centre) + side * aperture.width_m / 2 * tangent
            writer.polygon(
                shapely.box(0, 0, WALL_M, aperture.height_m),
                np.r_[centre, aperture.bottom_m],
                np.r_[inward, 0],
                np.array([0, 0, 1]),
                12,
                cell.identifier + "/" + name,
                reverse=side > 0,
            )
