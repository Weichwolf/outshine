"""Compile closed furnishing parts into one actual collision solid."""

import manifold3d as manifold
import numpy as np
from structural_shell_geometry import native


def furnishing_solid(mesh):
    pieces = [
        native(mesh, rows=np.flatnonzero(mesh["parts"] == part))
        for part in np.unique(mesh["parts"])
    ]
    result = manifold.Manifold.batch_boolean(pieces, manifold.OpType.Add)
    if result.status() != manifold.Error.NoError:
        raise ValueError("Actual furniture assembly is not a valid solid")
    return result
