"""Independent homogeneous-air/shape cell integration on captured native camera rays."""

import numpy as np
from cloud_field import segments
from cloud_radiance import SKY_RADIANCE, SUN, SUN_IRRADIANCE
from cloud_shape_field import extinction
from cloud_view_reference import coefficients, physical_air


def source(at, directions, layer, seconds, kind, light_samples, work):
    sun_depth = np.zeros(len(at))
    intervals = segments(at, SUN, layer)
    for interval in range(2):
        start, end = intervals[:, interval, 0], intervals[:, interval, 1]
        step = (end - start) / light_samples
        active = step > 0
        if not np.any(active):
            continue
        for sample in range(light_samples):
            point = (
                at[active]
                + (start[active] + (sample + 0.5) * step[active])[:, None] * SUN
            )
            sun_depth[active] += (
                extinction(point, layer, seconds, kind, work) * step[active]
            )
    cosine = np.sum(directions * SUN, axis=1)
    g = 0.65
    phase = (1 - g * g) / (4 * np.pi * (1 + g * g - 2 * g * cosine) ** 1.5)
    light = phase * np.exp(-sun_depth) + 0.15 * np.exp(-0.25 * sun_depth) / (4 * np.pi)
    return light[:, None] * SUN_IRRADIANCE + SKY_RADIANCE


def transfer(
    rays,
    medium,
    layer,
    seconds,
    kind,
    work=None,
    view_samples=16,
    light_samples=6,
    query_m=None,
):
    if medium[2] < 1e19 or medium[3] < 1e19 or np.any(medium[8:11] != 0):
        raise ValueError("the captured comparison must use homogeneous ozone-free air")
    origin, direction = rays[:, :3], rays[:, 3:6]
    begin, end = physical_air(rays, medium)
    queries = end if query_m is None else np.minimum(end, query_m)
    air_extinction, air_emission = coefficients(rays, medium)
    radiance = np.zeros((len(rays), 3))
    through = np.ones((len(rays), 3))
    position = begin.copy()

    def integrate(length, sigma, emission):
        nonlocal radiance, through
        length = np.maximum(0, length)[:, None]
        tau = sigma * length
        local = np.divide(
            -np.expm1(-tau) * emission,
            sigma,
            out=emission * length,
            where=sigma > 0,
        )
        radiance += through * local
        through *= np.exp(-tau)

    intervals = segments(origin, direction, layer)
    for interval in range(2):
        start = intervals[:, interval, 0]
        finish = np.minimum(end, intervals[:, interval, 1])
        valid = finish > start
        if not np.any(valid):
            continue
        start = np.where(valid, start, position)
        finish = np.where(valid, finish, position)
        integrate(np.minimum(queries, start) - position, air_extinction, air_emission)
        for sample in range(view_samples):
            a = start + (finish - start) * sample / view_samples
            b = start + (finish - start) * (sample + 1) / view_samples
            at = origin + (0.5 * (a + b))[:, None] * direction
            rho = np.zeros(len(rays))
            requested = valid & (queries > a)
            rho[requested] = extinction(at[requested], layer, seconds, kind, work)
            cloud_emission = np.zeros((len(rays), 3))
            active = rho > 0
            if np.any(active):
                cloud_emission[active] = rho[active, None] * source(
                    at[active],
                    direction[active],
                    layer,
                    seconds,
                    kind,
                    light_samples,
                    work,
                )
            integrate(
                np.minimum(queries, b) - a,
                air_extinction + rho[:, None],
                air_emission + cloud_emission,
            )
        position = np.maximum(position, finish)
    integrate(np.minimum(queries, end) - position, air_extinction, air_emission)
    return np.c_[radiance, through]
