"""Low, irregular Mojave pinyon crowns built from curved limbs and short needles."""

from functools import cache

import numpy as np

from outshine.vegetation.needle_pattern import NeedlePattern
from outshine.vegetation.plant_levels import GOLDEN_ANGLE
from outshine.vegetation.plant_plan import AxisState
from outshine.vegetation.shoot_plan import normal_frame
from outshine.vegetation.woody_axis import woody_axis
from outshine.world.mesh import Meshes

PINYON_TAXON = "outshine:taxon:wcvp:380231"
PINYON_PROFILE = "mojave-pinyon-1"
PINYON_UNITS = frozenset(("JOTR-2012:10160", "JOTR-2012:10161"))
BARK = "pinyon bark"
FOLIAGE = "pinyon needles"
NEEDLE_PATTERN = NeedlePattern(
    spacing=0.0021,
    group_size=1,
    first=0.12,
    pitch=0.98,
    angle_variation=0.36,
    short_length=0.001,
    scale_variation=0.15,
)
NEEDLE_LENGTH_M = 0.030
NEEDLE_RADIUS_M = 0.00084


def shoot_crown(curve, heading, extent, rng, shoots):
    for parameter in np.linspace(0.35, 1, 10):
        origin = curve(parameter)
        theta = heading + rng.uniform(-1.7, 1.7)
        reach = extent * rng.uniform(0.19, 0.32)
        out = np.array([np.cos(theta), np.sin(theta), rng.uniform(-0.28, 0.90)])
        for side in (-1, 1):
            shoots.append((origin, reach * out + [0, side * 0.057, side * 0.071]))


def limb(meshes, start, end, heading, base, rng, shoots):
    delta = end - start
    span = np.linalg.norm(delta[:2])
    controls = (
        start,
        start + 0.34 * delta - [0, 0, 0.13 * span],
        end - 0.22 * delta - [0, 0, 0.09 * span],
        end,
    )
    bough = woody_axis(
        meshes,
        controls,
        base,
        0.012,
        rng,
        stations=13,
        sides=9,
        wander=min(0.12, 0.065 * span),
        material=BARK,
    )
    for index, fraction in enumerate(np.linspace(0.30, 0.96, 7)):
        attachment = bough(fraction)
        angle = heading + (-1) ** index * rng.uniform(0.47, 1.48)
        extent = rng.uniform(0.49, 1.08) * (1.00 - 0.29 * fraction)
        shift = extent * np.array(
            [np.cos(angle), np.sin(angle), rng.uniform(0.32, 0.86)]
        )
        tip = attachment + shift
        controls = (attachment, attachment + 0.42 * shift - [0, 0, 0.11 * extent], tip)
        branch = woody_axis(
            meshes,
            controls,
            base * (0.33 - 0.13 * fraction),
            0.004,
            rng,
            stations=7,
            sides=6,
            wander=0.043 * extent,
            material=BARK,
        )
        shoot_crown(branch, angle, extent, rng, shoots)
    shoot_crown(bough, heading, 1.04, rng, shoots)


def pinyon_scaffold(meshes, variant, rng):
    height = (3.16, 4.07, 4.76, 5.39)[variant]
    width = (1.38, 1.84, 2.24, 2.41)[variant]
    girth = (0.127, 0.188, 0.228, 0.274)[variant]
    sway = rng.uniform(-0.5, 0.5, 2) * width
    end = np.r_[0.76 * sway, 0.94 * height]
    controls = ((0, 0, -0.31), (*(-0.38 * sway), 0.70), (*sway, height * 0.52), end)
    trunk = woody_axis(
        meshes,
        controls,
        girth,
        0.024,
        rng,
        stations=19,
        sides=14,
        trunk=True,
        wander=0.103,
        material=BARK,
    )
    shoots = []
    direction = rng.uniform(0, 2 * np.pi)
    for index, position in enumerate(np.linspace(0.34, 0.97, 8)):
        root = trunk(position)
        theta = direction + index * GOLDEN_ANGLE + rng.uniform(-0.44, 0.44)
        radius = width * (1.16 - 0.64 * position) * rng.uniform(0.72, 1.42)
        endpoint = root + radius * np.array([np.cos(theta), np.sin(theta), 0.0])
        endpoint[2] = height * rng.uniform(
            0.67 + 0.24 * position, 0.88 + 0.19 * position
        )
        limb(
            meshes,
            root,
            endpoint,
            theta,
            girth * (0.65 - 0.38 * position) * rng.uniform(0.8, 1.13),
            rng,
            shoots,
        )
    return shoots


