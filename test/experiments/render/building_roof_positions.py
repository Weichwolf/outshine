"""Controlled roof position recipes; native mesh/acceptance parity is separate."""

import numpy as np
from building_positions import building_positions


def roof_positions(local, plane, surface, *, recipe="float64"):
    if recipe == "float64":
        return building_positions(local, surface, recipe=recipe)
    if recipe != "snapped-f32":
        raise ValueError("explicit supported roof vertex recipe required")
    # An identical scalar operation sequence at every contour/triangle vertex
    # keeps the reference independent of BLAS array shape at millimetre ties.
    values = np.array(local, dtype=np.float64, copy=True)
    values[..., 2] = values[..., 0] * plane[0] + values[..., 1] * plane[1] + plane[2]
    return building_positions(values, surface, recipe=recipe)
