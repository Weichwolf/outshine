"""Camera-independent response samples of the unchanged owned dielectric basis."""

import numpy as np
from material_filter import direct_ggx


def angular_samples(*, validation=False):
    cosines = np.array([1e-6, 0.001, 0.01, 0.05, 0.15, 0.35, 0.6, 0.85, 1.0])
    azimuth = np.linspace(0, np.pi, 17)
    if validation:
        cosines = np.sqrt(cosines[:-1] * cosines[1:])
        azimuth = np.r_[(azimuth[:-1] + azimuth[1:]) / 2, np.pi]
    return np.stack(
        np.meshgrid(cosines, cosines, azimuth, indexing="ij"), axis=-1
    ).reshape(-1, 3)


def response_basis(palette, tables, samples):
    palette = np.asarray(palette, float)
    if (
        palette.ndim != 1
        or not 0 < len(palette) <= 16
        or not np.isfinite(palette).all()
        or np.any((palette <= 0) | (palette > 1))
    ):
        raise ValueError("bounded positive owned roughness palette required")
    samples = np.asarray(samples, float)
    if samples.ndim != 2 or samples.shape[1] != 3 or not 0 < len(samples) <= 16384:
        raise ValueError("bounded light/view cosine and azimuth samples required")
    nl, nv, phi = samples.T
    if not np.isfinite(samples).all() or np.any(
        (samples[:, :2] <= 0) | (samples[:, :2] > 1)
    ):
        raise ValueError("finite upper-hemisphere samples required")
    rows = np.zeros((len(samples), 20))
    rows[:, :3], rows[:, 4:7], rows[:, 7], rows[:, 10] = 1, 0.04, 1, 1
    rows[:, 12], rows[:, 14] = np.sqrt(1 - nl * nl), nl
    radial = np.sqrt(1 - nv * nv)
    rows[:, 16:19] = np.c_[radial * np.cos(phi), radial * np.sin(phi), nv]
    spectrum = np.zeros((len(samples), len(palette)))
    diffuse = None
    for component, roughness in enumerate(palette):
        rows[:, 3] = roughness
        value = direct_ggx(rows, tables)
        if diffuse is not None and not np.array_equal(diffuse, value[:, 0]):
            raise ValueError("this fit requires roughness-independent owned diffuse")
        if not np.array_equal(value[:, 4], value[:, 5]) or not np.array_equal(
            value[:, 5], value[:, 6]
        ):
            raise ValueError(
                "this fit requires the owned grey dielectric specular response"
            )
        diffuse = value[:, 0].copy()
        spectrum[:, component] = value[:, 4]
    if not np.isfinite(spectrum).all() or np.any(spectrum < 0):
        raise ValueError("finite nonnegative owned specular basis required")
    return {"specular": spectrum, "diffuse": diffuse, "samples": samples}
