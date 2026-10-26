"""Compare metric running-bond mortar filters with native facade masks."""

import itertools

import numpy as np
from facade_footprint import native_profiles
from outshine_ref.buildings.facades import interval_area

MODULE_M = np.array([0.25, 0.08])
JOINT_M = 0.01
BRICK_FRACTION = np.prod(1 - JOINT_M / MODULE_M)
BAND_M = 0.16
BAND_DARKENING = 0.35


def band(at, width, period, low, high):
    phase = (at - (low + high) / 2) / period + 0.5
    span = np.maximum(width / period, 0.0001)
    return np.clip(
        interval_area(phase - span / 2, phase + span / 2, (high - low) / period) / span,
        0,
        1,
    )


def brick_box(at, width):
    result = np.zeros(at.shape[:-1])
    for parity in (0, 1):
        row = band(
            at[..., 1],
            width[..., 1],
            2 * MODULE_M[1],
            parity * MODULE_M[1] + JOINT_M / 2,
            (parity + 1) * MODULE_M[1] - JOINT_M / 2,
        )
        column = band(
            at[..., 0] - parity * MODULE_M[0] / 2,
            width[..., 0],
            MODULE_M[0],
            JOINT_M / 2,
            MODULE_M[0] - JOINT_M / 2,
        )
        result += row * column
    return np.clip(result, 0, 1)


def brick_point(at):
    row = np.floor(at[..., 1] / MODULE_M[1])
    x = np.remainder(at[..., 0] - np.remainder(row, 2) * MODULE_M[0] / 2, MODULE_M[0])
    y = np.remainder(at[..., 1], MODULE_M[1])
    return (
        (x > JOINT_M / 2)
        & (x < MODULE_M[0] - JOINT_M / 2)
        & (y > JOINT_M / 2)
        & (y < MODULE_M[1] - JOINT_M / 2)
    ).astype(float)


def wall_box(at, width, profile):
    if profile[2] == 0 or profile[3] == 0:
        return np.ones(at.shape[:-1])
    u = band(
        at[..., 0],
        width[..., 0],
        profile[0],
        profile[0] * (1 - profile[2]) / 2,
        profile[0] * (1 + profile[2]) / 2,
    )
    v = band(
        at[..., 1],
        width[..., 1],
        profile[1],
        profile[1] * (1 - profile[3]) / 2,
        profile[1] * (1 + profile[3]) / 2,
    )
    storey_band = band(at[..., 1], width[..., 1], profile[1], -BAND_M / 2, BAND_M / 2)
    return np.maximum(1 - u * v - BAND_DARKENING * storey_band, 0)


def partitioned(at, dx, dy, profile, side):
    width = (np.abs(dx) + np.abs(dy)) / side
    result, mask = np.zeros(len(at)), np.zeros(len(at))
    nodes = (np.arange(side) + 0.5) / side - 0.5
    for x, y in itertools.product(nodes, repeat=2):
        point = at + x * dx + y * dy
        wall = wall_box(point, width, profile)
        result += (BRICK_FRACTION - brick_box(point, width)) * wall / side**2
        mask += wall / side**2
    return result, mask


def candidates(at, dx, dy, profile):
    joint9, mask = partitioned(at, dx, dy, profile, 3)
    result = {
        "point": (BRICK_FRACTION - brick_point(at)) * mask,
        "separate_box": (BRICK_FRACTION - brick_box(at, np.abs(dx) + np.abs(dy)))
        * mask,
        "joint_box": partitioned(at, dx, dy, profile, 1)[0],
        "joint4": partitioned(at, dx, dy, profile, 2)[0],
        "joint9": joint9,
        "joint16": partitioned(at, dx, dy, profile, 4)[0],
    }
    return {
        name: np.clip(value, -(1 - BRICK_FRACTION) * mask, BRICK_FRACTION * mask)
        for name, value in result.items()
    }


def fixtures():
    rng = np.random.default_rng(306)
    positions = rng.uniform([-3.2, -3.0], [3.2, 3.0], (12, 2))
    records = []
    for angle, width, shear, aspect in itertools.product(
        (0.0, 0.7, 1.3),
        (0.0025, 0.01, 0.025, 0.075, 0.15, 0.3, 0.8),
        (0.0, 1.8),
        (0.125, 1.0),
    ):
        u = np.array([np.cos(angle), np.sin(angle)])
        v = np.array([-np.sin(angle), np.cos(angle)])
        for at in positions:
            records.append((at, width * u, width * (aspect * v + shear * u)))
    for profile in native_profiles()[[1, 8]]:
        edge_u = profile[0] * np.array([1 - profile[2], 1 + profile[2]]) / 2
        edge_v = profile[1] * np.array([1 - profile[3], 1 + profile[3]]) / 2
        for x, y, size, angle in itertools.product(
            edge_u, edge_v, (0.01, 0.025, 0.075, 0.15, 0.3), (0.0, 0.7)
        ):
            u = np.array([np.cos(angle), np.sin(angle)])
            v = np.array([-np.sin(angle), np.cos(angle)])
            records.append(([x, y], size * u, size * (v + 0.8 * u)))
    tracks = []
    for kind in ("translation", "rotation", "growth"):
        begin = len(records)
        for step in range(61):
            t = step / 60
            angle = t * np.pi if kind == "rotation" else 0.7
            width = 0.0025 * 320**t if kind == "growth" else 0.075
            u = np.array([np.cos(angle), np.sin(angle)])
            v = np.array([-np.sin(angle), np.cos(angle)])
            for at in np.array([[0.125, 0.04], [1.612, 0.6815]]):
                p = at + (np.array([0.3, -0.08]) * t if kind == "translation" else 0)
                records.append((p, width * u, width * (v + 0.8 * u)))
        tracks.append({"kind": kind, "begin": begin, "steps": 61, "points": 2})
    for at in positions:
        records.append((at, [0, 0], [0, 0]))
        records.append((at, [0.1, 0.037], [0.2, 0.074]))
    return np.array(records), tracks, native_profiles()[[0, 1, 8]]
