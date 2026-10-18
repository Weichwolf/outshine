"""Simple original closed axes are cyclic; other repeated nodes stay unsupported."""

import numpy as np
import shapely
from stair_cycle_contacts import interior_profile_xy


def axis_form(source):
    nodes = [tuple(p) for p in source["mvt_xy"]]
    if not 2 <= len(nodes) <= 256:
        raise ValueError("bounded original seed axis required")
    if len(set(nodes)) == len(nodes):
        return "open"
    if (
        len(nodes) < 4
        or nodes[0] != nodes[-1]
        or len(set(nodes[:-1])) != len(nodes) - 1
    ):
        raise ValueError(
            "independent seed requires unique original source nodes or one simple closure"
        )
    xy = np.asarray(source.get("xy_m"), float)
    if (
        xy.shape != (len(nodes), 2)
        or not np.isfinite(xy).all()
        or not np.array_equal(xy[0], xy[-1])
        or np.any(np.linalg.norm(np.diff(xy, axis=0), axis=1) < 1e-6)
        or not shapely.LineString(xy).is_ring
    ):
        raise ValueError("closed seed requires a finite simple original ring")
    return "closed"


def seam_profile(row):
    xy = np.asarray(row["xy"])
    if len(xy) < 4 or not np.array_equal(xy[0], xy[-1]):
        raise ValueError("one original cyclic seam required")
    return interior_profile_xy({"xy": xy[[-2, 0, 1]], "width_m": row["width_m"]}, 1)
