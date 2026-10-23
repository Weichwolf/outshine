"""Ready city fields contain NumPy arrays only; no source/geometry rebuild on load."""

from dataclasses import dataclass

import numpy as np


@dataclass
class UrbanPatches:
    chunks: np.ndarray
    tiers: np.ndarray
    heights: np.ndarray
    frame: np.ndarray
    bounds: np.ndarray
    source_offsets: np.ndarray
    source_indices: np.ndarray

    @property
    def side(self):
        return self.tiers.shape[1]

    @property
    def pitch(self):
        return float(self.frame[2] * self.frame[3])

    def origin(self, chunk):
        return (
            self.chunks[chunk] * self.side * self.frame[3] - self.frame[:2]
        ) * self.frame[2]

    def arrays(self):
        return dict(vars(self))


def load(path):
    dtypes = {
        "chunks": np.dtype("int32"),
        "tiers": np.dtype("uint8"),
        "heights": np.dtype("float64"),
        "frame": np.dtype("float64"),
        "bounds": np.dtype("float32"),
        "source_offsets": np.dtype("uint32"),
        "source_indices": np.dtype("uint32"),
    }
    with np.load(path, allow_pickle=False) as archive:
        if set(archive.files) != set(dtypes):
            raise ValueError("invalid inferred city array fields")
        arrays = {key: archive[key] for key in dtypes}
    if any(arrays[key].dtype != dtype for key, dtype in dtypes.items()):
        raise ValueError("invalid inferred city array dtype")
    product = UrbanPatches(**arrays)
    if (
        product.chunks.ndim != 2
        or product.chunks.shape[1:] != (2,)
        or len(product.chunks) > 65536
        or product.tiers.ndim != 3
        or not 1 <= product.side <= 32
        or product.tiers.shape != (len(product.chunks), product.side, product.side)
        or product.bounds.shape != (len(product.chunks), 2, 3)
        or product.frame.shape != (5,)
        or product.heights.ndim != 1
        or not 2 <= len(product.heights) <= 256
        or product.source_offsets.shape != (len(product.chunks) + 1,)
        or product.source_indices.ndim != 1
    ):
        raise ValueError("invalid inferred city array shape")
    if (
        not np.isfinite(product.bounds).all()
        or np.any(product.bounds[:, 0] > product.bounds[:, 1])
        or not np.isfinite(product.frame).all()
        or not 0 < product.frame[2] <= 1
        or not 8 <= product.frame[3] <= 512
        or not 0 < product.frame[4] < 1
        or product.heights[0] != 0
        or not np.isfinite(product.heights).all()
        or np.any(product.heights[1:] <= product.heights[:-1])
        or (product.tiers.size and product.tiers.max() >= len(product.heights))
        or product.source_offsets[0] != 0
        or np.any(product.source_offsets[1:] < product.source_offsets[:-1])
        or product.source_offsets[-1] != len(product.source_indices)
    ):
        raise ValueError("invalid bounded inferred city product")
    return product


def descriptors(product, chunk):
    """Closed geometry: four sets of wall planes, occupied roof tiers and one floor."""
    origin = product.origin(chunk)
    pitch = product.pitch
    half = pitch * product.frame[4] / 2
    result = []
    for axis in range(2):
        for cell in range(product.side):
            centre = origin[axis] + (cell + 0.5) * pitch
            for sign in [-1, 1]:
                face = axis * 2 + (sign > 0)
                result.append((axis, sign, centre + sign * half, int(face), cell))
    for tier in np.unique(product.tiers[chunk]):
        if tier:
            result.append((2, 1, product.heights[tier], 4, int(tier)))
    result.append((2, -1, 0.0, 5, 0))
    return result
