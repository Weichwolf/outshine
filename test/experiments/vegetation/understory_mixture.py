"""Prepared spatial form marks on unchanged signed ground-cover root identities."""

import numpy as np
from ground_cover_scene import tuft_poses
from outshine_ref.vegetation import SpeciesHeightMixture
from species_competition import CompetingMarks

FORMS = {"convallaria-study": 0.05, "short-grass": 0.65, "tall-grass": 0.30}


def cover_keys(ids):
    ids = np.asarray(ids)
    if (
        ids.ndim != 2
        or ids.shape[1:] != (3,)
        or ids.dtype.kind != "i"
        or len(ids) > 1_000_000
        or np.any((ids[:, :2] < -100_000_000) | (ids[:, :2] > 100_000_000))
        or np.any((ids[:, 2] < 0) | (ids[:, 2] > 1_000_000))
    ):
        raise ValueError("bounded signed cover cell/ordinal IDs required")
    keys = ids.copy()
    xy = ids[:, :2]
    keys[:, :2] = np.where(xy >= 0, 2 * xy, -2 * xy - 1)
    return keys


def prepare(seed=316):
    names = tuple(sorted(FORMS))
    weights = tuple(FORMS[name] for name in names)
    mixture = SpeciesHeightMixture.prepare(names, (0, 1), (weights, weights))
    fields = CompetingMarks.prepare(
        names, seed, length_m=1.8, count=16, coordinate_frame="stand-planar-metres"
    )
    return mixture, fields


def selection(sites, mixture, fields, strength=0.92, score_mode="stream"):
    poses = tuft_poses(sites)
    poses["classes"] = fields.choose(
        mixture,
        poses["origins"],
        cover_keys(sites["ids"]),
        np.zeros(len(sites["ids"])),
        strength,
        score_mode,
    )
    if np.any(poses["classes"] < 0):
        raise ValueError("unassigned rooted study form")
    return poses


def shuffled_control(poses, seed=317):
    """Compare exact same intrinsic form packets at independently permuted roots.

    This whole-stand control is not the crop-replay generator.
    """
    order = np.random.Generator(np.random.PCG64(seed)).permutation(len(poses["ids"]))
    return {
        key: value.copy() if key in ("origins", "ids") else value[order]
        for key, value in poses.items()
    }
