"""Bounded rotation-minimizing frames, local Y forward and explicit cross-section roll."""

import numpy as np
from scipy.spatial.transform import Rotation


def unit(vectors):
    vectors = np.asarray(vectors, dtype=np.float64)
    if vectors.shape[-1:] != (3,) or not np.isfinite(vectors).all():
        raise ValueError("finite three-dimensional vectors required")
    scale = np.max(np.abs(vectors), axis=-1, keepdims=True)
    if np.any(scale == 0):
        raise ValueError("nonzero vectors required")
    scaled = vectors / scale
    return scaled / np.linalg.norm(scaled, axis=-1, keepdims=True)


def orthonormalize(matrix):
    matrix = np.asarray(matrix, dtype=np.float64)
    if matrix.shape[-2:] != (3, 3) or not np.isfinite(matrix).all():
        raise ValueError("finite three-vector frames required")
    axes = [unit(matrix[..., 0, :])]
    for index in (1, 2):
        vector = unit(matrix[..., index, :])
        for previous in axes:
            vector = (
                vector - np.sum(vector * previous, axis=-1, keepdims=True) * previous
            )
        if np.any(np.linalg.norm(vector, axis=-1) < 1e-10):
            raise ValueError("independent frame vectors required")
        axes.append(unit(vector))
    return np.stack(axes, axis=-2)


def frame_inputs(positions, tangents, initial):
    positions = np.asarray(positions, dtype=np.float64)
    if (
        positions.ndim != 2
        or positions.shape[1] != 3
        or not 2 <= len(positions) <= 1025
    ):
        raise ValueError("two through 1025 spatial stations required")
    if not np.isfinite(positions).all():
        raise ValueError("finite station positions required")
    tangents = unit(tangents)
    if tangents.shape != positions.shape:
        raise ValueError("one tangent per station required")
    chords = unit(np.diff(positions, axis=0))
    if np.any(np.sum(chords * tangents[:-1], axis=1) <= 1e-10) or np.any(
        np.sum(chords * tangents[1:], axis=1) <= 1e-10
    ):
        raise ValueError("forward-resolved station intervals required")
    initial = unit(initial)
    if initial.shape != (3,):
        raise ValueError("one initial reference direction required")
    initial = initial - np.dot(initial, tangents[0]) * tangents[0]
    if np.linalg.norm(initial) < 1e-10:
        raise ValueError("initial reference must span the normal plane")
    return positions, tangents, chords, unit(initial)


def reflection_steps(chords, tangents):
    reflected = (
        tangents[:-1]
        - 2 * np.sum(chords * tangents[:-1], axis=1, keepdims=True) * chords
    )
    second = unit(tangents[1:] - reflected)
    return np.column_stack((np.cross(chords, second), np.sum(chords * second, axis=1)))


def rotation_steps(tangents):
    scalar = np.linalg.norm(tangents[:-1] + tangents[1:], axis=1) / 2
    if np.any(scalar <= 1e-10):
        raise ValueError("tangent rotation requires a resolved non-antipodal step")
    return np.column_stack(
        (np.cross(tangents[:-1], tangents[1:]) / (2 * scalar[:, None]), scalar)
    )


def quaternion_prefix(steps):
    result = [(0.0, 0.0, 0.0, 1.0)]
    x, y, z, w = 0.0, 0.0, 0.0, 1.0
    for a, b, c, d in steps.tolist():
        x, y, z, w = (
            d * x + a * w + b * z - c * y,
            d * y - a * z + b * w + c * x,
            d * z + a * y - b * x + c * w,
            d * w - a * x - b * y - c * z,
        )
        result.append((x, y, z, w))
    return np.asarray(result)


def finish_frame(tangents, reference, roll):
    reference = unit(
        reference - np.sum(reference * tangents, axis=1, keepdims=True) * tangents
    )
    normal = np.cross(reference, tangents)
    roll = np.broadcast_to(np.asarray(roll, dtype=np.float64), (len(tangents),))
    if not np.isfinite(roll).all():
        raise ValueError("finite cross-section roll in radians required")
    rolled = np.cos(roll)[:, None] * reference + np.sin(roll)[:, None] * normal
    return np.stack((rolled, tangents, np.cross(rolled, tangents)), axis=1)


def transport_frames(
    positions, tangents, initial=(1, 0, 0), roll=0, method="reflection"
):
    _positions, tangents, chords, initial = frame_inputs(positions, tangents, initial)
    if method == "reflection":
        steps = reflection_steps(chords, tangents)
    elif method == "rotation":
        steps = rotation_steps(tangents)
    else:
        raise ValueError("unknown frame transport method")
    reference = Rotation.from_quat(quaternion_prefix(steps)).apply(
        np.broadcast_to(initial, tangents.shape)
    )
    return finish_frame(tangents, reference, roll)


def reflection_loop(positions, tangents, initial=(1, 0, 0), roll=0):
    _positions, tangents, chords, initial = frame_inputs(positions, tangents, initial)
    reference = np.empty_like(tangents)
    reference[0] = initial
    for index, chord in enumerate(chords):
        previous = reference[index]
        reflected = previous - 2 * np.dot(chord, previous) * chord
        target = tangents[index + 1]
        mirrored = tangents[index] - 2 * np.dot(chord, tangents[index]) * chord
        second = unit(target - mirrored)
        reference[index + 1] = reflected - 2 * np.dot(second, reflected) * second
    return finish_frame(tangents, reference, roll)
