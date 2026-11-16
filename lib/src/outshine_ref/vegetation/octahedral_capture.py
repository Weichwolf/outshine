"""Full-sphere octahedral capture directions for the existing native baker.

Cigolle et al., JCGT 3(2), 2014, section 3.1: https://jcgt.org/published/0003/02/01/
The paper's z-up construction is permuted to native y-up coordinates.
"""

import numpy as np


def octahedral_direction(uv):
    """Decode square coordinates [-1,1]^2 to unit vectors pointing toward eye."""
    uv = np.asarray(uv, dtype=np.float64)
    if uv.shape[-1:] != (2,) or not np.isfinite(uv).all() or np.any(np.abs(uv) > 1):
        raise ValueError('finite square octahedron coordinates required')
    x, z = uv[..., 0], uv[..., 1]
    y = 1 - np.abs(x) - np.abs(z)
    folded_x = (1 - np.abs(z)) * np.where(x >= 0, 1, -1)
    folded_z = (1 - np.abs(x)) * np.where(z >= 0, 1, -1)
    direction = np.stack((np.where(y < 0, folded_x, x), y,
                          np.where(y < 0, folded_z, z)), axis=-1)
    return direction / np.linalg.norm(direction, axis=-1, keepdims=True)


def capture_directions():
    """8x8 full sphere; view index = row*8+column, square UV includes endpoints.

    Row zero has square-v=-1, column zero square-u=-1. This is catalogue order,
    independent of the native image's own top-down texel rows. Repeated directions
    along the folded boundary are intentional seam samples, not a horizontal ring.
    """
    u, v = np.meshgrid(np.linspace(-1, 1, 8), np.linspace(-1, 1, 8))
    return octahedral_direction(np.stack((u, v), axis=-1)).reshape(-1, 3)
