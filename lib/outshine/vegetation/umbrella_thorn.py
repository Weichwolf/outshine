"""Seeded architecture for the spreading East African umbrella thorn."""

from functools import cache

import numpy as np

from outshine.vegetation.woody_axis import woody_axis
from outshine.world.mesh import Meshes

TAXON_ID = "outshine:taxon:wcvp:2609119"
PROFILE_ID = "umbrella-thorn-east-africa-1"
BARK = "umbrella thorn bark"
LEAVES = ("umbrella thorn foliage", "umbrella thorn shaded foliage")


def foliage_sprays(meshes, anchor, reach, rng, detailed):
    """Form gaps within each spray using separated groups of pinnate leaves."""
    count = 96
    angles = rng.uniform(0, 2 * np.pi, count)
    radial = rng.random(count) ** 0.65
    origin = anchor + np.column_stack(
        (
            reach * radial * np.cos(angles),
            reach * radial * np.sin(angles),
            0.23 * (1 - radial**2) + rng.uniform(-0.44, 0.35, count),
        )
    )
    angle = angles + rng.uniform(-1.25, 1.25, count)
    along = np.column_stack(
        (np.cos(angle), np.sin(angle), rng.uniform(-0.6, 0.6, count))
    )
    across = np.column_stack(
        (-np.sin(angle), np.cos(angle), rng.uniform(-0.7, 0.7, count))
    )
    stations = np.linspace(-0.43, 0.43, 9)
    split = np.array([-1, 1])
    stems = (
        origin[:, None, None, :]
        + (
            stations[None, :, None, None]
            + rng.uniform(-0.026, 0.026, (count, len(stations), 2, 1))
        )
        * along[:, None, None, :]
    )
    widths = (1.0 - 0.8 * abs(stations))[None, :, None, None] * rng.uniform(
        0.052, 0.145, (count, len(stations), 2, 1)
    )
    tips = stems + widths * split[None, None, :, None] * across[:, None, None, :]
    tips += along[:, None, None, :] * 0.075
    middle = 0.49 * stems + 0.51 * tips
    half_width = along[:, None, None, :] * rng.uniform(
        0.024, 0.051, (count, len(stations), 2, 1)
    )
    panels = np.stack((stems, middle - half_width, tips, middle + half_width), axis=-2)
    shade = rng.random(count) < 0.36
    if not detailed:
        panels = panels[::3, ::2]
        centres = panels.mean(axis=-2, keepdims=True)
        panels = centres + (panels - centres) * 2.1
        shade = shade[::3]
    for name, mask in zip(LEAVES, (~shade, shade), strict=True):
        vertices = panels[mask].reshape(-1, 3)
        offsets = np.arange(0, len(vertices), 4)[:, None, None]
        faces = (offsets + np.array([[0, 1, 2], [0, 2, 3]])).reshape(-1, 3)
        meshes.add(name, vertices, faces)


def crown_sector(meshes, start, end, heading, radius, spread, rng, detailed):
    chord = end - start
    controls = (
        start,
        start + 0.17 * chord + [0, 0, 0.34 * chord[2]],
        end - 0.40 * chord + [0, 0, 0.18 * chord[2]],
        end,
    )
    limb = woody_axis(
        meshes,
        controls,
        radius,
        0.026,
        rng,
        stations=15,
        sides=9,
        wander=0.29,
        material=BARK,
    )
    offset = rng.uniform(0.62, 0.96)
    stations = (0.34, 0.56, offset, 0.95)
    for i, joint in enumerate(stations):
        point = limb(joint)
        theta = heading + (-1) ** i * rng.uniform(0.47, 1.18)
        reach = spread * rng.uniform(0.9, 1.8)
        direction = np.array([np.cos(theta), np.sin(theta), 0])
        rise = rng.uniform(0.30, 0.85)
        tip = point + reach * direction + [0, 0, rise]
        controls = (
            point,
            point + 0.3 * reach * direction + [0, 0, 0.67 * rise],
            tip - 0.25 * reach * direction,
            tip,
        )
        woody_axis(
            meshes,
            controls,
            0.021 + 0.46 * radius * (1 - joint),
            0.006,
            rng,
            wander=0.055,
            material=BARK,
        )
        foliage_sprays(meshes, tip, reach * rng.uniform(0.76, 1.00), rng, detailed)
    foliage_sprays(meshes, end, spread * 1.22, rng, detailed)


@cache
def umbrella_thorn(variant, mature, detailed):
    rng = np.random.default_rng(0x9F50 + variant * 187 + 113 * mature)
    meshes = Meshes()
    width = rng.uniform(0.90, 1.10) * (1.0 if mature else 0.67)
    girth = rng.uniform(0.25, 0.40) * (1.0 if mature else 0.79)
    top = np.array([rng.uniform(0.4, 1.45), rng.uniform(-0.8, 1.0), 7.05])
    trunk = woody_axis(
        meshes,
        ((0, 0, -0.31), (-0.65, 0.31, 2.6), (0.30, -0.61, 4.95), top),
        girth,
        0.045,
        rng,
        stations=17,
        sides=16,
        trunk=True,
        wander=0.42,
        material=BARK,
    )
    count = rng.integers(6, 8)
    angles = np.linspace(0, 2 * np.pi, count, endpoint=False)
    angles += rng.uniform(-0.27, 0.27, count)
    forks = rng.permutation(np.linspace(0.28, 0.66, count))
    second_bole = None
    if variant % 3 != 1:
        direction = np.array([np.cos(angles[0]), np.sin(angles[0]), 0])
        base = trunk(rng.uniform(0.13, 0.23))
        shoulder = base + 1.7 * width * direction + [0, 0, 3.55]
        second_bole = woody_axis(
            meshes,
            (
                base,
                base + 0.27 * direction + [0, 0, 1.65],
                shoulder - 0.68 * direction - [0, 0, 0.33],
                shoulder,
            ),
            0.70 * girth,
            0.021,
            rng,
            stations=14,
            sides=12,
            wander=0.28,
            material=BARK,
        )
    for index, (joint, angle) in enumerate(zip(forks, angles, strict=True)):
        reach = width * rng.uniform(3.65, 5.25)
        z = rng.uniform(5.45, 6.35) + (0.12 if mature else -0.04) * reach
        end = np.array([reach * np.cos(angle), reach * np.sin(angle), z])
        collar = (1 - joint) ** 1.34 * girth * rng.uniform(0.60, 0.83)
        start = trunk(joint)
        if second_bole is not None and index in (0, count - 1):
            start = second_bole(0.65 + 0.21 * joint)
        crown_sector(meshes, start, end, angle, collar, width, rng, detailed)
    foliage_sprays(meshes, top, width * 1.3, rng, detailed)
    for name, (vertices, faces) in meshes.items():
        meshes[name] = (np.asarray(vertices, np.float32), np.asarray(faces, np.int32))
    return meshes
