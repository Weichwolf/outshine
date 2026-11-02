"""Bounded orthographic plane queries and matching production NDC velocities."""

import numpy as np


def sequence(size, frames, moving):
    if (
        type(size) is not int
        or type(frames) is not int
        or not 2 <= size <= 128
        or not 1 <= frames <= 128
    ):
        raise ValueError("bounded image size and frame count required")
    x, y = np.meshgrid(np.arange(size), np.arange(size))
    pixels = np.column_stack((x.ravel(), y.ravel()))
    half = (size - 1) / 2
    base = np.diag([1, -np.linalg.norm([0.6, 1])])
    queries, motions, poses = [], [], []
    for frame in range(frames):
        angle = 0.23 + (frame * 0.002 if moving else 0)
        rotation = np.array(
            [[np.cos(angle), -np.sin(angle)], [np.sin(angle), np.cos(angle)]]
        )
        transform = base @ rotation * (0.45 / size)
        centre = np.array([0.503, 0.497]) + (
            [frame * 0.1 * 0.45 / size, 0] if moving else [0, 0]
        )
        at = (centre + (pixels - half) @ transform.T).astype(np.float32)
        motion = np.zeros_like(at)
        if moving and frame:
            previous = poses[-1]
            past = (at - previous[:, 2]) @ np.linalg.inv(previous[:, :2]).T + half
            motion = ((pixels - past) * [2 / size, -2 / size]).astype(np.float32)
        row = np.zeros((len(at), 8), dtype="<f4")
        row[:, :2], row[:, 2:4], row[:, 4:6], row[:, 6] = (
            at,
            transform[:, 0],
            transform[:, 1],
            0.5,
        )
        reach = (np.abs(row[:, 2:4]) + np.abs(row[:, 4:6])) / 2
        if np.any(row[:, :2] - reach < 0) or np.any(row[:, :2] + reach > 1):
            raise ValueError("frame footprint exceeds resident diagnostic tile")
        queries.append(row)
        motions.append(motion)
        poses.append(np.column_stack((transform, centre)))
    return np.asarray(queries), np.asarray(motions), np.asarray(poses)
