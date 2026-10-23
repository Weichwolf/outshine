"""Single-scattering homogeneous surface-flake model; no opaque depth output."""

import numpy as np
from packed_normals import unpack
from urban_parent_light import light_at, shade


def intervals(arrays, origins, directions, stop, minimum=0.0):
    pitch = arrays["pitch_m"]
    for brick, cell in enumerate(arrays["cells"]):
        low = cell * pitch
        bounds = np.stack((low, low + pitch))
        parallel = directions == 0
        t = np.divide(
            bounds[None] - origins[:, None],
            directions[:, None],
            out=np.zeros((len(origins), 2, 3)),
            where=~parallel[:, None],
        )
        near = np.maximum(minimum, np.where(parallel, -np.inf, t.min(1)).max(1))
        far = np.minimum(stop, np.where(parallel, np.inf, t.max(1)).min(1))
        outside = np.any(parallel & ((origins < low) | (origins > low + pitch)), axis=1)
        rays = np.flatnonzero((far > near) & ~outside)
        if len(rays):
            yield brick, rays, near[rays], far[rays]


def densities(arrays, directions):
    owners = np.repeat(np.arange(len(arrays["cells"])), np.diff(arrays["offsets"]))
    normals = unpack(arrays["normal_words"])[arrays["normal_ids"]]
    projected = abs(np.einsum("ij,ij->i", normals, directions[owners]))
    weights = arrays["area_m2"] * projected / np.prod(arrays["pitch_m"])
    return (
        np.bincount(owners, weights=weights, minlength=len(arrays["cells"])),
        owners,
        normals,
        weights,
    )


def shadow(arrays, extinction, points, directions, stop):
    tau = np.zeros(len(points))
    visits = 0
    for brick, rays, near, far in intervals(arrays, points, directions, stop):
        tau[rays] += extinction[brick] * (far - near)
        visits += len(rays)
    return np.exp(-tau), {
        "box_ray_tests": len(arrays["cells"]) * len(points),
        "occupied_ray_brick_visits": visits,
    }


def prepare_view(arrays, eye, profile, retained_visibility=None):
    centres = (arrays["cells"] + 0.5) * arrays["pitch_m"]
    outgoing = eye - centres
    outgoing /= np.linalg.norm(outgoing, axis=1)[:, None]
    light, _energy, limit = light_at(centres, profile)
    light_extinction = densities(arrays, light)[0]
    visibility, shadow_work = shadow(arrays, light_extinction, centres, light, limit)
    if retained_visibility is not None:
        kept = np.asarray(retained_visibility(centres), float)
        if kept.shape != visibility.shape or not np.isfinite(kept).all():
            raise ValueError("finite retained-caster visibility per brick required")
        if np.any((kept < 0) | (kept > 1)):
            raise ValueError("retained-caster visibility must lie in [0, 1]")
        visibility *= kept
    extinction, owners, normals, weights = densities(arrays, outgoing)
    facing = np.einsum("ij,ij->i", normals, outgoing[owners])
    normals = normals * np.where(facing >= 0, 1, -1)[:, None]
    colour = shade(
        centres[owners], normals, outgoing[owners], 0.6, visibility[owners], profile
    )
    source = np.zeros((len(centres), 3))
    np.add.at(source, owners, weights[:, None] * colour)
    return {
        "extinction": extinction,
        "source_rgb": source,
        "light_extinction": light_extinction,
    }, {
        "bricks": len(centres),
        "normal_distribution_evaluations": len(weights),
        "light_shadow": shadow_work,
        "retained_caster_samples": len(centres) if retained_visibility else 0,
    }


def render_groups(arrays, view, origins, directions, stop, receiver_points, profile):
    """Stop and receivers belong only to retained opaque background/fallback."""
    records = []
    for brick, rays, near, far in intervals(
        arrays, origins, directions, stop, minimum=0.1
    ):
        records.append((np.full(len(rays), brick), rays, near, far))
    colour = np.zeros_like(origins)
    transmission = np.ones(len(origins))
    if records:
        brick, rays, near, far = (
            np.concatenate([row[i] for row in records]) for i in range(4)
        )
        order = np.lexsort((near, rays))
        brick, rays, near, far = (a[order] for a in (brick, rays, near, far))
        tau = view["extinction"][brick] * (far - near)
        cumulative = np.cumsum(tau)
        before = cumulative - tau
        first = np.r_[True, rays[1:] != rays[:-1]]
        base = np.repeat(
            before[first], np.diff(np.r_[np.flatnonzero(first), len(rays)])
        )
        weight = np.exp(-(before - base)) * -np.expm1(-tau)
        radiance = np.divide(
            view["source_rgb"][brick],
            view["extinction"][brick, None],
            out=np.zeros((len(brick), 3)),
            where=view["extinction"][brick, None] > 0,
        )
        np.add.at(colour, rays, weight[:, None] * radiance)
        transmission = np.exp(-np.bincount(rays, weights=tau, minlength=len(origins)))
        visits = len(rays)
    else:
        visits = 0
    visibility = np.ones(len(origins))
    receivers = np.flatnonzero(np.isfinite(stop))
    light, _energy, limit = light_at(receiver_points[receivers], profile)
    values, shadow_work = shadow(
        arrays, view["light_extinction"], receiver_points[receivers], light, limit
    )
    visibility[receivers] = values
    return (
        colour,
        transmission,
        visibility,
        {
            "primary_box_ray_tests": len(arrays["cells"]) * len(origins),
            "primary_ray_brick_visits": visits,
            "receiver_shadow": shadow_work,
            "sort_records": visits,
        },
    )
