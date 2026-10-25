"""Tangent Gaussian reconstruction with matched quadrature and unaveraged normals."""

import numpy as np
from building_mass_lighting import shade
from building_mass_material_image import hit_materials, samples
from building_material_reference import face_materials
from building_surface_samples import validate
from current_visibility import project
from triangle_image_reference import average_samples, image_rays


def tangent_axes(normal):
    normal = np.asarray(normal, float)
    normal /= np.linalg.norm(normal, axis=1)[:, None]
    axes = np.eye(3)[np.argmin(abs(normal), axis=1)]
    u = np.cross(normal, axes)
    u /= np.linalg.norm(u, axis=1)[:, None]
    return normal, u, np.cross(normal, u)


def footprint_covariance(surfels, camera):
    p = surfels["positions"].astype(float)
    local = (p - camera[0]) @ camera[1].T
    if np.any(local[:, 2] <= 0.1):
        raise ValueError("source samples must remain beyond near plane")
    derivative = (
        camera[1][None, :2]
        - (local[:, :2] / local[:, 2, None])[:, :, None] * camera[1][None, 2]
    )
    derivative *= (camera[2] / local[:, 2])[:, None, None] * np.array([1, -1])[
        None, :, None
    ]
    normal, u, v = tangent_axes(surfels["normal"])
    tangent = np.stack((u, v), axis=2)
    jacobian = derivative @ tangent * float(surfels["spacing"])
    return jacobian @ jacobian.transpose(0, 2, 1), normal


def records(surfels, camera, box, rate, directions):
    centre, _ = project(surfels["positions"].astype(float), camera)
    covariance, normal = footprint_covariance(surfels, camera)
    width, height = map(int, box[1] - box[0])
    rays, depth, owners, weight = [], [], [], []
    tested = 0
    for owner, point in enumerate(surfels["positions"].astype(float)):
        if np.dot(point - camera[0], normal[owner]) >= 0:
            continue
        radius = np.sqrt(2 * np.diag(covariance[owner]))
        low = np.maximum(
            np.floor((centre[owner, :2] - radius - box[0]) * rate), 0
        ).astype(int)
        high = np.minimum(
            np.ceil((centre[owner, :2] + radius - box[0]) * rate),
            [width * rate, height * rate],
        ).astype(int)
        if np.any(high <= low):
            continue
        x, y = np.meshgrid(np.arange(low[0], high[0]), np.arange(low[1], high[1]))
        xy = np.column_stack((x.ravel(), y.ravel()))
        delta = (xy + 0.5) / rate + box[0] - centre[owner, :2]
        exponent = np.einsum(
            "ni,ij,nj->n", delta, np.linalg.inv(covariance[owner]), delta
        )
        tested += len(xy)
        selected = exponent < 2
        ray = xy[selected, 1] * width * rate + xy[selected, 0]
        denominator = directions[ray] @ normal[owner]
        distance = np.divide(
            (point - camera[0]) @ normal[owner],
            denominator,
            out=np.full_like(denominator, np.inf),
            where=denominator < 0,
        )
        z = distance * (directions[ray] @ camera[1][2])
        valid = (denominator < 0) & (z > 0.1) & np.isfinite(z)
        rays.append(ray[valid])
        depth.append(z[valid])
        owners.append(np.full(int(valid.sum()), owner, int))
        density = np.abs(normal[owner]).sum()
        weight.append(np.exp(-0.5 * exponent[selected][valid]) / (2 * np.pi * density))
    return tuple(
        np.concatenate(x) if x else np.empty(0, dtype=int if i in (0, 2) else float)
        for i, x in enumerate((rays, depth, owners, weight))
    ), {
        "gaussian_candidate_samples": tested,
        "gaussian_records": sum(len(v) for v in rays),
    }


def native_remainder(rest, at, camera, box, tables, rate):
    width, height = map(int, box[1] - box[0])
    count = width * height * rate**2
    colour, depth = np.zeros((count, 3)), np.full(count, np.inf)
    if not len(rest["indices"]):
        return colour, depth, 0
    fields, needs = face_materials(rest, at)
    if needs:
        raise ValueError("complete native remainder materials required")
    data = samples(rest, camera, box, rate)
    rgb, weights = hit_materials(rest, fields, data["points"], data["faces"])
    value, work = shade(
        rgb,
        weights,
        at["palette"],
        fields["normal"][data["faces"]],
        data["outgoing"],
        [0.45, -0.5, 0.7],
        tables,
    )
    colour[data["hit"]] = value
    depth[data["hit"]] = (data["points"] - camera[0]) @ camera[1][2]
    return colour, depth, work["ggx_lobe_evaluations"]


def render(surfels, rest, at, camera, box, tables, rate=16):
    validate(surfels)
    width, height = map(int, box[1] - box[0])
    count = width * height * rate**2
    if count > 1048576:
        raise ValueError("bounded diagnostic sample count required")
    _, directions = image_rays(camera, box, width, height, rate)
    remainder, native_depth, native_lobes = native_remainder(
        rest, at, camera, box, tables, rate
    )
    (ray, depth, owner, weight), counters = records(
        surfels, camera, box, rate, directions
    )
    front = depth < native_depth[ray]
    ray, depth, owner, weight = [a[front] for a in (ray, depth, owner, weight)]
    nearest = np.full(count, np.inf)
    np.minimum.at(nearest, ray, depth)
    selected = depth <= nearest[ray] + float(surfels["spacing"])
    ray, owner, weight = ray[selected], owner[selected], weight[selected]
    visible, mapped = np.unique(owner, return_inverse=True)
    outgoing = camera[0] - surfels["positions"][visible].astype(float)
    outgoing /= np.linalg.norm(outgoing, axis=1)[:, None]
    value, work = shade(
        surfels["albedo"][visible],
        surfels["roughness_weights"][visible],
        surfels["roughness_palette"],
        surfels["normal"][visible],
        outgoing,
        [0.45, -0.5, 0.7],
        tables,
    )
    density = np.bincount(ray, weights=weight, minlength=count)
    colour = np.stack(
        [
            np.bincount(ray, weights=weight * value[mapped, k], minlength=count)
            for k in range(3)
        ],
        axis=1,
        dtype=float,
    )
    alpha = np.minimum(density / 0.4, 1)
    colour = (
        np.divide(
            colour,
            density[:, None],
            out=np.zeros_like(colour),
            where=density[:, None] > 0,
        )
        * alpha[:, None]
    )
    colour += (1 - alpha[:, None]) * remainder
    transmission = (1 - alpha) * (~np.isfinite(native_depth))
    counters.update(
        {
            "visible_surface_samples_shaded": len(visible),
            "ggx_lobe_evaluations": work["ggx_lobe_evaluations"] + native_lobes,
            "native_remainder_ggx_lobes": native_lobes,
            "known_record_arrays_bytes": sum(
                v.nbytes for v in (ray, depth, owner, weight)
            ),
            "all_pixel_samples": count,
            "scope": "Matched regular SS16 box quadrature of tangent Gaussians, not analytic EWA lowpass. Individual normals/materials shade before kernel mixing; no normal/roughness average. Empirical paper cutoff c=1/tau=.4, depth grouping width=source grid spacing; sparse hull sampling and occlusion remain approximations, not a certificate.",
        }
    )
    return {
        "C": average_samples(colour, width, height, rate),
        "T": average_samples(transmission, width, height, rate),
    }, counters
