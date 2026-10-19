"""Outer local bounds for the existing source trace, after RequireShapes."""

import numpy as np


def local_part_bounds(surface, *, padding_m):
    """Use original rings/holes and the native Bottom/Top parameter recipe.

    Unknown/unsupported geometry returns None, requiring the original trace.
    Padding is an explicit outer margin, never an opacity certificate.
    """
    if surface is None or padding_m is None or not surface["projectable"]:
        return None
    if not np.isfinite(padding_m) or padding_m < 0 or padding_m > 1e6:
        raise ValueError("finite nonnegative bounded outer padding required")
    if not 1 <= len(surface["shapes"]) <= 4096:
        return None
    boxes = []
    for shape in surface["shapes"]:
        rise = shape["rise_m"]
        if (
            not np.isfinite(rise)
            or rise < 0
            or (rise != 0 and shape["roof_kind"] not in (0, 1, 2, 3, 4))
        ):
            return None
        rings = [
            np.asarray(ring, dtype=np.float64)
            for ring in [shape["ring"], *shape["holes"]]
        ]
        if any(
            ring.ndim != 2 or ring.shape[1:] != (2,) or len(ring) < 3 for ring in rings
        ):
            return None
        xy = np.concatenate(rings)
        bottom, deck = shape["bottom_m"], shape["roof_deck_m"]
        top = deck + (rise if shape["roof_kind"] != 0 else 0)
        if (
            not np.isfinite(xy).all()
            or np.abs(xy).max() > 1e7
            or not np.isfinite([bottom, deck, top]).all()
            or np.abs([bottom, deck, top]).max() > 1e7
        ):
            return None
        boxes.append(
            [[*xy.min(axis=0), min(bottom, deck)], [*xy.max(axis=0), max(bottom, top)]]
        )
    result = np.asarray(boxes, dtype=np.float64)
    with np.errstate(under="ignore"):
        result[:, 0] = np.nextafter(result[:, 0] - padding_m, -np.inf)
        result[:, 1] = np.nextafter(result[:, 1] + padding_m, np.inf)
    result.setflags(write=False)
    return result


def union_bound(parts):
    if parts is None:
        return None
    result = np.stack((parts[:, 0].min(axis=0), parts[:, 1].max(axis=0)))
    result.setflags(write=False)
    return result


def source_local_rays(surface, origins, directions):
    """Explicit ordered source-frame placement, also matching single-ray batches.

    A native guard reuses the already computed local ray in Surface::Trace;
    these transforms are for independently replaying original parent-frame rays.
    """
    origin = np.asarray(surface["origin"], dtype=np.float64)
    axes = np.asarray(
        [surface[key] for key in ("east", "north", "up")], dtype=np.float64
    )
    points, vectors = (
        np.asarray(origins, dtype=np.float64),
        np.asarray(directions, dtype=np.float64),
    )
    if (
        origin.shape != (3,)
        or axes.shape != (3, 3)
        or points.shape[-1:] != (3,)
        or vectors.shape[-1:] != (3,)
    ):
        raise ValueError("explicit source frame and three-coordinate rays required")
    delta = points - origin

    def apply(values):
        return np.stack(
            [
                values[..., 0] * axis[0]
                + values[..., 1] * axis[1]
                + values[..., 2] * axis[2]
                for axis in axes
            ],
            axis=-1,
        )

    with np.errstate(over="ignore", invalid="ignore"):
        return apply(delta), apply(vectors)


def ray_candidates(bound, origins, directions, *, minimum=0.0, maximum=np.inf):
    """False only for proven empty represented local rays; at most 65,536 rays.

    Outward subtraction/division intervals include subnormal rounding. Invalid
    ray state is unproved and remains delegated to the original consumer.
    """
    origins, directions = np.broadcast_arrays(
        np.asarray(origins, dtype=np.float64), np.asarray(directions, dtype=np.float64)
    )
    if origins.ndim != 2 or origins.shape[1:] != (3,) or len(origins) > 65536:
        raise ValueError("bounded batch of local three-coordinate rays required")
    count = len(origins)
    if bound is None:
        return np.ones(count, dtype=bool)
    box = np.asarray(bound, dtype=np.float64)
    if box.shape != (2, 3) or not np.isfinite(box).all() or (box[0] > box[1]).any():
        raise ValueError("finite ordered local outer bound required")
    low, high = (
        np.broadcast_to(minimum, (count,)).astype(float, copy=True),
        np.broadcast_to(maximum, (count,)).astype(float, copy=True),
    )
    unproved = (
        ~np.isfinite(origins).all(axis=1)
        | ~np.isfinite(directions).all(axis=1)
        | ~np.isfinite(low)
        | np.isnan(high)
        | (low < 0)
        | (high < low)
    )
    valid = np.ones(count, dtype=bool)
    with np.errstate(over="ignore", divide="ignore", invalid="ignore", under="ignore"):
        for axis in range(3):
            a = np.nextafter(box[0, axis] - origins[:, axis], -np.inf)
            b = np.nextafter(box[1, axis] - origins[:, axis], np.inf)
            direction = directions[:, axis]
            moving = direction != 0
            first = np.divide(a, direction, out=np.full(count, -np.inf), where=moving)
            last = np.divide(b, direction, out=np.full(count, np.inf), where=moving)
            low = np.maximum(low, np.nextafter(np.minimum(first, last), -np.inf))
            high = np.minimum(high, np.nextafter(np.maximum(first, last), np.inf))
            valid &= moving | (
                (origins[:, axis] >= box[0, axis]) & (origins[:, axis] <= box[1, axis])
            )
    return unproved | (valid & (low <= high))


def trace_after_require(require_shapes, prepare_guard, original_trace):
    """Executable ordering contract: preparation errors precede every rejection.

    prepare_guard returns the bound and a single already-local origin/direction.
    All original part/hit ordering is delegated unchanged when admitted.
    """
    require_shapes()
    bound, origin, direction, minimum, maximum = prepare_guard()
    selected = ray_candidates(
        bound,
        np.asarray(origin)[None],
        np.asarray(direction)[None],
        minimum=minimum,
        maximum=maximum,
    )
    return original_trace() if selected[0] else None
