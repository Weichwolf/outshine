"""Pure array sampling shared by prefiltered appearance experiments."""

import numpy as np


def bilinear(image, uv):
    """Filter with zero exterior samples; caller supplies premultiplied fields."""
    height, width = image.shape[:2]
    p = uv * [width, height] - 0.5
    first = np.floor(p).astype(int)
    fraction = p - first
    result = np.zeros((len(uv), image.shape[2]))
    for x, y in ((0, 0), (1, 0), (0, 1), (1, 1)):
        index = first + [x, y]
        weight = (fraction[:, 0] if x else 1 - fraction[:, 0]) * (
            fraction[:, 1] if y else 1 - fraction[:, 1]
        )
        inside = ((index >= 0) & (index < [width, height])).all(1)
        result[inside] += (
            image[index[inside, 1], index[inside, 0]] * weight[inside, None]
        )
    return result
