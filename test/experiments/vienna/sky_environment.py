"""Scene-linear environment radiance for matched Vienna lighting comparisons."""

from dataclasses import dataclass

import numpy as np
from cloud_profile import SUN_MAP_SIZE

SKY_ENVIRONMENT_SIZE = (512, 256)
CLOUD_ENVIRONMENT_SIZE = (2048, 1024)


@dataclass(frozen=True)
class SkyEnvironment:
    radiance: np.ndarray
    cloud_sun: float
    cloud_sun_map: np.ndarray | None = None


def environment_path(directory, place):
    return directory / f"{place['id']}-sky.npz"


def save_environment(
    path,
    radiance,
    sun,
    eye_m,
    *,
    aerosol=0.0,
    clouds=None,
    cloud_sun=1.0,
    cloud_sun_map=None,
):
    path.parent.mkdir(parents=True, exist_ok=True)
    np.savez(
        path,
        radiance=radiance,
        sun_enu=sun,
        eye_m=eye_m,
        aerosol=aerosol,
        clouds=clouds.archive_key if clouds else (),
        cloud_sun=cloud_sun,
        **({"cloud_sun_map": cloud_sun_map} if cloud_sun_map is not None else {}),
    )


def load_environment(path, sun, eye_m, *, aerosol=0.0, clouds=None):
    with np.load(path, allow_pickle=False) as sky:
        radiance = sky["radiance"]
        saved_sun = sky["sun_enu"]
        if (
            saved_sun.shape != (3,)
            or not np.allclose(saved_sun, sun, rtol=0, atol=1e-6)
            or not np.isclose(sky["eye_m"].item(), eye_m, rtol=0, atol=0.01)
        ):
            raise ValueError(f"{path}: sky sun or eye altitude differs from this view")
        saved_aerosol = np.asarray(sky.get("aerosol", 0.0))
        if saved_aerosol.shape or not np.isclose(
            saved_aerosol, aerosol, rtol=0, atol=1e-7
        ):
            raise ValueError(f"{path}: sky aerosol extinction differs from this view")
        expected_clouds = np.array(clouds.archive_key if clouds else ())
        saved_clouds = np.asarray(sky.get("clouds", ()))
        if (
            clouds
            and saved_clouds.shape == expected_clouds.shape
            and saved_clouds[0] == 1
        ):
            # The first revision differs in cloud radiance and shares its density field.
            expected_clouds[0] = 1
        if saved_clouds.shape != expected_clouds.shape or not np.allclose(
            saved_clouds, expected_clouds, rtol=0, atol=1e-7
        ):
            raise ValueError(f"{path}: sky cloud recipe differs from this view")
        saved_sun_fraction = np.asarray(sky.get("cloud_sun", np.nan if clouds else 1.0))
        if saved_sun_fraction.shape or not 0.0 <= saved_sun_fraction <= 1.0:
            raise ValueError(f"{path}: expected a finite solar transmission fraction")
        cloud_sun = saved_sun_fraction.item()
        cloud_sun_map = sky.get("cloud_sun_map")
        if cloud_sun_map is not None and (
            clouds is None
            or cloud_sun_map.shape != (SUN_MAP_SIZE, SUN_MAP_SIZE)
            or cloud_sun_map.dtype != np.float32
            or not np.isfinite(cloud_sun_map).all()
            or (cloud_sun_map < 0).any()
            or (cloud_sun_map > 1).any()
        ):
            raise ValueError(f"{path}: expected a finite cloud solar transmission map")
    size = CLOUD_ENVIRONMENT_SIZE if clouds else SKY_ENVIRONMENT_SIZE
    if (
        radiance.shape != (*size[::-1], 3)
        or radiance.dtype != np.float32
        or not np.isfinite(radiance).all()
        or (radiance < 0).any()
    ):
        raise ValueError(f"{path}: expected finite scene-linear RGB sky radiance")
    return SkyEnvironment(radiance, cloud_sun, cloud_sun_map)


def diffuse_coefficients(radiance):
    """Project the environment and apply the Lambert kernel divided by pi."""
    height, width, _ = radiance.shape
    longitude = np.pi - 2 * np.pi * (np.arange(width) + 0.5) / width
    edges = np.linspace(-np.pi / 2, np.pi / 2, height + 1)
    latitude = (edges[1:] + edges[:-1]) * 0.5
    z = np.broadcast_to(np.sin(latitude)[:, None], (height, width))
    cos_lat = np.cos(latitude)[:, None]
    x, y = cos_lat * np.cos(longitude), cos_lat * np.sin(longitude)
    basis = np.stack(
        (
            np.full_like(z, 0.2820947918),
            0.4886025119 * y,
            0.4886025119 * z,
            0.4886025119 * x,
            1.092548431 * x * y,
            1.092548431 * y * z,
            0.3153915653 * (3 * z * z - 1),
            1.092548431 * x * z,
            0.5462742153 * (x * x - y * y),
        ),
        axis=-1,
    )
    solid_angle = (2 * np.pi / width) * np.diff(np.sin(edges))
    weighted = radiance * solid_angle[:, None, None]
    kernel = np.array([1, 2 / 3, 2 / 3, 2 / 3, 1 / 4, 1 / 4, 1 / 4, 1 / 4, 1 / 4])
    coefficients = basis.reshape(-1, 9).T @ weighted.reshape(-1, 3)
    return (coefficients * kernel[:, None]).astype("f4")


def sample_radiance(radiance, directions):
    """Bilinear lookup in the shared east/north/up equirectangular archive."""
    height, width, _ = radiance.shape
    east, north, up = np.asarray(directions).T
    longitude = np.where(east**2 + north**2 > 1e-10, np.arctan2(north, east), 0)
    u = (0.5 - longitude / (2 * np.pi)) * width - 0.5
    v = np.clip(
        (0.5 + np.arcsin(np.clip(up, -1, 1)) / np.pi) * height - 0.5,
        0,
        height - 1,
    )
    x, y = np.floor(u).astype(int), np.floor(v).astype(int)
    s, t = (u - x)[:, None], (v - y)[:, None]
    left, right = x % width, (x + 1) % width
    upper = np.minimum(y + 1, height - 1)
    bottom = (1 - s) * radiance[y, left] + s * radiance[y, right]
    top = (1 - s) * radiance[upper, left] + s * radiance[upper, right]
    return (1 - t) * bottom + t * top
