"""Authored limb-anchored attraction cohorts, not a botanical growth model."""

import numpy as np
from oak_scaffold import OakRecipe


def cohorts(recipe, placement="limb"):
    """Seven analytic ellipsoids inside the shared authored crown box."""
    if not isinstance(recipe, OakRecipe):
        raise TypeError("explicit bounded OakRecipe required")
    if placement not in ("raised", "limb"):
        raise ValueError("explicit raised or limb cohort placement required")
    positions, parents = recipe.skeleton()
    children = np.bincount(parents[1:], minlength=len(parents))
    hosts = np.flatnonzero(children == 0)
    # The trunk endpoint owns the upper central cohort; six others are limb ends.
    trunk_count = len(recipe.skeleton(False)[1])
    centre_host = trunk_count - 1
    hosts = np.r_[hosts[hosts != centre_host], centre_host]
    lower = np.array([-recipe.spread_m / 2] * 2 + [recipe.height_m * 0.23])
    upper = np.array([recipe.spread_m / 2] * 2 + [recipe.height_m])
    rng = np.random.Generator(
        np.random.PCG64(np.random.SeedSequence((recipe.seed, 736)))
    )
    centres, transforms = [], []
    for host in hosts:
        radial = positions[host].copy()
        radial[2] = 0
        radial /= np.linalg.norm(radial)
        side = np.cross([0, 0, 1], radial)
        frame = np.column_stack((radial, side, [0, 0, 1]))
        if host == centre_host:
            centre = np.array([0.0, 0.0, recipe.height_m * 0.86])
            radii = np.array([recipe.spread_m * 0.2] * 2 + [recipe.height_m * 0.14])
        else:
            raised = placement == "raised"
            centre = positions[host] * ([0.9, 0.9, 1] if raised else [1.1, 1.1, 1])
            centre[2] += recipe.height_m * rng.uniform(
                *((0.12, 0.16) if raised else (0.04, 0.08))
            )
            radii = np.array(
                [
                    recipe.spread_m
                    * rng.uniform(*((0.2, 0.25) if raised else (0.25, 0.30))),
                    recipe.spread_m
                    * rng.uniform(*((0.15, 0.21) if raised else (0.12, 0.18))),
                    recipe.height_m
                    * rng.uniform(*((0.16, 0.21) if raised else (0.12, 0.18))),
                ]
            )
        transform = frame * radii
        extent = np.linalg.norm(transform, axis=1)
        room = np.minimum(centre - lower, upper - centre)
        if np.any(room <= 0):
            raise ValueError("authored scaffold cohort centre outside crown box")
        transform *= min(1.0, float(np.min(room / extent)))
        centres.append(centre)
        transforms.append(transform)
    return {
        "hosts": hosts.astype(np.int32),
        "centres": np.array(centres),
        "transforms": np.array(transforms),
        "lower": lower,
        "upper": upper,
    }


def sample_space(recipe, placement="limb"):
    """Fixed-size volume-weighted mixture; overlap deliberately has extra density."""
    data = cohorts(recipe, placement)
    volume = abs(np.linalg.det(data["transforms"]))
    exact = recipe.attractor_count * volume / volume.sum()
    counts = np.floor(exact).astype(np.int64)
    order = np.argsort(-(exact - counts), kind="stable")
    counts[order[: recipe.attractor_count - int(counts.sum())]] += 1
    owners = np.repeat(np.arange(len(counts), dtype=np.int32), counts)
    rng = np.random.Generator(
        np.random.PCG64(np.random.SeedSequence((recipe.seed, 736, 1)))
    )
    unit = rng.normal(size=(recipe.attractor_count, 3))
    unit /= np.linalg.norm(unit, axis=1)[:, None]
    unit *= rng.random(recipe.attractor_count)[:, None] ** (1 / 3)
    points = data["centres"][owners] + np.einsum(
        "nij,nj->ni", data["transforms"][owners], unit
    )
    return points, owners, {**data, "counts": counts}
