"""Conservative per-pixel demand masks for the existing cube capture grid."""

import math
from collections import Counter
from fractions import Fraction as F

import numpy as np


def capture_rectangle(bounds, eye, axis, sign, size):
    """Exact represented-coordinate box projection, padded by one existing pixel.

    None is an unknown bound and admits the whole face. A box containing the
    observer also admits everything, including zero-distance RayEntry hits.
    This is possible ray demand, never an opacity or empty-source certificate.
    """
    if (
        type(size) is not int
        or not 1 <= size <= 8192
        or axis not in (0, 1, 2)
        or sign not in (-1, 1)
    ):
        raise ValueError("bounded cube grid and explicit axis/sign required")
    observer = np.asarray(eye, dtype=np.float64)
    if (
        observer.shape != (3,)
        or not np.isfinite(observer).all()
        or np.abs(observer).max() > 1e7
    ):
        raise ValueError("bounded finite observer required")
    if bounds is None:
        return (0, 0, size, size)
    box = np.asarray(bounds, dtype=np.float64)
    if (
        box.shape != (2, 3)
        or not np.isfinite(box).all()
        or np.abs(box).max() > 1e7
        or (box[0] > box[1]).any()
    ):
        raise ValueError("bounded finite ordered box required")
    if ((observer >= box[0]) & (observer <= box[1])).all():
        return (0, 0, size, size)
    ranges = [
        (
            F(float(box[0, i])) - F(float(observer[i])),
            F(float(box[1, i])) - F(float(observer[i])),
        )
        for i in range(3)
    ]
    near, far = ranges[axis] if sign == 1 else (-ranges[axis][1], -ranges[axis][0])
    x, y = ranges[(axis + 1) % 3], ranges[(axis + 2) % 3]
    if far <= 0 or any(
        far + interval[1] < 0 or far - interval[0] < 0 for interval in (x, y)
    ):
        return None
    if near <= F(1e-6):
        return (0, 0, size, size)
    ratios = [
        tuple(value / depth for value in interval for depth in (near, far))
        for interval in (x, y)
    ]
    low, high = [min(row) for row in ratios], [max(row) for row in ratios]
    if any(a > 1 or b < -1 for a, b in zip(low, high, strict=True)):
        return None
    lower = lambda value: max(
        0, min(size, math.floor((max(-1, min(1, value)) + 1) * size / 2) - 1)
    )
    upper = lambda value: max(
        0, min(size, math.ceil((max(-1, min(1, value)) + 1) * size / 2) + 1)
    )
    return (lower(low[0]), lower(low[1]), upper(high[0]), upper(high[1]))


def rectangles(values, size):
    result = []
    for value in values:
        if value is None:
            continue
        if len(value) != 4 or any(type(number) is not int for number in value):
            raise ValueError("integer half-open capture rectangle required")
        left, top, right, bottom = value
        if not (0 <= left <= right <= size and 0 <= top <= bottom <= size):
            raise ValueError("rectangle escapes its original capture face")
        if left != right and top != bottom:
            result.append(value)
    return result


def occupied_tiles(values, size):
    """The original 64×64 tile admission, without pixel filling."""
    return {
        (y, x)
        for left, top, right, bottom in rectangles(values, size)
        for y in range(top // 64, (bottom - 1) // 64 + 1)
        for x in range(left // 64, (right - 1) // 64 + 1)
    }


def tile_shape(tile, size):
    y, x = tile
    return min(64, size - y * 64), min(64, size - x * 64)


def word_masks(tiles, values, size, *, pair_budget=1_000_000):
    """OR rectangle rows into original tiles; budget exhaustion restores all rays.

    A row is one UInt64 word. Fine-only omission is a caller-selected demand
    contract; this function does not remove Fine occluders from ray traversal.
    """
    if (
        type(size) is not int
        or not 1 <= size <= 8192
        or type(pair_budget) is not int
        or pair_budget < 0
    ):
        raise ValueError("bounded grid and nonnegative pair budget required")
    admitted = set(tiles)
    width = (size + 63) // 64
    if any(
        len(tile) != 2 or any(type(c) is not int or not 0 <= c < width for c in tile)
        for tile in admitted
    ):
        raise ValueError("original occupied tile coordinates required")
    masks = {
        tile: np.zeros(tile_shape(tile, size)[0], dtype=np.uint64)
        for tile in sorted(admitted)
    }
    work = Counter(rectangles=0, tile_pair_visits=0, row_word_ors=0)
    exhausted = False
    for left, top, right, bottom in rectangles(values, size):
        work["rectangles"] += 1
        for y in range(top // 64, (bottom - 1) // 64 + 1):
            for x in range(left // 64, (right - 1) // 64 + 1):
                if work["tile_pair_visits"] >= pair_budget:
                    exhausted = True
                    break
                work["tile_pair_visits"] += 1
                if (y, x) not in masks:
                    continue
                low_x, high_x = max(left - x * 64, 0), min(right - x * 64, 64)
                low_y, high_y = max(top - y * 64, 0), min(bottom - y * 64, 64)
                bits = np.uint64(((1 << (high_x - low_x)) - 1) << low_x)
                masks[y, x][low_y:high_y] |= bits
                work["row_word_ors"] += high_y - low_y
            if exhausted:
                break
        if exhausted:
            break
    if exhausted:
        for tile, rows in masks.items():
            rows.fill(np.uint64((1 << tile_shape(tile, size)[1]) - 1))
    work["budget_exhausted"] = exhausted
    work["row_array_allocations"] = len(masks)
    work["row_array_bytes"] = sum(rows.nbytes for rows in masks.values())
    work["baseline_rays"] = sum(math.prod(tile_shape(tile, size)) for tile in masks)
    work["selected_rays"] = sum(
        int(np.bitwise_count(rows).sum()) for rows in masks.values()
    )
    for rows in masks.values():
        rows.setflags(write=False)
    return masks, dict(work)


def selected_pixels(masks):
    """Diagnostic enumeration; a native consumer can scan words using bit scans."""
    for (tile_y, tile_x), rows in masks.items():
        for local_y, given in enumerate(rows):
            bits = int(given)
            while bits:
                bit = bits & -bits
                yield tile_y * 64 + local_y, tile_x * 64 + bit.bit_length() - 1
                bits ^= bit
