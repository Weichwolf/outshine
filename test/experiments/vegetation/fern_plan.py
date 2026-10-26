"""Own two-level fern study: actual prepared pinna tables precede all mesh detail."""

import numpy as np
from outshine_ref.vegetation import FrondGuide, LeafletShape, PinnatePattern, Profile
from outshine_ref.vegetation.shoot_plan import normal_frame


def pinna_pattern(seed):
    return PinnatePattern(
        seed=seed,
        length=1,
        pairs=11,
        leaflet_length=0.32,
        aspect=0.45,
        angle=1.2,
        first=0.04,
        last=0.97,
        variance=0.04,
        leaflet_shape=LeafletShape(
            bend=Profile(np.full(4, 0.12)),
            fold=Profile(np.full(4, 0.10)),
        ),
    )


def rows(pattern):
    return [
        {
            key: value.tolist() if isinstance(value, np.ndarray) else value
            for key, value in row.items()
        }
        for row in pattern.organs()
    ]


def prepare(seed=221, fronds=7, pairs=9):
    if (
        type(fronds) is not int
        or not 3 <= fronds <= 12
        or type(pairs) is not int
        or not 3 <= pairs <= 16
    ):
        raise ValueError("three to12 fronds and three to16 primary pairs required")
    child = pinna_pattern(seed)
    rng = np.random.Generator(np.random.PCG64(seed))
    crown = []
    for i in range(fronds):
        age = 1 - i / (fronds + 0.5)
        length = (0.38 + 0.38 * age) * rng.uniform(0.94, 1.06)
        guide = FrondGuide(
            length=length,
            bend=-(0.7 + 0.75 * age),
            fold_per_m=0.15,
            roll=Profile(np.linspace(0, rng.uniform(-0.12, 0.12), 4)),
        )
        primary = PinnatePattern(
            seed=(seed + i + 1) % 2**64,
            length=length,
            pairs=pairs,
            leaflet_length=0.25 * length,
            aspect=0.25,
            angle=1.2,
            first=0.25,
            last=0.95,
            variance=0.06,
        )
        pinnae = rows(primary)
        extent = max(child.source_extent, 0.006)
        for row in pinnae:
            frame = np.asarray(row["frame"])
            y = row["origin"][1]
            backward = extent * abs(frame[1, 0])
            forward = frame[1, 1] + backward
            limit = (length - y) / forward
            if backward > 0:
                limit = min(limit, y / backward)
            row["length"] = min(row["length"], limit) * (1 - 1e-12)
        angle = i * np.pi * (3 - np.sqrt(5)) + rng.uniform(-0.10, 0.10)
        lean = 0.2 + 0.35 * age
        direction = (
            np.sin(lean) * np.cos(angle),
            np.sin(lean) * np.sin(angle),
            np.cos(lean),
        )
        crown.append(
            {
                "id": i,
                "guide": guide.recipe(),
                "pinnae": pinnae,
                "origin": [
                    0.018 * np.cos(angle),
                    0.018 * np.sin(angle),
                    0.008 + i * 0.0015,
                ],
                "frame": normal_frame(direction, (0, 0, 1)).tolist(),
            }
        )
    return {
        "format": "nested-fern-study-1",
        "seed": seed,
        "pinna_pattern": child.recipe(),
        "pinnules": rows(child),
        "fronds": crown,
        "scope": "Own doubly pinnate fern study after local positional-information/reference morphology; no accepted botanical species.",
    }
