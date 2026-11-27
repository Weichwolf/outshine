"""Scene-linear environment radiance for matched Vienna lighting comparisons."""

import numpy as np

SKY_ENVIRONMENT_SIZE = (512, 256)


def environment_path(directory, place):
    return directory / f"{place['id']}-sky.npz"


def save_environment(path, radiance, sun, eye_m):
    path.parent.mkdir(parents=True, exist_ok=True)
    np.savez(path, radiance=radiance, sun_enu=sun, eye_m=eye_m)


def load_environment(path, sun, eye_m):
    with np.load(path, allow_pickle=False) as sky:
        radiance = sky["radiance"]
        saved_sun = sky["sun_enu"]
        if (
            saved_sun.shape != (3,)
            or not np.allclose(saved_sun, sun, rtol=0, atol=1e-6)
            or not np.isclose(sky["eye_m"].item(), eye_m, rtol=0, atol=0.01)
        ):
            raise ValueError(f"{path}: sky sun or eye altitude differs from this view")
    if (
        radiance.shape != (*SKY_ENVIRONMENT_SIZE[::-1], 3)
        or radiance.dtype != np.float32
        or not np.isfinite(radiance).all()
        or (radiance < 0).any()
    ):
        raise ValueError(f"{path}: expected finite scene-linear RGB sky radiance")
    return radiance


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
