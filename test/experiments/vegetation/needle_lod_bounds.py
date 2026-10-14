"""Own parameter-interpolation position envelopes for closed needle prototypes."""

import numpy as np
from outshine_ref.vegetation import Needle
from outshine_ref.vegetation.needle_sampling import axial_parameters
from outshine_ref.vegetation.needle_section import section_sampling


def radial_bounds(shape, segments, axial_sampling="uniform"):
    first = axial_parameters(segments, shape.taper_start, axial_sampling)
    last = np.r_[first[1:], 1.0]
    start, radius = shape.taper_start, shape.radius
    lo, hi = np.clip(first / start, 0, 1), np.clip(last / start, 0, 1)
    peak = np.clip(0.5, lo, hi)
    body_first = 0.45 * 6 * radius * peak * (1 - peak) / start
    body_second = (
        0.45 * 6 * radius * np.maximum(abs(1 - 2 * lo), abs(1 - 2 * hi)) / start**2
    )
    body_second = np.where(first < start, body_second, 0)
    tip_first = 2 * radius * np.clip(last - start, 0, 1 - start) / (1 - start) ** 2
    tip_second = np.where(last > start, 2 * radius / (1 - start) ** 2, 0)
    maximum = shape.radial_profile(np.clip(start, first, last))[0]
    return (
        maximum,
        np.maximum(body_first, tip_first),
        np.maximum(body_second, tip_second),
    )


def angular_bounds(shape, sides):
    angles, _, tags = section_sampling(shape.section, sides)
    span = np.remainder(np.roll(angles, -1) - angles, 2 * np.pi)
    if shape.section == "ellipse":
        first, second = np.ones_like(span), np.ones_like(span)
    elif shape.section == "rhombus":
        first = np.full_like(span, np.hypot(1, shape.aspect) / (np.pi / 2))
        second = np.zeros_like(span)
    else:
        frequency = 2 / shape.sector_count
        first = np.where(tags == 1, frequency, 2 / np.pi)
        second = np.where(tags == 1, frequency**2, 0)
    return angles, span, first, second


def needle_mesh_bound(shape, segments, sides, axial_sampling="uniform"):
    if not isinstance(shape, Needle) or not 1e-6 <= shape.length <= 1000:
        raise ValueError("bounded physical Needle required")
    if (
        type(segments) is not int
        or not 2 <= segments <= 128
        or type(sides) is not int
        or sides not in (4, 8, 16, 32)
    ):
        raise ValueError("supported needle subdivisions required")
    parameters = axial_parameters(segments, shape.taper_start, axial_sampling)
    step = np.diff(np.r_[parameters, 1.0])
    radius, first, second = radial_bounds(shape, segments, axial_sampling)
    _, span, angular_first, angular_second = angular_bounds(shape, sides)
    speed = abs(shape.bend) + abs(shape.twist)
    axial = (
        shape.length * abs(shape.bend) + second + 2 * first * speed + radius * speed**2
    )
    longitudinal = axial[:, None] * step[:, None] ** 2 / 8
    transverse = radius[:, None] * angular_second[None] * span[None] ** 2 / 8
    mixed = (
        (first + radius * speed)[:, None]
        * angular_first[None]
        * span[None]
        * step[:, None]
        / 4
    )
    mixed[-1] = 0
    error = float(np.max(longitudinal + transverse + mixed))
    padding = 64 * np.finfo(float).eps * max(shape.length, shape.radius)
    return (error + padding) * (1 + 64 * np.finfo(float).eps)


def projected_mesh_bound(
    shape, segments, sides, origins, scales, camera, axial_sampling="uniform"
):
    scales = np.asarray(scales)
    reach = float(scales.max()) * (shape.length + shape.radius)
    lower, upper = np.min(origins, axis=0) - reach, np.max(origins, axis=0) + reach
    projection = camera.projected_span(lower, upper) / np.linalg.norm(upper - lower)
    return float(
        projection
        * scales.max()
        * needle_mesh_bound(shape, segments, sides, axial_sampling)
    )


def independent_interpolation_samples(shape, segments, sides, axial_sampling="uniform"):
    angles, span, _, _ = angular_bounds(shape, sides)
    parameters = axial_parameters(segments, shape.taper_start, axial_sampling)
    steps = np.diff(np.r_[parameters, 1.0])
    i, j, u, v = np.meshgrid(
        np.arange(segments),
        np.arange(len(angles)),
        np.linspace(0, 1, 9),
        np.linspace(0, 1, 9),
        indexing="ij",
    )
    t0, a0, h = parameters[i.ravel()], angles[j.ravel()], steps[i.ravel()]
    u, v, w = u.ravel(), v.ravel(), span[j.ravel()]
    a = shape.evaluate(t0, a0)[0]
    b = shape.evaluate(t0, a0 + w)[0]
    c = shape.evaluate(t0 + h, a0 + w)[0]
    d = shape.evaluate(t0 + h, a0)[0]
    weights = np.where(
        (u >= v)[:, None],
        np.column_stack((1 - u, np.zeros_like(u), v, u - v)),
        np.column_stack((1 - v, v - u, u, np.zeros_like(u))),
    )
    approximate = np.einsum("ni,nij->nj", weights, np.stack((a, b, c, d), axis=1))
    top = i.ravel() == segments - 1
    approximate[top] = (
        ((1 - u[top]) * (1 - v[top]))[:, None] * a[top]
        + ((1 - u[top]) * v[top])[:, None] * b[top]
        + u[top, None] * c[top]
    )
    actual = shape.evaluate(t0 + u * h, a0 + v * w)[0]
    errors = np.linalg.norm(actual - approximate, axis=1)
    bound = needle_mesh_bound(shape, segments, sides, axial_sampling)
    if errors.max() > bound:
        raise AssertionError("sample escaped analytic parameter envelope")
    return {
        "samples": len(errors),
        "max_sample_error_m": float(errors.max()),
        "bound_m": bound,
    }
