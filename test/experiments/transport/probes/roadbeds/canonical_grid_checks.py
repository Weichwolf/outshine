"""Distinguish actual interior loss from numerical multipolygon buffer artifacts."""

import json
from pathlib import Path
import sys

import numpy as np
import shapely

sys.path.insert(0,str(Path(__file__).resolve().parents[2]))
from transport_surface_regions import canonical_grid_region,has_resolvable_interior,verify,verify_canonical_grid


def check_component_erosion():
    strips=shapely.MultiPolygon([
        shapely.Polygon([(-73.85742108675234,388.83611742033554),(-79.62256210645137,395.187682495556),
            (-73.85741801933277,388.8361140409013),(-73.85742107326936,388.83611740548065)]),
        shapely.Polygon([(-70.80837244954719,388.5658720593708),(-71.46393648481936,388.87731593790835),
            (-71.463936484819,388.87731593790795),(-71.7537818784809,389.0150150316982)])])
    roundoff=64*np.finfo(float).eps*np.abs(shapely.get_coordinates(strips)).max()
    assert strips.is_valid and not has_resolvable_interior(strips,roundoff)
    real=shapely.box(-75.,393.,-75.+1e-6,393.+1e-6)
    assert has_resolvable_interior(real,roundoff)
    nested=shapely.GeometryCollection([strips,shapely.GeometryCollection([real])])
    assert has_resolvable_interior(nested,roundoff)
    source=shapely.box(-80,388,-70,398).difference(real)
    normalized=shapely.set_precision(source,.0001)
    difference=source.symmetric_difference(normalized)
    assert difference.area<=roundoff*source.length
    try:canonical_grid_region(source,.0001)
    except AssertionError as error:assert str(error)=='grid normalization changes a real-width feature'
    else:raise AssertionError('a resolved hole was removed below the total area threshold')
    return dict(roundoff_strips_accepted=True,actual_interior_retained=True,
                nested_components_checked=True,small_resolved_hole_rejected=True)


if __name__=='__main__':print(json.dumps(verify()|verify_canonical_grid()|check_component_erosion()))
