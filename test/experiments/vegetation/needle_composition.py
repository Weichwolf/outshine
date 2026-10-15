"""Depth-ordered premultiplied composition before the final coverage filter."""

import numpy as np
from leaf_sheet import average_blocks


def composite(images, factor=1):
    rgba = np.stack([image["rgba"] for image in images])
    depth = np.stack([image["depth"] for image in images])
    normal = np.stack([image["normal"] for image in images])
    order = np.argsort(
        -depth[..., 0] / np.maximum(rgba[..., 3], 1e-20), axis=0, kind="stable"
    )
    output, out_depth, out_normal = (
        np.zeros_like(rgba[0]),
        np.zeros_like(depth[0]),
        np.zeros_like(normal[0]),
    )
    y, x = np.indices(output.shape[:2])
    for selected in order:
        transmission = 1 - output[..., 3, None]
        output += transmission * rgba[selected, y, x]
        out_depth += transmission * depth[selected, y, x]
        out_normal += transmission * normal[selected, y, x]
    return dict(
        images[0],
        rgba=average_blocks(output, factor),
        depth=average_blocks(out_depth, factor),
        normal=average_blocks(out_normal, factor),
        pixel_area_m2=images[0]["pixel_area_m2"] * factor**2,
    )
