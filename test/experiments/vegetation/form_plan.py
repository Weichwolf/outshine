"""Topology contract shared by growth plans and diagnostic emission."""

import numpy as np


def validate_parents(parents):
    if parents.ndim != 1 or not np.issubdtype(parents.dtype, np.integer):
        raise ValueError("one-dimensional integer branch parents required")
    if not len(parents) or parents[0] != -1:
        raise ValueError("one rooted topologically ordered tree required")
    if np.any(parents[1:] < 0) or np.any(parents[1:] >= np.arange(1, len(parents))):
        raise ValueError("every child must follow its parent")
