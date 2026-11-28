"""Low, many-branched Coleogyne ramosissima with pairs of short shoots."""

from functools import cache

import numpy as np

from outshine.vegetation import AxisLevel, Blade, PlantPlan
from outshine.vegetation.woodland_shrubs import shrub_mesh

BLACKBRUSH_TAXON = "outshine:taxon:wcvp:2943856"
BLACKBRUSH_PROFILE = "mojave-blackbrush-1"
BLACKBRUSH_UNITS = frozenset(
    ("JOTR-2012:10025", "JOTR-2012:10032", "JOTR-2012:13012", "JOTR-2012:29032")
)


@cache
def blackbrush(variant, detailed):
    stem = AxisLevel(
        branches=1,
        length=(0.14, 0.19, 0.16, 0.23)[variant],
        base_radius=0.023,
        tip_radius=0.0095,
        bend=0.17,
    )
    limbs = AxisLevel(
        branches=10,
        length=(0.50, 0.69, 0.56, 0.87)[variant],
        base_radius=0.010,
        tip_radius=0.0018,
        radius_parent_ratio=0.73,
        first=0.12,
        last=0.95,
        angle=(1.14, 1.02, 0.96, 1.25)[variant],
        angle_tip=0.53,
        azimuth_variation=0.36,
        bend=0.23,
        length_shape="rounded",
        tip_scale=0.65,
        whorl_size=2,
    )
    branchlets = AxisLevel(
        branches=16 if detailed else 10,
        length=(0.175, 0.192, 0.220, 0.195)[variant],
        base_radius=0.0023,
        tip_radius=0.00048,
        radius_parent_ratio=0.70,
        first=0.33,
        last=0.96,
        angle=0.98,
        bend=0.17,
        whorl_size=2,
        leaf_internode=(0.0039 + 0.0009 * variant) if detailed else 0.017,
        leaf_first=0.39,
        leaf_last=0.96,
        leaf_angle=0.85,
        leaf_sky_alignment=0.28,
        arrangement="opposite",
    )
    leaf = Blade(
        length=0.0125 if detailed else 0.027,
        width=0.0017 if detailed else 0.0044,
        widest=0.7,
        fold=0.17,
        petiole=0.0007,
    )
    plan = PlantPlan(9313 + 229 * variant, (stem, limbs, branchlets), (leaf,))
    meshes, radius = shrub_mesh(
        plan, "blackbrush wood", "blackbrush leaves", 3 if detailed else 2
    )
    return {
        name: (vertices.astype(np.float32), faces.astype(np.int32))
        for name, (vertices, faces) in meshes.items()
    }, radius
