"""Arc-length circular centreline and untwisted/twisted frames, local Y forward."""

import numpy as np


def arc_frame(length, bend, twist, t):
    t = np.asarray(t, dtype=np.float64)
    angle, roll = bend * t, twist * t
    zeros = np.zeros_like(t)
    centre = np.stack(
        (
            zeros,
            length * t * np.sinc(angle / np.pi),
            length * bend * t * t / 2 * np.sinc(angle / (2 * np.pi)) ** 2,
        ),
        axis=-1,
    )
    tangent = np.stack((zeros, np.cos(angle), np.sin(angle)), axis=-1)
    lateral = np.stack(
        (np.cos(roll), -np.sin(roll) * np.sin(angle), np.sin(roll) * np.cos(angle)),
        axis=-1,
    )
    normal = np.cross(lateral, tangent)
    return centre, tangent, lateral, normal
