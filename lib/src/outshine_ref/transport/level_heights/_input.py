"""Bounded snapshots of explicit group identities and actual grade frames."""

import numpy as np


def checked_groups(groups, vertices):
    if not isinstance(groups, (tuple, list)) or not 1 <= len(groups) <= 20:
        raise ValueError("one to twenty explicit flat vertex groups required")
    seen, result = np.zeros(vertices, bool), []
    for group in groups:
        if (
            not isinstance(group, (tuple, list, np.ndarray))
            or (isinstance(group, np.ndarray) and group.ndim != 1)
            or not 3 <= len(group) <= vertices
        ):
            raise ValueError("bounded one-dimensional flat vertex groups required")
        if not isinstance(group, np.ndarray) and any(
            not isinstance(value, (int, np.integer))
            or isinstance(value, (bool, np.bool_))
            for value in group
        ):
            raise ValueError("flat group IDs must be scalar integers")
        values = np.asarray(group)
        if (
            values.ndim != 1
            or not 3 <= len(values) <= vertices
            or values.dtype.kind not in "iu"
            or np.any(values < 0)
            or np.any(values >= vertices)
            or len(np.unique(values)) != len(values)
        ):
            raise ValueError(
                "each flat group needs distinct existing integer vertex IDs"
            )
        values = np.array(values, dtype=np.intp, copy=True)
        if np.any(seen[values]):
            raise ValueError("flat vertex groups must be disjoint")
        seen[values] = True
        values.flags.writeable = False
        result.append(values)
    return tuple(result)


def checked_frames(frames, faces):
    if (
        not isinstance(frames, np.ndarray)
        or frames.shape != (faces, 2, 2)
        or frames.dtype.kind not in "fiu"
    ):
        raise ValueError("bounded numeric grade-frame array required")
    values = np.array(frames, dtype=float, copy=True)
    if (
        not faces
        or values.shape != (faces, 2, 2)
        or not np.isfinite(values).all()
        or np.any(np.linalg.det(values) == 0)
    ):
        raise ValueError("finite nonsingular grade frame per nonempty face required")
    return values
