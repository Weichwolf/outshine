"""Sample stable plant sites within a window of the source vector tiles."""

import numpy as np
from outshine_ref.places.sources import tile_geography
from outshine_ref.vegetation.ground_cover import GroundCoverPlan


def tile_sites(source, centre, radius, seed, *, density, cell_size):
    anchor = np.array([source.lon, source.lat])
    lower = anchor + (centre - radius) / source.metres
    upper = anchor + (centre + radius) / source.metres
    for receipt in source.receipts:
        if receipt["kind"] != "vector":
            continue
        tile = receipt["tile"]
        lat, lon = tile_geography([[0, 1], [1, 0]], 1, tile)
        sw, ne = np.column_stack((lon, lat))
        metres = np.array([111319.49 * np.cos(np.deg2rad(lat.mean())), 111132.0])
        lo, hi = np.maximum(sw, lower), np.minimum(ne, upper)
        if np.any(hi <= lo):
            continue
        plan = GroundCoverPlan(
            seed ^ (tile[1] << 24) ^ tile[2], density_m2=density, cell_m=cell_size
        )
        sites = plan.sites((lo - sw) * metres, (hi - sw) * metres)
        xy = (sw - anchor + sites["xy"] / metres) * source.metres
        keep = np.sum((xy - centre) ** 2, axis=1) < radius**2
        if keep.any():
            yield xy[keep], sites["marks"][keep]
