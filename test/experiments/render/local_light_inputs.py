"""Procedural benchmark lamps on retained real Vienna axes and DEM priors (#448)."""

import hashlib
import json
from dataclasses import dataclass
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[3]
AXES = ROOT / "test/experiments/transport/fixtures/vertical-transition-cases.json"


@dataclass
class Lights:
    positions: np.ndarray
    ranges: np.ndarray
    colours: np.ndarray
    source_ids: list[str]


@dataclass
class Camera:
    eye: np.ndarray
    basis: np.ndarray
    perspective: bool
    half: np.ndarray
    width: int = 1920
    height: int = 1080
    near: float = 0.1
    depth_extent: float = 4096

    def view(self, points):
        return (np.asarray(points) - self.eye) @ self.basis.T


def street_lights(spacing=16.0, count=None):
    document = json.loads(AXES.read_text())
    positions, ids = [], []
    for row in sorted(document["cases"], key=lambda r: r["source_id"]):
        xy = np.asarray(row["xy"], float)
        direction = xy[1] - xy[0]
        length = float(np.linalg.norm(direction))
        sideways = np.array([-direction[1], direction[0]]) / length
        z = np.asarray(row["target_heights_m"], float)
        for index, along in enumerate(np.arange(spacing / 2, length, spacing)):
            fraction = along / length
            centre = xy[0] + fraction * direction
            for side in [-1, 1]:
                position = np.r_[
                    centre
                    + side
                    * ((row["width_m"] if row["width_m"] is not None else 3.0) / 2 + 1)
                    * sideways,
                    z[0] + fraction * (z[1] - z[0]) + 6,
                ]
                positions.append(position)
                ids.append(f"{row['source_id']}/procedural-lamp/{index}/{side}")
    positions = np.asarray(positions, dtype="<f4").astype(float)
    if count is not None:
        if count > len(positions):
            raise ValueError(
                "declared scene exceeds its source-derived lamp population"
            )
        selected = np.linspace(0, len(positions) - 1, count).astype(int)
        positions, ids = positions[selected], [ids[i] for i in selected]
    colours = np.tile([600, 390, 180], (len(positions), 1)).astype("<f4").astype(float)
    return Lights(positions, np.full(len(positions), 32.0), colours, ids), {
        "fixture": str(AXES.relative_to(ROOT)),
        "fixture_sha256": hashlib.sha256(AXES.read_bytes()).hexdigest(),
        "source_sha256": document["source_sha256"],
        "source_axes": len(document["cases"]),
        "spacing_m": spacing,
        "unspecified_width_axes": sum(
            row["width_m"] is None for row in document["cases"]
        ),
        "procedural_fallback_width_m": 3.0,
        "pole_offset_from_edge_m": 1.0,
        "pole_height_above_interpolated_prior_m": 6.0,
        "scope": "Actual source XY and linearly interpolated DEM endpoint priors. Lamps, offsets, heights, photometry and receivers are explicitly procedural benchmark inputs; no observed lamps or constructed CAD-road/terrain claim.",
    }


def camera(lights, name):
    centre = np.median(lights.positions, axis=0)
    centre[2] -= 6
    if name == "ortho":
        eye = centre + [0, 0, 1200]
        forward = np.array([0, 0, -1.0])
        right = np.array([1.0, 0, 0])
        half = np.array([1200, 675.0])
    else:
        eye = centre + [0, -240, 8]
        target = centre + ([0, 600, 0] if name == "street" else [600, 0, 0])
        forward = target - eye
        forward /= np.linalg.norm(forward)
        right = np.cross(forward, [0, 0, 1])
        right /= np.linalg.norm(right)
        half = np.tan(np.deg2rad(60) / 2) * np.array([16 / 9, 1])
    up = np.cross(right, forward)
    return Camera(eye, np.stack((right, up, forward)), name != "ortho", half)


def receivers(frame, depth, width=160, height=90):
    x = (np.arange(width) + 0.5) * 2 / width - 1
    y = 1 - (np.arange(height) + 0.5) * 2 / height
    xx, yy = np.meshgrid(x, y)
    scale = depth if frame.perspective else 1
    view = np.stack(
        (
            xx * frame.half[0] * scale,
            yy * frame.half[1] * scale,
            np.full_like(xx, depth),
        ),
        axis=-1,
    )
    points = (view.reshape(-1, 3) @ frame.basis + frame.eye).astype("<f4").astype(float)
    return points, np.tile(-frame.basis[2], (len(points), 1))
