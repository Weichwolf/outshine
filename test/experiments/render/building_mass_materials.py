"""Steady-repeat opaque material statistics from the owned building shader policy."""

import hashlib
import re
from pathlib import Path

import numpy as np

WALL_TINTS = np.array(
    [
        [1, 1, 1],
        [0.90, 0.94, 1],
        [1.07, 1.05, 1.04],
        [0.90, 0.84, 0.74],
        [0.83, 0.88, 0.91],
        [1, 0.89, 0.84],
        [0.62, 0.43, 0.32],
        [0.82, 0.69, 0.61],
    ],
    np.float32,
).astype(float)
ROOF_TINTS = np.array(
    [
        [0.20, 0.43, 0.71],
        [0.20, 0.43, 0.71],
        [0.60, 0.65, 0.75],
        [0.60, 0.65, 0.75],
        [1, 1, 1],
        [1, 1, 1],
        [0.85, 1.10, 1.10],
        [0.65, 0.90, 1.12],
    ],
    np.float32,
).astype(float)


def policy(root=None):
    root = root or Path(__file__).resolve().parents[3]
    files = {
        name: (root / name).read_bytes()
        for name in (
            "src/engine/region/WorldRegionBuild.cpp",
            "src/generators/building/BuildingMaterials.h",
            "src/content/shade/FacadeOpeningValues.h",
            "src/content/shade/FacadeProfiles.inc",
            "src/render/shaders/facadePattern.glsl",
            "src/render/shaders/facadeOpenings.glsl",
            "src/render/shaders/facadeBands.glsl",
            "src/render/shaders/roofTiles.glsl",
        )
    }

    def scalar(name):
        for data in files.values():
            match = re.search(
                r"float\s+" + name + r"\s*=\s*([0-9.]+)f?\s*;", data.decode()
            )
            if match:
                return float(np.float32(match[1]))
        raise ValueError("owned material scalar missing: " + name)

    wall = re.search(
        r"kBuildingWallColour\{\{([^}]+)",
        files["src/generators/building/BuildingMaterials.h"].decode(),
    )[1]
    wall = np.array([float(x.rstrip("f")) for x in wall.split(",")], np.float32).astype(
        float
    )
    profiles = re.findall(
        r"FACADE_PROFILE\(\w+,\s*(\d+),[^,]+,[^,]+,\s*([0-9.]+),\s*([0-9.]+)\)",
        files["src/content/shade/FacadeProfiles.inc"].decode(),
    )
    if [int(r[0]) for r in profiles] != list(range(len(profiles))):
        raise ValueError("contiguous owned facade profiles required")
    rough = np.array([scalar("kWallRoughness"), scalar("kTileRoughness")])
    palette = np.unique(
        np.array(
            [
                0.12,
                0.55,
                1,
                *rough,
                *(max(r, x) for r in rough for x in (0.78, 0.84, 0.90, 0.91)),
            ],
            np.float32,
        )
    )
    return {
        "base": np.array(
            [wall, [scalar(k) for k in ("kTileRed", "kTileGreen", "kTileBlue")]]
        ),
        "roughness": rough,
        "palette": palette,
        "fractions": np.array([[float(r[1]), float(r[2])] for r in profiles]),
        "inset": scalar("kPaneInset"),
        "mullion": [scalar("kMullionLowU"), scalar("kMullionHighU")],
        "sources_sha256": {
            name: hashlib.sha256(data).hexdigest() for name, data in files.items()
        },
    }


def material(at, code, stream, paint, *, openings=True, storeys_per_m=0, slope=0):
    """Mean diffuse colour plus a discrete roughness measure, never mean roughness.

    Complete-repeat fractions erase phase and finite-edge effects intentionally.
    This is a source-informed opaque approximation, not the native filtered shader.
    """
    if stream not in (0, 1) or not np.isfinite([storeys_per_m, slope]).all():
        raise ValueError("known material stream and finite derivatives required")
    colour = at["base"][stream].copy()
    rough = at["roughness"][stream]
    weights = np.zeros(len(at["palette"]))

    def add(value, mass):
        index = np.flatnonzero(at["palette"] == np.float32(value))
        if len(index) != 1 or mass < 0:
            raise ValueError("known nonnegative roughness mass required")
        weights[index[0]] += mass

    if code < 0:
        trim, ident = (-code - 1) % 16, (-code - 1) // 16
        if trim == 11:
            colour = np.array([0.025, 0.035, 0.045])
            add(0.12, 1)
        elif trim in (1, 2):
            if trim == 2:
                colour *= [0.50, 1.05, 1.45]
                rough = max(rough, 0.90)
                joint = 0
            else:
                variant = ident % 8
                colour *= ROOF_TINTS[variant]
                rough = max(
                    rough, 0.78 if variant < 2 else 0.84 if variant < 4 else rough
                )
                joint = 0.055 + 0.03 - 0.055 * 0.03 if slope >= 0.05 else 0
            colour *= (1 - joint) + joint * np.array([0.68, 0.70, 0.73])
            add(rough, 1 - joint)
            add(1, joint)
        else:
            if trim == 0:
                colour *= paint
            elif trim == 8:
                colour *= [0.68, 0.70, 0.72]
                rough = max(rough, 0.91)
            add(rough, 1)
    else:
        count = len(at["fractions"])
        style, field, variant = code % count, (code // count) % 3, code // (count * 3)
        if not 0 <= variant < 8:
            raise ValueError("known wall variant required")
        fx, fy = at["fractions"][style]
        opening = fx * fy
        inset = at["inset"]
        left = max(0, at["mullion"][0] - ((1 - fx) / 2 + inset))
        right = max(0, (1 + fx) / 2 - inset - at["mullion"][1])
        pane = (left + right) * max(0, fy - 2 * inset)
        if style == 7 or field == 1:
            pane = pane / opening if opening > 0 else 0
            opening, band = 1, 0
        else:
            opening, pane = (opening, pane) if openings else (0, 0)
            band = min(0.16 * max(storeys_per_m, 0), 1 - fy) if min(fx, fy) > 0 else 0
            colour *= WALL_TINTS[variant] * paint
        colour = (
            colour * (1 - opening - 0.35 * band)
            + np.array([0.48, 0.46, 0.41]) * (opening - pane)
            + np.array([0.025, 0.035, 0.045]) * pane
        )
        add(rough, 1 - opening)
        add(0.55, opening - pane)
        add(0.12, pane)
    if not np.isfinite(colour).all() or np.any(colour < 0) or np.any(colour > 1):
        raise ValueError("physical finite mean albedo required; no silent clamp")
    return colour, weights
