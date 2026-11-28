"""Expected physical organ area from complete counts before needle placement."""

import math

import numpy as np


def area_summary(plant, unit_areas_m2):
    areas = np.asarray(unit_areas_m2)
    size = len(plant.needles)
    if (
        areas.shape != (size,)
        or areas.dtype.kind not in "fiu"
        or not np.isfinite(areas).all()
        or np.any(areas <= 0)
    ):
        raise ValueError(
            "one finite positive physical area per needle variant required"
        )
    areas = tuple(float(a) for a in areas)
    if any(not math.isfinite(a) or a <= 0 for a in areas):
        raise ValueError("physical areas must be representable as positive Float64")
    variants, orders, first = [0] * size, [], 0
    for count, pattern in zip(plant.needle_counts, plant.patterns):
        cycles, remainder = divmod(count, size)
        counts = [cycles] * size
        for j in range(remainder):
            counts[(first + plant.wood.seed + j) % size] += 1
        variance = pattern.scale_variation**2 / 3 if pattern is not None else 0
        orders.append(sum(n * a for n, a in zip(counts, areas)) * (1 + variance))
        variants = [a + b for a, b in zip(variants, counts)]
        first += count
    total = sum(orders)
    if not math.isfinite(total):
        raise ValueError("finite expected physical organ area required")
    return {
        "identity": plant.identity,
        "needles": first,
        "variant_counts": tuple(variants),
        "unit_areas_m2": areas,
        "expected_area_m2": total,
        "expected_area_by_order_m2": tuple(orders),
        "scale_model": "independent uniform scalar variation; E[scale^2]=1+variation^2/3",
    }
