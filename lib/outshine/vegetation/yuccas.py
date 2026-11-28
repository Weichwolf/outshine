"""Western Joshua tree and Mojave yucca architecture in metres."""

from functools import cache

import numpy as np

from outshine.vegetation.woody_axis import woody_axis
from outshine.world.mesh import Meshes

JOSHUA_TAXON = "outshine:taxon:wcvp:291788"
MOJAVE_YUCCA_TAXON = "outshine:taxon:wcvp:309930"
JOSHUA_PROFILE = "western-joshua-tree-1"
MOJAVE_YUCCA_PROFILE = "mojave-yucca-1"
JOSHUA_BARK = "joshua tree bark"
YUCCA_BARK = "mojave yucca bark"
DEAD_LEAVES = "yucca dead leaves"
ROSETTE_FACES = np.array([[0, 3, 1], [1, 3, 4], [1, 4, 2], [2, 4, 5]])
GOLDEN_ANGLE = np.pi * (3 - np.sqrt(5))


def unit(vector):
    return vector / np.linalg.norm(vector)


def leaf_ribbons(meshes, anchor, axis, name, rng, *, joshua, dry, detailed):
    """Folded, tapered leaves with outer blades turning away from the growth tip."""
    count = 78 if dry else 169 if joshua else 92
    angle = np.arange(count) * GOLDEN_ANGLE + rng.uniform(0, 2 * np.pi)
    layer = rng.uniform(0.05, 1.0, count)
    size = (0.19, 0.37) if joshua else (0.43, 0.91)
    length = rng.uniform(*size, count)
    width = (
        rng.uniform(0.010, 0.020, count) if joshua else rng.uniform(0.019, 0.044, count)
    )
    if dry:
        direction = -rng.uniform(0.62, 1.52, count)
        base_height = -rng.uniform(0.04, 0.29 if joshua else 0.25, count)
        length *= 0.88 if joshua else 0.72
    else:
        direction = np.pi / 2 - (1.84 if joshua else 1.56) * layer
        base_height = 0.04 * (1 - layer)
    cross = unit(np.cross((0, 1, 0), axis))
    tangent = np.cross(axis, cross)
    radial = np.cos(angle)[:, None] * cross + np.sin(angle)[:, None] * tangent
    lateral = np.cross(axis, radial)
    stations = np.linspace(0, 1, 6 if detailed else 4)
    rest = stations[None, :]
    growth = (
        (0.015 + 0.040 * layer[:, None])
        + 0.073 * layer[:, None] * np.sin(0.5 * np.pi * rest)
        + np.cos(direction)[:, None] * length[:, None] * rest
    )
    droop = (0.15 + 0.21 * layer) if not joshua else (0.09 + 0.11 * layer)
    height = base_height[:, None] + length[:, None] * (
        np.sin(direction)[:, None] * rest - droop[:, None] * rest**2
    )
    centres = anchor + radial[:, None, :] * growth[..., None] + axis * height[..., None]
    half = 0.5 * width[:, None] * (0.52 + 1.22 * rest) * (1 - rest) ** 0.66
    half[:, -1] = 0.00018
    if not detailed:
        centres, radial, lateral, half = (
            values[::3] for values in (centres, radial, lateral, half)
        )
        half = 1.50 * half
    middle = centres + 0.39 * half[..., None] * radial[:, None, :]
    offset = half[..., None] * lateral[:, None, :]
    vertices = np.stack((centres - offset, middle, centres + offset), axis=2)
    stride = 3 * len(stations)
    rows = np.arange(len(vertices))[:, None, None, None] * stride
    segments = np.arange(len(stations) - 1)[None, :, None, None] * 3
    faces = rows + segments + ROSETTE_FACES
    meshes.add(name, vertices.reshape(-1, 3), faces.reshape(-1, 3))


def rosette(meshes, anchor, axis, rng, joshua, detailed):
    # Own random stream so foliage LOD does not change the axis network.
    leaves = np.random.default_rng(rng.integers(2**32))
    if joshua:
        woody_axis(
            meshes,
            (anchor - 0.27 * axis, anchor - 0.06 * axis, anchor + 0.037 * axis),
            0.037,
            0.055,
            leaves,
            stations=5,
            sides=8,
            wander=0.006,
            material=JOSHUA_BARK,
        )
    name = "joshua tree leaves" if joshua else "mojave yucca leaves"
    leaf_ribbons(
        meshes, anchor, axis, name, leaves, joshua=joshua, dry=False, detailed=detailed
    )
    leaf_ribbons(
        meshes,
        anchor,
        axis,
        DEAD_LEAVES,
        leaves,
        joshua=joshua,
        dry=True,
        detailed=detailed,
    )


