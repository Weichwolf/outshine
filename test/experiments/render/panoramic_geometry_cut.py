"""Rotation-independent perspective candidates over declared geometric errors.

For either visible endpoint, z >= distance / hypot(1, tan_x, tan_y).
Shells may leave the original source bounds by their declared Hausdorff error.
These real-arithmetic bounds do not qualify native geometry or materials.
"""

import math
from dataclasses import dataclass


@dataclass(frozen=True)
class PanoramicView:
    eye: tuple
    focal: float
    width: float
    height: float
    radius: float = 0

    def __post_init__(self):
        if (
            len(self.eye) != 3
            or not all(math.isfinite(x) for x in self.eye)
            or any(not math.isfinite(x) or x <= 0 for x in
                   (self.focal, self.width, self.height))
            or not math.isfinite(self.radius)
            or self.radius < 0
        ):
            raise ValueError("finite eye, positive perspective viewport and nonnegative radius required")

    def bounds(self, box):
        distance = math.hypot(*(max(a - e, 0, e - b)
                                for a, b, e in zip(box[:3], box[3:6], self.eye)))
        return max(0, distance - self.radius), math.dist(box[:3], box[3:6])

    def error(self, distance, error, expansion):
        if not math.isfinite(error) or error < 0:
            raise ValueError("finite nonnegative declared error required")
        if error == 0:
            return 0.0
        tx, ty = self.width / (2 * self.focal), self.height / (2 * self.focal)
        depth = max(0, distance - expansion) / math.hypot(1, tx, ty)
        if depth <= error:
            return math.inf
        return self.focal * error * math.hypot(math.sqrt(2), tx + ty) / (depth - error)

    def hull_error(self, bounds):
        distance, diameter = bounds
        return self.error(distance, diameter, 0)

    def shell_error(self, bounds, error):
        return self.error(bounds[0], error, error)
