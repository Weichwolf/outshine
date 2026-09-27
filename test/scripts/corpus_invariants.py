"""Evaluate manifest relations on a client's scene-linear RGBA32F attachment."""

import math

import numpy as np


def quantity(record, name):
    value = record[name]["value"]
    if isinstance(value, (int, float)) and not math.isfinite(value):
        raise ValueError(f"{name} must be finite")
    return value


def nearest_rank(values, fraction):
    rank = max(0, min(values.size - 1, math.ceil(fraction * values.size) - 1))
    return float(np.partition(values.reshape(-1), rank)[rank])


def rectangle(frame, record, name):
    x, y, width, height = quantity(record, name)
    if any(not isinstance(v, int) for v in (x, y, width, height)) or min(x, y) < 0:
        raise ValueError(f"{name} must be a nonnegative integer rectangle")
    if width <= 0 or height <= 0 or x + width > frame.shape[1] or y + height > frame.shape[0]:
        raise ValueError(f"{name} lies outside the captured frame")
    return frame[y:y + height, x:x + width, :3]


def region_compare(frame, declared):
    source = rectangle(frame, declared, "fromPx")
    target = rectangle(frame, declared, "toPx")
    if source.shape != target.shape:
        raise ValueError("compared regions have different extents")
    channels = declared.get("channels", [0, 1, 2])
    if (not channels or len(set(channels)) != len(channels) or
            any(not isinstance(ch, int) or ch not in (0, 1, 2) for ch in channels)):
        raise ValueError("channels must name RGB components")
    scale = quantity(declared, "scale")
    if not isinstance(scale, (int, float)) or scale < 0:
        raise ValueError("region scale must be finite and nonnegative")
    a = source[..., channels]
    b = (target[..., channels].astype(np.float64) * float(scale)).astype(np.float32)
    if np.any(a < 0) or np.any(b < 0):
        raise ValueError("scene radiance must be nonnegative")
    difference = np.abs(a.astype(np.float64) - b.astype(np.float64))
    has_ulps = "maxUlps" in declared
    has_relative = "maxP95Relative" in declared
    if has_ulps == has_relative:
        raise ValueError("region comparison requires exactly one error currency")
    if has_ulps:
        bound = quantity(declared, "maxUlps")
        if not isinstance(bound, int) or bound < 0:
            raise ValueError("maxUlps must be a nonnegative integer")
        errors = np.abs(a.view(np.int32).astype(np.int64) - b.view(np.int32).astype(np.int64))
        maximum = float(errors.max())
        return maximum <= bound, f"max {maximum:.0f} ULP; bound {bound}; {int((errors > bound).sum())} components fail"
    bound = quantity(declared, "maxP95Relative")
    if not isinstance(bound, (int, float)) or bound < 0:
        raise ValueError("maxP95Relative must be finite and nonnegative")
    denominator = np.maximum(np.abs(a.astype(np.float64)), np.abs(b.astype(np.float64)))
    relative = np.divide(difference, denominator, out=np.zeros(difference.shape),
                         where=denominator > 0)
    p95 = nearest_rank(relative, .95)
    return p95 <= bound, f"p95 relative {p95:.6g}; bound {bound:.6g}"


def hue_of_brightest(frame, declared):
    rgb = frame[..., :3]
    brightness = rgb.sum(axis=2)
    covered = frame[..., 3] > 0
    if not covered.any():
        return False, "no covered pixels"
    fraction = quantity(declared, "brightestFraction")
    expected = np.asarray(quantity(declared, "hue"), dtype=np.float64)
    bound = quantity(declared, "maxHueError")
    if not 0 < fraction <= 1 or expected.shape != (3,) or not np.isfinite(expected).all():
        raise ValueError("invalid highlight population or hue")
    if bound < 0 or not math.isfinite(bound):
        raise ValueError("invalid hue error bound")
    population = brightness[covered]
    threshold = nearest_rank(population, 1.0 - fraction)
    selected = rgb[covered & (brightness >= threshold) & (brightness > 0)].astype(np.float64)
    if not selected.size:
        return False, "no bright covered pixels"
    hue = selected / selected.sum(axis=1, keepdims=True)
    maximum = float(np.abs(hue - expected).max())
    return maximum <= bound, f"max hue error {maximum:.9g}; bound {bound:.9g}"


def evaluate(frame, manifest):
    render = manifest["renders"]["default"]
    expected_shape = (render["resolutionY"], render["resolutionX"], 4)
    if frame.dtype != np.float32 or frame.shape != expected_shape or not np.isfinite(frame).all():
        raise ValueError("linear attachment has wrong shape, type or nonfinite values")
    invariants = manifest["statedInvariants"]
    if not invariants or not np.any(frame[..., :3] > 0):
        raise ValueError("declared invariants need a nonempty lit frame")
    results = []
    for declared in invariants:
        kind = declared["kind"]
        if kind == "region-compare":
            passed, detail = region_compare(frame, declared)
        elif kind == "hue-of-brightest":
            passed, detail = hue_of_brightest(frame, declared)
        else:
            raise ValueError(f"unknown stated invariant kind: {kind}")
        results.append((declared["name"], passed, detail))
    return results
