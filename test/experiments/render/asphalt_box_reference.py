"""Independent true box integration: Gaussian erf intervals and 1D quadrature."""

import itertools

import numpy as np
from asphalt_field import SIGMA, grain, point_field
from scipy.special import erf


def interval(projection, length, sigma):
    if length == 0:
        return np.exp(-projection * projection / (2 * sigma * sigma))
    low = (projection - length / 2) / (np.sqrt(2) * sigma)
    high = (projection + length / 2) / (np.sqrt(2) * sigma)
    return sigma * np.sqrt(np.pi / 2) / length * (erf(high) - erf(low))


def box_reference(points, dx, dy, order):
    points, dx, dy = (np.asarray(p) for p in (points, dx, dy))
    length_x, length_y = np.linalg.norm(dx), np.linalg.norm(dy)
    if length_x < length_y:
        dx, dy, length_x, length_y = dy, dx, length_y, length_x
    if length_x == 0:
        return point_field(points)
    cells = np.floor(points).astype(np.int64)
    extent = np.ceil((np.abs(dx) + np.abs(dy)) / 2).astype(int) + 3
    orthogonal = abs(np.dot(dx, dy)) <= 1e-12 * length_x * length_y
    nodes, weights = np.polynomial.legendre.leggauss(order)
    nodes, weights = nodes / 2, weights / 2
    result = np.zeros(len(points))
    unit = dx / length_x
    perpendicular = np.array([-unit[1], unit[0]])
    for offset in itertools.product(
        range(-extent[0], extent[0] + 1), range(-extent[1], extent[1] + 1)
    ):
        site, weight = grain(cells + offset)
        delta = points - site
        for sigma, coefficient in ((SIGMA, 1), (2 * SIGMA, -0.25)):
            if orthogonal:
                integral = interval(delta @ unit, length_x, sigma) * interval(
                    delta @ perpendicular, length_y, sigma
                )
            else:
                shifted = delta[:, None] + nodes[:, None] * dy
                along = shifted @ unit
                across = shifted @ perpendicular
                samples = interval(along, length_x, sigma) * np.exp(
                    -across * across / (2 * sigma * sigma)
                )
                integral = samples @ weights
            result += coefficient * weight * integral
    return result
