"""Camera-aware sampling of one coupled field without a second filtered image.

Parallel axes use the exact rectangular pixel footprint. Other views explicitly
opt into a bounding-box approximation and report its excess texture area. The
returned screen rectangle includes filter support beyond the capture card.
"""

import math
from dataclasses import dataclass

import numpy as np
from field_view_integrals import box_integrals
from field_view_projection import vector


@dataclass(frozen=True)
class FieldGrid:
    centre: tuple
    toward_eye: tuple
    half_extent_m: float

    def __post_init__(self):
        object.__setattr__(self, "centre", vector(self.centre))
        object.__setattr__(self, "toward_eye", vector(self.toward_eye))
        if abs(sum(x * x for x in self.toward_eye) - 1) > 1e-9:
            raise ValueError("A unit capture direction is required")
        if not math.isfinite(self.half_extent_m) or self.half_extent_m <= 0:
            raise ValueError("A finite positive capture extent is required")

    def axes(self):
        d = np.asarray(self.toward_eye)
        right = np.array([d[2], 0, -d[0]])
        length = np.linalg.norm(right)
        right = right / length if length else np.array([1.0, 0, 0])
        return right, np.cross(d, right)


@dataclass(frozen=True)
class ImageView:
    eye: tuple
    basis: tuple
    focal_px: float
    width: int = 1280
    height: int = 720

    def __post_init__(self):
        object.__setattr__(self, "eye", vector(self.eye))
        object.__setattr__(self, "basis", tuple(vector(a) for a in self.basis))
        if len(self.basis) != 3 or not np.allclose(
            np.asarray(self.basis) @ np.asarray(self.basis).T,
            np.eye(3),
            rtol=0,
            atol=1e-9,
        ):
            raise ValueError("An orthonormal camera basis is required")
        if not math.isfinite(self.focal_px) or self.focal_px <= 0:
            raise ValueError("A finite positive focal length is required")
        if any(type(v) is not int or v < 1 for v in (self.width, self.height)):
            raise ValueError("An explicit positive integer image profile is required")


def support(grid, view, axes, pixels_most, plane=None):
    corners = np.array([[-1, 1], [1, 1], [1, -1], [-1, -1]])
    world = np.asarray(grid.centre) + grid.half_extent_m * (
        corners[:, :1] * axes[0] + corners[:, 1:] * axes[1]
    )
    if plane is not None:
        world = plane.points(grid, corners)
    if not np.isfinite(world).all():
        raise ValueError("Finite physical support corners required")
    camera = (world - view.eye) @ np.asarray(view.basis).T
    if camera[:, 2].min() <= 0:
        raise ValueError("Capture card crosses the eye plane")
    xy = (
        np.array([view.width / 2, view.height / 2])
        + camera[:, :2] * [1, -1] * view.focal_px / camera[:, 2:]
    )
    dimensions = (view.width, view.height)
    low = np.clip(np.floor(xy.min(0) - 1), 0, dimensions).astype(np.int64)
    high = np.maximum(low, np.clip(np.ceil(xy.max(0) + 1), 0, dimensions)).astype(
        np.int64
    )
    if int(np.prod(high - low)) > pixels_most:
        raise ValueError("Projected field-pixel capacity exceeded before expansion")
    return low, high


def source_corners(grid, view, axes, pixels, dimensions, *, plane=None):
    offsets = np.array([[0, 0], [1, 0], [1, 1], [0, 1]])
    screen = pixels[:, None] + offsets - (view.width / 2, view.height / 2)
    directions = (
        np.asarray(view.basis)[2]
        + screen[:, :, :1] * np.asarray(view.basis)[0] / view.focal_px
        - screen[:, :, 1:] * np.asarray(view.basis)[1] / view.focal_px
    )
    normal = np.asarray(grid.toward_eye) if plane is None else plane.normal(grid)
    centre = np.asarray(grid.centre) if plane is None else plane.centre(grid)
    denominator = directions @ normal
    numerator = (centre - view.eye) @ normal
    if not np.isfinite(denominator).all() or not np.isfinite(numerator):
        raise ValueError("Finite physical plane intersection required")
    if np.any(denominator * numerator <= 0):
        raise ValueError("Pixel footprint crosses or faces away from its capture plane")
    relative = (
        np.asarray(view.eye)
        - grid.centre
        + directions * (numerator / denominator)[:, :, None]
    )
    uv = np.stack((relative @ axes[0], -(relative @ axes[1])), axis=-1)
    return (uv / (2 * grid.half_extent_m) + 0.5) * dimensions


def reconstruct(field, grid, view, *, pixels_most=16384, approximate=False, plane=None):
    if type(pixels_most) is not int or pixels_most < 1:
        raise ValueError("A positive projected-pixel capacity is required")
    axes = grid.axes()
    parallel = np.allclose(np.asarray(view.basis)[:2], axes, rtol=0, atol=1e-12)
    parallel = parallel and (plane is None or plane.coefficients[:2] == (0, 0))
    if not parallel and not approximate:
        raise ValueError(
            "Nonparallel footprints require explicit approximation admission"
        )
    low, high = support(grid, view, axes, pixels_most, plane)
    y, x = np.mgrid[low[1] : high[1], low[0] : high[0]]
    pixels = np.column_stack((x.ravel(), y.ravel()))
    uv = source_corners(
        grid, view, axes, pixels, (field.source_width, field.source_height), plane=plane
    )
    lower, upper = uv.min(1), uv.max(1)
    area = np.prod(upper - lower, axis=1)
    quad_area = (
        abs(
            np.sum(
                uv[:, :, 0] * np.roll(uv[:, :, 1], -1, axis=1)
                - uv[:, :, 1] * np.roll(uv[:, :, 0], -1, axis=1),
                axis=1,
            )
        )
        / 2
    )
    if np.any(area <= 0) or np.any(quad_area <= 0):
        raise ValueError("Degenerate pixel-to-source footprint")
    value = (
        box_integrals(field, lower, upper, boxes_most=pixels_most).astype(float)
        / area[:, None]
    )
    raw_minimum = float(value.min(initial=0))
    value[:, 3] = np.clip(value[:, 3], 0, 1)
    value[:, :3] = np.maximum(value[:, :3], 0)
    value[value[:, 3] == 0, :3] = 0
    return {
        "origin": tuple(map(int, low)),
        "rgba": value.reshape(*x.shape, 4),
        "output_pixels": len(pixels),
        "logical_prefix_evaluations": 4 * len(pixels),
        "logical_texel_fetches": 16 * len(pixels),
        "parallel": bool(parallel),
        "maximum_rectangle_excess_ratio": float(
            np.max(area / quad_area - 1, initial=0)
        ),
        "minimum_before_physical_roundoff_projection": raw_minimum,
        "scope": "One coupled first-hit field; exact box footprint for parallel axes, otherwise explicit bounding rectangle approximation. No coverage/radiance/depth/visibility admission follows from reconstruction alone.",
    }
