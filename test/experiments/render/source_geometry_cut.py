"""View-independent geometry products; projection only selects existing products.

Declared bounds and shell Hausdorff errors are inputs, not verified native proofs.
Real-arithmetic projection bounds do not certify GPU rounding/materials/coverage.
"""

import math
import struct
from dataclasses import dataclass


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def unit(a):
    size = math.hypot(*a)
    if not size or not math.isfinite(size):
        raise ValueError("finite nonzero direction required")
    return tuple(x / size for x in a)


def cross(a, b):
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


@dataclass(frozen=True)
class Camera:
    eye: tuple
    right: tuple
    up: tuple
    forward: tuple
    focal: float
    radius: float = 0
    orthographic: bool = False

    def __post_init__(self):
        axes = (self.right, self.up, self.forward)
        if (
            any(
                len(v) != 3 or not all(math.isfinite(x) for x in v)
                for v in (self.eye, *axes)
            )
            or not math.isfinite(self.focal)
            or self.focal <= 0
        ):
            raise ValueError("finite camera and positive focal scale required")
        if not math.isfinite(self.radius) or self.radius < 0:
            raise ValueError("nonnegative finite lookahead radius required")
        if any(
            abs(dot(a, b) - (i == j)) > 1e-12
            for i, a in enumerate(axes)
            for j, b in enumerate(axes)
        ):
            raise ValueError("orthonormal camera basis required")

    @classmethod
    def toward(cls, eye, target, focal, radius=0, orthographic=False):
        forward = unit(tuple(b - a for a, b in zip(eye, target)))
        helper = min(
            ((1, 0, 0), (0, 1, 0), (0, 0, 1)), key=lambda a: abs(dot(a, forward))
        )
        right = unit(cross(forward, helper))
        return cls(
            tuple(eye),
            right,
            unit(cross(right, forward)),
            forward,
            focal,
            radius,
            orthographic,
        )

    def bounds(self, box):
        centre = tuple(
            (a + b) / 2 - eye for a, b, eye in zip(box[:3], box[3:6], self.eye)
        )
        half = tuple((b - a) / 2 for a, b in zip(box[:3], box[3:6]))
        intervals = []
        for axis in (self.right, self.up, self.forward):
            middle = dot(centre, axis)
            extent = sum(abs(a) * h for a, h in zip(axis, half)) + self.radius
            intervals.append((middle - extent, middle + extent))
        return intervals

    def hull_error(self, bounds):
        x, y, z = bounds
        width = [max(0.0, hi - lo - 2 * self.radius) for lo, hi in bounds]
        if self.orthographic:
            return self.focal * (width[0] + width[1])
        if z[0] <= 0:
            return math.inf
        slope = (max(map(abs, x)) + max(map(abs, y))) / z[0]
        return self.focal * (width[0] + width[1] + slope * width[2]) / z[0]

    def shell_error(self, bounds, error):
        if error == 0:
            return 0.0
        if self.orthographic:
            return math.sqrt(2) * self.focal * error
        x, y, z = bounds
        if z[0] <= error:
            return math.inf
        slope = (max(map(abs, x)) + max(map(abs, y))) / z[0]
        return self.focal * error * (math.sqrt(2) + slope) / (z[0] - error)


@dataclass(frozen=True)
class GeometryProduct:
    node: int
    kind: str
    revision: str
    error_m: float = 0
    selection_error_px: float = 0

    @property
    def address(self):
        return self.revision, self.node, self.kind


class GeometrySources:
    def __init__(self, tree, data):
        if len(data) < 16:
            raise ValueError("geometry metadata header required")
        magic, count, revision_bytes = struct.unpack_from("<QII", data)
        if (
            magic != 0x3152544D4F454753
            or count != len(tree.order)
            or len(data) != 16 + revision_bytes + count * 12
        ):
            raise ValueError("complete matching native geometry metadata required")
        if data[16 : 16 + revision_bytes].decode("utf-8") != tree.revision:
            raise ValueError("geometry metadata belongs to another source revision")
        rows = tuple(struct.iter_unpack("<dI", data[16 + revision_bytes :]))
        if any(not math.isfinite(e) or (e < 0 and e != -1) for e, _ in rows):
            raise ValueError("finite declared error or explicit unknown required")
        self.tree = tree
        self.shell = tuple(None if e < 0 else e for e, _ in rows)
        self.faces = tuple(f for _, f in rows)
        self.supported = [False] * len(tree.nodes)
        self.group_error = [None] * len(tree.nodes)
        for at in range(len(tree.nodes) - 1, -1, -1):
            node = tree.nodes[at]
            self.supported[at] = (
                self.faces[tree.order[node[6]]] > 0
                if node[7] == 1
                else self.supported[node[8]] and self.supported[node[9]]
            )
            if node[7] == 1:
                self.group_error[at] = self.shell[tree.order[node[6]]]
            else:
                a, b = self.group_error[node[8]], self.group_error[node[9]]
                self.group_error[at] = (
                    max(a, b) if a is not None and b is not None else None
                )

    def cut(self, camera, allowed, *, cell_cohorts=False):
        if not math.isfinite(allowed) or allowed < 0:
            raise ValueError("finite nonnegative pixel budget required")
        pending, products, visited = [0], [], 0
        tree = self.tree
        while pending:
            at = pending.pop()
            visited += 1
            node = tree.nodes[at]
            supported = self.supported[at]
            eligible = supported and (not cell_cohorts or tree.cohorts[at] is not None)
            bounds = camera.bounds(node[:6]) if eligible else None
            hull = camera.hull_error(bounds) if eligible and allowed > 0 else math.inf
            error = self.group_error[at]
            shell = (
                camera.shell_error(bounds, error)
                if eligible and error is not None
                else math.inf
            )
            if hull <= allowed and node[7] > 1:
                products.append(
                    GeometryProduct(at, "hull", tree.revision, tree.radii[at] * 2, hull)
                )
            elif shell <= allowed:
                products.append(
                    GeometryProduct(at, "shell", tree.revision, error, shell)
                )
            elif hull <= allowed:
                products.append(
                    GeometryProduct(at, "hull", tree.revision, tree.radii[at] * 2, hull)
                )
            elif node[7] == 1:
                products.append(
                    GeometryProduct(
                        at, "original" if supported else "unresolved", tree.revision
                    )
                )
            else:
                pending.extend((node[9], node[8]))
        tree.validate_cut(products)
        return products, visited
