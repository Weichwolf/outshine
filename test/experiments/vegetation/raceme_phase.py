"""Prepared ordered floral births and authored static phenology, no botanical clock."""

import numpy as np
from outshine_ref.vegetation.curve_frame import arc_frame
from understory_plan import UPRIGHT, LilyStudy


def prepare(seed=121, flowers=9):
    base = LilyStudy(seed, flowers).recipe()
    rng = np.random.Generator(np.random.PCG64(seed ^ 331))
    births = np.r_[0.0, np.cumsum(rng.uniform(0.08, 0.12, flowers - 1))]
    fractions = 0.38 + 0.58 * births / births[-1]
    nodes = (
        arc_frame(base["stem_length_m"], base["stem_bend_rad"], 0, fractions)[0]
        @ UPRIGHT.T
    )
    for stalk, node in zip(base["stalks"], nodes, strict=True):
        stalk["node"] = node.tolist()
    return {
        "format": "raceme-phenology-study-1",
        "plant": base,
        "births": births.tolist(),
        "duration": 0.85,
        "opening": 0.68,
        "side_angles": rng.uniform(-0.12, 0.12, flowers).tolist(),
        "scope": "Authored floral phase units after SIGGRAPH1988 section4.1; fixed vegetative plant, not measured botanical time or continuous animation.",
    }


def states(recipe, phase):
    births = np.asarray(recipe["births"], float)
    angle = np.asarray(recipe["side_angles"], float)
    if (
        recipe["format"] != "raceme-phenology-study-1"
        or births.shape != (len(recipe["plant"]["stalks"]),)
        or not 3 <= len(births) <= 16
        or not np.isfinite(births).all()
        or births[0] != 0
        or births[-1] > 2
        or np.any(np.diff(births) <= 0)
        or angle.shape != births.shape
        or not np.isfinite(angle).all()
        or np.any(abs(angle) > 0.2)
        or not np.isfinite(phase)
        or not -1 <= phase <= 4
        or recipe["duration"] != 0.85
        or recipe["opening"] != 0.68
    ):
        raise ValueError(
            "complete ordered phase recipe and bounded scalar phase required"
        )
    growth = np.clip((phase - births) / recipe["duration"], 0, 1)
    smooth = growth**2 * (3 - 2 * growth)
    tilt = 0.4 + 2.5 * smooth
    direction = np.c_[
        np.sin(tilt) * np.cos(angle), np.sin(tilt) * np.sin(angle), np.cos(tilt)
    ]
    origins, ends, frames = [], [], []
    for i, stalk in enumerate(recipe["plant"]["stalks"]):
        node, scale = np.asarray(stalk["node"]), stalk["scale"]
        radial = np.array((np.cos(angle[i]), np.sin(angle[i]), 0.0))
        end = node + (0.004 + 0.014 * smooth[i]) * scale * radial
        end[2] += (0.006 - 0.012 * smooth[i]) * scale
        z = -direction[i]
        x = np.cross((0, 1, 0), z)
        x /= np.linalg.norm(x)
        frame = np.column_stack((x, np.cross(z, x), z))
        origins.append(node)
        ends.append(end)
        frames.append(frame)
    return {
        "active": phase > births,
        "growth": growth,
        "open": growth >= recipe["opening"],
        "nodes": np.asarray(origins),
        "ends": np.asarray(ends),
        "frames": np.asarray(frames),
    }
