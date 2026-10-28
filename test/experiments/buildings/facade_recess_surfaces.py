"""Exposed faces of the existing storey slabs and exterior wall thickness."""

import numpy as np
import shapely
from building_layout import WALL_M


def write(writer, prepared):
    for cell in prepared.recesses:
        aperture = cell.outer
        storey = prepared.local.storeys[aperture.storey]
        region = shapely.orient_polygons(
            shapely.from_wkb(cell.pocket_wkb).intersection(
                shapely.from_wkb(storey.footprint_wkb)
            )
        )
        for name, height, reverse in (
            ("floor", aperture.bottom_m, False),
            ("soffit", aperture.bottom_m + aperture.height_m, True),
        ):
            writer.polygon(
                region,
                np.array([0, 0, height]),
                np.array([1, 0, 0]),
                np.array([0, 1, 0]),
                12,
                cell.identifier + "/" + name,
                reverse=reverse,
            )
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
