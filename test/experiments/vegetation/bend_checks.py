"""Independent planar bend direction, physical gaps and empty-parent checks."""

from dataclasses import replace

import numpy as np
from bend_pattern import BendPattern
from profile_curve import Profile
from spatial_axis import SpatialAxis
from spatial_examples import spatial_axis
from spine_curve import CubicSpine


def check_bends():
    pattern = BendPattern(minimum_angle=0.1, window_m=0.2, minimum_spacing_m=0.25)
    straight = spatial_axis("straight")
    empty = pattern.sites(straight, straight.length, 0.15, 0.85)
    assert len(empty) == 0
    t = np.linspace(0, 1, 9)
    angle = t * np.pi / 2
    p = np.column_stack((1 - np.cos(angle), np.sin(angle), np.zeros_like(t)))
    d = np.column_stack(
        (np.pi / 2 * np.sin(angle), np.pi / 2 * np.cos(angle), np.zeros_like(t))
    )
    axis = SpatialAxis(CubicSpine.from_hermite(p, d), Profile(np.full(4, 0.01)))
    assert len(pattern.sites(axis, axis.length, 0.15, 0.85)) == 0
    positions = np.column_stack(
        (0.18 * np.sin(2 * np.pi * t) ** 2, 1.5 * t, np.zeros_like(t))
    )
    derivatives = np.column_stack(
        (
            0.18 * 4 * np.pi * np.sin(2 * np.pi * t) * np.cos(2 * np.pi * t),
            np.full_like(t, 1.5),
            np.zeros_like(t),
        )
    )
    axis = SpatialAxis(
        CubicSpine.from_hermite(positions, derivatives), Profile(np.full(4, 0.01))
    )
    sites = pattern.sites(axis, axis.length, 0.15, 0.85)
    assert len(sites) > 0
    assert sites[:, 0].min() >= 0.15 and sites[:, 0].max() <= 0.85
    assert (
        np.diff(np.sort(sites[:, 0])) * axis.length >= pattern.minimum_spacing_m - 1e-12
    ).all()
    sample = axis.field.sample(sites[:, 0])
    direction = (
        np.cos(sites[:, 1, None]) * sample["lateral"]
        + np.sin(sites[:, 1, None]) * sample["normal"]
    )
    reference = (
        -sample["curvature"] / np.linalg.norm(sample["curvature"], axis=1)[:, None]
    )
    error = np.linalg.norm(direction - reference, axis=1).max()
    assert error < 1e-5, error
    assert len(replace(pattern, limit=0).sites(axis, axis.length, 0, 1)) == 0
    assert len(pattern.sites(axis, axis.length, 0.5, 0.5)) == 0
    restored = BendPattern.from_recipe(pattern.recipe())
    np.testing.assert_array_equal(sites, restored.sites(axis, axis.length, 0.15, 0.85))
    return {
        "uniform_circle_sites": 0,
        "planar_bend_sites": len(sites),
        "outward_direction_error": float(error),
        "straight_sites": 0,
        "physical_gap_m": pattern.minimum_spacing_m,
    }
