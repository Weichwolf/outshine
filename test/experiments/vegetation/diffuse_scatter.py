"""Cosine-weighted sampling of the existing reciprocal diffuse R/T sheet model."""

import numpy as np
from outshine_ref.vegetation.leaf_lighting import unit_directions
from outshine_ref.vegetation.leaf_optics import bounded_optics, optical_mixture


def sample_sheet(optical, normals, outgoing, random):
    optical = bounded_optics(optical)
    count = len(optical)
    normals, outgoing = (unit_directions(a, count) for a in (normals, outgoing))
    random = np.asarray(random, float)
    if (
        random.shape != (count, 3)
        or not np.isfinite(random).all()
        or np.any((random < 0) | (random >= 1))
    ):
        raise ValueError("three uniform samples in [0,1) per sheet event required")
    reflected, transmitted, transmission_probability = optical_mixture(optical)
    reflect = random[:, 0] < 1 - transmission_probability
    weight = np.where(reflect[:, None], reflected, transmitted)
    side = np.where(np.sum(normals * outgoing, axis=1) >= 0, 1, -1)
    pole = normals * (side * np.where(reflect, 1, -1))[:, None]
    axis = np.eye(3)[np.argmin(abs(pole), axis=1)]
    tangent = np.cross(axis, pole)
    tangent /= np.linalg.norm(tangent, axis=1)[:, None]
    bitangent = np.cross(pole, tangent)
    radius, phi = np.sqrt(random[:, 1]), 2 * np.pi * random[:, 2]
    direction = (
        tangent * (radius * np.cos(phi))[:, None]
        + bitangent * (radius * np.sin(phi))[:, None]
        + pole * np.sqrt(1 - random[:, 1])[:, None]
    )
    return direction, weight


def uniform_samples(seed, first, count):
    if (
        type(seed) is not int
        or seed < 0
        or type(first) is not int
        or first < 0
        or type(count) is not int
        or not 0 <= count <= 65536
    ):
        raise ValueError("nonnegative seed/counter and bounded sample packet required")
    # Philox produces four words per counter; one complete counter belongs to a path.
    raw = (
        np.random.Philox(seed=seed, counter=first)
        .random_raw(count * 4)
        .reshape(count, 4)
    )
    return (raw[:, :3] >> np.uint64(11)).astype(float) * (1 / 2**53)
