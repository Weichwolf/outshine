"""Broad, low clumps of leaved Mueller oak on the Mojave woodland margins."""

from functools import cache

import numpy as np

from outshine.vegetation import AxisLevel, Blade, PlantPlan
from outshine.vegetation.woodland_shrubs import shrub_mesh

MULLER_OAK_TAXON = "outshine:taxon:wcvp:172316"
MULLER_OAK_PROFILE = "mojave-muller-oak-1"
MULLER_OAK_UNITS = frozenset(
    ("JOTR-2012:10032", "JOTR-2012:10161", "JOTR-2012:21230", "JOTR-2012:21234")
)


@cache
def muller_oak(variant, detailed):
    stem = AxisLevel(
        1,
        (0.29, 0.38, 0.40, 0.58)[variant],
        0.062,
        0.036,
        bend=0.26,
    )
    limbs = AxisLevel(
        13,
        (1.26, 1.55, 1.78, 1.94)[variant],
        0.026,
        0.004,
        radius_parent_ratio=0.83,
        first=0.13,
        last=0.87,
        angle=(1.14, 0.96, 1.28, 1.10)[variant],
        angle_tip=0.58,
        azimuth_variation=0.52,
        bend=0.46,
        bend_variation=0.24,
        upward_bias=0.19,
        tip_scale=0.45,
        length_shape="rounded",
    )
    twigs = AxisLevel(
        15 if detailed else 11,
        (0.49, 0.63, 0.66, 0.73)[variant],
        0.0052,
        0.0011,
        radius_parent_ratio=0.53,
        first=0.30,
        last=0.99,
        angle=0.90,
        angle_tip=0.52,
        azimuth_variation=0.91,
        bend=0.41,
        upward_bias=0.19,
        length_shape="rounded",
        leaf_internode=0.026 if detailed else 0.105,
        arrangement="spiral",
        leaf_first=0.28,
        leaf_last=0.92,
        leaf_sky_alignment=0.55,
    )
    shoots = AxisLevel(
        4 if detailed else 3,
        0.21,
        0.0015,
        0.0005,
        first=0.37,
        last=0.93,
        angle=0.99,
        tip_scale=0.64,
        azimuth_variation=0.81,
        bend=0.37,
        radius_parent_ratio=0.65,
        leaf_internode=0.014 if detailed else 0.044,
        arrangement="spiral",
        leaf_sky_alignment=0.66,
    )
    blade = Blade(
        length=0.039 if detailed else 0.062,
        width=0.025 if detailed else 0.044,
        widest=0.61,
        lobes=0,
        fold=0.22,
        petiole=0.0018,
    )
    plan = PlantPlan(17341 + variant * 403, (stem, limbs, twigs, shoots), (blade,))
    meshes, radius = shrub_mesh(
        plan, "muller oak bark", "muller oak leaves", 3 if detailed else 2
    )
    return {
        name: (vertices.astype(np.float32), faces.astype(np.int32))
        for name, (vertices, faces) in meshes.items()
    }, float(radius)