def grow_branch(meshes, start, heading, reach, rise, girth, forks, rng, detailed):
    step = reach * np.array([np.cos(heading), np.sin(heading), 0.0])
    up = np.array([0.0, 0.0, rise])
    end = start + step + up
    controls = (start, start + 0.29 * step + 0.40 * up, end - 0.27 * up, end)
    branch = woody_axis(
        meshes,
        controls,
        girth,
        girth * (0.56 if forks else 0.36),
        rng,
        stations=11,
        sides=9,
        wander=min(0.12, 0.07 * rise),
        material=JOSHUA_BARK,
    )
    if not forks:
        rosette(meshes, end, unit(branch.derivative()(1)), rng, True, detailed)
        return
    joint = branch(rng.uniform(0.74, 0.91))
    lean = rng.uniform(0.59, 1.22)
    for side in (-1, 1):
        grow_branch(
            meshes,
            joint,
            heading + side * (lean + rng.uniform(-0.26, 0.27)),
            reach * rng.uniform(0.42, 0.78),
            rise * rng.uniform(0.49, 0.82),
            0.60 * girth,
            forks - 1,
            rng,
            detailed,
        )


def joshua_tree(meshes, rng, variant, mature, detailed):
    sway = rng.uniform(0.14, 0.42, 2) * rng.choice((-1, 1), 2)
    height = rng.uniform(1.85, 2.55) if mature else rng.uniform(1.05, 2.22)
    end = np.r_[sway, height]
    girth = rng.uniform(0.18, 0.28) if mature else rng.uniform(0.092, 0.144)
    controls = ((0, 0, -0.31), (*(-0.48 * sway), 0.47), (0, 0, 0.75 * height), end)
    trunk = woody_axis(
        meshes,
        controls,
        girth,
        0.63 * girth,
        rng,
        stations=16,
        sides=12,
        trunk=True,
        wander=0.077,
        material=JOSHUA_BARK,
    )
    if not mature:
        rosette(meshes, end, unit(trunk.derivative()(1)), rng, True, detailed)
        return
    rotation = rng.uniform(0, 2 * np.pi)
    for branch in range(3 + variant % 2):
        heading = rotation + branch * GOLDEN_ANGLE + rng.uniform(-0.43, 0.43)
        body = trunk(1 - rng.uniform(0.06, 0.33) * (branch > 0))
        grow_branch(
            meshes,
            body,
            heading,
            rng.uniform(0.52, 1.14),
            rng.uniform(0.61, 1.34),
            girth * rng.uniform(0.38, 0.52),
            1 + (branch == 0 and variant != 0),
            rng,
            detailed,
        )


def mojave_yucca(meshes, rng, variant, detailed):
    rotation = rng.uniform(0, 2 * np.pi)
    for stem in range((1, 2, 3, 2)[variant]):
        heading = rotation + stem * GOLDEN_ANGLE
        radial = np.array([np.cos(heading), np.sin(heading), 0])
        length = rng.uniform(0.28, 0.94) * (1.0 if stem == 0 else 0.67)
        reach = rng.uniform(0.24, 0.49) * (0.49 if stem == 0 else 1.0)
        start = 0.084 * radial - [0, 0, 0.31]
        end = reach * radial + [0, 0, length]
        controls = (start, 0.23 * reach * radial, end - [0, 0, 0.23], end)
        base = rng.uniform(0.073, 0.118)
        stalk = woody_axis(
            meshes,
            controls,
            base,
            base * 0.68,
            rng,
            stations=10,
            sides=8,
            trunk=True,
            wander=0.028,
            material=YUCCA_BARK,
        )
        rosette(meshes, end, unit(stalk.derivative()(1)), rng, False, detailed)


@cache
def yucca_geometry(taxon, variant, mature, detailed):
    joshua = taxon == JOSHUA_TAXON
    rng = np.random.default_rng(0xDF29 + variant * 177 + 231 * mature + 127 * joshua)
    meshes = Meshes()
    if joshua:
        joshua_tree(meshes, rng, variant, mature, detailed)
    else:
        mojave_yucca(meshes, rng, variant, detailed)
    for name, (vertices, faces) in meshes.items():
        meshes[name] = np.asarray(vertices, np.float32), np.asarray(faces, np.int32)
    radius = max(
        np.linalg.norm(points[:, :2], axis=1).max() for points, _ in meshes.values()
    )
    return meshes, radius
