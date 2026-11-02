"""Static joint-material partition, fitted before camera or lighting queries."""

from dataclasses import dataclass

import numpy as np
from relief_shading_reference import normals
from scipy.cluster.vq import kmeans2
from terrain_material_grid import frozen


def features(raw):
    raw = np.asarray(raw, dtype=float)
    if (
        raw.ndim != 2
        or raw.shape[1] != 6
        or not 1 <= len(raw) <= 262144
        or not np.isfinite(raw).all()
    ):
        raise ValueError("bounded finite joint material points required")
    return np.column_stack(
        (
            np.clip(raw[:, :3], 0, 1),
            normals(raw[:, 4:]),
            2 * np.log(np.clip(raw[:, 3], 0.04, 1)),
        )
    )


@dataclass(frozen=True, slots=True)
class MaterialCodebook:
    centres: np.ndarray

    def classify(self, raw):
        delta = features(raw)[:, None] - self.centres[None]
        return np.argmin(np.sum(delta * delta, axis=-1), axis=-1)


def fit(raw):
    """At most four labels, 32 offline Lloyd iterations; no per-query refitting."""
    values = features(raw)
    unique = np.unique(values, axis=0)
    if len(unique) <= 4:
        centres = unique
    else:
        centres, _ = kmeans2(
            values,
            4,
            iter=32,
            minit="++",
            missing="raise",
            rng=np.random.default_rng(1934),
        )
    centres = centres[np.lexsort(centres.T[::-1])]
    return MaterialCodebook(frozen(centres))


def fit_recipe(evaluate, origin):
    """One fixed two-scale training bank; camera footprint never enters the fit."""
    axis = (np.arange(64) + 0.5) / 64
    x, y = np.meshgrid(axis, axis)
    unit = np.column_stack((x.ravel(), y.ravel()))
    step = 1e-7
    dx, dy = np.tile([step, 0], (len(unit), 1)), np.tile([0, step], (len(unit), 1))
    samples = []
    for extent in (0.0064, 6.4):
        raw = np.asarray(evaluate(np.asarray(origin) + unit * extent, dx, dy)).copy()
        raw[:, 4:] /= step
        samples.append(raw)
    return fit(np.concatenate(samples))
