"""Geometric full-source containment before fine orthographic field work.

The projection encloses exact finite camera coefficients, not GPU matrix/raster
rounding. Containment alone qualifies neither sampled coverage nor field error.
Per-source flags plan a source-bound cohort; they cannot mask a previously
captured whole-group field. Retained originals must not also remain in that field.
"""

import numpy as np
from field_view_projection import vector


def checked_bounds(lower, upper, bounds_most):
    if (
        not isinstance(lower, np.ndarray)
        or not isinstance(upper, np.ndarray)
        or lower.ndim != 2
        or lower.shape[1] != 3
        or lower.shape != upper.shape
        or lower.dtype.kind != "f"
        or upper.dtype.kind != "f"
        or type(bounds_most) is not int
        or not 1 <= len(lower) <= bounds_most
    ):
        raise ValueError("Bounded nonempty floating-point Nx3 source hulls required")
    if (
        not np.isfinite(lower).all()
        or not np.isfinite(upper).all()
        or np.any(lower > upper)
    ):
        raise ValueError("Ordered finite complete-source bounds required")
    return lower.astype(np.float64, copy=False), upper.astype(np.float64, copy=False)


def translated_bounds(lower, upper, translation):
    """Match #1716: Double translation, then Float32 original vertex storage.

    Both floating-point operations are monotone, so applying the same sequence
    to enclosing endpoints encloses every translated stored vertex. This is not
    an assumption about GPU instance-matrix execution or an unmodelled transform.
    """
    translation = np.asarray(vector(translation))
    with np.errstate(over="ignore", invalid="ignore"):
        lower = (lower + translation).astype(np.float32).astype(np.float64)
        upper = (upper + translation).astype(np.float32).astype(np.float64)
    if not np.isfinite(lower).all() or not np.isfinite(upper).all():
        raise ValueError("Original placement overflows Float32 vertex storage")
    return lower, upper


def capture_frame(camera):
    if camera.get("orthographic") is not True:
        raise ValueError("The supplied source capture must be orthographic")
    eye = np.asarray(vector(camera["eye_m"]))
    basis = np.asarray([vector(camera[k]) for k in ("right", "up", "forward")])
    if not np.allclose(basis @ basis.T, np.eye(3), rtol=0, atol=1e-9):
        raise ValueError("Orthonormal capture axes required")
    x, y, near, far = [
        float(camera[k]) for k in ("x_mag_m", "y_mag_m", "near_m", "far_m")
    ]
    if not np.isfinite([x, y, near, far]).all() or min(x, y, near) <= 0 or far <= near:
        raise ValueError(
            "Finite positive orthographic extents and clipping range required"
        )
    return eye, basis, np.array([-x, -y, near]), np.array([x, y, far])


def projected_intervals(lower, upper, eye, basis):
    """Three interval dot products; no expansion into eight corners per source."""
    with np.errstate(over="ignore", invalid="ignore"):
        relative_lower = np.nextafter(lower - eye, -np.inf)
        relative_upper = np.nextafter(upper - eye, np.inf)
        low, high = np.zeros_like(lower), np.zeros_like(upper)
        for axis in range(3):
            for coordinate in range(3):
                coefficient = basis[axis, coordinate]
                a, b = relative_lower[:, coordinate], relative_upper[:, coordinate]
                if coefficient < 0:
                    a, b = b, a
                product_low = np.nextafter(a * coefficient, -np.inf)
                product_high = np.nextafter(b * coefficient, np.inf)
                low[:, axis] = np.nextafter(low[:, axis] + product_low, -np.inf)
                high[:, axis] = np.nextafter(high[:, axis] + product_high, np.inf)
    return low, high


def capture_support(lower, upper, camera, *, translation=(0, 0, 0), bounds_most=4096):
    lower, upper = checked_bounds(lower, upper, bounds_most)
    eye, basis, limit_low, limit_high = capture_frame(camera)
    lower, upper = translated_bounds(lower, upper, translation)
    low, high = projected_intervals(lower, upper, eye, basis)
    finite = np.isfinite(low).all(1) & np.isfinite(high).all(1)
    inside = finite & ((low >= limit_low) & (high <= limit_high)).all(1)
    return {
        "inside": inside,
        "finite": finite,
        "lower_m": low,
        "upper_m": high,
        "capture_lower_m": limit_low,
        "capture_upper_m": limit_high,
    }
