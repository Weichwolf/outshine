"""Bounded lower service landing and climb volume on an existing cooling programme."""

import math
from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True, slots=True)
class Box:
    identifier: str
    centre: tuple
    extent: tuple


@dataclass(frozen=True, slots=True)
class ServiceAccess:
    tower: object
    bay: int
    angle: float
    parts: tuple
    clearances: tuple
    route: tuple
    floor_m: float
    ground_m: float
    material: int


def plan(tower, *, bay=0):
    if type(bay) is not int or not 0 <= bay < tower.bays:
        raise ValueError("existing support-bay identity required")
    radius = tower.source.knots[0][1]
    collector = tower.collector.knots[-1][1]
    ground = tower.footing.knots[-1][0]
    floor = tower.collector.knots[-1][0] + 0.02
    ladder = radius - 0.9
    inner, outer = collector - 0.35, ladder + 0.05
    stand, climb = collector + 0.4, ladder + 0.41
    if not 3 <= floor - ground <= 12 or outer - collector < 0.8:
        raise ValueError(
            "source leaves no bounded service landing outside the collector"
        )
    parts = []
    prefix = tower.source.identifier + ":service"

    def box(name, centre, extent):
        parts.append(Box(prefix + ":" + name, tuple(centre), tuple(extent)))

    box("floor", [(inner + outer) / 2, 0, floor - 0.075], [outer - inner, 1.2, 0.15])
    low, high = ground - 0.08, floor + 1.05
    for side in (-1, 1):
        box(
            f"ladder-rail-{side}",
            [ladder, side * 0.45, (low + high) / 2],
            [0.05, 0.05, high - low],
        )
    count = math.ceil((floor - ground - 0.5) / 0.3) + 1
    for index, height in enumerate(np.linspace(ground + 0.25, floor - 0.25, count)):
        box(f"rung-{index}", [ladder, 0, height], [0.04, 0.95, 0.04])
    for side in (-1, 1):
        for index, r in enumerate((inner + 0.04, outer - 0.04)):
            box(
                f"guard-post-{side}-{index}",
                [r, side * 0.6, floor + 0.5],
                [0.05, 0.05, 1.1],
            )
        for index, height in enumerate((0.55, 1.05)):
            box(
                f"guard-side-{side}-{index}",
                [(inner + outer) / 2, side * 0.6, floor + height],
                [outer - inner, 0.04, 0.04],
            )
    for index, height in enumerate((0.55, 1.05)):
        box(
            f"guard-back-{index}", [inner + 0.04, 0, floor + height], [0.04, 1.25, 0.04]
        )
    for part in parts:
        c, half = np.asarray(part.centre), np.asarray(part.extent) / 2
        far = abs(c[:2]) + half[:2]
        if (
            np.linalg.norm(far) > radius
            or c[2] - half[2] < tower.source.knots[0][0]
            or c[2] + half[2] > tower.source.knots[-1][0]
        ):
            raise ValueError("service construction escapes the retained source bounds")
    foot = ground + 0.02
    top = floor + 0.02
    clearances = (
        Box(
            prefix + ":climb-space",
            (climb, 0, (foot + top + 2) / 2),
            (0.7, 0.7, top + 2 - foot),
        ),
        Box(
            prefix + ":step-off-space",
            ((stand + climb) / 2, 0, top + 1),
            (climb - stand + 0.7, 0.7, 2),
        ),
    )
    route = ((climb, 0, foot), (climb, 0, top), (stand, 0, top))
    return ServiceAccess(
        tower,
        bay,
        2 * math.pi * bay / tower.bays,
        tuple(parts),
        clearances,
        route,
        floor,
        ground,
        tower.source.materials[2],
    )
