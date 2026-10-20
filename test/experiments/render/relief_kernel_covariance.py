"""Finite auxiliary GGX covariance and a rejected residual Taylor candidate (#938)."""

from collections import Counter

import numpy as np
from relief_finite_kernel import kernel_moments, point_response, transform
from relief_shading_reference import normals, specular_from_dots
from scipy.integrate import quad


def second_moment(vertices, work):
    """Integrate y*y'/(1+y.y)^2 over one transformed convex CCW polygon.

    J=integral 1/(1+y.y) is reduced to smooth edge integrals. QUADPACK's
    estimate is recorded, not called a rigorous floating-point error bound.
    """
    boundary = np.zeros((2, 2))
    radial = 0.0
    estimated_error = 0.0
    for a, b in zip(vertices, np.roll(vertices, -1, axis=0)):
        edge = b - a
        squared = edge @ edge
        if squared == 0:
            raise ValueError("nonzero polygon edges required")
        cross = a[0] * b[1] - a[1] * b[0]

        def integrand(t, a=a, edge=edge):
            point = a + t * edge
            radius = point @ point
            return np.log1p(radius) / radius if radius else 1.0

        result = quad(
            integrand, 0, 1, epsabs=1e-12, epsrel=1e-12, limit=50, full_output=1
        )
        if len(result) != 3:
            raise ValueError(f"edge quadrature failed: {result[3]}")
        integral, error, info = result
        radial += 0.5 * cross * integral
        estimated_error += 0.25 * abs(cross) * error
        work.update(
            quad_calls=1, quad_evaluations=info["neval"], quad_subintervals=info["last"]
        )
        distance = np.sqrt(squared + cross * cross)
        zeroth = np.arctan2(distance, 1 + a @ b) / distance
        first = (np.log1p(b @ b) - np.log1p(a @ a)) / (2 * squared) - (
            a @ edge
        ) / squared * zeroth
        line = a * zeroth + edge * first
        boundary += np.outer(line, [edge[1], -edge[0]])
    raw = 0.5 * (np.eye(2) * radial - boundary)
    return (raw + raw.T) / 2, estimated_error


def kernel_covariance(vertices, transformed):
    mass, mean = kernel_moments(vertices, transformed)
    centre, forward, inverse, factor, _ = transformed
    points = np.einsum("lij,lvj->lvi", forward, vertices[None] - centre[:, None])
    result, work, errors = [], Counter(), []
    for index, polygon in enumerate(points):
        raw, error = second_moment(polygon, work)
        m0 = mass[index] / factor[index]
        mu = np.linalg.solve(inverse[index], mean[index] - centre[index])
        central = raw / m0 - np.outer(mu, mu)
        covariance = inverse[index] @ central @ inverse[index].T
        if (
            not np.isfinite(covariance).all()
            or np.linalg.eigvalsh(covariance).min() < -2e-9
        ):
            raise ValueError("invalid finite kernel covariance; no PSD clamping")
        result.append(covariance)
        errors.append(error * np.linalg.norm(inverse[index], ord=2) ** 2 / m0)
    work.update(
        kernel_domains=len(mass),
        edge_visits=2 * len(mass) * len(vertices),
        boundary_log1p_evaluations=2 * len(mass) * len(vertices),
    )
    return mass, mean, np.asarray(result), dict(work), max(errors, default=0.0)


def residual(gradient, roughness, lights, view, half):
    unit = normals(gradient)
    value = specular_from_dots(
        roughness,
        np.maximum(np.sum(unit * lights, axis=1), 0),
        np.maximum(unit @ view, 1e-6),
        np.maximum(np.sum(unit * half, axis=1), 0),
        np.maximum(half @ view, 0),
    )
    quadratic = (
        1
        + np.sum(gradient * gradient, axis=1)
        + (roughness**4 - 1)
        * (half[:, 2] - np.sum(half[:, :2] * gradient, axis=1)) ** 2
    )
    return value / (roughness**4 / (np.pi * quadratic**2))


def residual_hessian(mean, roughness, lights, view, half, step):
    base = residual(mean, roughness, lights, view, half)
    hessian = np.zeros((len(mean), 2, 2))
    for axis in range(2):
        offset = np.eye(2)[axis] * step
        hessian[:, axis, axis] = (
            residual(mean + offset, roughness, lights, view, half)
            + residual(mean - offset, roughness, lights, view, half)
            - 2 * base
        ) / step**2
    x, y = np.eye(2) * step
    mixed = (
        residual(mean + x + y, roughness, lights, view, half)
        - residual(mean + x - y, roughness, lights, view, half)
        - residual(mean - x + y, roughness, lights, view, half)
        + residual(mean - x - y, roughness, lights, view, half)
    ) / (4 * step**2)
    hessian[:, 0, 1] = hessian[:, 1, 0] = mixed
    return base, hessian


def response(model, roughness, lights, view, *, step=1e-3):
    if not np.isfinite(step) or step <= 0:
        raise ValueError("finite positive derivative-control step required")
    gradient, coverage, corners, _ = model
    value = coverage @ point_response(gradient, roughness, lights, view)
    work = Counter(constant_brdf_evaluations=len(coverage) * len(lights))
    maximum_estimate = 0.0
    if corners:
        transformed = transform(roughness, lights, view)
        for vertices, density in corners:
            mass, mean, covariance, counted, error = kernel_covariance(
                vertices, transformed
            )
            base, hessian = residual_hessian(
                mean, roughness, lights, view, transformed[-1], step
            )
            value += (
                density
                * mass
                * (base + 0.5 * np.sum(hessian * covariance, axis=(1, 2)))
            )
            work.update(counted)
            work.update(residual_brdf_evaluations=9 * len(lights))
            maximum_estimate = max(maximum_estimate, error)
    return value, dict(work), maximum_estimate
