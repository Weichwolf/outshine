"""Conservative cached-plant view and sunlight demand before renderer objects."""

import numpy as np


def spheres(bounds, positions, frames, scales):
    bounds, positions, frames, scales = map(
        np.asarray, (bounds, positions, frames, scales)
    )
    if (
        bounds.shape != (2, 3)
        or np.any(bounds[0] > bounds[1])
        or not np.isfinite(bounds).all()
    ):
        raise ValueError("finite ordered complete angular-product bounds required")
    if (
        positions.ndim != 2
        or positions.shape[1:] != (3,)
        or len(positions) > 250000
        or frames.shape != (len(positions), 3, 3)
        or scales.shape != (len(positions),)
        or not all(np.isfinite(a).all() for a in (positions, frames, scales))
        or np.any(scales <= 0)
    ):
        raise ValueError(
            "bounded finite site frames and positive uniform scales required"
        )
    if len(frames) and (
        np.max(abs(np.einsum("nji,njk->nik", frames, frames) - np.eye(3))) > 2e-12
        or np.max(abs(np.linalg.det(frames) - 1)) > 2e-12
    ):
        raise ValueError("proper rigid site frames required")
    center = np.mean(bounds, axis=0)
    radius = np.linalg.norm((bounds[1] - bounds[0]) / 2)
    return positions + np.einsum("nij,j->ni", frames, center) * scales[
        :, None
    ], radius * scales


def demand(
    camera,
    ground,
    centers,
    radii,
    sun,
    min_receiver_z,
    *,
    near_m=0.1,
    tolerance_m=0.05,
    sun_angular_radius_deg=0,
):
    """Keep spheres/sunlight sweeps intersecting the expanded camera halfspaces.

    A sweep ends below every declared receiver. This is conservative rejection,
    without an occlusion or exact sphere/polyhedron-intersection claim.
    """
    centers, radii, sun = map(np.asarray, (centers, radii, sun))
    if (
        centers.ndim != 2
        or centers.shape[1:] != (3,)
        or len(centers) > 250000
        or radii.shape != (len(centers),)
        or sun.shape != (3,)
        or not all(np.isfinite(a).all() for a in (centers, radii, sun))
        or np.any(radii < 0)
        or not np.isfinite((min_receiver_z, near_m, tolerance_m)).all()
        or not 0 < near_m < camera.sight_m
        or tolerance_m < 0
        or not np.isfinite(sun_angular_radius_deg)
        or not 0 <= sun_angular_radius_deg <= 5
        or np.linalg.norm(sun) == 0
    ):
        raise ValueError(
            "finite bounded spheres, light, receiver floor and lens required"
        )
    right, up, back = camera.basis.T
    forward = -back
    ty = np.tan(np.deg2rad(camera.vertical_fov_deg) / 2)
    tx = ty * camera.native_profile[0] / camera.native_profile[1]
    normals = np.array(
        (
            forward,
            -forward,
            right + tx * forward,
            -right + tx * forward,
            up + ty * forward,
            -up + ty * forward,
        )
    )
    lengths = np.linalg.norm(normals, axis=1)
    normals /= lengths[:, None]
    offsets = np.array((-near_m, camera.sight_m, 0, 0, 0, 0)) / lengths
    relative = centers - camera.eye(ground)
    values = relative @ normals.T + offsets
    guard = (
        64
        * np.finfo(float).eps
        * (np.max(abs(relative), axis=1) + camera.sight_m + radii + 1)
    )
    radius = radii + tolerance_m + guard
    visible = (values >= -radius[:, None]).all(axis=1)
    sun = sun / np.linalg.norm(sun)
    angle = np.deg2rad(sun_angular_radius_deg)
    minimum_downward = sun[2] * np.cos(angle) - np.sqrt(
        max(0, 1 - sun[2] ** 2)
    ) * np.sin(angle)
    if minimum_downward <= 1e-6:
        return {"camera": visible, "sun": np.ones(len(centers), dtype=bool)}
    lower = np.zeros(len(centers))
    upper = np.maximum(0, (centers[:, 2] + radius - min_receiver_z) / minimum_downward)
    shadow = np.ones(len(centers), dtype=bool)
    # Every unit ray within the light cone differs by at most 2*sin(angle/2).
    # Swept spheres grow by this much per metre; retain finite-sun penumbrae.
    spread = 2 * np.sin(angle / 2)
    for index, slope in enumerate(-sun @ normals.T + spread):
        if slope == 0:
            shadow &= values[:, index] >= -radius
        else:
            crossing = (-radius - values[:, index]) / slope
            if slope > 0:
                lower = np.maximum(lower, crossing)
            else:
                upper = np.minimum(upper, crossing)
    shadow &= upper >= lower
    return {"camera": visible, "sun": shadow}
