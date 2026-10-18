"""Direct opaque visibility at saved contacts plus the existing thin-sheet response."""

import numpy as np
from grass_projection import guard_bounds, query_domain
from outshine_ref.vegetation.leaf_lighting import leaf_diffuse_response


def require_light_support(recipe, lower, upper, view, lights, bias):
    # Positive-height paths occupy the convex hull of0,H*view.xy/z,H*light.xy/z.
    bounds = np.asarray(recipe["root_bounds_m"])
    for direction in [view, *lights]:
        lo, hi = guard_bounds(
            lower,
            upper,
            direction,
            recipe["maximum_radius_m"] + 2 * bias,
            recipe["maximum_height_m"] + 2 * bias,
        )
        if np.any(lo < bounds[0]) or np.any(hi > bounds[1]):
            raise ValueError("missing root support for complete view/light paths")


def contact_points(view, lower, upper, product):
    size = len(product["coverage"])
    lower, upper, view = query_domain(lower, upper, view, size)
    axis = (np.arange(size) + 0.5) / size
    x, y = np.meshgrid(
        lower[0] + axis * (upper[0] - lower[0]), lower[1] + axis * (upper[1] - lower[1])
    )
    ground = np.c_[x.ravel(), y.ravel(), np.zeros(size * size)]
    covered = product["face"].ravel() >= 0
    height = product["height"].ravel()
    if (
        not np.isfinite(height).all()
        or np.any(height < 0)
        or np.any(height[~covered] != 0)
    ):
        raise ValueError(
            "nonnegative saved contact heights and explicit gap validity required"
        )
    points = ground + height[:, None] * view / view[2]
    normals = product["normal"].reshape(-1, 3).copy()
    normals[~covered] = (0, 0, 1)
    return ground, points, normals, covered


def lighting(
    index, geometry_recipe, product, footprint, view, state, bias=2e-6, batch=8192
):
    directions = [np.asarray(lamp["direction"], float) for lamp in state["lights"]]
    require_light_support(geometry_recipe, *footprint, view, directions, bias)
    ground, points, normals, covered = contact_points(view, *footprint, product)
    optical = np.broadcast_to(
        np.asarray(state["leaf_rt"], float), (int(covered.sum()), 6)
    )
    substrate = np.asarray(state["ground_albedo"], float)
    if (
        substrate.shape != (3,)
        or not np.isfinite(substrate).all()
        or np.any((substrate < 0) | (substrate > 1))
    ):
        raise ValueError("bounded substrate reflectance required")
    response, reflection_only, independent = (np.zeros_like(points) for _ in range(3))
    logs = []
    for lamp, direction in zip(state["lights"], directions, strict=True):
        irradiance = np.asarray(lamp["irradiance_rgb"], float)
        if (
            irradiance.shape != (3,)
            or not np.isfinite(irradiance).all()
            or np.any(irradiance < 0)
        ):
            raise ValueError("nonnegative finite declared beam irradiance required")
        visibility, work = index.visibility(points, normals, direction, bias, batch)
        ground_normals = np.broadcast_to(np.array((0, 0, 1.0)), ground.shape)
        ground_visibility, ground_work = index.visibility(
            ground, ground_normals, direction, bias, batch
        )
        weights = np.broadcast_to(substrate * direction[2] / np.pi, points.shape).copy()
        weights[covered] = leaf_diffuse_response(
            optical, normals[covered], view, direction
        )
        reflected = weights.copy()
        no_t = optical.copy()
        no_t[:, 3:] = 0
        reflected[covered] = leaf_diffuse_response(
            no_t, normals[covered], view, direction
        )
        response += weights * irradiance * visibility[:, None]
        reflection_only += reflected * irradiance * visibility[:, None]
        independent += weights * irradiance * float(ground_visibility.mean())
        logs.append(
            {
                "name": lamp["name"],
                "surface_visible_fraction": float(visibility.mean()),
                "leaf_visible_fraction": float(visibility[covered].mean())
                if covered.any()
                else None,
                "ground_visible_fraction": float(ground_visibility.mean()),
                "work": work,
                "ground_work": ground_work,
            }
        )
    size = len(product["coverage"])
    return {
        "radiance": response.reshape(size, size, 3),
        "reflection_only": reflection_only.reshape(size, size, 3),
        "independent_mean": independent.reshape(size, size, 3),
        "lights": logs,
    }