def needle_mesh(shoots, seed):
    """Sample physical needles to preserve the close silhouette of the sprays."""
    stride = 3
    width = NEEDLE_RADIUS_M
    length = NEEDLE_LENGTH_M
    corners = np.array(
        [
            [-0.2 * width, 0, 0],
            [0.2 * width, 0, 0],
            [-width, 0.44 * length, 0],
            [width, 0.44 * length, 0],
            [0, length, 0.16 * length],
        ]
    )
    pattern_faces = np.array([[0, 1, 2], [1, 3, 2], [2, 3, 4]])
    parts = []
    for index, (origin, vector) in enumerate(shoots):
        axis = AxisState(
            (),
            index,
            np.linalg.norm(vector),
            0.0,
            origin,
            normal_frame(vector, (0, 0, 1)),
            0.0,
            0.0,
        )
        rows = NEEDLE_PATTERN.detail(axis, (0.0017, 0.0005), seed, 384)
        select = slice(index % stride, None, stride)
        frames = rows["frames"][select]
        origins = rows["origins"][select]
        sizes = rows["scales"][select]
        points = np.einsum("mj,nij->nmi", corners, frames)
        points = points * sizes[:, None, None] + origins[:, None, :]
        parts.append(points)
    panels = np.concatenate(parts, axis=0)
    offsets = np.arange(len(panels), dtype=np.int32) * len(corners)
    faces = (pattern_faces + offsets[:, None, None]).reshape(-1, 3)
    return panels.reshape(-1, 3), faces


def needle_sprays(shoots, seed):
    """Gather the short needles of a shoot into a pair of broken crown profiles."""
    starts, directions = np.asarray(shoots).transpose(1, 0, 2)
    frames = normal_frame(directions, (0, 0, 1))
    rng = np.random.default_rng(seed ^ 0x5F7B)
    axis = frames[:, :, 1]
    phase = rng.uniform(0, np.pi, len(starts))
    side, vertical = frames[:, :, 0], frames[:, :, 2]
    cross = np.cos(phase)[:, None] * side + np.sin(phase)[:, None] * vertical
    normal = np.cross(axis, cross)
    length = np.linalg.norm(directions, axis=1)
    centre = starts + rng.uniform(0.56, 0.78, (len(starts), 1)) * directions
    profile = np.array(
        [
            [-0.18, -0.5],
            [0.25, -0.42],
            [0.47, -0.14],
            [0.39, 0.22],
            [0.14, 0.5],
            [-0.30, 0.37],
            [-0.49, 0.04],
            [-0.39, -0.29],
        ]
    )
    width = (0.063 + 0.24 * length) * rng.uniform(0.75, 1.35, len(starts))
    length = (0.059 + 0.92 * length) * rng.uniform(0.85, 1.22, len(starts))
    contour = profile * rng.uniform(0.83, 1.14, (len(starts), len(profile), 1))
    midpoints = 0.5 * (contour + np.roll(contour, -1, axis=1))
    midpoints *= rng.uniform(0.53, 1.18, (*midpoints.shape[:2], 1))
    outline = np.stack((contour, midpoints), axis=2).reshape(len(starts), -1, 2)
    contour = np.pad(outline, ((0, 0), (0, 1), (0, 0)))
    boundary = contour.shape[1]
    centres = (
        centre[:, None, :]
        + length[:, None, None] * contour[:, :, 1, None] * axis[:, None, :]
    )
    offsets = width[:, None, None] * contour[:, :, 0, None]
    profiles = np.stack(
        (
            centres + offsets * cross[:, None, :],
            centres + 0.88 * offsets * normal[:, None, :],
        ),
        axis=1,
    ).reshape(-1, boundary, 3)
    edge = np.arange(boundary - 1)
    triangle = np.column_stack(
        (np.full_like(edge, boundary - 1), edge, np.roll(edge, -1))
    )
    indices = (
        triangle + (boundary * np.arange(len(profiles), dtype=np.int32))[:, None, None]
    )
    return profiles.reshape(-1, 3), indices.reshape(-1, 3)


def shoot_wood(shoots):
    origins, directions = np.asarray(shoots).transpose(1, 0, 2)
    frames = normal_frame(directions, (0, 0, 1))
    angle = np.arange(3) * (2 * np.pi / 3)
    radial = (
        np.cos(angle)[None, :, None] * frames[:, None, :, 0]
        + np.sin(angle)[None, :, None] * frames[:, None, :, 2]
    )
    base = origins[:, None, :] + 0.0017 * radial
    tips = (origins + directions)[:, None, :] + 0.00045 * radial
    vertices = np.concatenate((base, tips), axis=1)
    triangles = np.array(
        [
            [0, 3, 1],
            [1, 3, 4],
            [1, 4, 2],
            [2, 4, 5],
            [2, 5, 0],
            [0, 5, 3],
            [0, 1, 2],
            [3, 5, 4],
        ]
    )
    faces = triangles + (6 * np.arange(len(shoots), dtype=np.int32))[:, None, None]
    return vertices.reshape(-1, 3), faces.reshape(-1, 3)


@cache
def pinyon(variant, detailed):
    seed = 0xE851 + variant * 193
    scaffold = Meshes()
    shoots = pinyon_scaffold(scaffold, variant, np.random.default_rng(seed))
    scaffold.add(BARK, *shoot_wood(shoots))
    points, faces = needle_sprays(shoots, seed)
    if detailed:
        organs, triangles = needle_mesh(shoots, seed)
        faces = np.concatenate((faces, triangles + len(points)))
        points = np.concatenate((points, organs))
    meshes = dict(scaffold) | {FOLIAGE: (points, faces)}
    data = {
        name: (
            np.asarray(vertices, dtype=np.float32),
            np.asarray(faces, dtype=np.int32),
        )
        for name, (vertices, faces) in meshes.items()
    }
    radius = max(np.linalg.norm(v[:, :2], axis=1).max() for v, _ in data.values())
    return data, float(radius)
