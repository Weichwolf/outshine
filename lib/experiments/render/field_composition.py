"""Full-source depth ordering and coupled opaque point-sample composition.

Bounds must enclose all original primitives in a shared world frame. A raw
first-hit depth range does not supply them. These operations certify neither
that provenance nor image error: callers retain those independent obligations.
"""

import math
from dataclasses import dataclass

import numpy as np
from field_view_projection import dot, point, subtract, vector


@dataclass(frozen=True)
class OriginalBounds:
    lower: tuple
    upper: tuple
    source_binding: str

    def __post_init__(self):
        for name in ("lower", "upper"):
            object.__setattr__(self, name, vector(getattr(self, name)))
        if (
            not isinstance(self.source_binding, str)
            or not self.source_binding
            or any(a > b for a, b in zip(self.lower, self.upper, strict=True))
        ):
            raise ValueError("Ordered complete-source bounds and binding required")


def forward_interval(bounds, eye, forward):
    """Enclose all AABB points using outward rounding after every operation."""
    eye, forward = vector(eye), vector(forward)
    if abs(sum(v * v for v in forward) - 1) > 1e-9:
        raise ValueError("A unit camera forward axis is required")
    relative = tuple(
        subtract((a, b), point(e))
        for a, b, e in zip(bounds.lower, bounds.upper, eye, strict=True)
    )
    return dot(relative, tuple(map(point, forward)))


def depth_order(front, back, eye, forward):
    """A sufficient whole-group order; overlap is unknown, not proven interleave."""
    a = forward_interval(front, eye, forward)
    b = forward_interval(back, eye, forward)
    if min(a[0], b[0]) <= 0:
        return "originals", "hull_crosses_eye"
    if a[1] < b[0]:
        return "front_before_back", "complete_hulls_separated"
    if b[1] < a[0]:
        return "back_before_front", "complete_hulls_separated"
    return "originals", "complete_hulls_overlap"


def opaque_points(image, texels_most):
    if (
        not isinstance(image, np.ndarray)
        or image.ndim != 3
        or image.shape[2] != 4
        or min(image.shape[:2]) < 1
        or image.dtype not in (np.dtype(np.float32), np.dtype(np.float64))
    ):
        raise ValueError("A Float32/Float64 HxWx4 point-sample image is required")
    if math.prod(image.shape[:2]) > texels_most:
        raise ValueError("Point-sample capacity exceeded before output allocation")
    alpha = image[:, :, 3]
    if (
        not np.isfinite(image).all()
        or np.any(image < 0)
        or np.any((alpha != 0) & (alpha != 1))
        or np.any(image[:, :, :3][alpha == 0] != 0)
    ):
        raise ValueError("Opaque point samples, finite HDR and clear black required")


def compose_points(front, back, *, texels_most=262144):
    """Compose matching rays before filtering; never compose averaged coverage.

    The caller must establish equal ray locations, world/camera/source/light leases
    and valid complete-hull order independently. Equal array shapes do not prove
    registration. This operation does not turn a capture sample into a continuous
    subtexel coverage or visibility certificate.
    """
    if type(texels_most) is not int or texels_most < 1:
        raise ValueError("A positive point-sample capacity is required")
    opaque_points(front, texels_most)
    opaque_points(back, texels_most)
    if front.shape != back.shape or front.dtype != back.dtype:
        raise ValueError("Matching point-sample shapes and numeric profiles required")
    result = (1 - front[:, :, 3:4]) * back
    result += front
    return result
