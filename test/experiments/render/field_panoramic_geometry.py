"""Whole-capture displacement under every camera rotation, for a fixed eye.

If one source/support point reaches the viewport, its forward distance is at
least d/hypot(1,tan_x,tan_y). Subtracting the joint hull diameter bounds every
other point's forward distance. The perspective-difference numerator is
h*(ray cross capture_direction); its norm is convex on the complete hull.
No sampled coverage, material, depth reconstruction or shadow authority.
"""

import math
import struct

from field_pose_geometry import hull_vertices, operands
from field_view_projection import add, divide, dot, multiply, point, subtract


def cross(a, b):
    return tuple(
        subtract(multiply(a[(i + 1) % 3], b[(i + 2) % 3]),
                 multiply(a[(i + 2) % 3], b[(i + 1) % 3]))
        for i in range(3)
    )


def norm(vector, upper=True):
    squared = point(0.0)
    for lo, hi in vector:
        v = max(abs(lo), abs(hi)) if upper else max(lo, 0.0, -hi)
        squared = add(squared, multiply(point(v), point(v)))
    result = math.sqrt(max(0.0, squared[1 if upper else 0]))
    return math.nextafter(result, math.inf if upper else -math.inf)


def panoramic_bound(plane, eye, projection, raster, *, normal=None, source_hull=None):
    identity = ((1, 0, 0), (0, 1, 0), (0, 0, 1))
    eye, _, matrix, raster = operands(eye, identity, projection, raster)
    if (tuple(projection[3]) != (0, 0, 1, 0)
            or projection[0][0] <= 0 or projection[1][1] <= 0
            or any(projection[r][c] != 0 for r, c in ((0, 1), (0, 3), (1, 0),
                                                      (1, 3), (2, 0), (2, 1)))):
        raise ValueError("Centred or jittered perspective projection required")
    direction = tuple(map(point, plane.toward_eye))
    normal = tuple(map(point, plane.toward_eye if normal is None else normal))
    centre = tuple(map(point, plane.centre))
    relative_eye = tuple(subtract(point(e), c) for e, c in zip(eye, centre))
    if dot(relative_eye, direction)[0] <= 0 or dot(relative_eye, normal)[0] <= 0:
        raise ValueError("Support faces away from eye")
    denominator = dot(direction, normal)
    rays, height, numerator = [], 0.0, 0.0
    for corner in hull_vertices(plane, source_hull):
        h = divide(dot(tuple(subtract(p, c) for p, c in zip(corner, centre)), normal),
                   denominator)
        ray = tuple(subtract(p, point(e)) for p, e in zip(corner, eye))
        support = tuple(subtract(p, multiply(h, d)) for p, d in zip(ray, direction))
        rays.extend((ray, support))
        height = max(height, abs(h[0]), abs(h[1]))
        numerator = max(numerator, norm(cross(ray, direction)))
    envelope = tuple((min(p[i][0] for p in rays), max(p[i][1] for p in rays))
                     for i in range(3))
    distance = max(0.0, norm(envelope, upper=False))
    diameter = norm(tuple(subtract(point(hi), point(lo)) for lo, hi in envelope))
    guards = tuple(struct.unpack('f', struct.pack('f', 1 / pixels))[0] for pixels in raster)
    slopes = tuple(divide(add(point(1), point(max(abs(projection[i][2]), guards[i]))), matrix[i][i])
                   for i in (0, 1))
    scale = norm((point(1), *slopes))
    basis_upper = math.nextafter(math.sqrt(1 + 3e-9), math.inf)
    basis_lower = math.nextafter(math.sqrt(1 - 3e-9), -math.inf)
    depth = subtract(multiply(point(basis_lower), divide(point(distance), point(scale))),
                     multiply(point(basis_upper), point(diameter)))[0]
    near = divide(matrix[2][3], add(point(1), matrix[2][2]))[1]
    far = divide(matrix[2][3], matrix[2][2])[0] if projection[2][2] > 0 else math.inf
    inside = depth >= near and multiply(point(basis_upper), point(norm(envelope)))[1] <= far
    error = math.inf
    if depth > 0:
        focal = tuple(multiply(point(pixels * .5), matrix[i][i])
                      for i, pixels in enumerate(raster))
        scale = multiply(multiply(point(height), point(numerator)), point(norm((*focal, point(0)))))
        scale = multiply(scale, point(math.nextafter(1 + 3e-9, math.inf)))
        error = divide(scale, multiply(point(depth), point(depth)))[1]
    return {"screen_l1_upper_px": error, "depth_inside": inside,
            "minimum_forward_m": depth, "joint_diameter_m": diameter}
