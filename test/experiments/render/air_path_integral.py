"""Single-scattered clear gases; finite RGB radiance per unit solar illuminance."""

import re
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path

import numpy as np


@dataclass(frozen=True)
class Medium:
    bottom_km: float
    top_km: float
    rayleigh_height_km: float
    rayleigh_per_km: tuple[float, float, float]
    ozone_centre_km: float
    ozone_width_km: float
    ozone_per_km: tuple[float, float, float]


def engine_clear_air(path: Path) -> Medium:
    body = (
        path.read_text()
        .split("inline constexpr Medium kEarthAir = {", 1)[1]
        .split("};", 1)[0]
    )

    def value(name):
        match = re.search(r"\." + name + r"\s*=\s*(\{[^}]+\}|[^,]+)", body)
        if match is None:
            raise ValueError(f"missing native medium field {name}")
        raw = match[1].strip().strip("{}")
        values = tuple(float(part.strip().removesuffix("f")) for part in raw.split(","))
        return values[0] if len(values) == 1 else values

    return Medium(
        value("BottomRadiusKm"),
        value("TopRadiusKm"),
        value("RayleighScaleHeightKm"),
        value("RayleighScatteringPerKm"),
        value("OzoneCentreKm"),
        value("OzoneHalfWidthKm"),
        value("OzoneAbsorptionPerKm"),
    )


@lru_cache(maxsize=12)
def quadrature(count):
    if not 1 <= count <= 1024:
        raise ValueError("quadrature count outside the experiment budget")
    x, weights = np.polynomial.legendre.leggauss(count)
    x = (x + 1.0) * 0.5
    weights = weights * 0.5
    x.flags.writeable = weights.flags.writeable = False
    return x, weights


def height_along(medium, radius, cosine, distance):
    above = np.asarray(radius) - medium.bottom_km
    raised = (
        above * (radius + medium.bottom_km)
        + distance * distance
        + 2.0 * radius * cosine * distance
    )
    end = np.sqrt(
        np.maximum(
            0.0,
            np.asarray(radius) ** 2 + distance**2 + 2.0 * radius * cosine * distance,
        )
    )
    return raised / (end + medium.bottom_km)


def boundary_distance(radius, cosine, boundary_km):
    discriminant = radius * radius * (cosine * cosine - 1.0) + boundary_km**2
    return np.maximum(0.0, -radius * cosine + np.sqrt(np.maximum(0.0, discriminant)))


def hits_ground(medium, radius, cosine):
    discriminant = radius * radius * (cosine * cosine - 1.0) + medium.bottom_km**2
    return (discriminant >= 0) & (
        -radius * cosine - np.sqrt(np.maximum(0, discriminant)) >= 0
    )


def coefficients(medium, height):
    height = np.maximum(0.0, height)
    scattering = (
        np.exp(-height[..., None] / medium.rayleigh_height_km) * medium.rayleigh_per_km
    )
    ozone = np.maximum(
        0.0, 1.0 - np.abs(height - medium.ozone_centre_km) / medium.ozone_width_km
    )
    extinction = scattering + ozone[..., None] * medium.ozone_per_km
    return scattering, extinction


def transmittance(medium, radius, cosine, distance, samples=64):
    nodes, weights = quadrature(samples)
    radius, cosine, distance = np.broadcast_arrays(radius, cosine, distance)
    heights = height_along(
        medium, radius[..., None], cosine[..., None], distance[..., None] * nodes
    )
    _, extinction = coefficients(medium, heights)
    depth = distance[..., None] * np.sum(extinction * weights[:, None], axis=-2)
    return np.exp(-depth)


def source(
    medium, height_km, view_cos, sun_cos, relative_cos, distances, sun_samples=64
):
    radius = medium.bottom_km + height_km
    height = height_along(medium, radius, view_cos, distances)
    here = medium.bottom_km + height
    view_sun = (
        view_cos * sun_cos
        + np.sqrt(max(0.0, 1.0 - view_cos**2))
        * np.sqrt(max(0.0, 1.0 - sun_cos**2))
        * relative_cos
    )
    sun_here = np.clip((radius * sun_cos + distances * view_sun) / here, -1.0, 1.0)
    reach = boundary_distance(here, sun_here, medium.top_km)
    sunlight = transmittance(medium, here, sun_here, reach, sun_samples)
    sunlight = np.where(hits_ground(medium, here, sun_here)[..., None], 0.0, sunlight)
    phase = 3.0 * (1.0 + view_sun**2) / (16.0 * np.pi)
    scattering, extinction = coefficients(medium, height)
    return extinction, scattering * sunlight * phase


def gauss_path(
    medium,
    height_km,
    view_cos,
    sun_cos,
    relative_cos,
    distance_km,
    samples,
    optical_samples=64,
):
    nodes, weights = quadrature(samples)
    distances = nodes * distance_km
    _, scattered = source(
        medium, height_km, view_cos, sun_cos, relative_cos, distances, optical_samples
    )
    toward_eye = transmittance(
        medium, medium.bottom_km + height_km, view_cos, distances, optical_samples
    )
    radiance = distance_km * np.sum(scattered * toward_eye * weights[:, None], axis=0)
    through = transmittance(
        medium, medium.bottom_km + height_km, view_cos, distance_km, optical_samples
    )
    return radiance, through


def midpoint_path(
    medium, height_km, view_cos, sun_cos, relative_cos, distance_km, samples
):
    step = distance_km / samples
    distances = (np.arange(samples) + 0.5) * step
    extinction, scattered = source(
        medium, height_km, view_cos, sun_cos, relative_cos, distances
    )
    step_depth = extinction * step
    previous_depth = np.cumsum(step_depth, axis=0) - step_depth
    integral = np.divide(
        -np.expm1(-step_depth),
        extinction,
        out=np.full_like(extinction, step),
        where=extinction > 0,
    )
    radiance = np.sum(np.exp(-previous_depth) * scattered * integral, axis=0)
    return radiance, np.exp(-np.sum(step_depth, axis=0))


def distant_sky(
    medium, height_km, view_cos, sun_cos, relative_cos, samples=512, optical_samples=64
):
    radius = medium.bottom_km + height_km
    horizon_cos = -np.sqrt(radius * radius - medium.bottom_km**2) / radius
    sky_cos = max(view_cos, horizon_cos)
    reach = boundary_distance(radius, sky_cos, medium.top_km)
    return gauss_path(
        medium,
        height_km,
        sky_cos,
        sun_cos,
        relative_cos,
        reach,
        samples,
        optical_samples,
    )[0]
