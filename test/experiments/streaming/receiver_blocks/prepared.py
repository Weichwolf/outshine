"""Prepare affine-light coefficients; bound fractional extrema at the two z faces."""

import math

import correlated
import interval


def multiply(a, b):
    return interval.outward(tuple(x * y for x in a for y in b))


def coefficients(inverse, eye, light):
    result = []
    for row in range(3):
        shift = light[12 + row], light[12 + row]
        for i in range(3):
            if light[4 * i + row] != 0:
                shift = interval.add(
                    shift, interval.scale((eye[i], eye[i]), -light[4 * i + row])
                )
        axis = []
        for column in range(4):
            value = interval.scale(shift, inverse[4 * column + 3])
            for i in range(3):
                coefficient = light[4 * i + row]
                if coefficient != 0:
                    a = inverse[4 * column + i]
                    value = interval.add(value, interval.scale((a, a), coefficient))
            axis.append(value)
        result.append(axis)
    return result


class Prepared:
    def __init__(self, inverse, eye, lights, error_box=None):
        self.inverse, self.eye, self.lights = inverse, eye, lights
        self.specialized = all(
            [m[3], m[7], m[11], m[15]] == [0, 0, 0, 1] for m in lights
        )
        self.frame_error = None
        if error_box is not None:
            try:
                self.frame_error = correlated.errors(error_box, inverse, eye, lights)
            except ArithmeticError:
                pass
        self.numerators = (
            [coefficients(inverse, eye, m) for m in lights]
            if self.specialized
            else None
        )

    def __call__(self, ndc):
        if not self.specialized:
            return correlated.project(ndc, self.inverse, self.eye, self.lights)
        error = (
            self.frame_error
            if self.frame_error is not None
            else correlated.errors(ndc, self.inverse, self.eye, self.lights)
        )
        output = []
        for i, light in enumerate(self.numerators):
            bounds = []
            for axis, coefficients in enumerate(light):
                xy = interval.add(coefficients[3], multiply(coefficients[0], ndc[0]))
                xy = interval.add(xy, multiply(coefficients[1], ndc[1]))
                lower, upper = math.inf, -math.inf
                for z in set(ndc[2]):
                    numerator = interval.add(xy, interval.scale(coefficients[2], z))
                    denominator = interval.add(
                        interval.scale(ndc[0], self.inverse[3]),
                        interval.scale(ndc[1], self.inverse[7]),
                    )
                    denominator = interval.add(
                        denominator, (self.inverse[15], self.inverse[15])
                    )
                    denominator = interval.add(
                        denominator, interval.scale((z, z), self.inverse[11])
                    )
                    quotient = interval.divide(numerator, denominator)
                    lower, upper = min(lower, quotient[0]), max(upper, quotient[1])
                radius = error[i][axis]
                bounds.append(
                    (
                        math.nextafter(lower - radius, -math.inf),
                        math.nextafter(upper + radius, math.inf),
                    )
                )
            output.append(bounds)
        return output
