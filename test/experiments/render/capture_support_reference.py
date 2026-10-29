"""Independent original primitives and an equal-quality eight-corner opponent."""

import hashlib
import json

import numpy as np
from capture_source_support import (
    capture_frame,
    checked_bounds,
    projected_intervals,
    translated_bounds,
)


def metadata_inputs(directory):
    manifest = json.loads((directory / "manifest.json").read_text())
    path = directory / "captures.npz"
    if (
        path.stat().st_size != manifest["npz_bytes"]
        or hashlib.sha256(path.read_bytes()).hexdigest() != manifest["npz_sha256"]
    ):
        raise ValueError("Frozen source-bound input differs from its receipt")
    with np.load(path, allow_pickle=False) as source:
        # Zip entries are lazy: do not decode any fine image or geometry here.
        arrays = {
            k: source[k] for k in ("source_ids", "source_least_m", "source_most_m")
        }
    ids = arrays["source_ids"]
    if (
        ids.ndim != 1
        or len(ids) != manifest["geometry_sources"]
        or len(np.unique(ids)) != len(ids)
    ):
        raise ValueError("Unique, complete source ordinals required")
    if not np.array_equal(ids, np.sort(ids)):
        raise ValueError("Ordered source ordinals required")
    return manifest, arrays


def corner_support(lower, upper, camera, *, translation=(0, 0, 0), bounds_most=4096):
    lower, upper = checked_bounds(lower, upper, bounds_most)
    eye, basis, limit_low, limit_high = capture_frame(camera)
    lower, upper = translated_bounds(lower, upper, translation)
    choices = np.array(
        [[x, y, z] for x in (False, True) for y in (False, True) for z in (False, True)]
    )
    corners = np.where(choices, upper[:, None], lower[:, None]).reshape(-1, 3)
    a, b = projected_intervals(corners, corners, eye, basis)
    low, high = a.reshape(-1, 8, 3).min(1), b.reshape(-1, 8, 3).max(1)
    finite = np.isfinite(low).all(1) & np.isfinite(high).all(1)
    return {
        "inside": finite & ((low >= limit_low) & (high <= limit_high)).all(1),
        "finite": finite,
        "lower_m": low,
        "upper_m": high,
        "capture_lower_m": limit_low,
        "capture_upper_m": limit_high,
    }


def original_parts(manifest):
    from field_background_inputs import original_inputs

    parts, receipt, _ = original_inputs()
    if receipt != manifest["source"]:
        raise ValueError("Originals differ from the source-bound receipt")
    for i, part in enumerate(parts):
        for key, value in part.items():
            if (
                hashlib.sha256(value.tobytes()).hexdigest()
                != manifest["geometry_arrays_sha256"][f"part{i}_{key}"]
            ):
                raise ValueError("Original geometry/lineage hash mismatch")
    return parts


def original_projection(parts, identities, camera, translation):
    eye, basis, low, high = capture_frame(camera)
    outside = np.zeros((len(identities), 3), bool)
    points = []
    for part in parts:
        moved = (
            (part["positions"].astype(float) + translation)
            .astype(np.float32)
            .astype(float)
        )
        projected = (moved - eye) @ basis.T
        face_outside = ((projected < low) | (projected > high))[part["indices"]].any(1)
        slots = np.searchsorted(identities, part["binding"][:, 1])
        if not np.array_equal(identities[slots], part["binding"][:, 1]):
            raise ValueError("Original triangle has an unknown source ordinal")
        np.logical_or.at(outside, slots, face_outside)
        points.append(projected)
    return outside, np.concatenate(points)
