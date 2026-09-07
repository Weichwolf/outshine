"""One geometric primitive the building bed needs on its hottest path."""
import numpy as np


def cross3(a, b):
    """THE CROSS PRODUCT OF TWO 3-VECTORS, WRITTEN OUT. `np.cross` is general -- it normalises
    axes and moves them on every call -- and at a few microseconds each that is the single
    largest cost in meshing a house: 48 647 calls over forty bodies, 1.06 s of 2.96 s, with
    `moveaxis` and `normalize_axis_tuple` underneath it. The arithmetic is three lines and the
    answer is the same to the bit."""
    return np.array((a[1] * b[2] - a[2] * b[1],
                     a[2] * b[0] - a[0] * b[2],
                     a[0] * b[1] - a[1] * b[0]))
