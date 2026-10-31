"""Complete captured posting window in the existing WGS84 Double reference.

Original Float32 height words survive; no native vertex/shader equivalence claim.
"""

import numpy as np
from outshine_ref.water.geodetic import local_points
from shore_bank_inputs import grid_faces


def load(root, anchor, manifest):
    pages = manifest["pages"]
    zoom = pages[0]["zoom"]
    x0, y0 = min(p["x"] for p in pages), min(p["y"] for p in pages)
    x1, y1 = max(p["x"] for p in pages), max(p["y"] for p in pages)
    expected = {(x, y) for x in range(x0, x1 + 1) for y in range(y0, y1 + 1)}
    if not 1 <= len(expected) <= 16 or len(pages) != len(expected):
        raise ValueError("complete bounded posting rectangle required")
    rows, columns = (y1 - y0 + 1) * 32 + 1, (x1 - x0 + 1) * 32 + 1
    heights = np.empty((rows, columns), dtype="<f4")
    seen = np.zeros((rows, columns), bool)
    with np.load(root / "deformed-pages.npz", allow_pickle=False) as arrays:
        for page in pages:
            tile = page["x"], page["y"]
            if (
                tile not in expected
                or page["zoom"] != zoom
                or page["side"] != 33
                or page["halo"] != 1
                or page["postings"] != 33
            ):
                raise ValueError("original uniform pages with explicit halo required")
            expected.remove(tile)
            field = arrays[page["array"]]
            if field.shape != (35, 35) or field.dtype != np.dtype("<f4"):
                raise ValueError("original Float32 page words required")
            row, column = (page["y"] - y0) * 32, (page["x"] - x0) * 32
            target = heights[row : row + 33, column : column + 33]
            known = seen[row : row + 33, column : column + 33]
            core = field[1:34, 1:34]
            if np.any(target.view("<u4")[known] != core.view("<u4")[known]):
                raise ValueError("shared original posting words disagree")
            target[:] = core
            known[:] = True
    if expected or not seen.all() or not np.isfinite(heights).all():
        raise ValueError("complete finite native posting window required")
    x, y = np.meshgrid(x0 + np.arange(columns) / 32, y0 + np.arange(rows) / 32)
    geo = np.c_[
        x.ravel() / 2**zoom * 360 - 180,
        np.rad2deg(np.arctan(np.sinh(np.pi * (1 - 2 * y.ravel() / 2**zoom)))),
    ]
    vertices = local_points(anchor, geo, heights.ravel())
    return vertices, grid_faces(rows, columns), heights
