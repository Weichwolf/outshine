"""Generator-owned complete local window geometry before any per-window expansion."""

from dataclasses import replace

import numpy as np
from terraced_facade_details import details
from window_geometry_templates import MAX_INSTANCES, WindowGeometry


def compile_geometry(prepared):
    windows = prepared.facade.windows
    ids = tuple(w.identifier for w in windows)
    if (
        len(ids) > MAX_INSTANCES
        or len(set(ids)) != len(ids)
        or any(not isinstance(i, str) or not i or len(i) > 1024 for i in ids)
    ):
        raise ValueError("Invalid source window identities")
    if prepared.mass.kind not in ("cascade", "paired-wings"):
        raise ValueError("Unsupported closed-window family")
    dimensions = np.asarray(
        [(w.width_m, w.height_m) for w in windows], dtype=np.float64
    ).reshape(-1, 2)
    placements = np.asarray(
        [(*w.centre, *w.tangent, w.bottom_m) for w in windows], dtype=np.float64
    ).reshape(-1, 5)
    frame = np.asarray(
        [
            prepared.mass.core.origin_xy,
            prepared.mass.core.across_xy,
            prepared.mass.core.along_xy,
        ],
        dtype=np.float64,
    )
    if (
        not np.isfinite(dimensions).all()
        or np.any(dimensions <= 0.11)
        or np.any(dimensions > 1e8)
    ):
        raise ValueError("Unsupported closed-window dimensions")
    if (
        not np.isfinite(placements).all()
        or np.any(np.abs(placements) > 1e8)
        or not np.isfinite(frame).all()
        or np.any(np.abs(frame) > 1e8)
    ):
        raise ValueError("Invalid closed-window placement")
    if np.any(np.abs(np.linalg.norm(placements[:, 2:4], axis=1) - 1) > 1e-8):
        raise ValueError("Window tangents must be unit vectors")
    templates, lookup, indices = [], {}, []
    for window, key in zip(windows, map(tuple, dimensions), strict=True):
        if key not in lookup:
            canonical = replace(
                window, centre=(0.0, 0.0), tangent=(1.0, 0.0), bottom_m=0.0
            )
            facade = replace(prepared.facade, windows=(canonical,))
            lookup[key] = len(templates)
            templates.append(details(prepared.local, facade, legacy_trim=False))
        indices.append(lookup[key])
    return WindowGeometry(
        prepared.mass.identifier,
        prepared.mass.kind,
        ids,
        frame,
        placements,
        np.asarray(indices, dtype=np.uint32),
        tuple(templates),
    )
