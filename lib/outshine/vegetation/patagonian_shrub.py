"""A narrowly leaved, multistem shrub representative of Patagonian open cover."""

from functools import cache

from outshine.vegetation import AxisLevel, Blade, PlantPlan
from outshine.vegetation.woodland_shrubs import shrub_mesh

PATAGONIAN_COMMUNITIES = frozenset(("CHL-2017:P70", "CHL-2017:P125"))
SHRUB_PROFILE = "patagonian-evergreen-shrub-1"


@cache
def patagonian_shrub(variant, detailed):
    bole = AxisLevel(
        branches=1,
        length=0.34,
        base_radius=0.017,
        tip_radius=0.003,
        bend=0.20,
    )
    limbs = AxisLevel(
        branches=11,
        length=(0.83, 0.97, 0.88)[variant],
        base_radius=0.007,
        tip_radius=0.0011,
        radius_parent_ratio=0.53,
        first=0.04,
        last=0.77,
        tip_scale=0.53,
        angle=(1.02, 1.24, 1.43)[variant],
        angle_tip=0.68,
        angle_variation=0.24,
        azimuth_variation=0.32,
        bend=0.39,
        upward_bias=0.10,
        length_shape="rounded",
    )
    shoots = AxisLevel(
        branches=15,
        length=0.26,
        base_radius=0.0015,
        tip_radius=0.00035,
        radius_parent_ratio=0.50,
        first=0.25,
        last=0.96,
        length_shape="rounded",
        angle=0.95,
        bend=0.26,
        leaf_internode=0.0068 if detailed else 0.024,
        leaf_sky_alignment=0.35,
    )
    blade = Blade(
        length=0.037 if detailed else 0.062,
        width=0.0054 if detailed else 0.017,
        widest=0.58,
        lobes=0,
        fold=0.25,
        petiole=0.0015,
    )
    plan = PlantPlan(7131 + 307 * variant, (bole, limbs, shoots), (blade,))
    return shrub_mesh(
        plan, "patagonian shrub wood", "patagonian shrub leaves", 3 if detailed else 2
    )
