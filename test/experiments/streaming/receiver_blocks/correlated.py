"""Projective corner extrema plus a uniform error bound for the original CPU graph."""

import itertools
import math

import interval

UNIT = 2.0**-53
ETA = math.ulp(0.0)


def up(value):
    if not math.isfinite(value):
        raise ArithmeticError("nonfinite error bound")
    return math.nextafter(value, math.inf)


def magnitude(bounds):
    return max(abs(bounds[0]), abs(bounds[1]))


def rounding(bounds):
    return up(up(UNIT * magnitude(bounds)) + ETA)


def add(a, b):
    bounds = interval.add(a[0], b[0])
    return bounds, up(up(a[1] + b[1]) + rounding(bounds))


def scale(a, coefficient):
    if coefficient == 0:
        return (0.0, 0.0), 0.0
    bounds = interval.scale(a[0], coefficient)
    return bounds, up(up(abs(coefficient) * a[1]) + rounding(bounds))


def divide(a, b):
    if b[0][0] <= 0:
        raise ArithmeticError("denominator is not uniformly positive")
    bounds = interval.divide(a[0], b[0])
    error = up(up(a[1] + up(magnitude(bounds) * b[1])) / b[0][0])
    return bounds, up(error + rounding(bounds))


def transform(matrix, point):
    result = []
    for row in range(4):
        value = (matrix[12 + row], matrix[12 + row]), 0.0
        for column in range(3):
            coefficient = matrix[4 * column + row]
            if coefficient != 0:
                value = add(value, scale(point[column], coefficient))
        result.append(value)
    return result


def errors(ndc, inverse, eye, lights):
    camera = transform(inverse, [(a, 0.0) for a in ndc])
    world = [divide(camera[i], camera[3]) for i in range(3)]
    shifted = [add(world[i], ((-eye[i], -eye[i]), 0.0)) for i in range(3)]
    output = []
    for matrix in lights:
        light = transform(matrix, shifted)
        output.append([divide(light[i], light[3])[1] for i in range(3)])
    return output


def project(ndc, inverse, eye, lights):
    error = errors(ndc, inverse, eye, lights)
    lower = [[math.inf] * 3 for _ in lights]
    upper = [[-math.inf] * 3 for _ in lights]
    endpoints = [(lo,) if lo == hi else (lo, hi) for lo, hi in ndc]
    for corner in itertools.product(*endpoints):
        bounds = interval.project([(v, v) for v in corner], inverse, eye, lights)
        for i, light in enumerate(bounds):
            for axis, value in enumerate(light):
                lower[i][axis] = min(lower[i][axis], value[0])
                upper[i][axis] = max(upper[i][axis], value[1])
    return [
        [
            (
                math.nextafter(lower[i][a] - error[i][a], -math.inf),
                math.nextafter(upper[i][a] + error[i][a], math.inf),
            )
            for a in range(3)
        ]
        for i in range(len(lights))
    ]
