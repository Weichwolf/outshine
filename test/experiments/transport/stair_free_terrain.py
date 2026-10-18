"""Keep hard reference vertices; free old terrain interiors are replaceable priors."""

import numpy as np
import shapely


def retained_seeds(old_xy, seed_ids, fixed_triangles, region):
    selected = old_xy[seed_ids]
    coordinates = np.vstack([selected, fixed_triangles[:, :, :2].reshape(-1, 2)])
    unique, inverse = np.unique(coordinates, axis=0, return_inverse=True)
    required = np.zeros(len(unique), bool)
    required[inverse[len(selected) :]] = True
    fixed = required[inverse[: len(selected)]]
    boundary = shapely.dwithin(shapely.points(selected), region.boundary, 1e-7)
    retained = seed_ids[fixed | boundary]
    return retained, {
        "old_seed_vertices": len(seed_ids),
        "retained_seed_vertices": len(retained),
        "omitted_free_interior_vertices": int(np.count_nonzero(~fixed & ~boundary)),
        "membership_coordinate_bytes": coordinates.nbytes,
        "membership_unique_bytes": unique.nbytes,
        "membership_inverse_bytes": inverse.nbytes,
        "membership_mask_bytes": required.nbytes,
    }
