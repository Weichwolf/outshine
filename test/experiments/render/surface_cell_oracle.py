"""Independent exact six-plane clipping, using binary input values as rationals."""

from fractions import Fraction


def clipped(triangle, centre):
    polygon = [tuple(Fraction(float(x)) for x in p) for p in triangle]
    for axis in range(3):
        for sign in (-1, 1):
            bound = sign * Fraction(float(centre[axis])) + Fraction(1, 2)
            output = []
            for a, b in zip(polygon, polygon[1:] + polygon[:1]):
                da, db = sign * a[axis] - bound, sign * b[axis] - bound
                if da <= 0:
                    output.append(a)
                if (da < 0 < db) or (db < 0 < da):
                    t = da / (da - db)
                    output.append(tuple(x + t * (y - x) for x, y in zip(a, b)))
            polygon = output
            if not polygon:
                return False
    return True
