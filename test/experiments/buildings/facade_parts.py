"""One final native facade stream from the retained body and admitted detail parts."""

import numpy as np


def append(mesh, additions):
    if not additions:
        return mesh
    chunks = [mesh, *additions]
    offsets = np.cumsum([0] + [len(c["vertices"]) for c in chunks[:-1]])
    result = {
        key: np.concatenate([c[key] for c in chunks])
        for key in ("vertices", "normals", "uv", "materials", "facade_extent")
    }
    result["faces"] = np.concatenate(
        [c["faces"] + offset for c, offset in zip(chunks, offsets, strict=True)]
    ).astype(np.uint32)
    return result
