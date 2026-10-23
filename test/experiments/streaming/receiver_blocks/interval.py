"""Outward float64 intervals for the existing sequential CPU receiver transform."""

import math


def outward(values):
    if not all(math.isfinite(v) for v in values):
        raise ArithmeticError("nonfinite arithmetic")
    return math.nextafter(min(values), -math.inf), math.nextafter(max(values), math.inf)


def add(a, b):
    return outward((a[0] + b[0], a[1] + b[1]))


def scale(a, coefficient):
    if coefficient == 0:
        return 0.0, 0.0
    return outward((a[0] * coefficient, a[1] * coefficient))


def divide(a, b):
    if b[0] <= 0 <= b[1]:
        raise ArithmeticError("denominator crosses zero")
    return outward(tuple(x / y for x in a for y in b))


def transform(matrix, point):
    result = []
    for row in range(4):
        value = matrix[12 + row], matrix[12 + row]
        for column in range(3):
            coefficient = matrix[4 * column + row]
            if coefficient != 0:
                value = add(value, scale(point[column], coefficient))
        result.append(value)
    return result


def project(ndc, inverse, eye, lights):
    world_h = transform(inverse, ndc)
    if world_h[3][0] <= 0:
        raise ArithmeticError("camera homogeneous w is not positive")
    world = [divide(world_h[i], world_h[3]) for i in range(3)]
    shifted = [add(world[i], (-eye[i], -eye[i])) for i in range(3)]
    output = []
    for matrix in lights:
        light = transform(matrix, shifted)
        if light[3][0] <= 0:
            raise ArithmeticError("light homogeneous w is not positive")
        output.append([divide(light[i], light[3]) for i in range(3)])
    return output
