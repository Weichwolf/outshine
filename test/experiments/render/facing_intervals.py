"""Small outward Float64 bounds for the geometric facing reference.

These enclose real operations on represented inputs. They are not a claim
about an unspecified GPU fast-math implementation or raster precision.
"""

import numpy as np


def point(value):
    value = np.asarray(value, dtype=np.float64)
    return value, value


def add(a, b):
    return np.nextafter(a[0] + b[0], -np.inf), np.nextafter(a[1] + b[1], np.inf)


def sub(a, b):
    return add(a, (-b[1], -b[0]))


def mul(a, b):
    values = np.stack(
        np.broadcast_arrays(a[0] * b[0], a[0] * b[1], a[1] * b[0], a[1] * b[1])
    )
    return np.nextafter(values.min(axis=0), -np.inf), np.nextafter(
        values.max(axis=0), np.inf
    )


def divide_positive(a, b):
    if np.any(b[0] <= 0):
        raise ValueError("strict positive interval denominator required")
    return mul(a, (np.nextafter(1 / b[1], -np.inf), np.nextafter(1 / b[0], np.inf)))


def dot(a, b):
    product = mul(a, b)
    total = point(np.zeros(product[0].shape[:-1]))
    for axis in range(3):
        total = add(total, (product[0][..., axis], product[1][..., axis]))
    return total


def length(a):
    minimum = np.where(a[0] > 0, a[0], np.where(a[1] < 0, -a[1], 0))
    maximum = np.maximum(np.abs(a[0]), np.abs(a[1]))
    squares = (
        np.nextafter(minimum * minimum, -np.inf),
        np.nextafter(maximum * maximum, np.inf),
    )
    total = point(np.zeros(minimum.shape[:-1]))
    for axis in range(3):
        total = add(total, (squares[0][..., axis], squares[1][..., axis]))
    low, high = np.maximum(0, total[0]), total[1]
    return np.maximum(0, np.nextafter(np.sqrt(low), -np.inf)), np.nextafter(
        np.sqrt(high), np.inf
    )


def geometric_normals(triangles):
    triangles = np.asarray(triangles, dtype=np.float64)
    first = sub(point(triangles[:, 1]), point(triangles[:, 0]))
    second = sub(point(triangles[:, 2]), point(triangles[:, 0]))
    low, high = [], []
    for axis in range(3):
        b, c = (axis + 1) % 3, (axis + 2) % 3
        value = sub(
            mul((first[0][:, b], first[1][:, b]), (second[0][:, c], second[1][:, c])),
            mul((first[0][:, c], first[1][:, c]), (second[0][:, b], second[1][:, b])),
        )
        low.append(value[0])
        high.append(value[1])
    return np.stack(low, axis=-1), np.stack(high, axis=-1)


def facing(triangles, camera):
    triangles = np.asarray(triangles, dtype=np.float64)
    return dot(geometric_normals(triangles), sub(point(camera), point(triangles[:, 0])))
