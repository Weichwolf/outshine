"""Montane woodland composition and grouped spruce needle sprays."""

import numpy as np
import shapely
from outshine_ref.vegetation import (
    AxisLevel,
    Needle,
    NeedledPlant,
    NeedlePattern,
    PlantPlan,
)

SPRAY_WIDTH_M = 0.156
NEEDLE_SPACING_M = 0.012
NEEDLES_PER_GROUP = 5


def conifer_sites(source, xy, relative_height, rank, *, features=None):
    share = np.zeros(len(xy))
    if 43.5 <= source.lat <= 49.5 and 5 <= source.lon <= 17:
        # A montane fallback for untyped forest in the central European reference.
        fraction = np.clip((relative_height + source.datum - 1200) / 370, 0, 1)
        share = fraction**2 * (3 - 2 * fraction)
    leaf_types = {"broadleaved": 0.0, "mixed": 0.5, "needleleaved": 1.0}
    for layer, shape, tags in source.features if features is None else features:
        leaf_type = tags.get("leaf_type")
        if (
            layer == "landcover"
            and tags.get("class") == "wood"
            and leaf_type in leaf_types
        ):
            share[shapely.contains_xy(shape, *xy.T)] = leaf_types[leaf_type]
    return rank < share


def spruce_plan(form):
    trunk = AxisLevel(
        1,
        (7.8, 11.1)[form],
        (0.112, 0.164)[form],
        0.009,
        bend=0.075,
        bend_variation=0.045,
    )
    branches = AxisLevel(
        (48, 55)[form],
        (1.94, 2.56)[form],
        (0.034, 0.047)[form],
        0.004,
        whorl_size=(4, 5)[form],
        first=(0.11, 0.16)[form],
        last=0.95,
        tip_scale=0.14,
        radius_parent_ratio=0.47,
        attachment_jitter=0.23,
        azimuth_variation=0.41,
        angle=1.65,
        angle_tip=1.18,
        angle_variation=0.21,
        bend=0.22,
        bend_variation=0.15,
    )
    twigs = AxisLevel(
        15,
        (0.51, 0.65)[form],
        0.0048,
        0.0010,
        first=0.14,
        last=0.98,
        tip_scale=0.26,
        radius_parent_ratio=0.52,
        attachment_jitter=0.25,
        azimuth_variation=0.75,
        angle=1.03,
        angle_tip=0.51,
        bend=0.21,
    )
    shoots = AxisLevel(
        4,
        0.18,
        0.0016,
        0.00042,
        first=0.19,
        last=0.91,
        tip_scale=0.43,
        radius_parent_ratio=0.50,
        attachment_jitter=0.28,
        azimuth_variation=1.04,
        angle=0.88,
        angle_tip=0.64,
        bend=0.19,
    )
    pattern = NeedlePattern(spacing=NEEDLE_SPACING_M, group_size=NEEDLES_PER_GROUP)
    wood = PlantPlan(2941 + 83 * form, (trunk, branches, twigs, shoots), ())
    branch_pattern = NeedlePattern(
        spacing=NEEDLE_SPACING_M, group_size=NEEDLES_PER_GROUP, first=0.30
    )
    return NeedledPlant(
        wood, (None, branch_pattern, pattern, pattern), (Needle(length=0.024),)
    )


def needle_sprays(plant, step):
    """Use two intersecting tapered profiles per physical needle window."""
    span = 0.12 * np.sqrt(step)
    budget = plant.window_summary(nominal_span_m=span)
    windows = plant.windows(budget["axes"], budget["windows"], nominal_span_m=span)
    retained = windows["orders"] == 2 if step >= 16 else slice(None)
    groups = windows["needle_counts"][retained] / NEEDLES_PER_GROUP
    length = (groups - 1) * NEEDLE_SPACING_M + 3.3 * plant.needles[0].length
    width = SPRAY_WIDTH_M * (0.88 + 0.12 * np.cos(windows["phases"][retained]))
    profile = np.array(
        [
            [0, -0.5],
            [0.35, -0.28],
            [0.49, -0.01],
            [0.28, 0.35],
            [0, 0.5],
            [-0.32, 0.26],
            [-0.47, -0.07],
            [-0.30, -0.34],
            [0, 0],
        ]
    )
    if step >= 16:
        # The side shoots are subpixel here. Represent them around their parent twig.
        width *= np.sqrt(sum(plant.needle_counts) / plant.needle_counts[2])
        profile = profile[[0, 2, 4, 6]]
        faces = np.array([[0, 1, 2], [0, 2, 3]])
    else:
        edge = np.arange(len(profile) - 1)
        faces = np.column_stack(
            (np.full_like(edge, len(edge)), edge, np.roll(edge, -1))
        )
    across = profile[:, 0] * width[:, None]
    along = profile[:, 1] * length[:, None]
    frames = windows["frames"][retained]
    centres = (
        windows["origins"][retained, None, :] + along[..., None] * frames[:, None, :, 1]
    )
    vertices = np.stack(
        [
            centres + across[..., None] * frames[:, None, :, 0],
            centres + 0.83 * across[..., None] * frames[:, None, :, 2],
        ],
        axis=1,
    ).reshape(-1, 3)
    face_offsets = len(profile) * np.arange(len(centres) * 2)
    return vertices, (faces + face_offsets[:, None, None]).reshape(-1, 3)
