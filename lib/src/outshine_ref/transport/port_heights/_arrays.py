"""Owned immutable arrays with strict index conversion for port planning."""

import numpy as np

from ...contracts import Mesh


def owned(value, dtype, shape, name, *, integer=False, finite=True):
    if len(value) != shape[0]:
        raise ValueError(f"{name} needs shape {shape}")
    raw = np.asarray(value)
    if raw.shape != shape:
        raise ValueError(f"{name} needs shape {shape}")
    if integer and raw.size and (raw.dtype.kind not in "iu" or raw.dtype.kind == "b"):
        raise ValueError(f"{name} needs integer indices without truncation")
    if integer and raw.size and (np.any(raw < -1) or np.any(raw > 2**31 - 1)):
        raise ValueError(f"{name} exceeds the supported index domain")
    if finite and not np.isfinite(raw).all():
        raise ValueError(f"{name} needs finite values")
    array = np.asarray(raw, dtype=dtype)
    return np.frombuffer(array.tobytes(), dtype=array.dtype).reshape(shape)


def immutable_mesh(surface):
    if not isinstance(surface, Mesh):
        raise TypeError("owned Mesh required")
    result = Mesh(surface.vertices, surface.triangles)
    for name in ("vertices", "triangles"):
        array = getattr(result, name)
        object.__setattr__(result, name, owned(array, array.dtype, array.shape, name))
    return result
