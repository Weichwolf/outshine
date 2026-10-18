"""One seam-free solid chart per existing C1 sweep, in source metres."""

import numpy as np

_NODES, _WEIGHTS = np.polynomial.legendre.leggauss(16)
_NODES.setflags(write=False)
_WEIGHTS.setflags(write=False)


def station_distances(spine, parameter):
    """Fixed sixteen-point quadrature within each sampled cubic interval."""
    half = np.diff(parameter) / 2
    middle = (parameter[:-1] + parameter[1:]) / 2
    samples = middle[:, None] + half[:, None] * _NODES
    velocity = spine.evaluate(samples, 1)
    lengths = half * (np.linalg.norm(velocity, axis=2) @ _WEIGHTS)
    return np.r_[0.0, np.cumsum(lengths)]


def bark_fields(spine, parameter, radius, sides):
    distance = station_distances(spine, parameter)
    angle = np.arange(sides) * (2 * np.pi / sides)
    chart = np.empty((len(parameter), sides, 3))
    chart[:, :, 0] = radius[:, None] * np.cos(angle)
    chart[:, :, 1] = radius[:, None] * np.sin(angle)
    chart[:, :, 2] = distance[:, None]
    return {
        "bark_coordinates": np.vstack(
            (chart.reshape(-1, 3), (0, 0, 0), (0, 0, distance[-1]))
        ),
        "bark_radius": np.r_[np.repeat(radius, sides), radius[0], radius[-1]],
    }


def mature_recipe():
    """Explicit uncalibrated bark preview, not a botanical oak default."""
    from bark_recipe import bark_recipe

    recipe = bark_recipe()
    recipe.update(
        noise_scale_per_m=[70, 70, 4],
        colour_positions=[0.0, 1.0],
        colour_low=[0.035, 0.027, 0.020, 1],
        colour_high=[0.19, 0.15, 0.11, 1],
        young_colour=[0.12, 0.065, 0.025, 1],
        radius_range_m=[0.005, 0.025],
        bump_strength=0.5,
        bump_distance_m=0.006,
        fissure_scale_per_m=[18, 18, 2],
        fissure_plateau=0.16,
    )
    return recipe
