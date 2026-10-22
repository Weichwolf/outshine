"""Experimental layered height/normal statistics, not a resolved-cell renderer."""

import numpy as np
from field_sampling import bilinear
from urban_parent_light import shade


def centres_at(parent, points, mode):
    extent = parent.frame[2] * parent.frame[3]
    return bilinear(parent.centre_fraction(mode)[..., None], points[:, :2] / extent)[
        :, 0
    ]


def shadow_transmission(parent, points, light, mode):
    centres = centres_at(parent, points, mode)
    scale, pitch, fill, vacancy = parent.frame[[2, 4, 5, 6]]
    probabilities = parent.probabilities
    roof = centres * (1 - vacancy) * fill**2
    above = ((parent.heights[None] > points[:, 2, None] + 1e-6) * probabilities).sum(
        axis=1
    )
    roof_transmission = 1 - roof * above
    lower = np.r_[0, parent.heights[:-1]]
    thickness = np.maximum(parent.heights - np.maximum(lower, points[:, 2, None]), 0)
    survival = np.cumsum(probabilities[::-1])[::-1]
    integrated = thickness @ survival
    angle = np.abs(light[:, :2]).sum(axis=1) / np.maximum(light[:, 2], 1e-8)
    extinction = centres * (1 - vacancy) * fill / (pitch * scale)
    return np.clip(roof_transmission * np.exp(-extinction * integrated * angle), 0, 1)


def interval(origins, directions, extent, height):
    bounds = np.array([[0, 0, 0], [extent, extent, height]])
    parallel = directions == 0
    times = np.divide(
        bounds[None] - origins[:, None],
        directions[:, None],
        out=np.zeros((len(origins), 2, 3)),
        where=~parallel[:, None],
    )
    near = np.where(parallel, -np.inf, times.min(axis=1)).max(axis=1)
    far = np.where(parallel, np.inf, times.max(axis=1)).min(axis=1)
    missed = np.any(parallel & ((origins < bounds[0]) | (origins > bounds[1])), axis=1)
    return np.maximum(near, 0.1), np.where(missed, -np.inf, far)


def wall_radiance(parent, points, directions, visibility, profile):
    weights = np.abs(directions[:, :2])
    weights /= np.maximum(weights.sum(axis=1)[:, None], 1e-15)
    result = np.zeros_like(points)
    for axis in range(2):
        normal = np.zeros_like(points)
        normal[:, axis] = -np.sign(directions[:, axis])
        result += weights[:, axis, None] * shade(
            points, normal, -directions, 0.6, visibility, profile
        )
    return result


def render_volume(
    parent, origins, directions, profile, *, mode="midpoint", phase_mixture=False
):
    from urban_parent_light import light_at

    if np.any(directions[:, 2] >= 0):
        raise ValueError("initial parent study requires down-looking rays")
    extent = parent.frame[2] * parent.frame[3]
    begin, end = interval(origins, directions, extent, parent.heights[-1])
    colour, transmission = np.zeros_like(origins), np.ones(len(origins))
    scale, pitch, fill, vacancy = parent.frame[[2, 4, 5, 6]]
    dz = directions[:, 2]
    active_phase = np.ones(len(origins))
    if phase_mixture:
        swept = (
            np.abs(directions[:, :2] / dz[:, None])
            * parent.heights[-1]
            / (pitch * scale)
        )
        active_phase = np.minimum(1, fill + swept).prod(axis=1)
    work = {
        "input_rays": len(origins),
        "roof_ray_samples": 0,
        "height_segment_samples": 0,
    }
    higher_probability = 0
    for index in reversed(range(len(parent.heights))):
        height = parent.heights[index]
        time = (height - origins[:, 2]) / dz
        ids = np.flatnonzero((time >= begin) & (time <= end))
        if len(ids):
            points = origins[ids] + time[ids, None] * directions[ids]
            roof_fraction = (
                centres_at(parent, points, mode)
                / active_phase[ids]
                * (1 - vacancy)
                * fill**2
            )
            alpha = (
                roof_fraction
                * parent.probabilities[index]
                / np.maximum(1 - roof_fraction * higher_probability, 1e-15)
            )
            light = light_at(points, profile)[0]
            visible = shadow_transmission(parent, points, light, mode)
            radiance = shade(
                points, np.array([0, 0, 1]), -directions[ids], 0.6, visible, profile
            )
            colour[ids] += (transmission[ids] * alpha)[:, None] * radiance
            transmission[ids] *= 1 - alpha
            work["roof_ray_samples"] += len(ids)
        lower = parent.heights[index - 1] if index else 0
        segment_begin = np.maximum(begin, time)
        segment_end = np.minimum(end, (lower - origins[:, 2]) / dz)
        ids = np.flatnonzero(segment_end > segment_begin)
        if len(ids):
            length = segment_end[ids] - segment_begin[ids]
            points = (
                origins[ids]
                + ((segment_begin[ids] + segment_end[ids]) / 2)[:, None]
                * directions[ids]
            )
            centres = centres_at(parent, points, mode) / active_phase[ids]
            survival = parent.probabilities[index:].sum()
            extinction = centres * (1 - vacancy) * fill / (pitch * scale) * survival
            opacity = -np.expm1(
                -extinction * np.abs(directions[ids, :2]).sum(axis=1) * length
            )
            light = light_at(points, profile)[0]
            visible = shadow_transmission(parent, points, light, mode)
            radiance = wall_radiance(parent, points, directions[ids], visible, profile)
            colour[ids] += (transmission[ids] * opacity)[:, None] * radiance
            transmission[ids] *= 1 - opacity
            work["height_segment_samples"] += len(ids)
        higher_probability += parent.probabilities[index]
    return (
        colour * active_phase[:, None],
        1 - active_phase + active_phase * transmission,
        work,
    )
